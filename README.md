# Smart Watch — Custom KiCad Smartwatch PCB

A fully custom smartwatch designed from the ground up, featuring a purpose-built PCB, BLE connectivity, and a 3D-printed case. Designed for complete control over hardware and software integration.

> Integration testing projected to begin July 2026.

## Features

- **MCU:** Nordic nRF54L15 — ultra-low power with integrated Bluetooth LE
- **Step counting** via IMU
- **Heart rate sensing**
- **Touchscreen display**
- **Music control** and **notification vibration** (haptic)
- **USB-C charging**
- **Custom 3D-printed case** — ~30% lighter than off-the-shelf alternatives, fully personalizable
- Designed for **JLCPCB assembly**

## Repository Structure

```
Smart_Watch/
├── Architecture.kicad_sch   # Top-level architecture schematic
├── MCU.kicad_sch            # nRF54L15 MCU schematic
├── Power.kicad_sch          # Power management schematic
├── Peripherals.kicad_sch   # Sensors & peripheral schematic
├── Smart_Watch.kicad_pcb   # PCB layout
├── Smart_Watch.pdf          # Full schematic export
├── Smart_Watch.pretty/     # Custom KiCad footprints
├── Smart_Watch.3dshapes/   # 3D models for PCB components
├── ZIF.pretty/              # ZIF connector footprints
├── B2B_PH_SM4_TB/          # Board-to-board connector footprints
├── Backpack_Board/         # Backpack extension board
├── Case/                   # 3D-printable case files
└── jlcpcb/                  # JLCPCB production files (Gerbers, BOM, CPL)
```

## Tools Used

- **KiCad** — Schematic capture and PCB layout
- **JLCPCB** — PCB fabrication and SMT assembly
- **SolidWorks / Fusion360** — Case design

## Key Components

| Component | Function |
|-----------|----------|
| nRF54L15 | MCU with integrated BLE |
| Heart rate sensor | Optical heart rate monitoring |
| IMU | Step counting and motion sensing |
| Touchscreen | User interface display |
| USB-C charger IC | Battery charging via USB-C |

## Author

**Jackson Barber** — [github.com/jacksterb1234](https://github.com/jacksterb1234)
