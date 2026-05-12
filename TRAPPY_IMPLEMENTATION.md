# TrappyStockfish

TrappyStockfish is an invasive Stockfish modification inspired by TrappyBeowulf and the Trappy
Minimax paper. It integrates trap-aware scoring into Stockfish search by recording iterative-depth
opponent-reply score vectors, computing trappiness/profit/quality, and applying a bounded trap bonus
inside recursive search rather than only re-ranking root moves.

## Base

- Stockfish source: official-stockfish/Stockfish
- Base commit: `dd321af5dfc0789de07c4e5c64915073995eb818`
- Base tag at checkout: `stockfish-dev-20260510-dd321af5`
- Trappy reference implementation: `vollmerm/TrappyBeowulf`

## UCI Options

- `Trappy Minimax`: enabled by default.
- `Trappy Max Ply`: default `5`, range `1..12`.
- `Trappy Assessment`: default `median`, choices `median`, `best`, `last`.
- `Trappy Max Sacrifice`: default `200` cp, range `0..2000`.
- `Trappy Bonus Cap`: default `300` cp, range `0..2000`.
- `Trappy Min Profit`: default `50` cp, range `0..2000`.
- `Trappy Trace`: default `false`; emits `info string trappy ...` diagnostics.

## Validation

Builds used the working MSYS2 UCRT64 GCC toolchain because the local MINGW64 toolchain failed to link
unmodified Stockfish with unresolved wide-character CRT symbols.

Commands:

```powershell
$env:PATH='C:\msys64\usr\bin;C:\msys64\ucrt64\bin;' + $env:PATH
mingw32-make -C src -j build ARCH=x86-64 COMP=gcc
```

Results:

- Baseline binary saved as `src\stockfish_base.exe`.
- Candidate binary saved as `src\stockfish_trappy.exe`.
- Disabled-mode equivalence passed:
  - baseline `bench 32 1 13 default depth`: `2,300,602` nodes
  - candidate with `Trappy Minimax false`: `2,300,602` nodes
- Enabled bench passed:
  - candidate default `bench 32 1 13 default depth`: `1,900,973` nodes
- UCI smoke tests passed at depths 6, 10, and 14 with `Trappy Trace true`.

The 200-game fastchess screening was interrupted by the user after the run had produced a full
autosaved result summary. It showed TrappyStockfish is intentionally much weaker than normal
Stockfish with the current loose defaults, which matches the project goal of style over maximum Elo.

