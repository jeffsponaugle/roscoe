# Roscoe - next PCB (V3) hardware changes

This is the routing and parts list for the next revision of the main board. It collects every board-level change from [TheoryOfOperations.md](TheoryOfOperations.md) (Sections 4-15), [changes.md](changes.md) (CS0-CS3) and [SRAMBurstProposal.md](SRAMBurstProposal.md), and says which signal goes to which pin.

Sources:
* **CPLD pins:** TQFP-100 pin numbers for the ATF1508s (BUSCTRL U58, DEVMAP U59, INTCTRL U60).
* **Free pins:** found in the V2 netlist (`schematic/RoscoeMainboardV2Final.net`, checked 2026-10-18), which also confirmed that every V2 CPLD pin assignment matches its net.
* **68030 pins:** PGA positions; their names come from the netlist and the MC68030 manual.

**Before freezing the layout,** trial-fit all three ATF1508 sources with the new pin assignments. The fitter has per-block routing limits, and the sources use `PREASSIGN = KEEP`.

Items marked *optional* depend on decisions that have not been made yet; the rest are recommended.

## 1 New signal routing

| # | Signal | Route to | Replaces on V2 | Why |
|---|---|---|---|---|
| 1 | `nCPU_AS` | **DEVMAP pin 89** (dedicated input) | the CS1 rework wire (U70 pin 4 to J28 pin 3, DEVMAP pin 35) | DEVMAP needs AS directly for 0-wait-state device acknowledges (changes.md 1.3). Pin 35 goes back to `TESTP0` on J28 pin 3. |
| 2 | IDE `IORDY`: J4 pin 27 and J6 pin 27 joined | **DEVMAP pin 90** (dedicated input), with a 1K pull-up to VCC | the CS3 rework wire to J28 pin 1 (DEVMAP pin 37) | Flow control for drives that stretch cycles. Pin 37 goes back to `TESTP2`. |
| 3 | IDE `/IOR`: new DEVMAP output `nIDE_RD` | **DEVMAP pin 42** to U39 pin 8 and U42 pin 8 (1A3), the same way `nIDE_WR` feeds pin 17 of both | `nCPU_RD` on U39/U42 pin 8 | DEVMAP shapes /IOR directly instead of steering BUSCTRL's read strobe (CS2 B9). `EXTRA_BC_DM1` becomes spare again, and the UARTs and memories keep `nCPU_RD`. |
| 4 | *Optional:* CPU `A31`-`A28` | **DEVMAP pins 77-80** (A28 on 77 through A31 on 80, next to the existing address inputs) | BUSCTRL's region selects `nDEV8BITCS`, `nDEV16BITCS`, `nDEV32BITCS` (BUSCTRL pins 1/2/5 to DEVMAP pins 1/2/5) | DEVMAP decodes the 0x3/0x4/0x5 regions itself, without waiting for BUSCTRL, and three BUSCTRL pins come free (FC is not needed for these regions, changes.md 1.7 B3). |
| 5 | *Optional (burst fills):* 68030 `/CBREQ` (PGA K1) | **BUSCTRL pin 75** (the only free BUSCTRL pin on V2) | not connected | Burst requests for SRAM cache fills (SRAMBurstProposal.md) |
| 6 | *Optional (burst fills):* 68030 `/CBACK` (PGA J1) | **BUSCTRL pin 1** (freed by item 4); otherwise a 10K pull-up with a jumper to GND (PGA J3, next to it, is GND) | not connected (floating) | Burst acknowledge, asserted with STERM. Without burst support it must still not float; see Section 2. |
| 7 | *Optional (burst fills):* SRAM riser address `BA0`/`BA1` | **two free INTCTRL I/O pins** (for example 99 and 100) to riser connector U6 pins **A18** and **B18** | `CPU_A2` and `CPU_A3` on U6 A18/B18 | The burst counter in INTCTRL drives SRAM A0/A1, and passes CPU A2/A3 through otherwise. INTCTRL already has CPU A3-A0 (pins 71/72/75/76). See SRAMBurstProposal.md 4.1. |

No other routing changes. In particular, the expansion connector keeps its pin-out; only the names change (Section 5).

