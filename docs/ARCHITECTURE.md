# Architecture

## Overview

This solver has two distinct subsystems that will eventually work together:

```
┌─────────────────────────────────────────────────────────┐
│                    User Interface (Python)               │
│  Streamlit app: range charts, equity graphs, strategy    │
│  browser, iteration inspector, ICM calculator            │
└────────────────────────┬────────────────────────────────┘
                         │ pybind11 (planned)
┌────────────────────────┴────────────────────────────────┐
│                   C++ Poker Engine                       │
│                                                          │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │   Core   │  │    Solver    │  │      Modes        │  │
│  │          │  │              │  │                   │  │
│  │ Card     │  │ GameState    │  │ CashGame (chipEV) │  │
│  │ HandEval │  │ Strategy     │  │ Tournament (ICM)  │  │
│  │ Range    │  │ CFRStrategy  │  │                   │  │
│  │ Equity   │  │              │  │                   │  │
│  └──────────┘  └──────────────┘  └───────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## Subsystem 1: Game Simulator

Responsibilities:
- Represent the rules of No-Limit Hold'em
- Track game state (pot, stacks, board, history)
- Generate legal actions given the current state and bet sizing config
- Determine when a hand is over and compute payoffs

Key classes: `GameState`, `Card`, `Deck`, `HandEvaluator`

### State Transition Model

```
              ┌───────────────────────────────────────┐
              │           PREFLOP                      │
              │  Blinds posted → UTG acts first        │
              │  Legal: fold, call, raise (2.236^k BB) │
              └──────────────┬────────────────────────┘
                             │ all bets matched
                             ▼
              ┌───────────────────────────────────────┐
              │            FLOP                        │
              │  3 community cards dealt               │
              │  BB acts first (HU) / first active     │
              │  Legal: check/bet (pot fractions)      │
              └──────────────┬────────────────────────┘
                             │ all bets matched
                             ▼
              ┌───────────────────────────────────────┐
              │            TURN                        │
              │  1 community card dealt                │
              │  Same action structure as flop         │
              └──────────────┬────────────────────────┘
                             │ all bets matched
                             ▼
              ┌───────────────────────────────────────┐
              │           RIVER                        │
              │  1 community card dealt                │
              │  Same action structure                 │
              └──────────────┬────────────────────────┘
                             │ all bets matched
                             ▼
              ┌───────────────────────────────────────┐
              │          SHOWDOWN                      │
              │  Best 5-of-7 hand wins the pot         │
              │  Ties split the pot                    │
              └───────────────────────────────────────┘

At any point:  fold → hand over (folder loses, last player wins pot)
               all-in → no more actions for that player
               only 1 player left → hand over immediately
```

### GameState Immutability

`GameState::applyAction()` returns a **new** `GameState`. The original is never modified.
This is critical for CFR tree traversal, where we explore multiple action branches
from the same state.

```cpp
GameState s0 = ...;
GameState s1_fold = s0.applyAction({FOLD, 0});   // s0 unchanged
GameState s1_call = s0.applyAction({CALL, 1.0});  // s0 unchanged
GameState s1_raise = s0.applyAction({BET, 5.0});  // s0 unchanged
// Now we can evaluate all three branches independently
```

## Subsystem 2: Strategy Evaluator (CFR+ Engine)

Responsibilities:
- Store an extensive-form strategy (action probabilities at every decision point)
- Train the strategy via CFR+ iterations
- Report what changed after each iteration
- Compare two strategies by simulating them against each other

### What Is an Information Set?

In poker, a player doesn't know the opponent's cards. An **information set** groups
all game states that are indistinguishable to the player:

```
Information set = (my hole cards, public board, public action history)
```

At each information set, the strategy specifies a probability distribution over actions.
For example:

```
Info set: "I have A♠K♠, flop is A♦7♥2♣, opponent checked"
Strategy: Check 30%, Bet 1/2 pot 45%, Bet pot 20%, All-in 5%
```

### CFR+ Algorithm (High Level)

```
for iteration = 1 to N:
    for each possible deal of hole cards:
        traverse the game tree:
            at each decision node:
                1. compute strategy from regrets (regret matching)
                2. compute EV for each action
                3. compute regret = (action EV) - (weighted average EV)
                4. cumulative_regret += regret  (clipped to ≥ 0 in CFR+)
                5. cumulative_strategy += strategy (weighted by reach probability)

    the average strategy (cumulative_strategy / sum) converges to Nash equilibrium
