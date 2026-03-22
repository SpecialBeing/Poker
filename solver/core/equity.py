"""Monte Carlo equity calculator for poker hands."""

from __future__ import annotations

from typing import List, Optional, Sequence

import numpy as np

from solver.core.hand_eval import HandEvaluator
from solver.core.constants import NUM_CARDS


class EquityCalculator:
    """Calculate hand equity via Monte Carlo simulation."""

    def __init__(self, num_simulations: int = 10000):
        self.num_simulations = num_simulations
        self._evaluator = HandEvaluator()

    def hand_vs_hand(
        self,
        hand_a: Sequence[int],
        hand_b: Sequence[int],
        board: Optional[Sequence[int]] = None,
    ) -> tuple[float, float, float]:
        """Calculate equity of hand_a vs hand_b on optional board.

        Returns (win_a, win_b, tie) as fractions summing to 1.0.
        """
        board = list(board) if board else []
        dead = set(hand_a) | set(hand_b) | set(board)
        remaining = [c for c in range(NUM_CARDS) if c not in dead]
        cards_needed = 5 - len(board)

        wins_a = wins_b = ties = 0

        for _ in range(self.num_simulations):
            runout = list(np.random.choice(remaining, size=cards_needed, replace=False))
            full_board = board + runout
            score_a = self._evaluator.evaluate(list(hand_a) + full_board)
            score_b = self._evaluator.evaluate(list(hand_b) + full_board)

            if score_a > score_b:
                wins_a += 1
            elif score_b > score_a:
                wins_b += 1
            else:
                ties += 1

        total = self.num_simulations
        return wins_a / total, wins_b / total, ties / total

    def hand_vs_range(
        self,
        hand: Sequence[int],
        opponent_range: List[tuple[int, int]],
        board: Optional[Sequence[int]] = None,
        weights: Optional[List[float]] = None,
    ) -> float:
        """Calculate equity of hand against a weighted range of opponent hands.

        Returns equity as a float 0.0-1.0.
        """
        board = list(board) if board else []
        hand_set = set(hand) | set(board)

        valid_combos = []
        valid_weights = []
        for i, (c1, c2) in enumerate(opponent_range):
            if c1 not in hand_set and c2 not in hand_set:
                valid_combos.append((c1, c2))
                valid_weights.append(weights[i] if weights else 1.0)

        if not valid_combos:
            return 0.5

        total_weight = sum(valid_weights)
        equity = 0.0

        sims_per_combo = max(1, self.num_simulations // len(valid_combos))

        for (c1, c2), w in zip(valid_combos, valid_weights):
            dead = hand_set | {c1, c2}
            remaining = [c for c in range(NUM_CARDS) if c not in dead]
            cards_needed = 5 - len(board)

            combo_wins = 0
            for _ in range(sims_per_combo):
                runout = list(np.random.choice(remaining, size=cards_needed, replace=False))
                full_board = board + runout
                score_h = self._evaluator.evaluate(list(hand) + full_board)
                score_o = self._evaluator.evaluate([c1, c2] + full_board)
                if score_h > score_o:
                    combo_wins += 1
                elif score_h == score_o:
                    combo_wins += 0.5

            combo_eq = combo_wins / sims_per_combo
            equity += combo_eq * (w / total_weight)

        return equity

    def range_vs_range(
        self,
        range_a: List[tuple[int, int]],
        range_b: List[tuple[int, int]],
        board: Optional[Sequence[int]] = None,
        num_matchups: int = 5000,
    ) -> float:
        """Estimate equity of range_a vs range_b via sampling matchups."""
        board = list(board) if board else []
        board_set = set(board)

        valid_a = [(c1, c2) for c1, c2 in range_a if c1 not in board_set and c2 not in board_set]
        valid_b = [(c1, c2) for c1, c2 in range_b if c1 not in board_set and c2 not in board_set]

        wins = 0.0
        count = 0

        for _ in range(num_matchups):
            ha = valid_a[np.random.randint(len(valid_a))]
            hb = valid_b[np.random.randint(len(valid_b))]
            if set(ha) & set(hb):
                continue

            dead = board_set | set(ha) | set(hb)
            remaining = [c for c in range(NUM_CARDS) if c not in dead]
            cards_needed = 5 - len(board)

            runout = list(np.random.choice(remaining, size=cards_needed, replace=False))
            full_board = board + runout

            sa = self._evaluator.evaluate(list(ha) + full_board)
            sb = self._evaluator.evaluate(list(hb) + full_board)

            if sa > sb:
                wins += 1.0
            elif sa == sb:
                wins += 0.5
            count += 1

        return wins / count if count > 0 else 0.5
