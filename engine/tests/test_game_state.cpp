#include <gtest/gtest.h>
#include "poker/game_state.h"

using namespace poker;

class GameStateTest : public ::testing::Test {
protected:
    BetSizingConfig bet_config = BetSizingConfig::standard();

    // Create a standard heads-up 50bb state
    GameState makeHU(double stack_bb = 50.0) {
        return GameState(2, {stack_bb, stack_bb}, 0.5, 1.0, 0.0, bet_config);
    }

    // Find a specific action type in a list
    static Action findAction(const std::vector<Action>& actions, ActionType type) {
        for (auto& a : actions) {
            if (a.type == type) return a;
        }
        return {ActionType::FOLD, 0};
    }

    static bool hasAction(const std::vector<Action>& actions, ActionType type) {
        for (auto& a : actions) {
            if (a.type == type) return true;
        }
        return false;
    }
};

// ── Initial State ───────────────────────────────────────────

TEST_F(GameStateTest, HU_InitialState_BlindsPosted) {
    auto state = makeHU();
    EXPECT_EQ(state.numPlayers(), 2);
    EXPECT_EQ(state.street(), Street::PREFLOP);
    EXPECT_DOUBLE_EQ(state.player(0).bet_this_round, 0.5);  // SB
    EXPECT_DOUBLE_EQ(state.player(1).bet_this_round, 1.0);  // BB
}

TEST_F(GameStateTest, HU_SBActsFirstPreflop) {
    auto state = makeHU();
    EXPECT_EQ(state.currentPlayer(), 0);
}

TEST_F(GameStateTest, HU_StacksReducedByBlinds) {
    auto state = makeHU(50.0);
    EXPECT_DOUBLE_EQ(state.player(0).stack, 49.5);  // 50 - 0.5
    EXPECT_DOUBLE_EQ(state.player(1).stack, 49.0);   // 50 - 1.0
}

TEST_F(GameStateTest, SixMax_InitialState_BlindsPosted) {
    GameState state(6, {100, 100, 100, 100, 100, 100}, 0.5, 1.0, 0.0, bet_config);
    EXPECT_EQ(state.numPlayers(), 6);
    EXPECT_DOUBLE_EQ(state.player(4).bet_this_round, 0.5);  // SB = seat 4
    EXPECT_DOUBLE_EQ(state.player(5).bet_this_round, 1.0);  // BB = seat 5
    EXPECT_EQ(state.currentPlayer(), 0);  // UTG acts first
}

// ── Legal Actions ───────────────────────────────────────────

TEST_F(GameStateTest, Preflop_SB_CanFoldCallRaise) {
    auto state = makeHU();
    auto actions = state.legalActions();

    EXPECT_TRUE(hasAction(actions, ActionType::FOLD));
    EXPECT_TRUE(hasAction(actions, ActionType::CALL));
    EXPECT_TRUE(hasAction(actions, ActionType::BET));
    EXPECT_TRUE(hasAction(actions, ActionType::ALL_IN));
}

TEST_F(GameStateTest, Preflop_SB_CannotCheck) {
    auto state = makeHU();
    auto actions = state.legalActions();
    EXPECT_FALSE(hasAction(actions, ActionType::CHECK));
}

TEST_F(GameStateTest, Preflop_RaiseSizes_Match2236Config) {
    auto state = makeHU();
    auto actions = state.legalActions();
    int bet_count = 0;
    for (auto& a : actions) {
        if (a.type == ActionType::BET) ++bet_count;
    }
    // Should have some bet sizes from 2.236^k config (those that are > current max bet)
    EXPECT_GE(bet_count, 1);
}

TEST_F(GameStateTest, Flop_CanCheckIfNoBet) {
    auto state = makeHU();
    // SB calls → round over → advance to flop
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    auto s3 = s2.advanceStreet();
    s3.setBoard(parseCards("As8h5d"));

    auto actions = s3.legalActions();
    EXPECT_TRUE(hasAction(actions, ActionType::CHECK));
    EXPECT_FALSE(hasAction(actions, ActionType::FOLD));  // no bet to fold to
}

TEST_F(GameStateTest, Flop_BetSizesArePotFractions) {
    auto state = makeHU();
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    auto s3 = s2.advanceStreet();
    s3.setBoard(parseCards("As8h5d"));

    auto actions = s3.legalActions();
    int bet_count = 0;
    for (auto& a : actions) {
        if (a.type == ActionType::BET) ++bet_count;
    }
    EXPECT_GE(bet_count, 1);
    EXPECT_TRUE(hasAction(actions, ActionType::ALL_IN));
}

// ── Action Application ──────────────────────────────────────

TEST_F(GameStateTest, ApplyAction_ReturnsNewState_OriginalUnchanged) {
    auto state = makeHU();
    double original_stack = state.player(0).stack;
    auto next = state.applyAction(findAction(state.legalActions(), ActionType::CALL));

    EXPECT_DOUBLE_EQ(state.player(0).stack, original_stack);  // original unchanged
    EXPECT_NE(next.player(0).stack, original_stack);           // new state changed
}

