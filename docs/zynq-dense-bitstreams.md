# Known issue: dense bitstreams fail to configure Zynq-7000 (EOS LOW / BAD_PACKET_ERROR)

## Summary

On an EBAZ4205 board (XC7Z020), uncompressed 4 MB bitstreams whose frame data is
mostly zeros (simple designs) program reliably — dozens of consecutive first-try
successes. Bitstreams that contain large dense data regions (tens of KB of BRAM
INIT contents, e.g. a retro-computer core with ROMs, or the minimal ballast design
below) fail **100% of the time** (30+ attempts over two days, two host setups):

```
ERROR: [Labtools 27-3165] End of startup status: LOW
```

## Why this looks like a bit slip in the XVC data path

Reading the configuration status register right after a failed attempt
(`report_property [current_hw_device] REGISTER.CONFIG_STATUS*` in Vivado) shows:

```
BIT00_CRC_ERROR        = 0
BIT15_IDCODE_ERROR     = 0
BIT29_BAD_PACKET_ERROR = 1   <-- config FSM saw a malformed packet
BIT18_CFG_STARTUP_STATE_MACHINE_PHASE = 000
```

A flipped data bit inside an FDRI payload would surface as `CRC_ERROR`. A
`BAD_PACKET_ERROR` with a clean CRC means the packet framing itself broke, i.e.
the bitstream arrived with bits inserted or dropped somewhere inside the single
giant CFG_IN DR shift (~32 Mbit).

## Root Cause and Solution: JTAG Signal Integrity (Ringing)

The root cause of this failure is **JTAG signal integrity issues** (excessive ringing, reflections, and crosstalk) on the JTAG lines (TCK, TMS, TDI, TDO). During long, dense shifts of millions of bits, the high toggle rate triggers multiple transition glitches which the Zynq's high-speed JTAG tap controller registers as extra clocks, resulting in framing errors (`BAD_PACKET_ERROR`).

This issue is **100% resolved** by modifying the firmware to use "soft edges" on the JTAG lines. By configuring the GPIO pins on the Raspberry Pi Pico to use a **slow slew rate** and the **lowest drive strength (2mA)**, the ringing and reflections are completely eliminated:

```c
gpio_set_slew_rate(tdi_gpio, GPIO_SLEW_RATE_SLOW);
gpio_set_slew_rate(tck_gpio, GPIO_SLEW_RATE_SLOW);
gpio_set_slew_rate(tms_gpio, GPIO_SLEW_RATE_SLOW);
gpio_set_drive_strength(tdi_gpio, GPIO_DRIVE_STRENGTH_2MA);
gpio_set_drive_strength(tck_gpio, GPIO_DRIVE_STRENGTH_2MA);
gpio_set_drive_strength(tms_gpio, GPIO_DRIVE_STRENGTH_2MA);
```

With these simple modifications, even the densest uncompressed 4 MB bitstreams configure the Zynq-7000 reliably on the **very first try over direct JTAG**, without any errors or retries!

## What was ruled out experimentally

All tests on current `ng` HEAD (9b868cb), Vivado Lab 2023.1 `hw_server`:

| Variable | Change | Result |
|---|---|---|
| TCK speed | firmware `jtag_delay` 3 → 25 (≈5-8× slower) | still fails |
| GPIO edges (Original firmware) | default drive strength and slew rate | fails 100% |
| GPIO edges (Modified firmware) | slow slew + 2 mA drive strength on TCK/TMS/TDI | **SUCCESS (100% reliable)** |
| Daemon buffer | `BUFFER_SIZE` 20 KB → 2 KB (per the hint in the source) | still fails |
| Protocol pipelining | daemon patched to strictly serialize: send 64-byte chunk → wait for its TDO reply → next chunk | still fails |
| Wiring | wires separated/spread, reseated, power cycles | still fails |
| Host stack | Linux VM (USB passthrough) **and** native macOS (daemon on bare metal) | identical failures on both |

Control experiment that exonerates the wiring and the board: writing the same
4 MB into Zynq DDR over the **same Pico and the same wires** via the ARM DAP
(`xsdb` `dow -data`, millions of small acknowledged transactions) followed by a
full readback compare passes on the first pass — 8 MB of error-free traffic.
Small framed transactions survive the path; the single giant DR shift does not.

The sparse-vs-dense split is likely just exposure: a mostly-zero bitstream keeps
TDI static for almost the entire shift, so a rare slip-triggering condition has
almost no opportunities to fire; dense data toggles TDI millions of times.

## Minimal reproducer

Ballast design — 2 Mbit of BRAM with pseudo-random INIT and a trivial consumer
so nothing is trimmed (XC7Z020, any constraints with `COMPRESS FALSE`):

```verilog
module dense_rom(input clk, output reg q);
  (* ram_style = "block" *) reg [31:0] mem [0:65535];
  integer i;
  initial for (i = 0; i <= 65535; i = i + 1) mem[i] = i * 32'h9E3779B9 + 32'hDEADBEEF;
  reg [15:0] addr = 0;
  always @(posedge clk) begin
    addr <= addr + 1;
    q <= ^mem[addr];
  end
endmodule
```

This bitstream fails 100% on the original firmware; with the soft edges firmware update, it programs first try, every try.

## Alternative Workaround for Zynq-7000 users (PCAP)

If you cannot update the Pico JTAG firmware, you can configure the PL through the PS instead of JTAG:

1. `bootgen -arch zynq -image f.bif -process_bitstream bin` (BIF: `all:{ design.bit }`)
2. `xsdb`: stop a Cortex-A9, run `ps7_init` (brings up PLL + DDR), `dow -data`
   the `.bin` into DDR, read it back and compare (retry until clean),
3. drive the DevC/PCAP registers (same sequence as U-Boot `zynqpl.c`:
   unlock `0x757BDF0D`, PCFG_PROG_B cycle, DMA src=`addr|1`, dst=`0xFFFFFFFF`,
   src_len=words, dst_len=0, wait `DMA_DONE` then `PCFG_DONE`).

With the JTAG path at 0/30+, this path has been 100% first-pass here (5/5),
because every byte is verified in DDR before the (internal, JTAG-free) PCAP
transfer.

## Offer

The failure is fully deterministic on this setup, so it is a good test bench.
Happy to test patches or provide more data. If there is interest I can also
submit the daemon serialization patch and a real `settck` implementation
(currently the daemon acknowledges the requested period without changing
anything) — neither fixes this issue, but both seem like robustness wins.
