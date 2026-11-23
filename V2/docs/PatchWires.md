# Roscoe V2 board: patches (the V3 schematic)

**Schematic versions:**
- **V2** (`schematic/RoscoeMainboardV2Final`) is the schematic the PCB was made from.
- **V3** (`schematic/RoscoeMainboardV3`: .dch, .net and .pdf, exported 23 Nov 2026) is that same V2 PCB with the patches below, which got the system working. It records the boards as they are now. It is not a new PCB layout.
- **V4** will be the next PCB: V3 plus the changes in [NextPCB.md](NextPCB.md). The FPGA migration ([Roscoe_FPGA_Redesign.md](../fpga/Roscoe_FPGA_Redesign.md)) starts from V4.

This page lists every patch on the V2 boards and the part that shows it in V3. Board status 2026-11-23: CS3, which includes CS1 and CS2. The reasons and the full notes are in [changes.md](changes.md).

## Wires and added parts

| # | Set | What | Connection on the board | Part | In V3 |
|---|---|---|---|---|---|
| 0.1 | CS0 | Pull-ups on the 68030's `/CDIS` and `/MMUDIS` (both unconnected in V2) | One wire joins PGA pins H12 (`/CDIS`) and F13 (`/MMUDIS`) on the underside, and runs to J23 ("5VPU") | spare 10K pull-up R44 or R45 | R63 (`/CDIS`) and R62 (`/MMUDIS`), drawn as two 10K pull-ups |
| 0.1a | CS0 | Pull-up on the 68030's `/CBACK` (unconnected in V2), added 22 Nov 2026 | PGA pin J1 to +5 V | 10K | R70 |
| 0.2 | CS0 | Pull-up on the RTC's `/IRQ` (open drain, net `vINT1A`) | U33 pin 19 to the other J23 pin | the other spare 10K pull-up | R64 |
| 0.3 | CS0 | Filter capacitor on the reset button (net `PWR_RSTBUTTON`) | Button net to `5VALWAYSON`, U65's supply. It was first fitted to VCC, at J301 pin 2. | 1 µF | C21 |
| P1 | CS1 | AS to DEVMAP | Logic header U70 pin 4 (`nCPU_AS`) to J28 pin 3 (DEVMAP U59 pin 35, was `TESTP0`) | wire | U59 pin 35 on `nCPU_AS`; J28 pin 3 stays on it as a probe point |
| P2 | CS1 | Faster release of the card acknowledge (`nEXP_DEV_WAIT`, connector A26) | R10 replaced | 10K → 1K | R10, 1k |
| P5 | CS2 | Pull-downs on IDE INTRQ | J4 pin 31 to J4 pin 30 (GND), and J6 pin 31 to J6 pin 30 | 2 × 10K | R65, R66 |
| P6 | CS2 | Pull-downs on IDE DD7 | J4 pin 3 to J4 pin 2 (GND), and J6 pin 3 to J6 pin 2 | 2 × 10K | R67, R68 |
| P7 | CS2 | TTL input thresholds in the IDE data path | U37, U38, U40, U41 replaced | SN74AHC245 → SN74AHCT245 | SN74AHCT245DW |
| P8 | CS3 | IDE IORDY to DEVMAP | J4 pin 27 to J6 pin 27, then on to J28 pin 1 (DEVMAP U59 pin 37, was `TESTP2`); pull-up from J4 pin 27 to J5 pin 1 (VCC) | wire, 1K | net `IDE_IORDY` with R69 (1k); J28 pin 1 stays on it |

## Values that differ from V2

- **R51/R52**, the DSACK0/1 pull-ups: 1K; V2 says 10K.
- **R48**, the `nEXP_DATA_EN` pull-up (connector A27): 1K; V2 says 10K.

## Not done

- **P3**, a 4.7K pull-up on connector B26: planned with the first expansion card (the VGA card's hold-off build), not fitted.
- The reset button still has no pull-up resistor; U65's internal pull-up holds it high. NextPCB.md adds a 10K to `5VALWAYSON` for V4.
