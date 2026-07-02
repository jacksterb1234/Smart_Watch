# Firmware Readiness Checklist

This checklist is the source of truth for getting the custom smartwatch from
board definition to useful firmware. When every item is complete, the firmware
should flash to the NORA-B106/nRF5340 board, pair with a phone, control phone
music, track steps, and report heart rate.

Assumption: "play music" means Bluetooth LE media control of phone playback. It
does not mean audio playback or streaming from the watch hardware.

## Current Baseline

- [x] Custom Zephyr board target exists for `custom_smartwatch/nrf5340/cpuapp`.
- [x] Board definition builds successfully with NCS v3.3.1.
- [x] Main I2C bus is mapped for nPM1300, BMI270, LPS22DF, and MAX30102 support.
- [x] PMIC, BMI270, LPS22DF, and MAX30102 interrupt GPIOs are represented.
- [x] Four user buttons are mapped through `gpio-keys`.
- [x] GC9A01 display SPI bus is represented.
- [x] W25Q32 QSPI flash is represented.
- [x] FAN5622 display backlight control GPIO is represented.
- [x] DRV2603 haptic PWM output is represented.
- [x] DRV2603 `LRA_EN` GPIO is represented as `haptic-en`.
- [x] DRV2603 `LRA_EN` and PWM are managed by a non-blocking haptics module.
- [x] FAN5622 backlight GPIO is managed by firmware as simple on/off control for bring-up.
- [x] Minimal firmware logs core device readiness.
- [x] Hardware pin ledger uses NORA-B106/nRF5340 terminology.
- [x] RTT logging and console backend are configured for board bring-up.
- [x] nRF5340 network-core `ipc_radio` target builds through sysbuild.
- [x] Basic BLE HID Consumer Control service is implemented for phone media keys.
- [x] Button events are deferred through static work and mapped to media actions.
- [x] Basic BMI270 step tracking is implemented through Zephyr Sensor API scheduled work.
- [x] Step totals are persisted through Zephyr settings/NVS with throttled writes.
- [x] Basic GC9A01 display UI renders BLE state, step count, heart-rate status, and live battery/USB state.
- [x] MAX30102 is represented using Zephyr's upstream `maxim,max30101` red/IR driver path.
- [x] SW7 starts a bounded MAX30102 heart-rate measurement.
- [x] nPM1300 charger profile is configured for a 4.2 V, 200 mA, no-thermistor single-cell LiPo.
- [x] nPM1300 battery voltage/current/USB telemetry is sampled through the upstream charger driver.
- [x] BLE Battery Service level is updated from the nPM1300 voltage estimate.
- [x] Simple user settings are persisted through Zephyr settings/NVS.
- [x] Display-off power state blanks the GC9A01 and disables the backlight after a persisted timeout.
- [x] Current app code has been checked for no `malloc`/`free`, no `printf`, no polling main loop, and no blocking sleep calls.

## Before First Useful Flash

- [x] Add a reliable board bring-up logging path, preferably RTT.
- [ ] Confirm `west flash` runner behavior with the selected debug probe.
- [ ] Verify first boot logs on hardware.
- [ ] Verify nPM1300 responds on I2C and reports charger/battery state.
- [ ] Verify BMI270 responds on I2C and produces accelerometer samples.
- [ ] Verify LPS22DF responds on I2C and produces pressure/temperature samples.
- [ ] Verify W25Q32 JEDEC ID, erase, write, and read through QSPI.
- [ ] Verify GC9A01 display initialization with a visible test pattern.
- [ ] Verify FAN5622 behavior and replace generic GPIO handling if brightness requires pulse control.
- [ ] Verify all four buttons generate input events with the expected active-low polarity.
- [ ] Verify DRV2603 enable plus PWM produces controlled haptic output.
- [x] Add Bluetooth LE support for nRF5340 using sysbuild so the network-core controller image is included.
- [x] Add MAX30102 support using the upstream MAX30101-compatible Zephyr Sensor API driver.
- [ ] Confirm the MAX30102 part ID matches the upstream driver's expected ID on real hardware.

## Product Firmware

- [x] Add a static button service that maps SW4 to play/pause, SW5 to next, SW6 to previous, and SW7 to screen cycle or heart-rate check.
- [x] Add Bluetooth pairing, bonding, reconnect, and HID-over-GATT Consumer Control media keys.
- [x] Add a baseline display UI for BLE status, step count, heart-rate placeholder, and battery/charge placeholder.
- [x] Replace battery/charge display placeholder with live nPM1300 telemetry.
- [x] Replace heart-rate display placeholder with live MAX30102 measurement status and BPM when available.
- [x] Add BMI270 step tracking using Zephyr Sensor API data and interrupt/workqueue processing.
- [x] Add MAX30102 heart-rate sampling with no-finger and poor-signal handling.
- [ ] Tune MAX30102 LED current, thresholds, and BPM filtering on real wrist/finger data.
- [x] Add haptic feedback patterns using `LRA_EN` and PWM without blocking delays.
- [x] Add persistent storage for Bluetooth bonding/settings data.
- [x] Add persistent storage for step totals.
- [x] Add persistent storage for simple user settings.
- [x] Add nPM1300 charger and battery telemetry reporting.
- [ ] Add nPM1300 regulator rail diagnostics if hardware bring-up shows they are needed.
- [x] Confirm battery charge current, termination voltage, and thermistor setup before enabling nPM1300 charging.
- [x] Add display-off power state with button wake and persisted timeout.
- [ ] Add sensor idle, BLE connected idle, and deep-sleep states after first current measurements.
- [x] Audit app code for no dynamic allocation, no `printf`, no polling main loop, and no blocking delays in steady-state behavior.

## Acceptance Tests

- [x] `west build --sysbuild -b custom_smartwatch/nrf5340/cpuapp -- -DBOARD_ROOT="$PWD"` passes cleanly.
- [ ] `west flash` programs both nRF5340 application and network cores.
- [ ] Boot log reports PMIC, BMI270, LPS22DF, display, flash, buttons, haptic PWM, haptic enable, and MAX30102 readiness or clear failure state.
- [ ] Phone pairs over Bluetooth LE and the watch controls media playback.
- [ ] Button mapping matches the intended default controls.
- [ ] Step count increases during walking or repeatable motion testing and persists across reboot.
- [ ] Heart-rate screen produces BPM readings with clear handling for no-finger/poor-signal cases.
- [ ] Display renders the normal UI, backlight control works, and haptic feedback fires on expected events.
- [ ] Sleep/display-off current is measured and recorded as a regression target.

## Build References

Current board-only build:

```powershell
west build --pristine=always -b custom_smartwatch/nrf5340/cpuapp -- -DBOARD_ROOT="$PWD"
```

Final flash-ready build target:

```powershell
west build --sysbuild -b custom_smartwatch/nrf5340/cpuapp -- -DBOARD_ROOT="$PWD"
```
