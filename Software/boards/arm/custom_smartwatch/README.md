# Custom Smartwatch Board

Initial Zephyr board definition for the NORA-B106 / nRF5340 application core.
Pin mappings use the local KiCad NORA-B106 symbol and preserve the same
schematic net distribution used before the module swap.

Build from the application root with the NCS v3.3.1 toolchain environment:

```powershell
$tc = 'C:\ncs\toolchains\936afb6332'
$env:PATH = "$tc;$tc\mingw64\bin;$tc\bin;$tc\opt\bin;$tc\opt\bin\Scripts;$tc\opt\nanopb\generator-bin;$tc\nrfutil\bin;$tc\opt\zephyr-sdk\arm-zephyr-eabi\bin;$tc\opt\zephyr-sdk\riscv64-zephyr-elf\bin;$env:PATH"
$env:PYTHONPATH = "$tc\opt\bin;$tc\opt\bin\Lib;$tc\opt\bin\Lib\site-packages"
$env:NRFUTIL_HOME = "$tc\nrfutil\home"
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
$env:ZEPHYR_SDK_INSTALL_DIR = "$tc\opt\zephyr-sdk"
$env:ZEPHYR_BASE = 'C:\ncs\v3.3.1\zephyr'
west build --sysbuild -b custom_smartwatch/nrf5340/cpuapp -- -DBOARD_ROOT="$PWD"
```

Track bring-up and product readiness in the
[firmware readiness checklist](../../../docs/firmware_readiness_checklist.md).

The board currently enables the main I2C bus, nPM1300, BMI270, LPS22DF, four
buttons, GC9A01 display SPI, W25Q32 QSPI flash, FAN5622 backlight GPIO, and
DRV2603 haptic PWM/enable pins where Zephyr has suitable upstream bindings. The
sysbuild path also builds the nRF5340 network-core `ipc_radio` image for BLE
HCI IPC.

The nPM1300 charger profile is configured for a single-cell LiPo with 4.2 V
termination, 200 mA charge current, 3.7 V nominal voltage, and no thermistor.
