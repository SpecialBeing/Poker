"""Card and Deck representation using integer encoding for speed."""

from __future__ import annotations

import random
from typing import List, Optional, Sequence

import numpy as np

from solver.core.constants import (
    RANKS, SUITS, RANK_TO_INT, SUIT_TO_INT, INT_TO_RANK, INT_TO_SUIT,
    NUM_CARDS,
)


class Card:
    """A card encoded as a single integer 0-51.

    Encoding: card_int = rank * 4 + suit
    rank: 0=2, 1=3, ..., 12=A
    suit: 0=c, 1=d, 2=h, 3=s
    """

    __slots__ = ('_int',)

    def __init__(self, card_int: int):
        self._int = card_int

    @classmethod
    def from_str(cls, s: str) -> Card:
        """Parse 'As', 'Td', '2c' etc."""
        rank = RANK_TO_INT[s[0]]
        suit = SUIT_TO_INT[s[1]]
        return cls(rank * 4 + suit)

    @property
    def rank(self) -> int:
        return self._int // 4

    @property
    def suit(self) -> int:
        return self._int % 4

    @property
    def rank_char(self) -> str:
        return INT_TO_RANK[self.rank]

    @property
    def suit_char(self) -> str:
        return INT_TO_SUIT[self.suit]

    def __int__(self) -> int:
        return self._int

    def __eq__(self, other) -> bool:
        if isinstance(other, Card):
            return self._int == other._int
        return NotImplemented

    def __hash__(self) -> int:
        return self._int

    def __repr__(self) -> str:
        return f'{self.rank_char}{self.suit_char}'

    def __lt__(self, other: Card) -> bool:
        return self._int < other._int


class Deck:
    """A standard 52-card deck with fast dealing."""

    def __init__(self, excluded: Optional[Sequence[int]] = None):
        if excluded:
            ex_set = set(excluded)
            self._cards = np.array([i for i in range(NUM_CARDS) if i not in ex_set], dtype=np.int8)
        else:
            self._cards = np.arange(NUM_CARDS, dtype=np.int8)
        self._idx = 0

    def shuffle(self) -> None:
        np.random.shuffle(self._cards)
        self._idx = 0

    def deal(self, n: int = 1) -> np.ndarray:
        cards = self._cards[self._idx:self._idx + n]
        self._idx += n
        return cards

    def remaining(self) -> int:
        return len(self._cards) - self._idx


def parse_cards(s: str) -> List[Card]:
    """Parse a string like 'AsKh' or 'As Kh' into a list of Cards."""
    s = s.replace(' ', '')
    return [Card.from_str(s[i:i+2]) for i in range(0, len(s), 2)]


def cards_to_ints(cards: List[Card]) -> np.ndarray:
    return np.array([int(c) for c in cards], dtype=np.int8)
