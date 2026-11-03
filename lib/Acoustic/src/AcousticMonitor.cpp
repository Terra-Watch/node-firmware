#include "AcousticMonitor.h"

#include <Arduino.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "AcousticConfig.h"
#include "AcousticTrigger.h"
#include "AudioCapture.h"

namespace terrawatch::acoustic
{
    namespace
    {
        constexpr size_t kPreSamples = ACOUSTIC_PRE_MS * (ACOUSTIC_SAMPLE_RATE / 1000);
        constexpr size_t kPostSamples = ACOUSTIC_POST_MS * (ACOUSTIC_SAMPLE_RATE / 1000);
        static_assert(ACOUSTIC_PRE_MS + ACOUSTIC_POST_MS == 1000, "PRE + POST must be 1 s");
        static_assert(kPreSamples + kPostSamples == ACOUSTIC_WINDOW_SAMPLES, "window length mismatch");
        static_assert(ACOUSTIC_RING_SAMPLES >= ACOUSTIC_WINDOW_SAMPLES + 4000, "ring must hold a window plus margin");

        SampleRing *g_ring = nullptr;
        int16_t *g_window = nullptr;
        Inmp441Capture g_capture;
        Max4466Sampler g_sampler;
        TaskHandle_t g_eventTask = nullptr;
        volatile uint32_t g_events = 0;

        void *psramOrInternal(size_t bytes)
        {
            void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }

        EnergyDetectorConfig detectorConfig()
        {
            EnergyDetectorConfig c{};
            c.relative = true;
            c.highDb = ACOUSTIC_TRIGGER_HIGH_DB;
            c.lowDb = ACOUSTIC_TRIGGER_LOW_DB;
            c.sustainBlocks = ACOUSTIC_SUSTAIN_BLOCKS;
            c.maxFloor = ACOUSTIC_MAX_NOISE_FLOOR_RMS;
            c.warmupBlocks = ACOUSTIC_NOISE_WARMUP_MS * MAX4466_SAMPLE_RATE / 1000 / MAX4466_BLOCK_SIZE;
            return c;
        }

        void captureTask(void *)
        {
            for (;;)
                if (g_capture.pump(100) < 0) vTaskDelay(pdMS_TO_TICKS(50));
        }

        void triggerTask(void *)
        {
            EnergyDetector detector(detectorConfig());
            BlockStats st;
            uint32_t lastDebugMs = 0, cooldownEndMs = 0;
            bool reported = false;
            double dcSum = 0.0;
            uint32_t dcBlocks = 0;
            // The ADC DMA delivers a start-up transient; ignore the first 0.5 s so it cannot pollute the warm-up.
            int settleLeft = 500 * MAX4466_SAMPLE_RATE / 1000 / MAX4466_BLOCK_SIZE;
            Serial.printf("[ACOUSTIC] trigger monitor started (+%.0f dB, %d Hz, %d-sample blocks)\n",
                          static_cast<double>(ACOUSTIC_TRIGGER_HIGH_DB), MAX4466_SAMPLE_RATE, MAX4466_BLOCK_SIZE);
            for (;;)
            {
                if (!g_sampler.readBlock(st, 100)) continue;
                if (settleLeft > 0) { --settleLeft; continue; }
                const uint32_t now = millis();
                const auto edge = detector.update(st.rms);
                if (!reported)
                {
                    dcSum += st.mean;
                    ++dcBlocks;
                    if (detector.warmedUp())
                    {
                        reported = true;
                        Serial.printf("[ACOUSTIC] DC mean %.0f counts, noise floor %.1f RMS\n", dcSum / dcBlocks, static_cast<double>(detector.noiseFloor()));
                        if (detector.sensorFault())
                            Serial.println("[ACOUSTIC] SENSOR FAULT: MAX4466 input is flat or too noisy; trigger disabled until reset (check wiring, power, gain)");
                        else
                            Serial.printf("[ACOUSTIC] sensor OK, trigger at %.1f RMS\n", static_cast<double>(detector.highThreshold()));
                    }
                }
#if ACOUSTIC_DEBUG_INTERVAL_MS > 0
                if (now - lastDebugMs >= ACOUSTIC_DEBUG_INTERVAL_MS)
                {
                    lastDebugMs = now;
                    Serial.printf("[ACOUSTIC] RMS=%.1f floor=%.1f dB=%+.1f%s\n", static_cast<double>(st.rms), static_cast<double>(detector.noiseFloor()),
                                  static_cast<double>(detector.lastDeltaDb()), detector.warmedUp() ? "" : " (warming up)");
                }
#endif
                if (edge == EnergyDetector::Edge::Rising && static_cast<int32_t>(now - cooldownEndMs) >= 0)
                {
                    cooldownEndMs = now + ACOUSTIC_COOLDOWN_MS;
                    Serial.printf("[ACOUSTIC] trigger (RMS=%.1f, %+.1f dB)\n", static_cast<double>(st.rms), static_cast<double>(detector.lastDeltaDb()));
                    xTaskNotify(g_eventTask, g_ring->total(), eSetValueWithOverwrite);
                }
            }
        }

