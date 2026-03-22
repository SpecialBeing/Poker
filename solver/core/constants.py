"""Poker constants for 6-max games."""

RANKS = '23456789TJQKA'
SUITS = 'cdhs'

RANK_TO_INT = {r: i for i, r in enumerate(RANKS)}
SUIT_TO_INT = {s: i for i, s in enumerate(SUITS)}
INT_TO_RANK = {i: r for r, i in RANK_TO_INT.items()}
INT_TO_SUIT = {i: s for s, i in SUIT_TO_INT.items()}

NUM_RANKS = 13
NUM_SUITS = 4
NUM_CARDS = 52

HAND_RANKS = {
    'High Card': 0,
    'Pair': 1,
    'Two Pair': 2,
    'Three of a Kind': 3,
    'Straight': 4,
    'Flush': 5,
    'Full House': 6,
    'Four of a Kind': 7,
    'Straight Flush': 8,
}

POSITIONS_6MAX = ['UTG', 'HJ', 'CO', 'BTN', 'SB', 'BB']
