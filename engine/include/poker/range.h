/// @file range.h
/// @brief Weighted hand range representation.
///
/// A Range maps each possible hole-card combination to a weight (probability).
/// Weight 0 means the hand is not in the range; weight 1 means full frequency.
///
/// Supports:
///   - Setting a single specific hand (e.g. K♦3♣ at 100%)
///   - Setting canonical hand classes (e.g. "AKs" at 60%)
///   - Setting all combos of a class (e.g. all AKo combos)
///   - Parsing range strings like "AA, AKs, QQ-TT, 87s"
///   - Dead card removal (combos containing dead cards get weight 0)

#pragma once

#include "common.h"
#include "card.h"
#include <array>
#include <string>
#include <vector>
#include <functional>

namespace poker {

/// A hand range: weights for each of the 1326 possible hole-card pairs.
class Range {
public:
    Range();

    /// Set every combo to the same weight (e.g. 1.0 for "any two cards").
    void setAll(double weight);

    /// Set weight for one specific combo (two concrete cards).
    void setCombo(Card c1, Card c2, double weight);

    /// Set weight for all combos of a canonical hand class.
    /// hand_str examples: "AKs", "QTo", "99", "A5s"
    void setHand(const std::string& hand_str, double weight);

    /// Parse a range string like "AA, AKs, QQ-TT, 87s:0.5"
    /// Supports:
    ///   "AA"       → all 6 combos at weight 1.0
    ///   "AKs"      → all 4 suited combos at weight 1.0
    ///   "AKs:0.6"  → suited combos at weight 0.6
    ///   "QQ-TT"    → QQ, JJ, TT at weight 1.0
    void parse(const std::string& range_str);

    /// Set range to a single specific hand at 100%.
    /// e.g. exactHand("Kd", "3c") → only K♦3♣ is in range.
    void setExact(Card c1, Card c2);

    /// Zero out any combo that uses a dead card.
    void removeDead(const std::vector<Card>& dead);

    /// Get weight for a specific combo.
    double getWeight(Card c1, Card c2) const;

    /// Get weight by hole-card index.
    double getWeight(int index) const { return weights_[index]; }

    /// Number of combos with nonzero weight.
    int numCombos() const;

    /// Sum of all weights (useful for normalization).
    double totalWeight() const;

    /// Iterate over all combos with nonzero weight.
    /// Callback: (card1_id, card2_id, weight) → void
    void forEachCombo(std::function<void(uint8_t, uint8_t, double)> fn) const;

    /// Get the raw weight array (for performance-critical inner loops).
    const std::array<double, NUM_HOLE_COMBOS>& weights() const { return weights_; }

    /// Pretty-print the range as a 13×13 grid.
    std::string toGrid() const;

private:
    std::array<double, NUM_HOLE_COMBOS> weights_;

    /// Expand a canonical hand class (e.g. "AKs") into all concrete combos.
    std::vector<std::pair<uint8_t, uint8_t>> expandHand(const std::string& hand_str) const;
};

} // namespace poker
