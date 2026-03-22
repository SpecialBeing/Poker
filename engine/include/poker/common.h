/// @file common.h
/// @brief Shared types, constants, and enumerations for the poker engine.

#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace poker {

// ── Fundamental Constants ───────────────────────────────────

constexpr int NUM_RANKS = 13;
constexpr int NUM_SUITS = 4;
constexpr int NUM_CARDS = 52;
/// C(52,2) = 1326 possible hole card combinations.
constexpr int NUM_HOLE_COMBOS = 1326;
constexpr int MAX_PLAYERS = 6;

// ── Enumerations ────────────────────────────────────────────

enum class Rank : uint8_t {
    TWO = 0, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN,
    JACK, QUEEN, KING, ACE
};

enum class Suit : uint8_t {
    CLUBS = 0, DIAMONDS, HEARTS, SPADES
};

/// Betting rounds.
enum class Street : uint8_t {
    PREFLOP = 0, FLOP, TURN, RIVER
};

/// 6-max table positions.
enum class Position : uint8_t {
    UTG = 0, HJ, CO, BTN, SB, BB
};

enum class ActionType : uint8_t {
    FOLD = 0,
    CHECK,
    CALL,
    BET,      // includes raise — distinguished by context
    ALL_IN
};

/// Hand ranking categories, ordered worst → best.
enum class HandCategory : uint8_t {
    HIGH_CARD = 0,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    STRAIGHT_FLUSH
};

// ── Bet Sizing ──────────────────────────────────────────────

/// Configurable bet sizing for a strategy / game tree.
///
/// Preflop sizes are in BB (e.g. 2.236^k for k=0..3).
/// Postflop sizes are fractions of the pot (e.g. 0.25, 0.5, ...).
/// ALL-IN is always implicitly available.
struct BetSizingConfig {
    std::vector<double> preflop_sizes;   // absolute BB amounts
    std::vector<double> postflop_sizes;  // pot fractions

    /// Default: preflop 2.236^k (k=0..3), postflop 1/4 to 1.25x pot.
    static BetSizingConfig standard() {
        return {
            {1.0, 2.236, 5.0, 11.180},            // 2.236^0, ^1, ^2, ^3
            {0.25, 0.50, 0.75, 1.00, 1.25}
        };
    }
};

/// An action a player can take, pairing type with sizing.
struct Action {
    ActionType type;
    double size;   // BB for preflop bets, chips for postflop bets, 0 for fold/check/call

    bool operator==(const Action& o) const {
        return type == o.type && std::abs(size - o.size) < 0.01;
    }

    std::string toString() const;
};

// ── Utility ─────────────────────────────────────────────────

inline constexpr const char* RANK_CHARS = "23456789TJQKA";
inline constexpr const char* SUIT_CHARS = "cdhs";

/// Unicode suit symbols for display.
inline constexpr const char* SUIT_SYMBOLS[] = {"♣", "♦", "♥", "♠"};

inline char rankToChar(Rank r) { return RANK_CHARS[static_cast<int>(r)]; }
inline char suitToChar(Suit s) { return SUIT_CHARS[static_cast<int>(s)]; }

inline Rank charToRank(char c) {
    for (int i = 0; i < NUM_RANKS; ++i)
        if (RANK_CHARS[i] == c) return static_cast<Rank>(i);
    return Rank::TWO; // fallback
}

inline Suit charToSuit(char c) {
    for (int i = 0; i < NUM_SUITS; ++i)
        if (SUIT_CHARS[i] == c) return static_cast<Suit>(i);
    return Suit::CLUBS; // fallback
}

inline std::string positionToString(Position p) {
    constexpr const char* names[] = {"UTG", "HJ", "CO", "BTN", "SB", "BB"};
    return names[static_cast<int>(p)];
}

inline std::string streetToString(Street s) {
    constexpr const char* names[] = {"Preflop", "Flop", "Turn", "River"};
    return names[static_cast<int>(s)];
}

inline std::string categoryToString(HandCategory c) {
    constexpr const char* names[] = {
        "High Card", "Pair", "Two Pair", "Three of a Kind",
        "Straight", "Flush", "Full House", "Four of a Kind",
        "Straight Flush"
    };
    return names[static_cast<int>(c)];
}

} // namespace poker