TEST_F(GameStateTest, Fold_EndsHand) {
    auto state = makeHU();
    auto next = state.applyAction({ActionType::FOLD, 0});
    EXPECT_TRUE(next.isTerminal());
    EXPECT_EQ(next.playersInHand(), 1);
}

TEST_F(GameStateTest, Call_MatchesBet) {
    auto state = makeHU();
    // SB calls the BB
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    EXPECT_DOUBLE_EQ(s2.player(0).bet_this_round, 1.0);  // matched BB's bet
}

TEST_F(GameStateTest, AllIn_SetsStackToZero) {
    auto state = makeHU();
    auto next = state.applyAction({ActionType::ALL_IN, state.player(0).stack});
    EXPECT_LE(next.player(0).stack, 0.001);
    EXPECT_TRUE(next.player(0).all_in);
}

// ── Round Completion ────────────────────────────────────────

TEST_F(GameStateTest, HU_SBLimps_BBStillHasOption) {
    auto state = makeHU();
    // SB calls (limps) → BB still has option to check or raise
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    EXPECT_FALSE(s2.isRoundOver());  // BB hasn't acted yet
    EXPECT_EQ(s2.currentPlayer(), 1);  // BB's turn

    // BB checks → now round is over
    auto s3 = s2.applyAction({ActionType::CHECK, 0});
    EXPECT_TRUE(s3.isRoundOver());
}

TEST_F(GameStateTest, HU_RaiseReopensAction) {
    auto state = makeHU();
    // SB raises
    auto raise = findAction(state.legalActions(), ActionType::BET);
    auto s2 = state.applyAction(raise);
    // BB now has to act → round not over
    EXPECT_FALSE(s2.isRoundOver());
}

// ── Street Advancement ──────────────────────────────────────

TEST_F(GameStateTest, AdvanceStreet_PreflopToFlop) {
    auto state = makeHU();
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    auto s3 = s2.advanceStreet();
    EXPECT_EQ(s3.street(), Street::FLOP);
}

TEST_F(GameStateTest, AdvanceStreet_CollectsBetsIntoPot) {
    auto state = makeHU();
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    auto s3 = s2.advanceStreet();
    EXPECT_GT(s3.pot(), 0.0);
    EXPECT_DOUBLE_EQ(s3.player(0).bet_this_round, 0.0);  // bets reset
    EXPECT_DOUBLE_EQ(s3.player(1).bet_this_round, 0.0);
}

TEST_F(GameStateTest, AdvanceStreet_SetBoard) {
    auto state = makeHU();
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    auto s3 = s2.advanceStreet();
    s3.setBoard(parseCards("AsKhQd"));
    EXPECT_EQ(s3.board().size(), 3u);
}

// ── Terminal Detection ──────────────────────────────────────

TEST_F(GameStateTest, NotTerminal_AtStart) {
    auto state = makeHU();
    EXPECT_FALSE(state.isTerminal());
}

TEST_F(GameStateTest, Terminal_AfterFold) {
    auto state = makeHU();
    auto next = state.applyAction({ActionType::FOLD, 0});
    EXPECT_TRUE(next.isTerminal());
}

TEST_F(GameStateTest, Terminal_AfterRiverBetting) {
    auto state = makeHU();
    // Play through all streets with check-check
    auto s = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    // Flop
    s = s.advanceStreet();
    s.setBoard(parseCards("As8h5d"));
    s = s.applyAction({ActionType::CHECK, 0});
    s = s.applyAction({ActionType::CHECK, 0});
    // Turn
    s = s.advanceStreet();
    s.setBoard(parseCards("As8h5dTc"));
    s = s.applyAction({ActionType::CHECK, 0});
    s = s.applyAction({ActionType::CHECK, 0});
    // River
    s = s.advanceStreet();
    s.setBoard(parseCards("As8h5dTc2s"));
    s = s.applyAction({ActionType::CHECK, 0});
    s = s.applyAction({ActionType::CHECK, 0});

    EXPECT_TRUE(s.isTerminal());
}

// ── History ─────────────────────────────────────────────────

TEST_F(GameStateTest, History_RecordsActions) {
    auto state = makeHU();
    auto s2 = state.applyAction({ActionType::FOLD, 0});
    EXPECT_EQ(s2.history().size(), 1u);
    EXPECT_EQ(s2.history()[0].action.type, ActionType::FOLD);
    EXPECT_EQ(s2.history()[0].player, 0);
}

TEST_F(GameStateTest, HistoryString_NotEmpty) {
    auto state = makeHU();
    auto s2 = state.applyAction(findAction(state.legalActions(), ActionType::CALL));
    EXPECT_FALSE(s2.historyString().empty());
}
