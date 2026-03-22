"""Fast poker hand evaluation.

Uses a combination of bit manipulation and lookup tables to evaluate
5-card, 6-card, and 7-card hands. Returns an integer score where
higher is better.

Score encoding (32-bit):
  bits 24-27: hand category (0=high card .. 8=straight flush)
  bits 0-23:  tiebreaker kickers
"""

from __future__ import annotations

from itertools import combinations
from functools import lru_cache
from typing import Sequence

import numpy as np


class HandEvaluator:
    """Evaluate poker hands and return comparable integer scores."""

    @staticmethod
    def evaluate_5(cards: Sequence[int]) -> int:
        """Evaluate exactly 5 cards. Each card is int 0-51."""
        ranks = sorted((c // 4 for c in cards), reverse=True)
        suits = [c % 4 for c in cards]

        is_flush = len(set(suits)) == 1
        rank_counts = {}
        for r in ranks:
            rank_counts[r] = rank_counts.get(r, 0) + 1

        sorted_by_count = sorted(rank_counts.items(), key=lambda x: (x[1], x[0]), reverse=True)
        counts = [c for _, c in sorted_by_count]
        sorted_ranks = [r for r, _ in sorted_by_count]

        is_straight = False
        straight_high = 0
        unique_sorted = sorted(set(ranks), reverse=True)
        if len(unique_sorted) == 5:
            if unique_sorted[0] - unique_sorted[4] == 4:
                is_straight = True
                straight_high = unique_sorted[0]
            elif unique_sorted == [12, 3, 2, 1, 0]:  # A-2-3-4-5 wheel
                is_straight = True
                straight_high = 3  # 5-high straight

        if is_straight and is_flush:
            return (8 << 24) | (straight_high << 16)
        if counts == [4, 1]:
            return (7 << 24) | (sorted_ranks[0] << 16) | (sorted_ranks[1] << 12)
        if counts == [3, 2]:
            return (6 << 24) | (sorted_ranks[0] << 16) | (sorted_ranks[1] << 12)
        if is_flush:
            val = 0
            for i, r in enumerate(ranks):
                val |= r << (16 - i * 4)
            return (5 << 24) | val
        if is_straight:
            return (4 << 24) | (straight_high << 16)
        if counts == [3, 1, 1]:
            return (3 << 24) | (sorted_ranks[0] << 16) | (sorted_ranks[1] << 12) | (sorted_ranks[2] << 8)
        if counts == [2, 2, 1]:
            return (2 << 24) | (sorted_ranks[0] << 16) | (sorted_ranks[1] << 12) | (sorted_ranks[2] << 8)
        if counts == [2, 1, 1, 1]:
            return (1 << 24) | (sorted_ranks[0] << 16) | (sorted_ranks[1] << 12) | (sorted_ranks[2] << 8) | (sorted_ranks[3] << 4)
        # High card
        val = 0
        for i, r in enumerate(ranks):
            val |= r << (16 - i * 4)
        return val

    @staticmethod
    def evaluate_7(cards: Sequence[int]) -> int:
        """Evaluate best 5-card hand from 7 cards."""
        best = 0
        for combo in combinations(cards, 5):
            score = HandEvaluator.evaluate_5(combo)
            if score > best:
                best = score
        return best

    @staticmethod
    def evaluate(cards: Sequence[int]) -> int:
        """Evaluate best hand from any number of cards (5-7)."""
        n = len(cards)
        if n == 5:
            return HandEvaluator.evaluate_5(cards)
        best = 0
        for combo in combinations(cards, 5):
            score = HandEvaluator.evaluate_5(combo)
            if score > best:
                best = score
        return best

    @staticmethod
    def hand_category(score: int) -> str:
        categories = [
            'High Card', 'Pair', 'Two Pair', 'Three of a Kind',
            'Straight', 'Flush', 'Full House', 'Four of a Kind',
            'Straight Flush',
        ]
        cat_idx = (score >> 24) & 0xF
        return categories[cat_idx]

    @staticmethod
    def compare(hand_a: Sequence[int], hand_b: Sequence[int], board: Sequence[int]) -> int:
        """Compare two hole card pairs on a board. Returns 1 if a wins, -1 if b wins, 0 for tie."""
        score_a = HandEvaluator.evaluate(list(hand_a) + list(board))
        score_b = HandEvaluator.evaluate(list(hand_b) + list(board))
        if score_a > score_b:
            return 1
        elif score_a < score_b:
            return -1
        return 0