## 2 Pull-ups, pull-downs and other passives

| Net | Where | Part | Status on V2 | Reference |
|---|---|---|---|---|
| 68030 `/CDIS` (PGA H12) | to VCC (PGA H11, next to it, is VCC) | 10K pull-up | rework wire to spare pull-up R44/R45 | CS0 0.1 |
| 68030 `/MMUDIS` (PGA F13) | to VCC (PGA F11 is VCC) | 10K pull-up | same rework wire | CS0 0.1 |
| 68030 `/CBACK` (PGA J1) | if not routed to BUSCTRL (item 6): to VCC, with a 2-pin jumper to GND for burst experiments | 10K pull-up + jumper | floating | CS0 0.1a |
| RTC `/IRQ` (`vINT1A`, U33 pin 19) | to VCC | 10K pull-up | rework wire to the other spare pull-up | CS0 0.2 |
| Reset button (`PWR_RSTBUTTON`: U66, J17 pin 1, U65 pin 12) | next to U65 | 10K pull-up to VCC, plus the filter capacitor to VCC (same value as the rework, 100 nF typical) | rework capacitor to VCC via J301 pin 2; no pull-up at all | CS0 0.3 |
| `nCPU_DSACK0/1` | R51, R52 | **1K** (schematic says 10K) | 1K fitted | TheoryOfOperations 4.5 |
| `nEXP_ACK` (connector A26, today `nEXP_DEV_WAIT`) | R10 | **1K** | 10K (P2 changes it) | changes.md 1.5 |
| `nEXP_VMEM_BUSY` (connector B26, today `EXP_BC_CPLD1`) | new resistor to VCC | 4.7K pull-up | rework P3 | changes.md 1.4 |
| `nEXP_DATA_EN` (connector A27) | R48 | consider 1K | 10K | TheoryOfOperations 13.2: a 10K release is too slow |
| IDE `INTRQ` (J4 and J6 pin 31) | each to GND | 10K pull-down | none (rework P5) | changes.md 2.3 |
| IDE `DD7` (J4 and J6 pin 3) | each to GND | 10K pull-down | none (rework P6) | changes.md 2.3 |
| IDE `IORDY` (J4/J6 pin 27, item 2) | one, to VCC | 1K pull-up | none (rework P8) | changes.md 3.2 |
| IDE `/IOR`, `/IOW`, `/CS0`, `/CS1`, `DA0`-`DA2` | in series at the U39/U42 outputs, both ports | 22-33 ohm | none | ringing on the cable can double-clock a strobe |
| IDE `DD0`-`DD15` | in series on the IDE side of U37/U38/U40/U41 | 22-33 ohm (optional) | none | same reason, smaller effect |
| IDE `CSEL` (J4 and J6 pin 28) | to GND through a 2-pin jumper | jumper | not connected | lets cable select work |
| IDE `DASP` (J4 and J6 pin 39) | LED and series resistor to VCC | LED + 470 ohm | not connected | drive activity light |

Keep the two spare 10K pull-ups R44/R45 on header J23. They made the CS0 rework easy.

## 3 Part changes

| Ref | V2 part (netlist) | V3 part | Why |
|---|---|---|---|
| U37, U38, U40, U41 (IDE data) | SN74AHC245 | **SN74AHCT245** | TTL input thresholds. ATA drives only promise 2.4 V for a high, and some drive 3.3 V levels (changes.md P7). |
| U54-U57 (expansion data) | SN74AHC245 | **SN74AHCT245** | cards are then free to use TTL-level outputs |
| U35 (RTC data) | SN74AHC245 | SN74AHCT245 (optional) | the DS12887's specified VOH is 2.4 V |

The other AHC parts (U39/U42, U49-U53, U68, U34, U26-U28) are driven by CPLDs or the CPU, whose outputs swing to the rail, so they can stay.

## 4 Test points and headers

* J28 goes back to `TESTP0`..`TESTP2` (DEVMAP 35/36/37) once AS and IORDY have their own pins (items 1 and 2).
* Keep logic headers U4, U5, U23, U24, U25, U69 and U70. U5 shows `EXTRA_BC_DM1` and U69 shows the VMEM busy/grant pair (B26/B30), both useful when debugging.

