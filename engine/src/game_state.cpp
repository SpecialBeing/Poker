#include "poker/game_state.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cassert>
#include <iomanip>

namespace poker {

// ── Construction ────────────────────────────────────────────

GameState::GameState(
    int num_players, const std::vector<double>& stacks,
    double sb_amount, double bb_amount, double ante,
    BetSizingConfig bet_config
)
    : num_players_(num_players)
    , street_(Street::PREFLOP)
    , pot_(0)
    , sb_amount_(sb_amount)
    , bb_amount_(bb_amount)
    , ante_(ante)
    , current_player_(0)
    , last_raiser_(-1)
    , actions_this_round_(0)
    , bet_config_(std::move(bet_config))
{
    assert(num_players >= 2 && num_players <= MAX_PLAYERS);
    for (int i = 0; i < num_players; ++i) {
        players_[i] = {stacks[i], 0.0, false, false};
    }
    postBlinds();
}

void GameState::postBlinds() {
    // Antes
    for (int i = 0; i < num_players_; ++i) {
        double a = std::min(ante_, players_[i].stack);
        players_[i].stack -= a;
        pot_ += a;
    }

    // Blinds: SB = seat (num_players - 2), BB = seat (num_players - 1)
    // Heads-up special: BTN/SB = seat 0, BB = seat 1
    int sb_seat, bb_seat;
    if (num_players_ == 2) {
        sb_seat = 0;
        bb_seat = 1;
    } else {
        sb_seat = num_players_ - 2;
        bb_seat = num_players_ - 1;
    }

    double sb = std::min(sb_amount_, players_[sb_seat].stack);
    players_[sb_seat].stack -= sb;
    players_[sb_seat].bet_this_round = sb;
    if (players_[sb_seat].stack <= 0) players_[sb_seat].all_in = true;

    double bb = std::min(bb_amount_, players_[bb_seat].stack);
    players_[bb_seat].stack -= bb;
    players_[bb_seat].bet_this_round = bb;
    if (players_[bb_seat].stack <= 0) players_[bb_seat].all_in = true;

    // Preflop: UTG (seat 0) acts first in 6-max.
    // Heads-up: SB (seat 0) acts first preflop.
    current_player_ = 0;
}

// ── Queries ─────────────────────────────────────────────────

bool GameState::isTerminal() const {
    if (playersInHand() <= 1) return true;
    if (street_ == Street::RIVER && isRoundOver()) return true;
    return false;
}

bool GameState::isRoundOver() const {
    if (playersInHand() <= 1) return true;

    int active = activePlayers();
    if (active == 0) return true; // everyone all-in

    if (actions_this_round_ < active) return false;

    // All active players have acted and bets are matched
    double mb = maxBet();
    for (int i = 0; i < num_players_; ++i) {
        if (!players_[i].folded && !players_[i].all_in) {
            if (std::abs(players_[i].bet_this_round - mb) > 0.001) return false;
        }
    }
    return true;
}

int GameState::playersInHand() const {
    int n = 0;
    for (int i = 0; i < num_players_; ++i) if (!players_[i].folded) ++n;
    return n;
}

int GameState::activePlayers() const {
    int n = 0;
    for (int i = 0; i < num_players_; ++i)
        if (!players_[i].folded && !players_[i].all_in) ++n;
    return n;
}

double GameState::toCall(int p) const {
    return maxBet() - players_[p].bet_this_round;
}

double GameState::maxBet() const {
    double m = 0;
    for (int i = 0; i < num_players_; ++i)
        m = std::max(m, players_[i].bet_this_round);
    return m;
}

// ── Legal Actions ───────────────────────────────────────────

std::vector<Action> GameState::legalActions() const {
    std::vector<Action> actions;
    int p = current_player_;
    double call_amount = toCall(p);
    double stack = players_[p].stack;

    if (call_amount > 0) {
        // Facing a bet/raise
        actions.push_back({ActionType::FOLD, 0});
        actions.push_back({ActionType::CALL, std::min(call_amount, stack)});
    } else {
        actions.push_back({ActionType::CHECK, 0});
    }

    // Raise/bet options (only if we have chips beyond calling)
    if (stack > call_amount + 0.001) {
        double pot_after_call = pot_ + call_amount;
        for (int i = 0; i < num_players_; ++i)
            pot_after_call += players_[i].bet_this_round;
        // Dedup the current player's contribution (already counted in bet_this_round)
        pot_after_call -= players_[p].bet_this_round;
        // After calling, effective pot includes everyone's bets
        // Simpler: pot_after_call = pot + 2 * maxBet (for heads-up)

        if (street_ == Street::PREFLOP) {
            for (double bb_mult : bet_config_.preflop_sizes) {
                double raise_to = bb_mult * bb_amount_;
                if (raise_to <= maxBet()) continue; // must raise above current bet
                double raise_amount = raise_to - players_[p].bet_this_round;
                if (raise_amount > 0 && raise_amount < stack) {
                    actions.push_back({ActionType::BET, raise_amount});
                }
            }
        } else {
            for (double frac : bet_config_.postflop_sizes) {
                double bet_size = pot_after_call * frac;
                double raise_amount;
                if (call_amount > 0) {
                    // Raising: raise_to = maxBet + bet_size
                    raise_amount = call_amount + bet_size;
                } else {
                    raise_amount = bet_size;
                }
                if (raise_amount > 0 && raise_amount < stack) {
                    actions.push_back({ActionType::BET, raise_amount});
                }
            }
        }

        // All-in is always available
        actions.push_back({ActionType::ALL_IN, stack});
    }

    return actions;
}

// ── Apply Action ────────────────────────────────────────────

GameState GameState::applyAction(const Action& action) const {
    GameState next = *this;
    int p = next.current_player_;
    double amount = 0;

    switch (action.type) {
        case ActionType::FOLD:
            next.players_[p].folded = true;
            break;

        case ActionType::CHECK:
            break;

        case ActionType::CALL: {
            amount = std::min(next.toCall(p), next.players_[p].stack);
            next.players_[p].bet_this_round += amount;
            next.players_[p].stack -= amount;
            if (next.players_[p].stack <= 0.001) next.players_[p].all_in = true;
            break;
        }

        case ActionType::BET: {
            amount = std::min(action.size, next.players_[p].stack);
            next.players_[p].bet_this_round += amount;
            next.players_[p].stack -= amount;
            next.last_raiser_ = p;
            if (next.players_[p].stack <= 0.001) next.players_[p].all_in = true;
            break;
        }

        case ActionType::ALL_IN: {
            amount = next.players_[p].stack;
            next.players_[p].bet_this_round += amount;
            next.players_[p].stack = 0;
            next.players_[p].all_in = true;
            next.last_raiser_ = p;
            break;
        }
    }

    next.history_.push_back({p, action, amount});
    next.actions_this_round_++;
    next.current_player_ = next.nextActivePlayer(p);

    return next;
}

// ── Advance Street ──────────────────────────────────────────

GameState GameState::advanceStreet() const {
    GameState next = *this;

    // Collect bets into pot
    for (int i = 0; i < next.num_players_; ++i) {
        next.pot_ += next.players_[i].bet_this_round;
        next.players_[i].bet_this_round = 0;
    }

    next.actions_this_round_ = 0;
    next.last_raiser_ = -1;

    // Advance street
    next.street_ = static_cast<Street>(static_cast<int>(next.street_) + 1);

    // Postflop: first active player after dealer acts first
    // In heads-up: BB (seat 1) acts first postflop
    // In 6-max: first non-folded player from SB onward
    if (next.num_players_ == 2) {
        next.current_player_ = next.nextActivePlayer(0); // seat 1 if alive
        // Actually, in HU postflop BB acts first = seat 1
        if (!next.players_[1].folded && !next.players_[1].all_in)
            next.current_player_ = 1;
        else
            next.current_player_ = 0;
    } else {
        // Find first active player starting from SB position
        next.current_player_ = next.nextActivePlayer(next.num_players_ - 1);
    }

    return next;
}

void GameState::setBoard(const std::vector<Card>& cards) {
    board_.clear();
    for (auto c : cards) board_.push_back(c.id());
}

// ── Internal ────────────────────────────────────────────────

int GameState::nextActivePlayer(int from) const {
    for (int i = 1; i <= num_players_; ++i) {
        int p = (from + i) % num_players_;
        if (!players_[p].folded && !players_[p].all_in) return p;
    }
    return from; // no one else active — shouldn't happen in normal play
}

// ── Display ─────────────────────────────────────────────────

std::string GameState::toString() const {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    os << "Street: " << streetToString(street_) << "  Pot: " << pot_ << "\n";

    for (int i = 0; i < num_players_; ++i) {
        os << "  P" << i;
        if (i == current_player_) os << "*";
        os << ": stack=" << players_[i].stack
           << " bet=" << players_[i].bet_this_round;
        if (players_[i].folded) os << " [FOLD]";
        if (players_[i].all_in) os << " [ALL-IN]";
        os << "\n";
    }

    if (!board_.empty()) {
        os << "Board: ";
        for (auto c : board_) os << Card(c).toString() << " ";
        os << "\n";
    }
    return os.str();
}

std::string GameState::historyString() const {
    std::ostringstream os;
    for (auto& rec : history_) {
        switch (rec.action.type) {
            case ActionType::FOLD:   os << "f"; break;
            case ActionType::CHECK:  os << "x"; break;
            case ActionType::CALL:   os << "c"; break;
            case ActionType::BET:    os << "b" << std::fixed << std::setprecision(2) << rec.action.size; break;
            case ActionType::ALL_IN: os << "a"; break;
        }
        os << ":";
    }
    return os.str();
}

} // namespace poker
