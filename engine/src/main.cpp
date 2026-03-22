/// @file main.cpp
/// @brief Test / demo for the poker engine core.

#include "poker/card.h"
#include "poker/hand_evaluator.h"
#include "poker/range.h"
#include "poker/equity_calculator.h"
#include "poker/game_state.h"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>

using namespace poker;

// ── Helpers ─────────────────────────────────────────────────

void printSection(const std::string& title) {
    std::cout << "\n══════════════════════════════════════════════\n"
              << "  " << title
              << "\n══════════════════════════════════════════════\n";
}

// ── Test: Card Basics ───────────────────────────────────────

void testCards() {
    printSection("Card & Deck");

    Card as = Card::fromString("As");
    Card kh = Card::fromString("Kh");
    Card tc = Card::fromString("Tc");
    Card deuce_c = Card::fromString("2c");

    std::cout << "  As = id " << (int)as.id() << ", rank=" << as.rankInt()
              << ", suit=" << as.suitInt() << "\n";
    std::cout << "  Kh = id " << (int)kh.id() << "\n";
    std::cout << "  Tc = id " << (int)tc.id() << "\n";
    std::cout << "  2c = id " << (int)deuce_c.id() << "\n";

    assert(as.rank() == Rank::ACE);
    assert(as.suit() == Suit::SPADES);
    assert(deuce_c.rank() == Rank::TWO);
    assert(deuce_c.suit() == Suit::CLUBS);

    auto cards = parseCards("AsKhTc");
    assert(cards.size() == 3);
    assert(cards[0] == as);
    assert(cards[1] == kh);

    // Hole card index
    int idx = holeCardIndex(as, kh);
    std::cout << "  holeCardIndex(As, Kh) = " << idx << "\n";

    std::cout << "  [PASS] Card basics\n";
}

// ── Test: Hand Evaluator ────────────────────────────────────

void testHandEvaluator() {
    printSection("Hand Evaluator");

    HandEvaluator eval;

    // Royal flush: As Ks Qs Js Ts
    uint8_t royal[5];
    royal[0] = Card(Rank::ACE, Suit::SPADES).id();
    royal[1] = Card(Rank::KING, Suit::SPADES).id();
    royal[2] = Card(Rank::QUEEN, Suit::SPADES).id();
    royal[3] = Card(Rank::JACK, Suit::SPADES).id();
    royal[4] = Card(Rank::TEN, Suit::SPADES).id();
    HandRank r_royal = eval.evaluate5(royal);
    std::cout << "  Royal flush: " << r_royal.toString()
              << " (0x" << std::hex << r_royal.value << std::dec << ")\n";
    assert(r_royal.category() == HandCategory::STRAIGHT_FLUSH);

    // Full house: AAA KK
    uint8_t fh[5];
    fh[0] = Card(Rank::ACE, Suit::SPADES).id();
    fh[1] = Card(Rank::ACE, Suit::HEARTS).id();
    fh[2] = Card(Rank::ACE, Suit::DIAMONDS).id();
    fh[3] = Card(Rank::KING, Suit::SPADES).id();
    fh[4] = Card(Rank::KING, Suit::HEARTS).id();
    HandRank r_fh = eval.evaluate5(fh);
    std::cout << "  Full house (AAA-KK): " << r_fh.toString() << "\n";
    assert(r_fh.category() == HandCategory::FULL_HOUSE);
    assert(r_royal > r_fh);

    // Pair of aces vs king-high
    uint8_t pair_a[5] = {
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::ACE, Suit::HEARTS).id(),
        Card(Rank::FIVE, Suit::DIAMONDS).id(),
        Card(Rank::THREE, Suit::CLUBS).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
    };
    uint8_t high_k[5] = {
        Card(Rank::KING, Suit::SPADES).id(),
        Card(Rank::QUEEN, Suit::HEARTS).id(),
        Card(Rank::JACK, Suit::DIAMONDS).id(),
        Card(Rank::NINE, Suit::CLUBS).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
    };
    assert(eval.evaluate5(pair_a) > eval.evaluate5(high_k));
    std::cout << "  Pair of aces > king high: OK\n";

    // Wheel straight: A-2-3-4-5
    uint8_t wheel[5] = {
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
        Card(Rank::THREE, Suit::DIAMONDS).id(),
        Card(Rank::FOUR, Suit::CLUBS).id(),
        Card(Rank::FIVE, Suit::HEARTS).id(),
    };
    HandRank r_wheel = eval.evaluate5(wheel);
    std::cout << "  Wheel (A2345): " << r_wheel.toString() << "\n";
    assert(r_wheel.category() == HandCategory::STRAIGHT);

    // 7-card evaluation
    uint8_t seven[7] = {
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::KING, Suit::SPADES).id(),
        Card(Rank::QUEEN, Suit::SPADES).id(),
        Card(Rank::JACK, Suit::SPADES).id(),
        Card(Rank::TEN, Suit::SPADES).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
        Card(Rank::THREE, Suit::DIAMONDS).id(),
    };
    HandRank r7 = eval.evaluate7(seven);
    std::cout << "  7-card with royal: " << r7.toString() << "\n";
    assert(r7.category() == HandCategory::STRAIGHT_FLUSH);

    // Performance: evaluate 1M random 5-card hands
    std::cout << "\n  Benchmarking 5-card evaluator...\n";
    auto start = std::chrono::high_resolution_clock::now();
    volatile uint32_t sink = 0;
    for (int i = 0; i < 1000000; ++i) {
        // Vary the hand slightly to prevent optimization
        pair_a[4] = (i % 40);  // different kicker
        sink += eval.evaluate5(pair_a).value;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "  1M 5-card evals: " << std::fixed << std::setprecision(1) << ms << " ms"
              << " (" << std::setprecision(0) << (1e6 / ms * 1000) << " evals/sec)\n";

    std::cout << "  [PASS] Hand evaluator\n";
}

