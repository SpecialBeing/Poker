/// @file hand_evaluator.h
/// @brief Fast poker hand evaluation using bit manipulation.
///
/// Hand strength is encoded as a single uint32_t so that stronger hands
/// always compare greater. This enables O(1) comparison of evaluated hands.
///
/// Encoding (32 bits):
///   bits 24–27 : HandCategory (0 = High Card … 8 = Straight Flush)
///   bits  0–23 : tiebreaker kickers, packed in descending importance
///
/// For the equity calculator this evaluator will be called millions of times,
/// so every micro-optimization matters. The 5-card evaluator uses:
///   - 13-bit rank bitmask for O(1) straight detection via lookup table
///   - rank frequency array for pair/trips/quads classification
///   - suit counting for flush detection

#pragma once

#include "common.h"
#include "card.h"
#include <array>
#include <cstdint>
#include <string>

namespace poker {

/// Result of evaluating a hand: a comparable 32-bit score.
struct HandRank {
    uint32_t value;

    HandCategory category() const {
        return static_cast<HandCategory>((value >> 24) & 0xF);
    }

    bool operator> (HandRank o) const { return value >  o.value; }
    bool operator< (HandRank o) const { return value <  o.value; }
    bool operator==(HandRank o) const { return value == o.value; }
    bool operator!=(HandRank o) const { return value != o.value; }
    bool operator>=(HandRank o) const { return value >= o.value; }
    bool operator<=(HandRank o) const { return value <= o.value; }

    std::string toString() const;
};

class HandEvaluator {
public:
    HandEvaluator();

    /// Evaluate exactly 5 cards. Each card is 0–51.
    HandRank evaluate5(const uint8_t cards[5]) const;

    /// Evaluate best 5-card hand from 7 cards (2 hole + 5 board).
    /// Iterates over C(7,5) = 21 combinations.
    HandRank evaluate7(const uint8_t cards[7]) const;

    /// Convenience: evaluate best hand from any 5–7 cards.
    HandRank evaluate(const uint8_t* cards, int n) const;

    /// Compare two hole-card pairs on a shared board.
    /// Returns +1 if hand_a wins, -1 if hand_b wins, 0 for tie.
    int compare(const uint8_t hand_a[2], const uint8_t hand_b[2],
                const uint8_t* board, int board_len) const;

private:
    /// straight_table_[bitmask] = high card of straight (rank int), or -1.
    /// bitmask is 13 bits, one per rank.
    std::array<int8_t, 8192> straight_table_;

    void initStraightTable();
};

} // namespace poker
