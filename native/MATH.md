# The native build's VU0 maths

The PS2 side does the game's vector maths on VU0 in four platform files: `src/platform/ps2/math.cpp` (`Platform::Math`,
`include/platform/math.h`), `matrices.cpp`, `collisionmaths.cpp` and `bounds.cpp`. Their asm is VU0 macro mode (`lqc2`, `vmula`,
`vopmsub`, `vdiv`, ...), the EE's 128 bit moves (`lq`, `pextlw`, `qmfc2`, ...), three EE FPU instructions (`max.s`, `min.s`,
`rsqrt.s`), and calls of VU0 microprograms (`vcallms`) in the retail microcode. `src/platform/native/math/` is the same code for
the host. It reproduces what the PS2 computes to the bit, as PCSX2 (the decomp's reference) computes it.

## How it's built

- **`vufloat.h`**: the float arithmetic on bit patterns. This is the one place the rounding is decided (see below).
- **`vu0.h`, `vu0.cpp`**: VU0 itself. It has the registers (vf00-vf31 with vf00 = (0, 0, 0, 1), vi00-vi15 of 16 bits, ACC, Q,
  I, R, P), the MAC and clip flags, the 4 KB data memory, and an operation for each instruction with its dest mask, broadcast
  field and I/Q operands, in PCSX2's semantics.
  - An FMAC operation sets the MAC flags of the fields it writes and clears the other fields' flags.
  - Writes to vf00 are dropped, but they still set the flags.
  - MAX, MINI, ABS, MOVE and MR32 set no flags.
  - OPMULA and OPMSUB leave the w field's flags alone.

  There is one VU0 (`NativeMath::TheVu0()`), so registers left by one function are there for the next, as on the PS2 (some
  functions rely on that: `TriangleBounds`' corners' w, the edge tests' second half, `SetRay`).
- **`math.cpp`, `matrices.cpp`, `collisionmaths.cpp`, `bounds.cpp`**: the PS2 files' functions under the same names (and so the
  same `RETAIL` symbols). Each asm line is one operation on the VU0, in the asm's order. Loops keep the asm's counters, including
  32 bit wrap-round and reading one element past the end where the asm does. **`mmi.h`** has the EE's word shuffles.
- **`microprograms.cpp`**: the microprograms the maths calls, translated from the retail microcode. This file is generated, not
  hand-written: `native/math-tests/tools/vu0translate.py` decodes the microcode words from the split
  (`assets/vutext.textbin.bin`, through the three sets' DMA chains' MPG codes; `tools/vudis.py` is its disassembler). It writes
  one operation per instruction, with each instruction's disassembly as a comment. The pipelines' timing is worked out ahead of
  time, the way PCSX2's VU0 interpreter runs the program:
  - An FMAC instruction reading a field written in the last 4 cycles stalls until the write is done.
  - FMAND reads the MAC flags of the last FMAC-pipeline instruction 4 or more cycles back. Programs use stalls on purpose:
    `abs.x vf00, vf11 | fmand` waits for vf11 and so for its flags.
  - Q gets DIV's and SQRT's result 7 cycles later, RSQRT's 13 cycles later; until then the old Q is read. A DIV, SQRT, RSQRT or
    WAITQ in the lower slot waits for the one before it, before its own upper instruction reads Q.
  - A branch right after an integer instruction that writes its register reads the old value.
  - A lower instruction reading the register its upper instruction writes reads the old value (kept in `Vu0::Temporary`).
  - LOI's immediate is I after its upper instruction.
  - Delay slots run, and the end (the E bit's delay slot) writes Q if it's still pending.

  The tool simulates every path through a program and checks that each instruction comes out the same on every path. Each
  program is then one function in address order, with labels and gotos for the branches. The programs are:

  | Address | Set | What calls it |
  |---|---|---|
  | 0x0F0 | standard | `SinCos` |
  | 0x1C0 | standard | `SlerpRotations` |
  | 0x548, 0x668, 0x790 | standard | `JointMatrix`, `MultiplyByParent` |
  | 0x818 | standard | `EulerRotation` |
  | 0xA70 | standard | `TurnRotation` |
  | 0xAB8 | standard | `StartRayTriangle` |
  | 0xA08, 0xB90 | culling | `ParticleBlockView` |
  | 0xCB0 | culling | `ViewDistance` |

  To regenerate the file, run `python3 native/math-tests/tools/vu0translate.py`.
- **Which code is loaded.** A set's chain only writes what it loads: the standard set writes 0x000-0xD97, the culling set
  0x000-0xCF7, the decals' set 0x000-0x51F, and 0x000-0x1B8 is the same in all three. `NativeMath::Vu0ProgramsLoaded(set)`
  records which set's code is where. `Vu0CallMicroprogram` then checks that every instruction the program can run is code it
  was translated from. If another set's code is there, it stops with a message rather than make something up. Until the first
  `Vu0ProgramsLoaded` call the check is off.

## Rounding: what's VU-exact and what isn't

`vufloat.h` gives PCSX2's results with its default settings (VU0 and the FPU round towards zero, denormals are zero, overflow is
clamped):

- **Operands**: an exponent of 0 is a zero of its sign. An exponent of 255 (an infinity or a NaN, which the VU doesn't have) is
  the largest float of its sign (PCSX2's `vuDouble`).
- **Results**: the exact result rounded towards zero. Below the smallest normal float the result is a zero of its sign; it never
  goes past the largest float. The zero, sign, underflow and overflow flags are as PCSX2's `VU_MAC_UPDATE` sets them.
- **MADD, MSUB, OPMSUB**: the product is rounded, then the sum. The VU's FMAC isn't fused.
- **DIV, SQRT, RSQRT**: a zero divisor gives the largest float with the signs' exclusive or (RSQRT's 0 / 0 gives a zero).
  SQRT and RSQRT take the magnitude of a negative operand. RSQRT rounds the root, then the quotient.
- **The EE FPU's MAX.S and MIN.S**: done as PCSX2's recompiler does them. Both operands are clamped and denormals flushed, then
  compared.
- **The EE FPU's RSQRT.S**: rounds the root and then the quotient, towards zero. A zero or denormal divisor gives the largest
  float with fs's sign.

The host has no rounding towards zero in its ordinary arithmetic, so the rounding is done exactly in software:

- A sum's exact error comes from TwoSum in double precision.
- A product of two floats is exact in double precision.
- A quotient or a square root is checked against its exact remainder with `fma`.

None of this depends on the host's rounding mode, compiler or CPU (arm64 and x86-64 give the same bits), so the native build
keeps `-ffp-contract=off` and the host's default rounding. Defining `TWIN_VU_FLOAT_HOST` switches to the host's IEEE arithmetic
(round to nearest), for comparison only.

Where the native results can differ from a PS2:

| What | The native build (PCSX2's model) | A real PS2 | Effect |
|---|---|---|---|
| VU and FPU multiplication | Rounded towards zero, exactly | PCSX2 notes (`iFPU.cpp`) that the PS2's multiplier sometimes gives a mantissa 1 below the result rounded towards zero. Nobody has modelled when | The last bit of a product, now and then. PCSX2 has the same difference, so the decomp's references share it |
| VU addition | Exact sum rounded towards zero (PCSX2's VU; its EE FPU adds keep fewer guard bits, but those are the game's C++, not these files) | The VU adder's guard bits haven't been measured | None known |
| Operands with an exponent of 255 (infinities, NaNs; the VU never makes them) | Clamped to the largest float before use (PCSX2's interpreter) | Taken as huge numbers | Only for data that isn't floats. PCSX2's own recompiler (microVU) doesn't clamp VU operands and gives NaNs there instead: the probe's 3 differences below |
| `max.s`/`min.s` of +0 and -0 | +0 is the larger | PCSX2's recompiler gives either, by its register allocation | None known |
| `rsqrt.s` of a negative zero divisor | The largest float with fs's sign (PCSX2's recompiler) | PCSX2's interpreter takes ft's sign | Sums of squares never give -0 |
| The status flag (vi16) | Not modelled | Kept | Nothing reads it: no FSAND, FSEQ or FSOR in any of the three microcode sets, no CFC2 of vi16 in the C++ |
| Pipeline timing | PCSX2's interpreter's model (above) | The hardware's | The microprograms' reads of Q and the flags come out as their authors planned (the stall tricks work the same) |
| VI registers | 16 bit, CFC2 gives them zero-extended | 16 bit | PCSX2 keeps 32 bits after a CTC2. Nothing writes more than 16 |

## Shared code edits

None. Everything is in `src/platform/native/math/` and `native/math-tests/`. The PS2 build is unchanged:
`build/SLES_525.68.elf` after `tools/build.py` is the same bytes (sha1 206e273e…).

## For the graphics side

The culling programs (`ParticleBlockView`, `ViewDistance`) read the particle views that
`Platform::Graphics::LoadParticleView` writes into VU0's data memory. Two calls connect the sides:

- `NativeMath::SetVu0DataMemory(u8*)` points the maths' VU0 at the data memory the graphics side writes. Without it, the maths
  has its own 4 KB.
- `NativeMath::Vu0ProgramsLoaded(set)`, called from `Platform::Graphics::UseHelperPrograms`, turns on the loaded-code check.

`vu0.h`'s VU0 can also run the graphics side's macro mode (CLIP and the clip flags are there). The graphics side's own
`ee/vu.h` interpreter is a separate VU0, so registers either side leaves aren't seen by the other.

## Tests

```
cmake -S native/math-tests -B build/native/math-tests -G Ninja && cmake --build build/native/math-tests
build/native/math-tests/math_tests [runs per function, default 20000] [seed]
```

The tests compare every function against a reference written separately (`native/math-tests/reference/`), which shares no code
with the implementation:

- PCSX2's VU0 semantics, with the floats on the host FPU switched to rounding towards zero (`fesetround`) and flushed as
  PCSX2's FTZ does.
- A cycle-timed microcode interpreter modelled on PCSX2's `_vu0Exec` (FMAC, FDIV and integer pipelines, branch delay, the E bit,
  VI backup). It runs the microcode words.
- An interpreter of the PS2 side's own asm text: `tools/make_reference.py` turns `src/platform/ps2/{math,matrices,
  collisionmaths,bounds}.cpp` into `Ref_` functions whose asm statements are run from their text. The C++ around the asm is the
  PS2 side's own.

Each run starts both VU0s from the same random state (every register, ACC, Q, I, R, the flags, the data memory) and gives both
the same random inputs. Inputs are mostly the game's kind, with edges, denormals, infinities and NaNs mixed in. The outputs and
**the whole VU0 state afterwards** must match to the bit. The float operations are also tested alone, with 100 times as many
operands, including cancellations and roundings at powers of two. The microprogram tests count the instructions run, to show
every path is taken (for example slerp's 26/28/106/108: straight or spherical, the sign flipped or not).

All functions pass. The tests fail when:

- the FMAC latency is changed to 3;
- Q is written one cycle early;
- the same-pair old value is dropped;
- MADD is fused;
- two of `VuMultiplyMatrices`' multiply-adds are swapped.

### PCSX2 ground truth

`native/math-tests/pcsx2/run_probe.py <scratch>` runs the same cases in PCSX2:

1. It builds a scratch copy of the PS2 build (a detached worktree) with 780 cases and `probe_ps2.cpp` linked in. `Main` runs
   them on the PS2 build's own functions before anything else.
2. It boots that ELF once with `tools/run_pcsx2.py` (muted, in the background; it only closes the PCSX2 it starts), freezes it
   at frame 30 and reads the results out over PINE.
3. `math_tests --probe-check` checks the results against the native functions.

`probe_common.cpp` is the same source on both sides. The VU0 state before each case comes from a seed and goes in through the
same instructions; the whole state is read out after.

With PCSX2 2.8.2 (its recompilers, default settings), **777 of 780 cases match to the bit**:

- every microprogram case of `SinCos`, `SlerpRotations`, `JointMatrix`, `MultiplyByParent`, the ray tests, `TurnRotation`,
  `Lerp`, `ViewDistance` and `ParticleBlockView`;
- all of `matrices.cpp`, `collisionmaths.cpp` and `bounds.cpp`;
- `rsqrt.s`.

The 3 that differ each have an infinity or a NaN among the inputs. PCSX2's microVU passes NaNs through where its interpreter (and
this code) clamps. The recorded inputs and PCSX2's results are kept in `native/math-tests/pcsx2/recorded/`. To check them
again without PCSX2:

```
gunzip -k native/math-tests/pcsx2/recorded/*.gz
build/native/math-tests/math_tests --probe-check native/math-tests/pcsx2/recorded/probe_inputs.bin \
    native/math-tests/pcsx2/recorded/pcsx2_2.8.2_results.bin
```
