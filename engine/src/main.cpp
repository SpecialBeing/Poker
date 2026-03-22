/// @file main.cpp
/// @brief Interactive demo of the poker engine.
///
/// This is NOT the test suite. Tests live in engine/tests/ and are run via ctest.
/// This file demonstrates how to use the engine API.

#include "poker/card.h"
#include "poker/hand_evaluator.h"
#include "poker/range.h"
#include "poker/equity_calculator.h"
#include "poker/game_state.h"

#include <iostream>
#include <iomanip>
#include <chrono>

using namespace poker;

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║       Poker Engine — Demo                ║\n"
              << "╚══════════════════════════════════════════╝\n\n";

    // ── 1. Equity Calculation ───────────────────────────────
    std::cout << "── Equity Calculator ──\n\n";

    EquityCalculator calc;

    // AA vs KK preflop
    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ah"),
        Card::fromString("Ks"), Card::fromString("Kh"),
        {}
    );
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  AA vs KK preflop (exhaustive):\n"
              << "    Equity: " << result.equity * 100 << "%\n"
              << "    Win/Tie/Lose: " << result.win_pct * 100 << "% / "
              << result.tie_pct * 100 << "% / " << result.lose_pct * 100 << "%\n"
              << "    Boards evaluated: " << result.matchups_evaluated << "\n"
              << "    Time: " << std::setprecision(0) << ms << " ms\n\n";

    // AKs vs a range on a flop
    Range villain;
    villain.parse("AA, KK, QQ, JJ, TT, AKs, AQs, AKo");
    auto board = parseCards("Ad7h2c");
    auto r2 = calc.handVsRange(
        Card::fromString("As"), Card::fromString("Ks"),
        villain, board, 50000
    );
    std::cout << std::setprecision(2);
    std::cout << "  AKs vs {AA,KK,QQ,JJ,TT,AKs,AQs,AKo} on Ad7h2c:\n"
              << "    Equity: " << r2.equity * 100 << "%\n\n";

    // ── 2. All-In Equity by Street ──────────────────────────
    std::cout << "── All-In Equity Tracker ──\n\n";

    auto full_board = parseCards("As8h5dTc2s");
    auto equities = calc.allInEquityByStreet(
        Card::fromString("Qs"), Card::fromString("Ts"),
        Card::fromString("Ah"), Card::fromString("Kd"),
        full_board
    );
    std::cout << "  QTs vs AKo on board As 8h 5d Tc 2s:\n";
    if (equities.flop >= 0)
        std::cout << "    Flop  (As 8h 5d):         " << equities.flop * 100 << "%\n";
    if (equities.turn >= 0)
        std::cout << "    Turn  (As 8h 5d Tc):      " << equities.turn * 100 << "%\n";
    if (equities.river >= 0)
        std::cout << "    River (As 8h 5d Tc 2s):   " << equities.river * 100 << "%\n";
    std::cout << "\n";

    // ── 3. Game State Demo ──────────────────────────────────
    std::cout << "── Game State (50bb HU) ──\n\n";

    auto bet_config = BetSizingConfig::standard();
    GameState state(2, {50.0, 50.0}, 0.5, 1.0, 0.0, bet_config);
    std::cout << state.toString();

    auto actions = state.legalActions();
    std::cout << "  SB legal actions:\n";
    for (auto& a : actions)
        std::cout << "    " << a.toString() << "\n";

    // SB raises smallest size
    Action raise = actions[2];  // first BET option
    auto s2 = state.applyAction(raise);
    std::cout << "\n  After SB " << raise.toString() << ":\n" << s2.toString();

    // BB calls
    auto bb_acts = s2.legalActions();
    Action call{ActionType::FOLD, 0};
    for (auto& a : bb_acts) {
        if (a.type == ActionType::CALL) { call = a; break; }
    }
    auto s3 = s2.applyAction(call);
    auto s4 = s3.advanceStreet();
    s4.setBoard(parseCards("As8h5d"));
    std::cout << "\n  Flop dealt:\n" << s4.toString();

    auto flop_acts = s4.legalActions();
    std::cout << "  Flop actions:\n";
    for (auto& a : flop_acts)
        std::cout << "    " << a.toString() << "\n";

    std::cout << "\nDone.\n";
    return 0;
}
