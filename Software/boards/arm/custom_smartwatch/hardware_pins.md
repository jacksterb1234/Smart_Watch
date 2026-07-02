# Custom Smartwatch Pin Ledger

This ledger records schematic net names against NORA-B106 module pads and the
local KiCad NORA-B106 symbol GPIO mapping.

## Blocking Checks

- Per user direction, the main I2C routing is corrected. Firmware maps NORA pad
  `A3` / `P1.03` as SCL and pad `B4` / `P1.02` as SDA.
- MAX30102 is represented on I2C at `0x57` using Zephyr's upstream
  `maxim,max30101` red/IR driver path. Its active-low interrupt net is on
  `P0.21`.

## Main I2C

| Signal | NORA pad | nRF5340 GPIO | Zephyr pinctrl |
| --- | --- | --- | --- |
| `SCL` | `A3` | `P1.03` | `NRF_PSEL(TWIM_SCL, 1, 3)` |
| `SDA` | `B4` | `P1.02` | `NRF_PSEL(TWIM_SDA, 1, 2)` |

Shared by BMI270, MAX30102, LPS22DF, nPM1300, and display touch I2C.

## Interrupts

| Net | NORA pad | nRF5340 GPIO | Firmware status |
| --- | --- | --- | --- |
| `PMIC_INT` | `H1` | `P0.24` | Enabled |
| `BMI_INT` | `C8` | `P0.22` | Enabled |
| `MAX_INT` | `E3` | `P0.21` | Enabled via MAX30101-compatible driver |
| `LPS_INT` | `A5` | `P0.20` | Enabled |

## Buttons

All buttons are active-low to ground and should use pull-ups.

| Switch | Net | NORA pad | nRF5340 GPIO |
| --- | --- | --- | --- |
| `SW4` | `BTN_1` | `G2` | `P0.26` |
| `SW5` | `BTN_2` | `B5` | `P0.03` |
| `SW6` | `BTN_3` | `A4` | `P1.00` |
| `SW7` | `BTN_4` | `F8` | `P1.14` |

## Display SPI And Control

| Net | NORA pad | nRF5340 GPIO |
| --- | --- | --- |
| `DISPLAY_CS` | `A2` | `P0.12` |
| `DISPLAY_CLK` | `B3` | `P0.11` |
| `DISPLAY_DATA` | `C1` | `P0.10` |
| `DISPLAY_DC` | `C2` | `P0.09` |
| `DISPLAY_RST` | `J9` | `P0.31` |

## Display Touch And Backlight

| Net | NORA pad | nRF5340 GPIO |
| --- | --- | --- |
| `CTP_INT` | `B1` | `P0.08` |
| `CTP_RST` | `E8` | `P0.05` |
| `TOUCH_EN` | `D8` | `P0.04` |
| `DISPLAY_LIGHT_IO` | `G3` | `P0.25` |

## External Flash

| Net | NORA pad | nRF5340 GPIO |
| --- | --- | --- |
| `QSPI_IO2` | `D1` | `P0.15` |
| `QSPI_IO0` | `D2` | `P0.13` |
| `QSPI_CS` | `E1` | `P0.18` |
| `QSPI_IO1` | `E2` | `P0.14` |
| `QSPI_CLK` | `F1` | `P0.17` |
| `QSPI_IO3` | `F2` | `P0.16` |

## Haptic And Backlight

| Net | NORA pad | nRF5340 GPIO |
| --- | --- | --- |
| `LRA_PWM` | `H7` | `P0.29` |
| `LRA_EN` | `J8` | `P0.28` |
| `DISPLAY_LIGHT_IO` | `G3` | `P0.25` |
