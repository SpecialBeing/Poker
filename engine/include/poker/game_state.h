/// @file game_state.h
/// @brief Game rules and mutable state for No-Limit Hold'em.
///
/// GameState tracks everything about a hand in progress:
///   - Street, pot, per-player stacks/bets/status
///   - Full action history
///   - Legal action generation (respecting bet sizing config)
///
/// Designed for two uses:
///   1. Simulation: play out hands with concrete cards
///   2. Tree traversal: CFR walks the game tree through state transitions
///
/// The state is fully copyable (for tree branching) and supports
/// both heads-up and 6-max, though the solver starts heads-up.

#pragma once

#include "common.h"
#include "card.h"
#include <array>
#include <vector>
#include <string>

namespace poker {

/// Record of one action taken during the hand.
struct ActionRecord {
    int player;
    Action action;
    double amount_put_in;   // chips this action added to the pot
};

/// Per-player state within a hand.
struct PlayerState {
    double stack;
    double bet_this_round;  // chips committed in the current betting round
    bool   folded;
    bool   all_in;
};

/// Full state of a hand in progress.
class GameState {
public:
    /// Create an initial state for a new hand.
    /// @param num_players  number of players (2–6)
    /// @param stacks       starting stack for each player (in chips)
    /// @param sb_amount    small blind size
    /// @param bb_amount    big blind size
    /// @param ante         ante per player (0 for no ante)
    /// @param bet_config   bet sizing configuration
    GameState(int num_players, const std::vector<double>& stacks,
              double sb_amount, double bb_amount, double ante,
              BetSizingConfig bet_config);

    // ── Queries ─────────────────────────────────────────────

    int            numPlayers()       const { return num_players_; }
    Street         street()           const { return street_; }
    double         pot()              const { return pot_; }
    int            currentPlayer()    const { return current_player_; }
    bool           isTerminal()       const;
    bool           isRoundOver()      const;
    int            playersInHand()    const;
    int            activePlayers()    const;  // not folded and not all-in
    double         toCall(int p)      const;
    double         maxBet()           const;
    const PlayerState& player(int i)  const { return players_[i]; }
    const std::vector<ActionRecord>& history() const { return history_; }
    const std::vector<uint8_t>& board() const { return board_; }
    const BetSizingConfig& betConfig() const { return bet_config_; }

    /// Get all legal actions for the current player.
    std::vector<Action> legalActions() const;

    // ── Mutations ───────────────────────────────────────────

    /// Apply an action. Returns a new state (original is unchanged).
    GameState applyAction(const Action& action) const;

    /// Advance to the next street (collect bets into pot, deal board cards).
    /// Board cards are NOT dealt here — the caller must set them via setBoard.
    GameState advanceStreet() const;

    /// Set the community board cards.
    void setBoard(const std::vector<Card>& cards);

    // ── Display ─────────────────────────────────────────────

    /// Human-readable summary of current state.
    std::string toString() const;

    /// Compact history string for info-set keys. e.g. "r2.236:c|b0.5:f"
    std::string historyString() const;

private:
    int num_players_;
    Street street_;
    double pot_;
    double sb_amount_, bb_amount_, ante_;
    int current_player_;
    int last_raiser_;
    int actions_this_round_;
    BetSizingConfig bet_config_;

    std::array<PlayerState, MAX_PLAYERS> players_;
    std::vector<uint8_t> board_;
    std::vector<ActionRecord> history_;

    void postBlinds();
    int  nextActivePlayer(int from) const;
};

} // namespace poker
