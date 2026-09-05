/**
 * BoatNode_FreeRTOS.ino
 * PalavariRaksha — LoRa Mesh Maritime Emergency Safety System
 *
 * Component  : BoatNode
 * MCU        : ESP32-S3
 * Framework  : Arduino + ESP32 FreeRTOS
 * Phase      : 1 — FreeRTOS Sensor Acquisition Foundation
 *
 * Author     : PalavariRaksha Team (RESQ-LoRa / Jalari)
 * Target     : Chennai coastal waters (lat ≈ 13.08N, lon ≈ 80.27E)
 *
 * PURPOSE:
 *   This sketch implements the FreeRTOS-based embedded foundation for
 *   the BoatNode. It reads four sensors concurrently, protects shared
 *   data with mutexes, monitors system health, and prints diagnostic
 *   output to Serial.
 *
 *   This is Phase 1 (Sensor Framework). The following are NOT yet
 *   implemented and will be added in later phases:
 *     - Six local safety algorithms (Outcomes 1–6)
 *     - JALARI packet creation and LoRa/SX1262 transmission
 *     - Kalman filtering, dead reckoning, IMBL geofencing
 *     - ML/TinyML
 *     - Wi-Fi/cloud communication
 *
 * ============================================================
 * HARDWARE WIRING SUMMARY
 * ============================================================
 *
 *   NEO-6M GPS:
 *     VCC → 3.3V
 *     GND → GND
 *     TX  → GPIO16 (ESP32-S3 RX)
 *     RX  → GPIO17 (ESP32-S3 TX)
 *
 *   HMC5883L Compass (I2C addr 0x1E):
 *     VCC → 3.3V
 *     GND → GND
 *     SDA → GPIO21
 *     SCL → GPIO22
 *
 *   ADXL345 Accelerometer (I2C addr 0x53):
 *     VCC  → 3.3V
 *     GND  → GND
 *     SDA  → GPIO21  (shared I2C bus)
 *     SCL  → GPIO22  (shared I2C bus)
 *     SDO  → VCC     (sets address to 0x53, NOT 0x1D)
 *     CS   → VCC     (I2C mode)
 *
 *   DS3231 RTC (I2C addr 0x68):
 *     VCC → 3.3V
 *     GND → GND
 *     SDA → GPIO21  (shared I2C bus)
 *     SCL → GPIO22  (shared I2C bus)
 *
 *   NOTE: Pull-up resistors (4.7kΩ) on SDA and SCL are REQUIRED
 *         for reliable I2C at 400 kHz with 3 devices on the bus.
 *
 * ============================================================
 * REQUIRED LIBRARIES (install via Arduino Library Manager)
 * ============================================================
 *
 *   TinyGPSPlus     — mikalhart/TinyGPSPlus
 *   Adafruit HMC5883 Unified — "Adafruit HMC5883 Unified"
 *   Adafruit ADXL345 Unified — "Adafruit ADXL345"
 *   Adafruit Unified Sensor   — "Adafruit Unified Sensor" (dependency)
 *   RTClib          — "RTClib" by Adafruit
 *
 * ============================================================
 * BOARD SETTINGS (Arduino IDE)
 * ============================================================
 *
 *   Board         : ESP32S3 Dev Module
 *   Upload Speed  : 921600
 *   CPU Frequency : 240 MHz
 *   Flash Mode    : QIO 80MHz
 *   Partition     : Default 4MB with spiffs
 *   Core Debug    : None (or Verbose for troubleshooting)
 *
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "telemetry.h"
#include "gps_task.h"
#include "compass_task.h"
#include "imu_task.h"
#include "rtc_task.h"
#include "telemetry_task.h"
#include "local_analysis.h"
#include "system_health.h"

// ============================================================
// STEP 1: SETUP — runs once on Core 1 before the scheduler
// ============================================================

void setup() {
    // --- Serial Monitor ---
    Serial.begin(SERIAL_BAUD);
    delay(500);   // Allow USB Serial to enumerate on ESP32-S3

    Serial.println();
    Serial.println("========================================");
    Serial.println("  PalavariRaksha — BoatNode FreeRTOS");
    Serial.println("  Phase 1: Sensor Acquisition Framework");
    Serial.println("========================================");
    Serial.printf("  CPU Freq   : %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("  Free heap  : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("  SDK version: %s\n", ESP.getSdkVersion());
    Serial.println("========================================");

    // --- STEP 2: Initialize I2C bus ---
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_SPEED_HZ);
    Serial.printf("[SETUP] I2C bus started: SDA=%d SCL=%d @ %d Hz\n",
                  I2C_SDA_PIN, I2C_SCL_PIN, I2C_SPEED_HZ);

    // --- STEP 3: Initialize telemetry struct to safe defaults ---
    telemetry_init();
    Serial.println("[SETUP] Telemetry struct initialized");

    // --- STEP 4: Create FreeRTOS synchronization objects ---
    // MUST happen before any task is created.
    if (!telemetry_create_sync_objects()) {
        Serial.println("[SETUP] FATAL: Could not create sync objects. Halting.");
        while (1) { delay(1000); }
    }

    // --- STEP 5–10: Create FreeRTOS tasks ---
    // Order: sensor tasks first, then monitoring tasks.

    Serial.println("[SETUP] Creating FreeRTOS tasks...");

    // STEP 5: GPS Task (Core 0, Priority 4)
#if ENABLE_GPS
    gps_task_start();
    Serial.println("[SETUP]   GPS task created");
#else
    Serial.println("[SETUP]   GPS disabled: module not connected");
#endif

    // STEP 6: Compass Task (Core 1, Priority 3)
    compass_task_start();
    Serial.println("[SETUP]   Compass task created");

    // STEP 7: IMU Task (Core 1, Priority 3)
    imu_task_start();
    Serial.println("[SETUP]   IMU task created");

    // STEP 8: RTC Task (Core 1, Priority 2)
    rtc_task_start();
    Serial.println("[SETUP]   RTC task created");

    // STEP 9: Telemetry Task (Core 0, Priority 2)
    telemetry_task_start();
    Serial.println("[SETUP]   Telemetry task created");

    // STEP 10: Local safety analysis task (Core 0, Priority 2)
    local_analysis_task_start();
    Serial.println("[SETUP]   Local analysis task created");

    // STEP 11: System Health Task (Core 0, Priority 1)
    health_task_start();
    Serial.println("[SETUP]   System Health task created");

    Serial.println("[SETUP] All tasks created. FreeRTOS scheduler running.");
    Serial.println("[SETUP] Entering loop()...");
    Serial.println();

    // After setup() returns, the FreeRTOS scheduler takes over.
    // All sensor reading and reporting happens in the tasks above.
}

// ============================================================
// LOOP — runs as the Arduino loop task (Priority 1 on Core 1)
// We deliberately leave this empty. All real work is in tasks.
// Putting heavy work here would interfere with FreeRTOS scheduling.
// ============================================================

void loop() {
    // Nothing here. The loop task simply yields.
    vTaskDelay(pdMS_TO_TICKS(10000));
}
