# CDFJ+ driver and DASM includes (third-party)

Every CDFJ+ ROM starts with a 2 KB ARM driver written by the CDFJ authors
(Chris Walton, Darrell Spice Jr, Fred Quimby, John Champeau). These files are
copied unchanged from the community template
https://github.com/Aganarr/CDFJplus-template at commit
`4823c5842d8d6c902fee981e5790b80099afb247` (2026-05-26). That repository has
no license file; the driver is the one the CDFJ authors share for homebrew use.

| File | What it is |
|---|---|
| `cdfjplus48_p1..p4.bin` | The driver, split so the build can insert three option bytes between the pieces: LDX fast fetch (`$A2` on / `$A9` off), LDY fast fetch (`$A0` on / `$A9` off), and the fast-fetch offset (0-200). Concatenated with those bytes it is exactly 2048 bytes. |
| `cdfjplus.h` | DASM register map: `DSxDATA`, `DSWRITE`, `DSPTR`, `SETMODE`, `CALLFN`, `BANK0-6`, `FASTJMP`. |
| `vcs.h`, `macro.h` | Standard DASM Atari 2600 includes. |
| `tia_constants.h` | TIA and RIOT register names (from the same template). |

The boot files in `src/arm/boot/` (`custom.S` and `custom.boot.lds`) are also from
this template, unchanged except that `custom.boot.lds` places the C code at
`$2800` (C_START) instead of the template's `$6800`.

## Which driver variant this is

This is the **48-pin** driver (LPC210x: Harmony, Harmony Encore, Melody),
meant for 32 KB ROM / 8 KB RAM carts. The 64/128/256/512 KB ROM sizes with
16/32 KB RAM use a separate **64-pin** (LPC213x) driver, which is shared on the
AtariAge forums (needs a login). It is not included here.

Stella does not run the driver code. It only reads its signature and option
bytes, always provides 32 KB of cart RAM, and accepts ROM images up to 512 KB.
So this driver works in Stella at any ROM size. For real hardware above 32 KB,
swap in the 64-pin driver.

SHA-256:

```
2b2ea16e95fb8a08dfe0e1d724763e8a8769196d72781b86c91aeef94ad48fbe  cdfjplus48_p1.bin
becbaa23e4198ef6c581bd59938c296722c06d63aace5cab9565a954303ca0b7  cdfjplus48_p2.bin
bd19bc28c9d81573f9499da86d834992ed82ba45e7e512479691ed9bdbce502d  cdfjplus48_p3.bin
c6f24b9910711b3fa379464b2c173a3daa1b81b288c3ac2a90214069b42e318c  cdfjplus48_p4.bin
```
