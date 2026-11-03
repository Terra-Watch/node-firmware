#ifdef TW_ACOUSTIC_MODEL
#include "AcousticClassifier.h"

#include <Arduino.h>
#include <ESP_TF.h>
#include <cmath>
#include <cstring>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "MelFrontend.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace terrawatch::acoustic
{
    namespace
    {
        constexpr std::size_t kArenaBytes = 700 * 1024;
        void *spiram(std::size_t bytes) { return heap_caps_aligned_alloc(16, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    }

    struct AcousticClassifier::Impl
    {
        MelFrontend frontend;
        std::uint8_t *model = nullptr, *arena = nullptr;
        float *patch = nullptr;
        std::int16_t *pcm = nullptr;
        tflite::MicroMutableOpResolver<12> resolver;
        tflite::MicroInterpreter *interpreter = nullptr;
        TaskHandle_t task = nullptr;
        volatile bool busy = false;
        Callback callback = nullptr;
    };

    bool AcousticClassifier::begin(const std::uint8_t *model, std::size_t modelBytes, Callback callback)
    {
        impl_ = new Impl();
        Impl &m = *impl_;
        m.model = static_cast<std::uint8_t *>(spiram(modelBytes)); m.arena = static_cast<std::uint8_t *>(spiram(kArenaBytes));
        m.patch = static_cast<float *>(heap_caps_malloc(sizeof(float) * MelFrontend::kBands * MelFrontend::kFrames, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        m.pcm = static_cast<std::int16_t *>(spiram(sizeof(std::int16_t) * MelFrontend::kSamples));
        if (!m.model || !m.arena || !m.patch || !m.pcm || !m.frontend.begin()) { Serial.println("[CLASSIFIER] out of memory"); return false; }
        std::memcpy(m.model, model, modelBytes);                    // weights in PSRAM: a few percent faster than flash
        m.resolver.AddConv2D(); m.resolver.AddDepthwiseConv2D(); m.resolver.AddAdd(); m.resolver.AddMul(); m.resolver.AddMean(); m.resolver.AddReshape(); m.resolver.AddHardSwish();
        m.resolver.AddFullyConnected(); m.resolver.AddLogistic(); m.resolver.AddPad(); m.resolver.AddReduceMax();
        m.interpreter = new tflite::MicroInterpreter(tflite::GetModel(m.model), m.resolver, m.arena, kArenaBytes);
        if (m.interpreter->AllocateTensors() != kTfLiteOk) { Serial.println("[CLASSIFIER] AllocateTensors failed (arena too small or an operation is missing)"); return false; }
        m.callback = callback;
        Serial.printf("[CLASSIFIER] ready: model %u bytes, arena used %u bytes\n", (unsigned)modelBytes, (unsigned)m.interpreter->arena_used_bytes());
        if (callback) xTaskCreatePinnedToCore(taskEntry, "ac-classify", 16384, this, 1, &m.task, 0);      // core 0: the Arduino loop (radio, sensors) has core 1 to itself; sharing it doubled the model time
        return true;
    }

    bool AcousticClassifier::classify(const std::int16_t *pcm, ClassifierResult &out)
    {
        if (!impl_) return false;
        Impl &m = *impl_;
        const uint32_t t0 = micros();
        m.frontend.compute(pcm, m.patch);
        const uint32_t t1 = micros();
        TfLiteTensor *in = m.interpreter->input(0);                 // [1, 106 bands, 100 frames, 1], int8
        const float scale = in->params.scale; const int zero = in->params.zero_point;
        for (int i = 0; i < MelFrontend::kBands * MelFrontend::kFrames; ++i)
        {
            int q = static_cast<int>(std::lround(m.patch[i] / scale)) + zero;
            in->data.int8[i] = static_cast<std::int8_t>(q < -128 ? -128 : q > 127 ? 127 : q);
        }
        if (m.interpreter->Invoke() != kTfLiteOk) return false;
        const uint32_t t2 = micros();
        const TfLiteTensor *o = m.interpreter->output(0);
        float logits[6], mx = -1e30f;
        for (int i = 0; i < 6; ++i) { logits[i] = (static_cast<float>(o->data.int8[i]) - o->params.zero_point) * o->params.scale; if (logits[i] > mx) mx = logits[i]; }
        float sum = 0.0f; for (int i = 0; i < 6; ++i) { out.probs[i] = std::exp(logits[i] - mx); sum += out.probs[i]; }
        for (int i = 0; i < 6; ++i) out.probs[i] /= sum;
        out.frontendMs = (t1 - t0) / 1000.0f; out.modelMs = (t2 - t1) / 1000.0f;
        return true;
    }

    bool AcousticClassifier::submit(const std::int16_t *pcm)
    {
        if (!impl_ || !impl_->task || impl_->busy) return false;
        std::memcpy(impl_->pcm, pcm, sizeof(std::int16_t) * MelFrontend::kSamples);
        impl_->busy = true;
        xTaskNotifyGive(impl_->task);
        return true;
    }

    void AcousticClassifier::taskEntry(void *self)
    {
        auto *c = static_cast<AcousticClassifier *>(self);
        for (;;)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ClassifierResult r{};
            if (c->classify(c->impl_->pcm, r) && c->impl_->callback) c->impl_->callback(r);
            c->impl_->busy = false;
        }
    }
} // namespace terrawatch::acoustic
#endif
