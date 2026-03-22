# Glossary

Terminology reference for both poker concepts and computer science concepts
used throughout this codebase.

---

## Poker Terms

### Game Structure

| Term | Definition |
|---|---|
| **NL Hold'em** | No-Limit Texas Hold'em. Each player gets 2 hole cards; 5 community cards are dealt. Best 5-of-7 hand wins. "No-Limit" means you can bet any amount up to your stack. |
| **6-max** | A table with at most 6 players (as opposed to 9-max "full ring"). |
| **Heads-up (HU)** | A hand or game between exactly 2 players. |
| **Cash game** | A game where chips have fixed monetary value. Players can buy in and leave freely. |
| **Tournament** | A game where players start with a fixed chip count. Blinds increase over time. Last player(s) standing win prizes from the pool. |
| **Poker Now** | An online poker platform (pokernow.club) commonly used for private tournaments. |

### Positions (6-max)

| Position | Seat | Notes |
|---|---|---|
| **UTG** | 0 | Under The Gun. First to act preflop. Tightest position. |
| **HJ** | 1 | Hijack. Second to act preflop. |
| **CO** | 2 | Cutoff. One before the dealer. |
| **BTN** | 3 | Button / Dealer. Best position (acts last postflop). |
| **SB** | 4 | Small Blind. Posts half a big blind. Worst position. |
| **BB** | 5 | Big Blind. Posts a full big blind. Acts last preflop. |

In heads-up, BTN = SB (seat 0) and BB (seat 1). SB acts first preflop; BB acts first postflop.

### Streets (Betting Rounds)

| Street | Community Cards | Description |
|---|---|---|
| **Preflop** | 0 | After hole cards dealt, before any community cards. |
| **Flop** | 3 | First 3 community cards dealt simultaneously. |
| **Turn** | 4 | Fourth community card. |
| **River** | 5 | Fifth and final community card. |
| **Showdown** | 5 | After final betting, remaining players reveal hands. |

### Actions

| Action | Meaning |
|---|---|
| **Fold** | Surrender the hand. Lose all chips already committed. |
| **Check** | Pass without betting (only legal if no one has bet in this round). |
| **Call** | Match the current bet to stay in the hand. |
| **Bet** | Put chips in when no one has bet this round (or raise over a previous bet). |
| **Raise** | Increase a previous bet. In this codebase, `BET` covers both bet and raise. |
| **All-in** | Put all remaining chips in. Cannot be forced to fold after going all-in. |

### Hand Rankings (Worst to Best)

| Rank | Example | Code Category |
|---|---|---|
| High Card | A♠ K♦ 9♣ 7♥ 2♠ | 0 |
| Pair | A♠ A♦ K♣ 9♥ 2♠ | 1 |
| Two Pair | A♠ A♦ K♣ K♥ 2♠ | 2 |
| Three of a Kind | A♠ A♦ A♣ K♥ 2♠ | 3 |
| Straight | 5♠ 6♦ 7♣ 8♥ 9♠ | 4 |
| Flush | A♠ J♠ 8♠ 5♠ 2♠ | 5 |
| Full House | A♠ A♦ A♣ K♥ K♠ | 6 |
| Four of a Kind | A♠ A♦ A♣ A♥ K♠ | 7 |
| Straight Flush | T♠ J♠ Q♠ K♠ A♠ | 8 |

Special: A-2-3-4-5 is the lowest straight ("wheel"). The Ace plays low.

### Hand Notation

| Notation | Meaning | Combos |
|---|---|---|
| `AA` | Pocket aces (any two aces) | 6 |
| `AKs` | Ace-King suited (same suit) | 4 |
| `AKo` | Ace-King offsuit (different suits) | 12 |
| `AK` | All ace-king combos (suited + offsuit) | 16 |
| `QQ-TT` | Range of pairs: QQ, JJ, TT | 18 |
| `A5s:0.6` | Ace-five suited at 60% frequency | 4 × 0.6 |

### Equity

| Term | Definition |
|---|---|
| **Equity** | Your expected share of the pot if the hand were checked down to showdown. Expressed as a percentage (e.g., AA has ~82% equity vs KK preflop). |
| **EV (Expected Value)** | The average amount you expect to win or lose from a decision, accounting for all possible outcomes. |
| **Pot odds** | The ratio of the current pot to the cost of calling. If pot = 100 and call = 50, pot odds are 3:1 (33% equity needed to call). |

### Tournament-Specific

| Term | Definition |
|---|---|
| **ICM** | Independent Chip Model. Converts chip stacks into equity in the prize pool. Chips have diminishing marginal value: doubling your chips does NOT double your equity. |
| **Bubble** | The point where one more elimination means everyone remaining wins money. ICM pressure is highest here. |
| **Push/fold** | When stack is short (< ~15 BB), strategy simplifies to either all-in or fold. |
| **Chip EV** | Evaluating decisions purely by expected chip gain, ignoring prize structure. |
| **ICM EV** | Evaluating decisions by expected change in prize equity (accounting for ICM). |

---

## Computer Science / Algorithm Terms

### CFR (Counterfactual Regret Minimization)

| Term | Definition |
|---|---|
| **CFR** | An algorithm for computing Nash equilibrium strategies in extensive-form games. Iteratively minimizes "regret" for not having taken each action. |
| **CFR+** | A variant that clips cumulative regrets to ≥ 0 after each iteration. Converges faster in practice. Used by Libratus (the AI that beat top poker pros in 2017). |
| **MCCFR** | Monte Carlo CFR. Instead of traversing the full game tree, samples paths. Allows solving larger games. |
| **Nash equilibrium** | A strategy profile where no player can improve their EV by unilaterally changing their strategy. The "GTO" solution. |
| **Exploitability** | How much EV an opponent could gain by playing a perfect counter-strategy. Zero exploitability = Nash equilibrium. |

### Game Theory

| Term | Definition |
|---|---|
| **Extensive-form game** | A game represented as a tree of decision nodes, chance nodes, and terminal nodes. Poker is an extensive-form game. |
| **Information set** | A set of game states that are indistinguishable to a player. In poker, you can't see your opponent's cards, so states with different opponent hands but same board and action history are in the same information set. |
| **Strategy (policy)** | A mapping from every information set to a probability distribution over actions. A complete strategy specifies what to do in every possible situation. |
| **Regret** | For a specific action at a specific info set: how much better (or worse) that action performed compared to the strategy's weighted average. High regret → strategy should shift toward that action. |
| **Regret matching** | Derive a strategy from cumulative regrets: positive regret → proportional probability, zero/negative regret → zero probability. |
| **Reach probability** | The probability of reaching a particular game state, given both players' strategies and chance events. |
| **Counterfactual value** | The expected value at a node, weighted by the opponent's probability of reaching that node (but NOT the player's own probability). |

### Implementation

| Term | Definition |
|---|---|
| **Card abstraction** | Grouping similar hands into "buckets" to reduce the number of information sets. E.g., all "medium pairs" might be one bucket. Trades accuracy for speed. |
| **Betting abstraction** | Limiting the set of allowed bet sizes to reduce the game tree. E.g., only allowing [1/4, 1/2, 3/4, 1x] pot instead of any amount. |
| **Dead card** | A card that's known to be unavailable (in someone's hand or on the board). Must be excluded from enumeration/sampling. |
| **Runout** | The remaining community cards to be dealt. On the flop, the "runout" is the turn and river. |
| **Combo** | A specific pair of hole cards. "AKs" is a hand class; "A♠K♠" is one combo of that class. |
