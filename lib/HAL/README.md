# HAL

Purpose: isolate Arduino and ESP32 APIs behind small hardware adapters.

Public API: `IClock`, `IDelay`, `IConsole`, and `ISpiBus`; ESP32 implementations are composition-root dependencies.

Roadmap: add I2C, UART, ADC, GPIO, storage, and watchdog adapters when hardware users require them.

Decision: interfaces are intentionally narrow so host tests can provide fakes without carrying Arduino headers.
