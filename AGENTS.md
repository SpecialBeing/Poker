# AGENTS.md — AI Agent Guide

> This file is the entry point for any AI agent working on this codebase.
> Read this FIRST before making any changes.

## What Is This Project?

A **GTO (Game Theory Optimal) poker solver** for 6-max No-Limit Texas Hold'em.
It computes Nash equilibrium strategies using CFR+ (Counterfactual Regret Minimization Plus).

Two game modes:
- **Cash game** — chip EV, stacks reset each hand
- **Tournament** (Poker Now format) — ICM-adjusted equity, stacks persist

## Language Split

| Layer | Language | Why |
|---|---|---|
| Core engine (evaluation, equity, CFR+, game tree) | **C++17** | Performance-critical: hand evaluator runs 51M evals/sec, CFR+ needs billions of iterations |
| Visualization, analysis UI | **Python** (planned) | Rapid iteration, Streamlit/Plotly for charts |

**Rule: Never implement core algorithms in Python.** Python is only for visualization and orchestration.

## Repository Structure

```
engine/                     ← C++ core (the important stuff)
├── CMakeLists.txt          ← build system (CMake ≥ 3.16, C++17)
├── include/poker/
│   ├── common.h            ← enums, constants, BetSizingConfig, Action
│   ├── card.h              ← Card (uint8 id=rank*4+suit), Deck, parseCards()
│   ├── hand_evaluator.h    ← HandRank (uint32 score), HandEvaluator
│   ├── range.h             ← Range (1326-element weight array over hole card combos)
│   ├── equity_calculator.h ← EquityCalculator (hand-vs-hand, hand-vs-range, range-vs-range)
│   ├── game_state.h        ← GameState (pot, stacks, bets, legal actions, state transitions)
│   └── strategy.h          ← Strategy (abstract), CFRStrategy, compareStrategies()
├── src/
│   ├── card.cpp
│   ├── hand_evaluator.cpp
│   ├── range.cpp
│   ├── equity_calculator.cpp
│   ├── game_state.cpp
│   └── main.cpp            ← test suite / demo
python/                     ← visualization (planned)
docs/
├── ARCHITECTURE.md         ← detailed design, data flow, algorithms
└── GLOSSARY.md             ← poker + CS terminology
```

## Key Data Representations

```
Card:       uint8_t  id = rank * 4 + suit
            rank: 0=2, 1=3, ..., 12=Ace
            suit: 0=clubs, 1=diamonds, 2=hearts, 3=spades
            Example: A♠ = 12*4+3 = 51,  2♣ = 0*4+0 = 0

HandRank:   uint32_t value
            bits 24-27: category (0=High Card ... 8=Straight Flush)
            bits  0-23: kickers (higher = better within category)
            Comparison: just compare the uint32_t values

Range:      double[1326]  weights
            Index = holeCardIndex(c1, c2) = max(c1,c2) * (max(c1,c2)-1) / 2 + min(c1,c2)
            Weight 0 = not in range, weight 1 = full frequency

InfoSetKey: string  "P{player}:B{bucket}:{history}"
            Used as hash key in CFR+ info-set map
```

## Bet Sizing Convention

- **Preflop**: sizes are in BB (big blinds). Default: 2.236^k for k∈{0,1,2,3} → [1, 2.236, 5, 11.18]
- **Postflop**: sizes are fractions of pot. Default: [0.25, 0.50, 0.75, 1.00, 1.25]
- **ALL-IN** is always implicitly available as an action
- Bet sizing is a property of the **Strategy**, not the game. Different strategies can have different bet sizes.

## Build & Test

```bash
cd engine && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./poker_test    # runs all tests, prints results
```

## Implementation Status

### Done
- Card/Deck encoding and manipulation
- Hand evaluator (5-card and 7-card, bit manipulation + straight lookup)
- Weighted hand ranges with string parsing
- Equity calculator (exhaustive + Monte Carlo, multi-threaded)
- Game state with full NL Hold'em rules
- Strategy abstract class hierarchy (designed, not yet implemented)

### Not Yet Implemented
- `CFRStrategy` training logic (the `.cpp` for strategy.h)
- Strategy serialization (save/load)
- Strategy comparison (`compareStrategies()`)
- ICM (Independent Chip Model) for tournaments
- Python visualization layer
- pybind11 bridge between C++ and Python

## Design Invariants (Do Not Break)

1. `GameState::applyAction()` returns a **new** state; it never mutates the original.
2. `HandRank` values are directly comparable via `<`, `>`, `==` — no special comparison needed.
3. `Range` always has exactly 1326 entries (one per unordered hole-card pair).
4. All card integers are in [0, 51]. Out-of-range values are undefined behavior.
5. `BetSizingConfig` belongs to `Strategy`, not `GameState`. `GameState` receives it at construction to generate legal actions.
6. Namespace: everything is in `namespace poker`.

## When Modifying Code

- **Adding a new action type**: update `ActionType` enum in `common.h`, then update `legalActions()` and `applyAction()` in `game_state.cpp`, and `Action::toString()` in `card.cpp`.
- **Changing bet sizes**: modify `BetSizingConfig::standard()` or create a new config. Do NOT hardcode sizes in game logic.
- **Adding a new equity calculation mode**: add a method to `EquityCalculator`. Keep the existing methods unchanged.
- **Implementing CFR+**: create `engine/src/strategy.cpp`. The interface is already defined in `strategy.h`.
