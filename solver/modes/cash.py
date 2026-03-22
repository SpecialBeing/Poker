"""Cash game mode: chip EV-based strategy computation.

In cash games, chips have a linear value — 1 chip = 1 unit of value.
Strategy is purely based on maximizing expected chip EV.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import numpy as np

from solver.engine.cfr import CFRSolver, PreflopSolver
from solver.engine.game_tree import GameConfig, GameState, Street, ActionWithSize
from solver.core.card import Card, parse_cards


@dataclass
class CashGameConfig:
    """Configuration for a cash game session."""
    num_players: int = 6
    stack_size_bb: float = 100.0
    small_blind: float = 0.5
    big_blind: float = 1.0
    ante_bb: float = 0.0
    # Bet sizing
    bet_sizes_flop: List[float] = field(default_factory=lambda: [0.33, 0.67, 1.0])
    bet_sizes_turn: List[float] = field(default_factory=lambda: [0.5, 0.75, 1.0])
    bet_sizes_river: List[float] = field(default_factory=lambda: [0.5, 0.75, 1.0])

    def to_game_config(self) -> GameConfig:
        return GameConfig(
            num_players=self.num_players,
            stack_size=self.stack_size_bb,
            small_blind=self.small_blind,
            big_blind=self.big_blind,
            ante=self.ante_bb,
            bet_sizes_flop=self.bet_sizes_flop,
            bet_sizes_turn=self.bet_sizes_turn,
            bet_sizes_river=self.bet_sizes_river,
        )


class CashGameSolver:
    """Solver for cash game spots."""

    def __init__(self, config: CashGameConfig):
        self.config = config
        self.game_config = config.to_game_config()
        self._preflop_solver: Optional[PreflopSolver] = None
        self._postflop_solver: Optional[CFRSolver] = None

    def solve_preflop(self, num_iterations: int = 5000, show_progress: bool = True) -> Dict:
        """Solve preflop opening ranges for all positions."""
        self._preflop_solver = PreflopSolver(self.game_config)
        self._preflop_solver.solve_preflop(num_iterations, show_progress)

        result = {}
        for pos in self._preflop_solver.positions:
            result[pos] = {
                'strategies': self._preflop_solver.strategies[pos],
                'open_pct': self._preflop_solver.get_opening_range_pct(pos),
                'raise_range': self._preflop_solver.get_range(pos, 'raise', 0.5),
                'call_range': self._preflop_solver.get_range(pos, 'call', 0.5),
            }
        return result

    def solve_postflop(
        self,
        hero_cards: str,
        villain_cards: str,
        board: str,
        pot_bb: float,
        stacks_bb: Optional[List[float]] = None,
        num_iterations: int = 1000,
        show_progress: bool = True,
    ) -> Dict:
        """Solve a specific postflop spot.

        Args:
            hero_cards: e.g. "AsKh"
            villain_cards: e.g. "QdJd"
            board: e.g. "Tc9h2s" (flop) or "Tc9h2s5d" (turn) etc.
            pot_bb: current pot size in BB
            stacks_bb: [hero_stack, villain_stack] in BB
        """
        hero = parse_cards(hero_cards)
        villain = parse_cards(villain_cards)
        board_cards = parse_cards(board)

        hero_ints = (int(hero[0]), int(hero[1]))
        villain_ints = (int(villain[0]), int(villain[1]))
        board_ints = [int(c) for c in board_cards]

        if stacks_bb is None:
            stacks_bb = [self.config.stack_size_bb, self.config.stack_size_bb]

        num_board = len(board_ints)
        if num_board >= 5:
            street = Street.RIVER
        elif num_board >= 4:
            street = Street.TURN
        elif num_board >= 3:
            street = Street.FLOP
        else:
            street = Street.PREFLOP

        self._postflop_solver = CFRSolver(
            self.game_config,
            num_buckets=10,
            ev_mode='chip',
        )

        from solver.engine.game_tree import GameTreeBuilder
        builder = GameTreeBuilder(self.game_config)
        initial_state = builder.create_initial_state(
            num_players=2,
            street=street,
            pot=pot_bb,
            stacks=stacks_bb,
            board=board_ints,
        )

        hands = [hero_ints, villain_ints]
        strategies = self._postflop_solver.solve(
            initial_state, hands, num_iterations, show_progress,
        )

        return {
            'strategies': strategies,
            'iterations': self._postflop_solver.iterations_done,
            'num_info_sets': len(self._postflop_solver.info_sets),
        }

    def get_preflop_range(self, position: str) -> Dict[str, Dict[str, float]]:
        """Get the solved preflop strategy for a position."""
        if self._preflop_solver is None:
            self.solve_preflop(show_progress=False)
        return self._preflop_solver.strategies.get(position, {})