// ── Test: Equity Calculator ─────────────────────────────────

void testEquity() {
    printSection("Equity Calculator");

    EquityCalculator calc;

    // AA vs KK preflop — should be ~81%
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = calc.handVsHand(
            Card::fromString("As"), Card::fromString("Ah"),
            Card::fromString("Ks"), Card::fromString("Kh"),
            {}
        );
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "  AA vs KK preflop:\n"
                  << "    Equity: " << std::fixed << std::setprecision(2)
                  << (result.equity * 100) << "%\n"
                  << "    Win: " << (result.win_pct * 100) << "%"
                  << "  Tie: " << (result.tie_pct * 100) << "%"
                  << "  Lose: " << (result.lose_pct * 100) << "%\n"
                  << "    Matchups: " << result.matchups_evaluated << "\n"
                  << "    Time: " << std::setprecision(0) << ms << " ms\n";

        assert(result.equity > 0.75 && result.equity < 0.87);
    }

    // AKs vs QQ on a flop of A-7-2 rainbow
    {
        std::vector<Card> board = {
            Card::fromString("Ad"), Card::fromString("7h"), Card::fromString("2c")
        };
        auto result = calc.handVsHand(
            Card::fromString("As"), Card::fromString("Ks"),
            Card::fromString("Qd"), Card::fromString("Qh"),
            board
        );
        std::cout << "  AKs vs QQ on A72r flop:\n"
                  << "    Equity: " << std::fixed << std::setprecision(2)
                  << (result.equity * 100) << "%\n";
        assert(result.equity > 0.85); // AK has top pair, should dominate
    }

    // Hand vs range: AKs vs top 20% range on A72 flop
    {
        Range villain;
        villain.parse("AA, KK, QQ, JJ, TT, 99, 88, 77, AKs, AQs, AJs, ATs, AKo, AQo, KQs");
        auto board = parseCards("Ad7h2c");
        auto result = calc.handVsRange(
            Card::fromString("As"), Card::fromString("Ks"),
            villain, board, 50000
        );
        std::cout << "  AKs vs top-20% on A72r:\n"
                  << "    Equity: " << (result.equity * 100) << "%\n";
    }

    std::cout << "  [PASS] Equity calculator\n";
}

// ── Test: Range ─────────────────────────────────────────────

