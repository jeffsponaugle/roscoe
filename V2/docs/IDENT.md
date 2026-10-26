Here's the table again with each field's byte offset in the 512-byte IDENTIFY buffer. Word n starts at offset 2n. In your captures the BIOS buffer began at 0x101D_3120, so add that base to get a memory address: word 49, for example, was at 0x101D_3182.

| Word | Offset | Field | Notes |
|---|---|---|---|
| 0 | 0x000 | General configuration | `848Ah` is the CompactFlash signature; many cards report `044Ah` instead (your good and bad cards). Bit 15 = 0 means ATA; bit 7 = removable; bit 6 = fixed. |
| 1 | 0x002 | Default cylinders | CHS geometry |
| 2 | 0x004 | Specific configuration | ATA-5 and later; usually 0 on CF |
| 3 | 0x006 | Default heads | |
| 4-5 | 0x008-0x00B | Obsolete | unformatted bytes per track and per sector |
| 6 | 0x00C | Default sectors per track | |
| 7-8 | 0x00E-0x011 | Sectors on the card | CF only; word 7 is the high word |
| 9 | 0x012 | Retired | |
| 10-19 | 0x014-0x027 | Serial number | 20 ASCII characters, space-padded |
| 20-22 | 0x028-0x02D | Retired | buffer type, buffer size, ECC bytes; often filled in on CF |
| 23-26 | 0x02E-0x035 | Firmware revision | 8 ASCII characters |
| 27-46 | 0x036-0x05D | Model number | 40 ASCII characters |
| 47 | 0x05E | READ/WRITE MULTIPLE limit | bits 7-0 = maximum sectors per block; bits 15-8 = `80h` |
| 48 | 0x060 | Reserved | ATA-1: 32-bit I/O; CF: 0 |
| 49 | 0x062 | Capabilities | bit 11 = IORDY supported, bit 10 = IORDY can be disabled, bit 9 = LBA, bit 8 = DMA |
| 50 | 0x064 | Capabilities 2 | bit 14 = 1 when valid |
| 51 | 0x066 | PIO mode | bits 15-8 = PIO mode 0-2 |
| 52 | 0x068 | Single-word DMA mode | obsolete |
| 53 | 0x06A | Which fields are valid | bit 0: words 54-58, bit 1: words 64-70, bit 2: word 88 |
| 54-56 | 0x06C-0x071 | Current cylinders, heads, sectors per track | |
| 57-58 | 0x072-0x075 | Current CHS capacity in sectors | |
| 59 | 0x076 | Current multiple setting | bit 8 = valid; bits 7-0 = sectors per block now set |
| 60-61 | 0x078-0x07B | LBA capacity in sectors | 28-bit |
| 62 | 0x07C | Single-word DMA | obsolete |
| 63 | 0x07E | Multiword DMA | bits 2-0 = modes supported, bits 10-8 = mode selected |
| 64 | 0x080 | Advanced PIO modes | bit 0 = PIO 3, bit 1 = PIO 4 |
| 65-66 | 0x082-0x085 | Multiword DMA cycle time | minimum and recommended, in ns |
| 67 | 0x086 | Minimum PIO cycle without IORDY | ns |
| 68 | 0x088 | Minimum PIO cycle with IORDY | ns |
| 69-79 | 0x08A-0x09F | Reserved | queue depth in word 75 (0x096) |
| 80 | 0x0A0 | Major version | bit n set = supports ATA-n |
| 81 | 0x0A2 | Minor version | |
| 82 | 0x0A4 | Command sets supported | bit 0 = SMART, 1 = security, 3 = power management, 5 = write cache, 6 = look-ahead, 12 = WRITE BUFFER, 13 = READ BUFFER, 14 = NOP |
| 83 | 0x0A6 | Command sets supported, continued | bit 2 = CFA feature set, 3 = advanced power management, 10 = 48-bit LBA, 12 = FLUSH CACHE, 13 = FLUSH CACHE EXT; bits 15-14 = `01` when valid |
| 84 | 0x0A8 | Command sets supported, more | bits 15-14 = `01` when valid |
| 85-87 | 0x0AA-0x0AF | Command sets enabled | same bits as words 82-84; word 85 bit 5 = write cache on |
| 88 | 0x0B0 | Ultra DMA | bits 6-0 = modes supported, bits 14-8 = mode selected |
| 89-90 | 0x0B2-0x0B5 | Security erase times | |
| 91-94 | 0x0B6-0x0BD | Power, password, reset and acoustic values | 91 = power level, 92 = master password revision, 93 = hardware reset result, 94 = acoustic level |
| 100-103 | 0x0C8-0x0CF | 48-bit LBA capacity | |
| 128 | 0x100 | Security status | |
| 129-159 | 0x102-0x13F | Vendor specific | |
| 160 | 0x140 | CFA power mode 1 | bit 15 = word valid; bits 11-0 = maximum current in mA |
| 161-175 | 0x142-0x15F | Reserved for the CompactFlash Association | words 163-164 (0x146-0x149) report CF-only faster modes, which Roscoe can't use |
| 176-205 | 0x160-0x19B | Media serial number | ATA-7 |
| 255 | 0x1FE | Integrity word | low byte `A5h` if present; then all 512 bytes add up to 0 (mod 256) |

These offsets match a buffer stored exactly as read from the Data register. Your BIOS stores IDENTIFY byte-swapped, as it does sector data, so in its buffer each word's two bytes are reversed (`848Ah` sits at 0x000 as `8A 84`). The offsets themselves don't change.