```

**Key property**: as iterations → ∞, the average strategy's exploitability → 0.
In practice, 1000–10000 iterations produce usable strategies for simplified games.

### Strategy Comparison

Two strategies with **different bet sizes** (e.g. A uses [1/4, 1/2, 3/4, 1x] pot,
B uses [1/3, 2/3, 1x] pot) produce different game trees. To compare them:

1. Play A vs B: at each node, the acting player uses THEIR bet sizes.
2. The responding player maps the bet to their closest available response action.
3. Simulate many hands, measure average EV difference.
4. Repeat with A as P1 and A as P2 to remove positional bias.

This allows questions like:
- "Does strategy C (with 10 bet sizes) beat A (with 4 bet sizes) after 1000 iterations?"
- "How many iterations does C need to match A's converged strategy?"

### Iteration Visibility

Each CFR+ iteration produces an `IterationResult` containing:
- Which info sets were updated
- For the most-changed info sets: old strategy, new strategy, regrets
- This lets the user watch convergence in real time

## Data Flow: Equity Calculation

```
Input:  hero_hand = (A♠, K♠)
        villain_range = "AA, KK, QQ, AKs, AKo"
        board = [A♦, 7♥, 2♣]

  ┌───────────────────────────────────────────────────┐
  │  1. Expand villain range into concrete combos      │
  │     "AA" → {AhAd, AhAc, AdAc}  (As dead)         │
  │     "AKs" → {KhAh, KdAd, KcAc} (As,Ks dead)      │
  │     ...                                            │
  │  2. For each villain combo (that doesn't conflict):│
  │     - Dead cards = hero + villain + board           │
  │     - Remaining deck = 52 - dead                   │
  │     - Enumerate all turn+river runouts (C(n,2))    │
  │     - For each runout:                             │
  │         evaluate hero 7-card hand                  │
  │         evaluate villain 7-card hand               │
  │         count win / tie / loss                     │
  │  3. Weight results by villain combo frequency       │
  │  4. Return equity = (wins + ties/2) / total         │
  └───────────────────────────────────────────────────┘

Output: equity = 0.7976 (79.76%)
```

## Data Flow: Street-by-Street All-In Equity

Use case: both players are all-in. Show equity at each street as cards are dealt.

```
Input:  hero = Q♠T♠,  villain = A♥K♦
        board = [A♠, 8♥, 5♦, T♣, 2♠]  (full 5-card board)

Output:
  Flop  (A♠ 8♥ 5♦):         hero equity =  8.69%  (villain has top pair)
  Turn  (A♠ 8♥ 5♦ T♣):      hero equity = 11.36%  (hero picks up a pair of tens)
  River (A♠ 8♥ 5♦ T♣ 2♠):   hero equity =  0.00%  (villain's pair of aces holds)
```

## Card Encoding Details

A card is a single `uint8_t` in [0, 51]:

```
id = rank * 4 + suit

rank (0-12):  2  3  4  5  6  7  8  9  T  J  Q  K  A
suit (0-3):   c  d  h  s

Examples:
  2♣ =  0    2♦ =  1    2♥ =  2    2♠ =  3
  3♣ =  4    3♦ =  5    ...
  A♣ = 48    A♦ = 49    A♥ = 50    A♠ = 51
```

A **hole card combo** is an unordered pair {c1, c2} mapped to index [0, 1325]:

```
index = max(c1,c2) * (max(c1,c2) - 1) / 2 + min(c1,c2)
```

This gives a unique index for each of C(52,2) = 1326 possible combos.

## Hand Evaluation Details

The evaluator returns a 32-bit `HandRank` where higher value = stronger hand:

```
bits 24-27: category
  0 = High Card        5 = Flush
  1 = Pair             6 = Full House
  2 = Two Pair         7 = Four of a Kind
  3 = Three of a Kind  8 = Straight Flush
  4 = Straight

bits 0-23: kickers (packed 4 bits per rank, most significant first)
  Example: Full House (AAA-KK) → category=6, kickers=A(12),K(11)
           → value = 0x060C_B000
```

For 7-card hands, we evaluate all C(7,5) = 21 five-card subsets and take the best.
The 21 combinations are precomputed as a static array.

Straight detection uses a lookup table indexed by 13-bit rank bitmask (8192 entries).

## Bet Sizing Rationale

Preflop uses geometric sizing: 2.236 ≈ √5, so the series 1, √5, 5, 5√5 gives
a logarithmically-spaced set of raise sizes. This balances expressiveness with
game tree size.

Postflop uses pot-fraction sizing: [1/4, 1/2, 3/4, 1x, 1.25x] covers the
standard range of bet sizes used in practice. ALL-IN is always available.

These are defaults. Each `Strategy` can override with its own `BetSizingConfig`.

## Seat Numbering

For 6-max, seats are numbered 0–5:

```
Seat 0 = UTG (Under the Gun)     — first to act preflop
Seat 1 = HJ  (Hijack)
Seat 2 = CO  (Cutoff)
Seat 3 = BTN (Button / Dealer)
Seat 4 = SB  (Small Blind)
Seat 5 = BB  (Big Blind)
```

For heads-up (2 players), seat 0 = SB/BTN and seat 1 = BB.
Preflop: SB acts first. Postflop: BB acts first.

## Threading Model

The equity calculator uses `std::thread` for parallel board enumeration.
Default thread count = `std::thread::hardware_concurrency()`.
Each thread evaluates a slice of the remaining-card combinations independently,
then results are merged under a mutex.

CFR+ will also be parallelized (planned): external-sampling MCCFR allows
independent traversals that can be batched across threads.

## File Dependencies

```
common.h          ← standalone, no dependencies
    ↑
card.h            ← depends on common.h
    ↑
hand_evaluator.h  ← depends on common.h, card.h
    ↑
range.h           ← depends on common.h, card.h
    ↑
equity_calculator.h ← depends on common.h, card.h, hand_evaluator.h, range.h
    ↑
game_state.h      ← depends on common.h, card.h
    ↑
strategy.h        ← depends on common.h, game_state.h
```
