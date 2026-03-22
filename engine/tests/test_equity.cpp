#include <gtest/gtest.h>
#include "poker/equity_calculator.h"
#include <chrono>
#include <iostream>

using namespace poker;

class EquityTest : public ::testing::Test {
protected:
    EquityCalculator calc;
};

// ── Hand vs Hand: Known Preflop Matchups ────────────────────

TEST_F(EquityTest, AAvsKK_Preflop_Around82Percent) {
    auto result = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ah"),
        Card::fromString("Ks"), Card::fromString("Kh"),
        {}
    );
    EXPECT_NEAR(result.equity, 0.826, 0.02);  // AA ~82.6% vs KK
    EXPECT_GT(result.matchups_evaluated, 0u);
}

TEST_F(EquityTest, AAvsKK_WinPlusTiePlusLoss_SumsTo1) {
    auto result = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ah"),
        Card::fromString("Ks"), Card::fromString("Kh"),
        {}
    );
    EXPECT_NEAR(result.win_pct + result.tie_pct + result.lose_pct, 1.0, 0.001);
}

TEST_F(EquityTest, SameHand_50Percent) {
    // AcKc vs AdKd — same ranks, should be ~50% (ties + split equity)
    auto result = calc.handVsHand(
        Card::fromString("Ac"), Card::fromString("Kc"),
        Card::fromString("Ad"), Card::fromString("Kd"),
        {}
    );
    EXPECT_NEAR(result.equity, 0.5, 0.05);
}

// ── Hand vs Hand: Postflop ──────────────────────────────────

TEST_F(EquityTest, TopPair_DominatesUnderPair_OnFlop) {
    // AKs vs QQ on A-7-2 rainbow → AK has top pair, ~91%
    std::vector<Card> board = parseCards("Ad7h2c");
    auto result = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ks"),
        Card::fromString("Qd"), Card::fromString("Qh"),
        board
    );
    EXPECT_GT(result.equity, 0.85);
}

TEST_F(EquityTest, FlushDraw_HasEquity_OnFlop) {
    // Flush draw vs top pair: should have ~35% equity
    std::vector<Card> board = parseCards("As7s2d");
    auto result = calc.handVsHand(
        Card::fromString("Ks"), Card::fromString("Qs"),  // flush draw
        Card::fromString("Ah"), Card::fromString("Kh"),  // top pair
        board
    );
    EXPECT_GT(result.equity, 0.25);
    EXPECT_LT(result.equity, 0.50);
}

TEST_F(EquityTest, River_NoBoardCardsLeft_DeterministicResult) {
    // On river, equity is either 0, 0.5, or 1
    std::vector<Card> board = parseCards("As7h2d4cTd");
    auto result = calc.handVsHand(
        Card::fromString("Ac"), Card::fromString("Kc"),
        Card::fromString("Qs"), Card::fromString("Jh"),
        board
    );
    EXPECT_EQ(result.matchups_evaluated, 1u);
    // AK makes pair of aces, QJ has nothing → AK wins 100%
    EXPECT_DOUBLE_EQ(result.equity, 1.0);
}

// ── Hand vs Range ───────────────────────────────────────────

TEST_F(EquityTest, HandVsRange_NutsHasHighEquity) {
    // AK on A-7-2 flop vs a range that contains underpairs and draws
    Range villain;
    villain.parse("QQ, JJ, TT, 99, 88");
    std::vector<Card> board = parseCards("Ad7h2c");
    auto result = calc.handVsRange(
        Card::fromString("As"), Card::fromString("Ks"),
        villain, board, 20000
    );
    EXPECT_GT(result.equity, 0.85);
}

TEST_F(EquityTest, HandVsRange_ExactVillain_MatchesHandVsHand) {
    // If villain range is one exact hand, should match handVsHand
    Range villain;
    villain.setExact(Card::fromString("Ks"), Card::fromString("Kh"));
    auto hvh = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ah"),
        Card::fromString("Ks"), Card::fromString("Kh"),
        {}
    );
    auto hvr = calc.handVsRange(
        Card::fromString("As"), Card::fromString("Ah"),
        villain, {}, 0  // exhaustive
    );
    EXPECT_NEAR(hvr.equity, hvh.equity, 0.01);
}

// ── Range vs Range ──────────────────────────────────────────

TEST_F(EquityTest, RangeVsRange_MirrorRanges_50Percent) {
    // Same range vs itself should be ~50%
    Range r;
    r.parse("AA, KK, QQ");
    auto result = calc.rangeVsRange(r, r, {}, 50000);
    EXPECT_NEAR(result.equity, 0.5, 0.05);
}

TEST_F(EquityTest, RangeVsRange_StrongerRangeWins) {
    Range strong, weak;
    strong.parse("AA, KK");
    weak.parse("22, 33, 44");
    auto result = calc.rangeVsRange(strong, weak, {}, 50000);
    EXPECT_GT(result.equity, 0.7);
}

// ── All-In Equity by Street ─────────────────────────────────

TEST_F(EquityTest, AllInByStreet_FlopTurnRiver_Computed) {
    auto board = parseCards("As8h5dTc2s");
    auto equities = calc.allInEquityByStreet(
        Card::fromString("Qs"), Card::fromString("Ts"),
        Card::fromString("Ah"), Card::fromString("Kd"),
        board
    );
    // All three streets should be computed
    EXPECT_GE(equities.flop, 0.0);
    EXPECT_GE(equities.turn, 0.0);
    EXPECT_GE(equities.river, 0.0);
    // Equity values should be in [0, 1]
    EXPECT_LE(equities.flop, 1.0);
    EXPECT_LE(equities.turn, 1.0);
    EXPECT_LE(equities.river, 1.0);
}

TEST_F(EquityTest, AllInByStreet_FlopOnly_TurnAndRiverNegative) {
    auto board = parseCards("As8h5d");  // only flop
    auto equities = calc.allInEquityByStreet(
        Card::fromString("Qs"), Card::fromString("Ts"),
        Card::fromString("Ah"), Card::fromString("Kd"),
        board
    );
    EXPECT_GE(equities.flop, 0.0);
    EXPECT_LT(equities.turn, 0.0);   // not computed
    EXPECT_LT(equities.river, 0.0);  // not computed
}

TEST_F(EquityTest, AllInByStreet_River_EitherZeroOrOne) {
    auto board = parseCards("As8h5dTc2s");
    auto equities = calc.allInEquityByStreet(
        Card::fromString("Qs"), Card::fromString("Ts"),
        Card::fromString("Ah"), Card::fromString("Kd"),
        board
    );
    // On the river, outcome is deterministic
    EXPECT_TRUE(equities.river == 0.0 || equities.river == 0.5 || equities.river == 1.0);
}

// ── Performance ─────────────────────────────────────────────

TEST_F(EquityTest, Benchmark_PreflopExhaustive) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = calc.handVsHand(
        Card::fromString("As"), Card::fromString("Ah"),
        Card::fromString("Ks"), Card::fromString("Kh"),
        {}
    );
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[BENCHMARK] AA vs KK preflop exhaustive: " << ms << " ms ("
              << result.matchups_evaluated << " boards)" << std::endl;
    EXPECT_LT(ms, 5000) << "Preflop equity too slow";
}
