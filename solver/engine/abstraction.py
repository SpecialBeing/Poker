"""Card and betting abstraction for tractable solving.

Card abstraction groups similar hands into "buckets" so the game tree
remains manageable. We use hand strength + potential as features,
then cluster with simple binning or k-means.
"""

from __future__ import annotations

from typing import Dict, List, Optional, Sequence, Tuple

import numpy as np

from solver.core.hand_eval import HandEvaluator
from solver.core.constants import NUM_CARDS, RANKS, SUITS, RANK_TO_INT


def canonical_hole_cards() -> List[Tuple[str, str, bool]]:
    """Generate all 169 canonical preflop hand types.

    Returns list of (rank1, rank2, suited) tuples.
    """
    hands = []
    for i in range(12, -1, -1):
        for j in range(i, -1, -1):
            r1 = RANKS[i]
            r2 = RANKS[j]
            if i == j:
                hands.append((r1, r2, False))  # pocket pair
            else:
                hands.append((r1, r2, True))    # suited
                hands.append((r1, r2, False))   # offsuit
    return hands


def hand_to_canonical(c1: int, c2: int) -> Tuple[str, str, bool]:
    """Convert two card ints to canonical (rank1, rank2, suited) form."""
    r1, r2 = c1 // 4, c2 // 4
    s1, s2 = c1 % 4, c2 % 4
    suited = s1 == s2

    if r1 < r2:
        r1, r2 = r2, r1

    from solver.core.constants import INT_TO_RANK
    return (INT_TO_RANK[r1], INT_TO_RANK[r2], suited)


def canonical_to_str(rank1: str, rank2: str, suited: bool) -> str:
    """Convert canonical form to string like 'AKs', 'QTo', '99'."""
    if rank1 == rank2:
        return f'{rank1}{rank2}'
    return f'{rank1}{rank2}{"s" if suited else "o"}'


class CardAbstraction:
    """Groups hands into equity buckets for tractable solving."""

    def __init__(self, num_buckets: int = 10):
        self.num_buckets = num_buckets
        self._evaluator = HandEvaluator()
        self._cache: Dict[Tuple, int] = {}

    def compute_hand_strength(
        self,
        hole_cards: Sequence[int],
        board: Sequence[int],
        num_samples: int = 300,
    ) -> float:
        """Estimate hand strength as win-rate against random opponent hand."""
        dead = set(hole_cards) | set(board)
        remaining = [c for c in range(NUM_CARDS) if c not in dead]

        wins = 0.0
        total = 0
        cards_to_deal = 5 - len(board)

        for _ in range(num_samples):
            sample = np.random.choice(remaining, size=cards_to_deal + 2, replace=False)
            opp = sample[:2]
            runout = list(board) + list(sample[2:2 + cards_to_deal])

            my_score = self._evaluator.evaluate(list(hole_cards) + runout)
            opp_score = self._evaluator.evaluate(list(opp) + runout)

            if my_score > opp_score:
                wins += 1.0
            elif my_score == opp_score:
                wins += 0.5
            total += 1

        return wins / total if total > 0 else 0.5

    def get_bucket(
        self,
        hole_cards: Sequence[int],
        board: Sequence[int],
        num_samples: int = 300,
    ) -> int:
        """Get the abstraction bucket for a hand+board combination."""
        key = (tuple(sorted(hole_cards)), tuple(sorted(board)))
        if key in self._cache:
            return self._cache[key]

        strength = self.compute_hand_strength(hole_cards, board, num_samples)
        bucket = min(int(strength * self.num_buckets), self.num_buckets - 1)
        self._cache[key] = bucket
        return bucket

    def preflop_bucket(self, c1: int, c2: int) -> int:
        """Assign a preflop bucket based on hand ranking tables."""
        rank1, rank2, suited = hand_to_canonical(c1, c2)
        r1 = RANK_TO_INT[rank1]
        r2 = RANK_TO_INT[rank2]

        # Simple tier system based on conventional hand rankings
        if r1 == r2:  # pocket pairs
            if r1 >= 10:    # TT+
                return 9
            elif r1 >= 6:   # 77-99
                return 7
            elif r1 >= 3:   # 44-66
                return 5
            else:           # 22-33
                return 4

        high = max(r1, r2)
        low = min(r1, r2)
        gap = high - low
        bonus = 1 if suited else 0

        if high == 12:  # Ax
            if low >= 10:   # ATs+, ATo+
                return 8 + bonus
            elif low >= 7:  # A8-A9
                return 6 + bonus
            else:
                return 3 + bonus
        elif high == 11:  # Kx
            if low >= 10:
                return 7 + bonus
            elif low >= 8:
                return 5 + bonus
            else:
                return 2 + bonus
        elif high == 10:  # Qx
            if low >= 9:
                return 6 + bonus
            else:
                return 2 + bonus

        if suited and gap <= 2 and low >= 4:  # suited connectors
            return 5
        if suited and gap <= 1:
            return 4

        return max(0, 1 + bonus)
