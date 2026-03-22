# Poker GTO Solver

A personal GTO (Game Theory Optimal) poker solver for 6-max No-Limit Hold'em.  
Supports **cash games** and **Poker Now tournaments**.

## Architecture

```
engine/                 C++ core (performance-critical)
├── include/poker/
│   ├── common.h              types, enums, bet sizing config
│   ├── card.h                Card (int-encoded) and Deck
│   ├── hand_evaluator.h      5/7-card evaluation (bit manipulation + lookup)
│   ├── range.h               weighted hand ranges
│   ├── equity_calculator.h   range-vs-range equity, multi-threaded
│   ├── game_state.h          game rules, legal actions, state transitions
│   └── strategy.h            abstract strategy interface + CFR+ design
├── src/
│   ├── card.cpp
│   ├── hand_evaluator.cpp
│   ├── range.cpp
│   ├── equity_calculator.cpp
│   ├── game_state.cpp
│   └── main.cpp              test suite / demo
└── CMakeLists.txt

python/                 visualization (planned)
docs/                   documentation (planned)
```

## Build

```bash
cd engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./poker_test
```

## Performance

Benchmarked on Apple Silicon (M-series):

| Operation | Speed |
|---|---|
| 5-card hand evaluation | **51M evals/sec** |
| AA vs KK preflop equity (exhaustive, 1.7M boards) | **638 ms** |
| Hand vs range (AKs vs top-20%, flop, 50K MC samples) | ~100 ms |

## Bet Sizing

Configurable per strategy. Default:

- **Preflop**: 2.236^k BB for k ∈ {0,1,2,3} → 1, 2.236, 5, 11.18 BB + ALL-IN
- **Postflop**: 1/4, 1/2, 3/4, 1x, 1.25x pot + ALL-IN

## Features

### Implemented
- [x] Card/Deck with compact integer encoding
- [x] Fast hand evaluator (bit manipulation + straight lookup table)
- [x] Weighted hand ranges with range string parsing (`"AA, AKs, QQ-TT"`)
- [x] Equity calculator: hand vs hand, hand vs range, range vs range
- [x] All-in equity tracking across streets (flop → turn → river)
- [x] Game state with full rules enforcement (blinds, legal actions, street transitions)
- [x] Abstract strategy interface with extensible bet sizing

### Planned
- [ ] CFR+ solver implementation (C++)
- [ ] Per-iteration strategy diffs (inspect convergence)
- [ ] Cross-strategy comparison (different bet sizes, different iterations)
- [ ] ICM model for tournament play
- [ ] Python visualization layer (Streamlit)
- [ ] Push/fold charts for short-stack tournament play

## Strategy Design

Strategies are extensible via inheritance:

```
Strategy (abstract)
├── CFRStrategy      — trainable via CFR+, stores per-info-set regrets
└── FixedStrategy    — for testing (always fold, always call, etc.)
```

Key design: **bet sizing is part of the strategy, not the game**.  
Two strategies with different bet sizes produce different game trees.  
Cross-strategy comparison uses "closest available action" mapping.
