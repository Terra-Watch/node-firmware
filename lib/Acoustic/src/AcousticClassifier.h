#pragma once
// The sound classifier: 1 s of 16 kHz audio -> class probabilities {chainsaw, blast, fire, wood, wind, background}, with the int8 mn10_as (acoustic-ml) on TFLite Micro + ESP-NN.
// Built only when TW_ACOUSTIC_MODEL=1 (it needs the ESP_TF library, PSRAM and the model file). One classification takes about 1.85 s (front end 0.175 s + model 1.67 s on the ESP32-S3).

#ifdef TW_ACOUSTIC_MODEL
#include <cstddef>
#include <cstdint>

namespace terrawatch::acoustic
{
    struct ClassifierResult
    {
        float probs[6];                  // softmax, in the order above
        float frontendMs, modelMs;
    };

    class AcousticClassifier
    {
    public:
        using Callback = void (*)(const ClassifierResult &);
        // model: the .tflite bytes (embedded in the firmware). Copies them to PSRAM. callback runs on the classifier task (core 0, priority 1, below the audio capture and trigger tasks).
        bool begin(const std::uint8_t *model, std::size_t modelBytes, Callback callback);
        bool submit(const std::int16_t *pcm);                       // copies 16000 samples and queues them; false when busy or not started
        bool classify(const std::int16_t *pcm, ClassifierResult &out);   // synchronous, for tests; do not call while the task may run

    private:
        static void taskEntry(void *self);
        struct Impl;
        Impl *impl_ = nullptr;
    };
} // namespace terrawatch::acoustic
#endif
