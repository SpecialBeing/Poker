/// @file equity_calculator.h
/// @brief Range-vs-range equity calculation, multi-threaded.
///
/// Two modes:
///   1. Exhaustive enumeration — exact but slow for preflop
///   2. Monte Carlo sampling  — fast approximation with configurable samples
///
/// Usage examples:
///   - Hero (exact hand) vs villain (weighted range) on a known flop
///   - Track equity changes street by street when both players are all-in
///   - Full range vs range preflop equity

#pragma once

#include "common.h"
#include "card.h"
#include "hand_evaluator.h"
#include "range.h"
#include <vector>
#include <functional>

namespace poker {

/// Result of an equity calculation.
struct EquityResult {
    double equity;     // player's equity [0, 1]
    double win_pct;
    double tie_pct;
    double lose_pct;
    uint64_t matchups_evaluated;
};

/// Equity at each street (for all-in equity tracking).
struct StreetEquities {
    double preflop;    // -1 if not computed
    double flop;       // -1 if board < 3
    double turn;       // -1 if board < 4
    double river;      // -1 if board < 5

    StreetEquities() : preflop(-1), flop(-1), turn(-1), river(-1) {}
};

class EquityCalculator {
public:
    explicit EquityCalculator(int num_threads = 0);  // 0 = auto-detect

    /// ── Hand vs Hand ───────────────────────────────────────
    /// Exact hand vs exact hand, optional partial board.
    /// Uses exhaustive enumeration for remaining board cards.
    EquityResult handVsHand(
        Card h1a, Card h1b,            // player 1's hole cards
        Card h2a, Card h2b,            // player 2's hole cards
        const std::vector<Card>& board  // 0, 3, 4, or 5 community cards
    ) const;

    /// ── Hand vs Range ──────────────────────────────────────
    /// Exact hand vs a weighted range on a partial board.
    EquityResult handVsRange(
        Card h1a, Card h1b,
        const Range& villain_range,
        const std::vector<Card>& board,
        int monte_carlo_samples = 0     // 0 = exhaustive
    ) const;

    /// ── Range vs Range ─────────────────────────────────────
    /// Full range vs range equity. Uses Monte Carlo if requested.
    EquityResult rangeVsRange(
        const Range& range_a,
        const Range& range_b,
        const std::vector<Card>& board,
        int monte_carlo_samples = 100000
    ) const;

    /// ── Street-by-Street Equity (All-In Tracker) ───────────
    /// Given two exact hands, compute equity at each remaining street.
    /// Useful for: both players all-in, want to see flop/turn/river equity changes.
    StreetEquities allInEquityByStreet(
        Card h1a, Card h1b,
        Card h2a, Card h2b,
        const std::vector<Card>& board  // current known board (0–5 cards)
    ) const;

private:
    int num_threads_;
    HandEvaluator evaluator_;

    /// Enumerate all possible remaining board cards and evaluate matchup.
    EquityResult enumerateBoards(
        const uint8_t hand_a[2],
        const uint8_t hand_b[2],
        const std::vector<uint8_t>& board,
        const std::vector<uint8_t>& remaining
    ) const;
};

} // namespace poker
