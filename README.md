# GTO Poker Solver

A personal GTO (Game Theory Optimal) poker solver supporting 6-max cash games and tournaments (Poker Now).

## Features

- **CFR+ Algorithm**: Counterfactual Regret Minimization Plus for computing Nash equilibrium strategies
- **6-Max Support**: Full 6-player preflop range solving
- **Cash Game Mode**: Chip EV-based strategy computation
- **Tournament Mode**: ICM-adjusted strategies for tournament play
- **Postflop Solver**: Heads-up postflop spot solving with card abstraction
- **Web UI**: Interactive Streamlit-based interface

## Quick Start

```bash
pip install -r requirements.txt
streamlit run solver/ui/app.py
```

## Architecture

```
solver/
├── core/           # Poker primitives: cards, hand evaluation, equity
├── engine/         # CFR+ solver, game tree, abstraction
├── modes/          # Cash game (chip EV) and tournament (ICM)
└── ui/             # Streamlit web interface
```

## Algorithm

Uses CFR+ (Counterfactual Regret Minimization Plus) with:
- **Card abstraction**: Hand strength histogram clustering via k-means
- **Betting abstraction**: Configurable bet sizing (pot fractions)
- **ICM integration**: Independent Chip Model for tournament equity conversion
