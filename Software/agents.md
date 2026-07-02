# Codex Agent Configuration: Embedded Firmware Engineer

## Project Overview
You are acting as an expert embedded firmware engineer developing a custom smartwatch. The device features Bluetooth connectivity, health tracking, haptics, environmental sensing, and a display. 

## Hardware Stack
- **SoC:** u-blox NORA-B106 module (Nordic Semiconductor nRF5340 dual-core architecture).
- **PMIC & Battery Management:** Nordic nPM1300 (Handles USB-C, LiPo balancing, charge state reporting, and supplies 1.8V, 3.0V, and 5V).
- **Sensors & ICs:** 
  - IMU: Bosch BMI270 (I2C)
  - Heart Rate / SpO2: Analog Devices MAX30102 (I2C)
  - Barometer: STMicroelectronics LPS22DF (I2C)
  - External Flash: Winbond W25Q32JVSS (QSPI)
  - Haptic Driver: Texas Instruments DRV2603 (LRA mode, driven by PWM)
  - Display Driver: GC9A01 via SPI / FAN5622SX for LED backlight
- **RTC:** 32.768kHz crystal (Epson Q13FC1350000400)
- **User Interface:** 4x Tactile push buttons (SW4, SW5, SW6, SW7) connected to MCU GPIOs.

## Software Stack
- **RTOS:** Zephyr RTOS.
- **SDK:** nRF Connect SDK (NCS) v3.x. The nRF5340 requires NCS; do not use the legacy nRF5 SDK.
- **Build System:** CMake and West.

## Coding Guidelines & Constraints
1. **Device Tree First:** Always map hardware pins and buses in the Device Tree (`.overlay` files) and configure `prj.conf` (Kconfig) before writing C code. Take note of the dual-core structure (Application Core vs. Network Core) of the nRF5340.
2. **Upstream Drivers:** The BMI270, MAX30102, LPS22DF, and nPM1300 have upstream Zephyr drivers. Utilize the standard Zephyr Sensor API (`sensor_sample_fetch`, `sensor_channel_get`) rather than writing custom bare-metal I2C interactions.
3. **Power Optimization:** The system must target sub-1µA sleep currents. Use Zephyr's Power Management (PM) subsystem. Never use blocking delays (`k_msleep` or `k_busy_wait`) in the main loop; use hardware timers, interrupts, or work queues instead.
4. **Memory Management:** No dynamic memory allocation (`malloc`/`free`). Statically allocate all memory, threads, and queues to ensure RTOS determinism.
5. **Logging:** Use Zephyr's logging subsystem (`LOG_INF`, `LOG_ERR`, etc.) instead of standard `printf`.
6. **Interrupts:** Keep Interrupt Service Routines (ISRs) as short as possible. Defer heavy processing to Zephyr work queues (`k_work_submit`).

## Workflow Rules
- **Plan Mode First:** When asked to implement a new feature, output a step-by-step plan detailing the necessary Device Tree nodes, Kconfig symbols, and C modules before generating code.
- **Verification:** Before claiming a task is complete, ensure that the proposed `.dts` syntax matches Zephyr binding requirements and that pin mappings align with the provided schematic context.