        void eventTask(void *)
        {
            for (;;)
            {
                uint32_t triggerIndex = 0;
                xTaskNotifyWait(0, 0xFFFFFFFFu, &triggerIndex, portMAX_DELAY);
                const uint32_t deadline = millis() + 3000;
                while (static_cast<uint32_t>(g_ring->total() - triggerIndex) < kPostSamples && millis() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
                if (!g_ring->copyWindow(triggerIndex - static_cast<uint32_t>(kPreSamples), ACOUSTIC_WINDOW_SAMPLES, g_window))
                {
                    Serial.println("[ACOUSTIC] audio window unavailable (INMP441 capture stalled?)");
                    continue;
                }
                const PcmStats s = computePcmStats(g_window, ACOUSTIC_WINDOW_SAMPLES);
                ++g_events;
                Serial.printf("[ACOUSTIC EVENT #%lu] INMP441 window: rms=%.0f min=%d max=%d samples=%u\n", static_cast<unsigned long>(g_events),
                              static_cast<double>(s.rms), s.min, s.max, static_cast<unsigned>(s.count));
            }
        }
    } // namespace

    bool AcousticMonitor::begin()
    {
        auto *storage = static_cast<int16_t *>(psramOrInternal(sizeof(int16_t) * ACOUSTIC_RING_SAMPLES));
        g_window = static_cast<int16_t *>(psramOrInternal(sizeof(int16_t) * ACOUSTIC_WINDOW_SAMPLES));
        if (storage == nullptr || g_window == nullptr) return false;
        g_ring = new SampleRing(storage, ACOUSTIC_RING_SAMPLES);
        if (!g_capture.begin(TW_I2S_BCLK_PIN, TW_I2S_WS_PIN, TW_I2S_DATA_PIN, ACOUSTIC_SAMPLE_RATE, ACOUSTIC_I2S_SHIFT, g_ring))
        {
            Serial.println("[ACOUSTIC] INMP441 init failed");
            return false;
        }
        if (!g_sampler.begin(TW_MAX4466_PIN, MAX4466_SAMPLE_RATE, MAX4466_BLOCK_SIZE))
        {
            Serial.println("[ACOUSTIC] MAX4466 init failed");
            return false;
        }
        xTaskCreatePinnedToCore(eventTask, "ac-event", 4096, nullptr, 1, &g_eventTask, 0);
        xTaskCreatePinnedToCore(captureTask, "ac-capture", 4096, nullptr, 3, nullptr, 0);
        xTaskCreatePinnedToCore(triggerTask, "ac-trigger", 8192, nullptr, 2, nullptr, 0);
        return true;
    }

    std::uint32_t AcousticMonitor::eventCount() const { return g_events; }
} // namespace terrawatch::acoustic