void testRange() {
    printSection("Range");

    Range r;
    r.parse("AA, KK, QQ, AKs, AKo");
    std::cout << "  Parsed 'AA, KK, QQ, AKs, AKo'\n";
    std::cout << "  Combos: " << r.numCombos() << "\n";
    // AA=6, KK=6, QQ=6, AKs=4, AKo=12 → 34
    assert(r.numCombos() == 34);

    // Exact hand
    Range exact;
    exact.setExact(Card::fromString("Kd"), Card::fromString("3c"));
    assert(exact.numCombos() == 1);
    assert(exact.getWeight(Card::fromString("Kd"), Card::fromString("3c")) == 1.0);

    // Dead card removal
    Range r2;
    r2.parse("AA");
    assert(r2.numCombos() == 6);
    r2.removeDead({Card::fromString("As")});
    assert(r2.numCombos() == 3); // only combos without As remain

    std::cout << "  [PASS] Range\n";
}

// ── Test: Game State ────────────────────────────────────────

void testGameState() {
    printSection("Game State");

    auto bet_config = BetSizingConfig::standard();

    // Heads-up, 50bb deep
    GameState state(2, {50.0, 50.0}, 0.5, 1.0, 0.0, bet_config);
    std::cout << state.toString();

    // SB raises to 2.236bb (first preflop bet size)
    auto actions = state.legalActions();
    std::cout << "  Legal actions for P0 (SB):\n";
    for (auto& a : actions) {
        std::cout << "    " << a.toString() << "\n";
    }

    // Apply a raise
    Action raise{ActionType::BET, 0};
    for (auto& a : actions) {
        if (a.type == ActionType::BET) { raise = a; break; }
    }
    auto state2 = state.applyAction(raise);
    std::cout << "\n  After SB raises " << raise.toString() << ":\n";
    std::cout << state2.toString();

    // BB calls
    auto bb_actions = state2.legalActions();
    std::cout << "  Legal actions for P1 (BB):\n";
    for (auto& a : bb_actions) {
        std::cout << "    " << a.toString() << "\n";
    }

    // BB calls → round should be over
    Action call{ActionType::CALL, 0};
    for (auto& a : bb_actions) {
        if (a.type == ActionType::CALL) { call = a; break; }
    }
    auto state3 = state2.applyAction(call);
    std::cout << "\n  After BB calls:\n";
    std::cout << state3.toString();
    std::cout << "  Round over: " << (state3.isRoundOver() ? "yes" : "no") << "\n";

    // Advance to flop
    auto state4 = state3.advanceStreet();
    state4.setBoard(parseCards("As8h5d"));
    std::cout << "\n  Flop dealt:\n";
    std::cout << state4.toString();

    // Check legal actions on flop
    auto flop_actions = state4.legalActions();
    std::cout << "  Legal flop actions:\n";
    for (auto& a : flop_actions) {
        std::cout << "    " << a.toString() << "\n";
    }

    std::cout << "  [PASS] Game state\n";
}

// ── Test: All-In Equity by Street ───────────────────────────

void testAllInEquityByStreet() {
    printSection("All-In Equity by Street");

    EquityCalculator calc;

    // Both all-in preflop: show flop/turn/river equity changes
    auto board = parseCards("As8h5dTc2s");

    auto equities = calc.allInEquityByStreet(
        Card::fromString("Qs"), Card::fromString("Ts"),  // QTs
        Card::fromString("Ah"), Card::fromString("Kd"),   // AKo
        board
    );

    std::cout << std::fixed << std::setprecision(2);
    if (equities.flop >= 0)
        std::cout << "  Flop (As 8h 5d):      QTs = " << (equities.flop * 100) << "%\n";
    if (equities.turn >= 0)
        std::cout << "  Turn (As 8h 5d Tc):   QTs = " << (equities.turn * 100) << "%\n";
    if (equities.river >= 0)
        std::cout << "  River (As 8h 5d Tc 2s): QTs = " << (equities.river * 100) << "%\n";

    std::cout << "  [PASS] All-in equity by street\n";
}

// ────────────────────────────────────────────────────────────

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║      Poker Engine — Test Suite           ║\n"
              << "╚══════════════════════════════════════════╝\n";

    testCards();
    testHandEvaluator();
    testRange();
    testEquity();
    testAllInEquityByStreet();
    testGameState();

    std::cout << "\n✅ All tests passed!\n";
    return 0;
}
