"""Tests for core poker primitives."""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from solver.core.card import Card, Deck, parse_cards
from solver.core.hand_eval import HandEvaluator
from solver.core.equity import EquityCalculator


def test_card_basics():
    ace_spades = Card.from_str('As')
    assert ace_spades.rank == 12  # Ace
    assert ace_spades.suit == 3   # spades
    assert str(ace_spades) == 'As'

    two_clubs = Card.from_str('2c')
    assert two_clubs.rank == 0
    assert two_clubs.suit == 0
    print('✓ Card basics passed')


def test_parse_cards():
    cards = parse_cards('AsKh')
    assert len(cards) == 2
    assert str(cards[0]) == 'As'
    assert str(cards[1]) == 'Kh'

    cards = parse_cards('TcJd9h')
    assert len(cards) == 3
    print('✓ Parse cards passed')


def test_hand_evaluator():
    ev = HandEvaluator()

    # Royal flush
    royal = [Card.from_str(s) for s in ['As', 'Ks', 'Qs', 'Js', 'Ts']]
    royal_ints = [int(c) for c in royal]
    score_royal = ev.evaluate_5(royal_ints)
    assert ev.hand_category(score_royal) == 'Straight Flush'

    # Full house
    fh = [Card.from_str(s) for s in ['As', 'Ah', 'Ad', 'Ks', 'Kh']]
    fh_ints = [int(c) for c in fh]
    score_fh = ev.evaluate_5(fh_ints)
    assert ev.hand_category(score_fh) == 'Full House'

    assert score_royal > score_fh

    # Pair vs high card
    pair = [Card.from_str(s) for s in ['As', 'Ah', '5d', '3c', '2h']]
    high = [Card.from_str(s) for s in ['Ks', 'Qh', 'Jd', '9c', '2h']]
    score_pair = ev.evaluate_5([int(c) for c in pair])
    score_high = ev.evaluate_5([int(c) for c in high])
    assert score_pair > score_high

    print('✓ Hand evaluator passed')


def test_7card_eval():
    ev = HandEvaluator()
    cards = [Card.from_str(s) for s in ['As', 'Ks', 'Qs', 'Js', 'Ts', '2h', '3d']]
    score = ev.evaluate_7([int(c) for c in cards])
    assert ev.hand_category(score) == 'Straight Flush'
    print('✓ 7-card evaluation passed')


def test_equity_calculator():
    ec = EquityCalculator(num_simulations=5000)

    # AA vs KK preflop — AA should be ~80%
    aa = (Card.from_str('As')._int, Card.from_str('Ah')._int)
    kk = (Card.from_str('Ks')._int, Card.from_str('Kh')._int)

    win_a, win_b, tie = ec.hand_vs_hand(aa, kk)
    print(f'  AA vs KK: {win_a:.1%} / {win_b:.1%} / {tie:.1%}')
    assert win_a > 0.70, f'AA should beat KK most of the time, got {win_a:.1%}'
    print('✓ Equity calculator passed')


def test_icm():
    from solver.modes.tournament import ICMCalculator

    # 3 players, equal stacks
    icm = ICMCalculator([50, 30, 20])
    equities = icm.calculate_equity([1000, 1000, 1000])

    # Equal stacks should have equal equity
    assert abs(equities[0] - equities[1]) < 0.1
    assert abs(equities[1] - equities[2]) < 0.1
    assert abs(sum(equities) - 100) < 0.1
    print(f'  Equal stacks ICM: {[f"{e:.2f}" for e in equities]}')

    # Chip leader should have less than proportional equity
    equities_unequal = icm.calculate_equity([5000, 3000, 2000])
    chip_leader_share = 5000 / 10000
    icm_leader_share = equities_unequal[0] / sum(equities_unequal)
    assert icm_leader_share < chip_leader_share, 'ICM should compress chip leader equity'
    print(f'  Unequal stacks ICM: {[f"{e:.2f}" for e in equities_unequal]}')
    print(f'  Chip leader: {chip_leader_share:.1%} chips, {icm_leader_share:.1%} ICM equity')
    print('✓ ICM calculator passed')


if __name__ == '__main__':
    test_card_basics()
    test_parse_cards()
    test_hand_evaluator()
    test_7card_eval()
    test_equity_calculator()
    test_icm()
    print('\n✅ All tests passed!')
