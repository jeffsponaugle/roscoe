# Timing diagrams

The `src/*.json` files are WaveDrom WaveJSON sources, and the `*.svg` files next to this README are rendered from them. The diagrams are used in [TheoryOfOperations.md](../TheoryOfOperations.md), [DRAM.md](../DRAM.md), [SRAMBurstProposal.md](../SRAMBurstProposal.md) and [changes.md](../changes.md).

To regenerate the SVGs after editing a source:

```bash
powershell -ExecutionPolicy Bypass -File docs\timing\render-timing.ps1
```

`render-timing.ps1` needs no external tools. It supports the common WaveJSON subset (waves, data labels, groups, nodes and edges, `hscale`), plus one extension, `markers`, which draws labelled vertical guide lines. It warns when a row's length does not match the rest of its diagram. The sources also paste into <https://wavedrom.com/editor.html>, which simply ignores `markers`.

Conventions used in every diagram:

* One slot is a quarter of the CPU clock: 12.5 ns at 20 MHz. A CPU clock is `"wave": "p...", "period": 4`.
* Each CPLD logic level, or flop clock-to-output, is drawn as one slot. The diagrams show ordering and approximate margins; the documents give nanosecond budgets where they matter.
* Drawn for 20 MHz. At the 12 and 16 MHz clocks the boards currently use, a CPLD delay is well under one slot, so most margins are wider. Anything decided at a clock edge, such as DSACK sampling, keeps the same slot positions.
* `u` draws an RC pull-up curve, used for the open-collector DSACK lines.
* Diagrams whose titles start with "PROPOSED" show logic that has not been implemented.
