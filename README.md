\# nRF52840 Macro Pad



A custom USB macro pad with 6 mechanical keys, 2 rotary encoders, and a small OLED screen. The board is a single-layer PCB that can be etched at home.



\*\*Author:\*\* Bidhan Aryal

\*\*Status:\*\* Version 0.1. The design is complete. The board has not been built or tested yet.



!\[PCB layout](images/pcb-top.png)



\## Overview



This project is a macro pad that sends shortcuts to a computer over USB. It uses an nRF52840 SuperMini board as the controller. The PCB was designed in EasyEDA with two goals: use one copper layer only, and use no vias. To reach these goals, the controller pins were split between two headers so that the traces do not cross.



\## Features



\- 6 Cherry MX keys

\- 2 rotary encoders with push buttons

\- 1.3-inch I2C OLED display

\- USB connection through the nRF52840 SuperMini

\- Single-layer PCB with no vias

\- Designed for toner transfer and ferric chloride etching



\## Parts List



The full list is in \[BOM.csv](BOM.csv).



| Part | Quantity |

|------|----------|

| nRF52840 SuperMini | 1 |

| Cherry MX switch (1U) | 6 |

| Rotary encoder with push button | 2 |

| 1.3-inch I2C OLED module (4 pins) | 1 |

| 13-pin 2.54 mm headers | 2 |



\## Pin Assignments



The controller is mounted sideways, with the USB port facing left. Header P1 is the top row of pins. Header P2 is the bottom row of pins. Pin numbers count along each header. Because the nRF52840 lets most pins act as general inputs and outputs, the signal pins can be changed in the firmware as long as the wiring matches.



\*\*Table 1\*\*



\*Pin Assignments for Header P1 (Keys and Ground)\*



| P1 pin | Connection |

|--------|------------|

| 4 | Ground |

| 5 | Ground |

| 6 | Key 1 (S1) |

| 7 | Key 4 (S4) |

| 8 | Key 2 (S2) |

| 9 | Key 5 (S5) |

| 10 | Key 3 (S3) |

| 11 | Key 6 (S6) |



\*\*Table 2\*\*



\*Pin Assignments for Header P2 (Display, Power, and Encoders)\*



| P2 pin | Connection |

|--------|------------|

| 3 | Ground |

| 5 | 3.3 V power (OLED) |

| 6 | OLED clock (SCL) |

| 7 | OLED data (SDA) |

| 8 | Encoder 1, A |

| 9 | Encoder 1, switch (D) |

| 10 | Encoder 1, B |

| 11 | Encoder 2, A |

| 12 | Encoder 2, switch (D) |

| 13 | Encoder 2, B |



\*Note.\* All keys share one ground line. Each encoder uses its middle pin and its switch pin as ground. Check the pin order printed on your own OLED module before you solder it. Some modules list power and ground in the opposite order.



\## Repository Contents



| Folder or file | Description |

|----------------|-------------|

| `hardware/easyeda` | EasyEDA project files (source) |

| `hardware/gerbers` | Gerber and drill files for ordering the board |

| `hardware/pdf` | Schematic and PCB drawings |

| `images` | Screenshots, renders, and photos |

| `BOM.csv` | Parts list |



\## Building the Board



The board is made for hand etching. The steps below are the plan for this project.



1\. Print the bottom copper layer on paper, and compare it with the real parts to check the fit.

2\. Transfer the print to a copper-clad board with heat.

3\. Etch the board in ferric chloride, then clean off the toner.

4\. Drill the holes from the copper side.

5\. Solder the headers, encoders, and OLED, then the switches.

6\. Place the controller on the headers.



\## Firmware



The firmware is planned but not written yet. It will use Arduino C++ with the Adafruit TinyUSB library. The keys and encoder switches will use the internal pull-up resistors. Planned profiles are VS Code, media and system controls, and KiCad.



\## Known Limits



\- The board has not been tested. Treat the design as a first version.

\- Many 1.3-inch OLED modules use the SH1106 chip, not the SSD1306 chip. The display library may need to change.

\- On the nRF52840, two pins (P0.09 and P0.10) are set for NFC by default. Avoid them, or set them as normal pins in the firmware.



\## License



Copyright (c) 2026 Bidhan Aryal.



\- \*\*Hardware files\*\* (schematic, PCB, Gerbers, and drawings): CERN Open Hardware Licence Version 2, Permissive (CERN-OHL-P-2.0).

\- \*\*Firmware\*\* (when added): MIT License.



You may use, copy, change, and share this work, including for commercial use, as long as you keep the copyright notice.

