"""Tournament mode: ICM-adjusted strategy computation.

In tournaments, chips don't have linear value. The Independent Chip Model (ICM)
converts chip stacks to equity in the prize pool. This fundamentally changes
optimal strategy — especially near the bubble and at final tables.

Designed for use with Poker Now tournament format.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from itertools import permutations
from typing import Dict, List, Optional, Tuple

import numpy as np

from solver.engine.cfr import CFRSolver, PreflopSolver
from solver.engine.game_tree import GameConfig, GameState, Street
from solver.core.card import parse_cards


class ICMCalculator:
    """Independent Chip Model calculator.

    Converts chip stacks to equity in the prize pool using the
    Malmuth-Harville model: probability of finishing in each position
    is proportional to chip stack share, computed recursively.
    """

    def __init__(self, payouts: List[float]):
        """
        Args:
            payouts: Prize pool distribution, e.g. [50, 30, 20] for top 3.
                     Values should be percentages or dollar amounts.
        """
        self.payouts = payouts
        self._cache: Dict[tuple, float] = {}

    def calculate_equity(self, stacks: List[float]) -> List[float]:
        """Calculate ICM equity for each player.

        Returns list of equity values (same units as payouts).
        """
        total_chips = sum(stacks)
        if total_chips == 0:
            return [0.0] * len(stacks)

        equities = []
        for player in range(len(stacks)):
            eq = self._player_equity(player, stacks, total_chips)
            equities.append(eq)

        return equities

    def _player_equity(
        self,
        player: int,
        stacks: List[float],
        total_chips: float,
    ) -> float:
        """Calculate a single player's ICM equity recursively."""
        equity = 0.0
        n_payouts = min(len(self.payouts), len([s for s in stacks if s > 0]))

        for place in range(n_payouts):
            prob = self._prob_finish(player, place, stacks, total_chips)
            equity += prob * self.payouts[place]

        return equity

    def _prob_finish(
        self,
        player: int,
        place: int,
        stacks: List[float],
        total_chips: float,
    ) -> float:
        """Probability that player finishes in a given place (0-indexed)."""
        if stacks[player] <= 0:
            return 0.0

        if place == 0:
            return stacks[player] / total_chips

        # For places > 0, sum over all possible first-place finishers
        prob = 0.0
        for first in range(len(stacks)):
            if first == player or stacks[first] <= 0:
                continue

            p_first = stacks[first] / total_chips

            remaining_stacks = stacks.copy()
            remaining_stacks[first] = 0
            remaining_total = total_chips - stacks[first]

            if remaining_total > 0:
                sub_prob = self._prob_finish(
                    player, place - 1, remaining_stacks, remaining_total,
                )
                prob += p_first * sub_prob

        return prob

    def icm_pressure(self, stacks: List[float], player: int) -> float:
        """Calculate ICM pressure factor for a player.

        Returns a value 0-1 where higher means more pressure (should play tighter).
        Bubble situations create the most pressure.
        """
        equities = self.calculate_equity(stacks)
        current_eq = equities[player]
        chip_share = stacks[player] / sum(stacks)

        # ICM pressure = how much equity you'd lose relative to chip share
        if chip_share > 0:
            return max(0, 1 - current_eq / (chip_share * sum(self.payouts)))
        return 0.0


@dataclass
class TournamentConfig:
    """Configuration for a tournament game."""
    num_players: int = 6
    starting_stack: float = 10000
    current_stacks: List[float] = field(default_factory=list)
    blinds: Tuple[float, float] = (50, 100)
    ante: float = 0.0
    payouts: List[float] = field(default_factory=lambda: [50, 30, 20])
    # Bet sizing (tighter in tournaments)
    bet_sizes_flop: List[float] = field(default_factory=lambda: [0.33, 0.5, 0.75])
    bet_sizes_turn: List[float] = field(default_factory=lambda: [0.5, 0.67, 1.0])
    bet_sizes_river: List[float] = field(default_factory=lambda: [0.5, 0.75, 1.0])

    def to_game_config(self) -> GameConfig:
        bb = self.blinds[1]
        return GameConfig(
            num_players=self.num_players,
            stack_size=self.current_stacks[0] / bb if self.current_stacks else self.starting_stack / bb,
            small_blind=0.5,
            big_blind=1.0,
            ante=self.ante / bb,
            bet_sizes_flop=self.bet_sizes_flop,
            bet_sizes_turn=self.bet_sizes_turn,
            bet_sizes_river=self.bet_sizes_river,
        )