## 5 Net names, schematic and silkscreen

* Expansion connector:
  * A26 becomes `nEXP_ACK`.
  * B26 becomes `nEXP_VMEM_BUSY`, and B30 becomes `nEXP_VMEM_GNT`.
  * A21 stays `nEXP_VMEM_CS`, covering 2 MB from CS1 on.
* R51/R52: show the fitted 1K value.
* Y1: note that the boards run with 12 or 16 MHz oscillators. CS2's IDE timing counts assume 16 MHz or less, so a 20 MHz oscillator would need longer counts.
* SRAM riser: the fitted parts are IS61C25616AL-12 (12 ns).
* RTC symbol: DS12887, not "DS12877".
* U3: the symbol is SST39SF010A, but the footprint is wired for, and fitted with, an SST39SF040 (A17/A18 on pins 30/1).
* Sheet 2: the "common cathode" note is wrong; the displays' common pins go to +5V.

## 6 CPLD source changes that go with V3

* **DEVMAP:**
  * `Pin[89] = nCPU_AS;`, `Pin[90] = IDE_IORDY;`, and pins 35 and 37 back to `TESTP0` and `TESTP2`.
  * New registered output on pin 42: `nIDE_RD.d = !(IDE_CYC & CPU_RW & ((!IDE_FAST & T1 & !T8) # (IDE_FAST & !T3)));`, clocked on `!CPU_CLK`, with `.ap = nCPU_AS # !nSYS_RESET`. That is the CS2 window, generated in DEVMAP.
  * Drop `nIDE_RD_INH`.
  * With item 4, decode `DEV8`/`DEV16`/`DEV32` from A31-A28 locally.
* **BUSCTRL:**
  * `nCPU_RD.d = !CPU_RW;` again (CS2 B9 is no longer needed).
  * With item 4, drop the `nDEV8/16/32BITCS` outputs.
  * With items 5 and 6, add `/CBREQ` and `/CBACK` for the SRAM burst logic.
* **INTCTRL:** with item 7, add the burst counter outputs `BA0`/`BA1` (SRAMBurstProposal.md 4.2-4.3).

## 7 Free CPLD pins on V2

From the netlist: pins with no net at all.

| CPLD | Free I/O pins | Free dedicated inputs |
|---|---|---|
| BUSCTRL U58 | 75 | none |
| DEVMAP U59 | 9, 10, 12, 19, 20, 40, 41, 42, 77, 78, 79, 80, 81, 85, 92, 94, 99 | 89, 90 |
| INTCTRL U60 | 1, 2, 5, 6, 17, 22, 23, 28, 33, 35, 36, 37, 44, 47, 48, 49, 50, 55, 56, 68, 69, 81, 83, 84, 85, 92, 93, 94, 96, 97, 98, 99, 100 | none |
| PTC U11 (TQFP-44) | 8, 10, 11, 12, 13, 28, 30, 38 | |

BUSCTRL is effectively full. Item 4 is the cleanest way to give it room, freeing pins 1, 2 and 5. The `TESTP0`-`TESTP3` pins on J29 are the fallback.

Spare inter-CPLD nets on V2:
* BUSCTRL-DEVMAP: `EXTRA_BC_DM1` (used by CS2 until item 3 replaces it) and `EXTRA_BC_DM2`.
* BUSCTRL-INTCTRL: `EXTRA_BC_INT1/2`.
* DEVMAP-INTCTRL: `EXTRA_DM_INT1/2`.

## 8 Checklist before ordering

1. Trial-fit BUSCTRL, DEVMAP and INTCTRL with the new pin assignments, and check the fit reports for logic levels on the DSACK and acknowledge paths (changes.md 1.3).
2. Export the V3 netlist and repeat the V2 check: every CUPL `Pin[]` assignment against its net, and every pull-up/pull-down above against its net.
3. Check that no rework from CS0-CS3 is still needed on V3: the CPU and RTC pull-ups, the reset filter, R10, the B26 pull-up, the AS wire, the IDE pull-downs and IORDY.
