# Resident Evil 4 (GameCube) decompilation

This is a complete decompilation of the GameCube debug build of Resident Evil 4, `G4BE08`, the
"Nov 25 2004" prototype on two discs. The debug build shipped with `Bio4.sym` symbol files that name
every function, and that is what made a full decompilation possible.

Building the repository gives back the exact original files: `main.dol` and all 114 REL overlays,
checked against `config/G4BE08/build.sha1` on every build. The source is about 570,000 lines of C and
C++ in `src/` and 44,000 lines of headers in `include/`, with no assembly files.

The code is built with the compilers the original was built with:

| Code | Compiler |
|---|---|
| The game | SN Systems ProDG 3.9.3, GCC 2.95.3 "SN BUILD v1.79", built from SN's GPL source release |
| CRI middleware (`adx_*`, `sfd_*`, `mpv_*` and the rest in `src/lib/`) | Metrowerks CodeWarrior 2.4.7, which CRI shipped the libraries with |
| Nintendo SDK (`OS*`, `GX*` and the rest in `src/lib/`) | Metrowerks CodeWarrior GC/1.2.5n, sources from [dolsdk2004](https://github.com/doldecomp/dolsdk2004) |

There are no game assets in this repository. To build it you need your own images of the debug
discs. Disc 1 has `main.dol` and most of the RELs, disc 2 has the four RELs for the island stages.

## Building

You need Linux, Python 3 and [ninja](https://ninja-build.org/). The first configure run downloads
decomp-toolkit, objdiff, wibo and the CodeWarrior compilers. The SN GCC is built once from SN's
source:

```sh
# 1. build the SN compiler once (needs SN's GPL source release, see tools/sn-gcc/build.sh)
SN_GCC_SRC=/path/to/NGC_GNU_SRC/NGC tools/sn-gcc/build.sh

# 2. copy in your disc images
cp re4_debug_disc1.iso re4_debug_disc2.gcm orig/G4BE08/

# 3. build
python3 configure.py && ninja
```

`ninja` finishes with a progress report that should say 100% matched and linked. To check the result,
`build/tools/dtk shasum -c config/G4BE08/build.sha1` should print one `OK` per file, 115 in all.

When working on one unit, `python3 tools/bytecmp.py game/foo` compares its object file with the
original, and `python3 tools/fdiff.py game/foo <symbol>` shows one function side by side.

## What's where

- `src/game/` is the game itself. `src/em*/` are the enemies, `src/wep*/` the weapons, `src/pl*/` the
  player characters and `src/st*/` the rooms, one REL per room. `src/t_*/`, `src/Tools/` and
  `src/tools/` are the in-game debug editors, `src/Sscrn/` the menu screens and `src/lib/` the SDK,
  CRI and runtime libraries.
- `include/` has the headers, including the reconstructed structs.
- `config/G4BE08/` has the unit lists, symbols, splits, linker scripts and per-REL data.
- `tools/` has the build scripts, the compare tools, the SN compiler build and the research kit.
  `tools/motion_export.py` exports the game's animations to glTF and BVH, see `tools/motion/README.md`.
- `docs/overview.md` is a guide to how the engine fits together. `docs/matching.md` explains how the
  matching was done and `docs/unit-notes.md` has notes per unit. `docs/research/` is the detailed log.

## How close is the match

Every unit compiles to the original bytes with the original compilers. In some places the compiler
only picks the same registers or instruction order as the original if the code is written in a
particular way, and no natural way of writing it was found. Those spots are marked with a
`// COMPILER-DIFF:` comment, 526 of them at the moment. Most are a dead test, an empty `asm("")` or a
`register T x asm("rN")` declaration. None of them puts an instruction into the output, and
`python3 tools/asmcheck.py --all` checks that. `docs/matching.md` explains the reason behind each
kind. Getting this number down is ongoing work.

Almost all the assembly left is code the original developers also wrote in assembly, because their
compilers couldn't express it in C. That's the paired-single math and matrix routines, the GQR setup,
an exception handler, the cache and SPR code in the CRI video decoder, and eight startup, debugger and
decompression units that were assembly to begin with. In the game code `asmcheck.py` counts 231
instructions of it, with the eight assembly units counted separately. The exceptions are in the CRI
libraries: one small block in `dct_ac` that works around a difference between our build of
CodeWarrior and Capcom's, and 28 register pins that emit nothing. `docs/naming.md` lists all of it.

## Names

Function names are Capcom's own, from `Bio4.sym`. They are C++ mangled names, which is why the game
code is C++ and the SDK, CRI and C library code is C. File names and the split into units come from
the `D:/Bio4/Prog/<file>.cpp` paths the asserts left in the binaries.

Struct and field names come from three places. Most are Capcom's, taken from the debug information in
the PS2 debug build and matched to the GameCube layouts with `tools/ps2sym.py`. Some are ours, named
from how the code uses them. The rest are placeholders like `x1C`, named after their offset because
their meaning isn't known yet. Capcom's names keep Capcom's spelling, so the naming style is mixed on
purpose. Constants use the PS2 build's enums. `docs/naming.md` has the details.

## Contributing

See `CONTRIBUTING.md` for the build, the three checks a change has to pass and the rules. The main rule
is that the output bytes never change.

## Legal

The reconstructed game and SDK source is the property of Capcom, Nintendo and CRI Middleware and is
published for research and preservation only. The build scripts, tools, configuration and
documentation written for this project are released under CC0, see `LICENSE`.