class TournamentSolver:
    """Solver for tournament spots with ICM adjustments."""

    def __init__(self, config: TournamentConfig):
        self.config = config
        self.icm = ICMCalculator(config.payouts)
        self.game_config = config.to_game_config()
        self._preflop_solver: Optional[PreflopSolver] = None

    def get_icm_equities(self) -> List[float]:
        """Get current ICM equities for all players."""
        return self.icm.calculate_equity(self.config.current_stacks)

    def get_icm_adjusted_ranges(
        self,
        hero_position: int,
        num_iterations: int = 3000,
        show_progress: bool = True,
    ) -> Dict:
        """Solve preflop ranges adjusted for ICM pressure.

        ICM adjustment tightens ranges when the player has ICM pressure
        (e.g., near the bubble with a medium stack).
        """
        stacks = self.config.current_stacks
        bb = self.config.blinds[1]

        # Compute ICM pressure for hero
        pressure = self.icm.icm_pressure(stacks, hero_position)

        # Effective stack in BB
        eff_stack_bb = stacks[hero_position] / bb

        self._preflop_solver = PreflopSolver(self.game_config)
        self._preflop_solver.solve_preflop(num_iterations, show_progress)

        # Apply ICM adjustment: tighten ranges based on pressure
        positions = self._preflop_solver.positions
        adjusted_strategies = {}

        for pos in positions:
            adjusted_strategies[pos] = {}
            for hand, strat in self._preflop_solver.strategies[pos].items():
                adj = strat.copy()
                # Reduce raise frequency under ICM pressure
                adj['raise'] = strat['raise'] * (1 - pressure * 0.5)
                adj['call'] = strat['call'] * (1 - pressure * 0.3)
                adj['fold'] = 1.0 - adj['raise'] - adj['call']
                adj['fold'] = max(0, adj['fold'])

                # Normalize
                total = adj['fold'] + adj['call'] + adj['raise']
                if total > 0:
                    adj = {a: v / total for a, v in adj.items()}
                adjusted_strategies[pos][hand] = adj

        # Push/fold ranges for short stacks (< 15BB)
        push_fold = None
        if eff_stack_bb < 15:
            push_fold = self._compute_push_fold(hero_position)

        return {
            'strategies': adjusted_strategies,
            'icm_equities': self.get_icm_equities(),
            'icm_pressure': pressure,
            'effective_stack_bb': eff_stack_bb,
            'push_fold': push_fold,
        }

    def _compute_push_fold(self, hero_position: int) -> Dict[str, str]:
        """Compute push/fold chart for short-stacked tournament play."""
        from solver.engine.abstraction import canonical_hole_cards, canonical_to_str
        from solver.core.constants import RANK_TO_INT

        stacks = self.config.current_stacks
        bb = self.config.blinds[1]
        eff_bb = stacks[hero_position] / bb
        pressure = self.icm.icm_pressure(stacks, hero_position)

        hands = canonical_hole_cards()
        push_range = {}

        for r1, r2, suited in hands:
            hand_str = canonical_to_str(r1, r2, suited)
            r1_int = RANK_TO_INT[r1]
            r2_int = RANK_TO_INT[r2]

            # Simple push/fold heuristic based on hand strength and stack depth
            high = max(r1_int, r2_int)
            low = min(r1_int, r2_int)
            pair = r1_int == r2_int

            score = 0.0
            if pair:
                score = 5.0 + high * 0.5
            else:
                score = (high + low * 0.7) * 0.3
                if suited:
                    score += 1.0
                gap = high - low
                if gap <= 2:
                    score += 0.5

            # Adjust threshold by stack depth and ICM pressure
            threshold = 4.0 + (15 - eff_bb) * 0.3 + pressure * 3.0

            if score >= threshold:
                push_range[hand_str] = 'PUSH'
            else:
                push_range[hand_str] = 'FOLD'

        return push_range

    def analyze_spot(
        self,
        hero_cards: str,
        villain_cards: str,
        board: str,
        pot_chips: float,
        hero_position: int,
        stacks: Optional[List[float]] = None,
        num_iterations: int = 1000,
        show_progress: bool = True,
    ) -> Dict:
        """Analyze a specific postflop spot with ICM considerations."""
        if stacks:
            self.config.current_stacks = stacks

        bb = self.config.blinds[1]
        pot_bb = pot_chips / bb

        hero = parse_cards(hero_cards)
        villain = parse_cards(villain_cards)
        board_cards = parse_cards(board)

        hero_ints = (int(hero[0]), int(hero[1]))
        villain_ints = (int(villain[0]), int(villain[1]))
        board_ints = [int(c) for c in board_cards]

        stacks_bb = [s / bb for s in self.config.current_stacks[:2]]

        num_board = len(board_ints)
        if num_board >= 5:
            street = Street.RIVER
        elif num_board >= 4:
            street = Street.TURN
        elif num_board >= 3:
            street = Street.FLOP
        else:
            street = Street.PREFLOP

        solver = CFRSolver(self.game_config, ev_mode='icm')
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
        strategies = solver.solve(initial_state, hands, num_iterations, show_progress)

        icm_equities = self.get_icm_equities()
        pressure = self.icm.icm_pressure(self.config.current_stacks, hero_position)

        return {
            'strategies': strategies,
            'icm_equities': icm_equities,
            'icm_pressure': pressure,
            'chip_ev_pot': pot_chips,
        }
