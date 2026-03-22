#include "poker/hand_evaluator.h"
#include <algorithm>
#include <cassert>

namespace poker {

// ── Initialization ──────────────────────────────────────────

HandEvaluator::HandEvaluator() {
    initStraightTable();
}

void HandEvaluator::initStraightTable() {
    straight_table_.fill(-1);

    // A straight is 5 consecutive rank bits set.
    // Rank bits: bit 0 = 2, bit 1 = 3, ..., bit 12 = Ace
    // Special case: A-2-3-4-5 (wheel) = bits {12, 0, 1, 2, 3}
    for (int high = 4; high <= 12; ++high) {
        uint16_t mask = 0;
        for (int i = 0; i < 5; ++i) mask |= (1 << (high - i));
        straight_table_[mask] = high;
    }
    // Wheel: A(12)-2(0)-3(1)-4(2)-5(3) → high card is 3 (the Five)
    uint16_t wheel = (1 << 12) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
    straight_table_[wheel] = 3;

    // Also mark subsets that contain a straight (for hands with >5 unique ranks,
    // we'll extract the 5-card subset when evaluating). But for 5-card eval,
    // we only have exactly 5 unique ranks for a straight, so the direct bitmask works.
}

// ── 5-Card Evaluation ───────────────────────────────────────

HandRank HandEvaluator::evaluate5(const uint8_t cards[5]) const {
    // Extract ranks and suits
    int ranks[5], suits[5];
    int rank_count[NUM_RANKS] = {};
    int suit_count[NUM_SUITS] = {};
    uint16_t rank_mask = 0;

    for (int i = 0; i < 5; ++i) {
        ranks[i] = cards[i] / 4;
        suits[i] = cards[i] % 4;
        rank_count[ranks[i]]++;
        suit_count[suits[i]]++;
        rank_mask |= (1 << ranks[i]);
    }

    bool is_flush = (suit_count[0] == 5 || suit_count[1] == 5 ||
                     suit_count[2] == 5 || suit_count[3] == 5);

    int8_t straight_high = straight_table_[rank_mask];
    bool is_straight = (straight_high >= 0);

    // Classify by rank pattern.
    // Build sorted groups: (count, rank) sorted by (count desc, rank desc).
    struct Group { int count; int rank; };
    Group groups[5];
    int ng = 0;
    for (int r = NUM_RANKS - 1; r >= 0; --r) {
        if (rank_count[r] > 0) {
            groups[ng++] = {rank_count[r], r};
        }
    }
    // Sort by count descending, then rank descending
    std::sort(groups, groups + ng, [](const Group& a, const Group& b) {
        return a.count != b.count ? a.count > b.count : a.rank > b.rank;
    });

    uint32_t value = 0;

    if (is_straight && is_flush) {
        // Straight Flush
        value = (8u << 24) | (static_cast<uint32_t>(straight_high) << 16);

    } else if (groups[0].count == 4) {
        // Four of a Kind
        value = (7u << 24)
              | (static_cast<uint32_t>(groups[0].rank) << 16)
              | (static_cast<uint32_t>(groups[1].rank) << 12);

    } else if (groups[0].count == 3 && groups[1].count == 2) {
        // Full House
        value = (6u << 24)
              | (static_cast<uint32_t>(groups[0].rank) << 16)
              | (static_cast<uint32_t>(groups[1].rank) << 12);

    } else if (is_flush) {
        // Flush — use all 5 ranks as kickers
        // Sort ranks descending
        int sorted[5];
        for (int i = 0; i < 5; ++i) sorted[i] = ranks[i];
        std::sort(sorted, sorted + 5, std::greater<int>());
        value = (5u << 24);
        for (int i = 0; i < 5; ++i) {
            value |= (static_cast<uint32_t>(sorted[i]) << (16 - i * 4));
        }

    } else if (is_straight) {
        // Straight
        value = (4u << 24) | (static_cast<uint32_t>(straight_high) << 16);

    } else if (groups[0].count == 3) {
        // Three of a Kind
        value = (3u << 24)
              | (static_cast<uint32_t>(groups[0].rank) << 16)
              | (static_cast<uint32_t>(groups[1].rank) << 12)
              | (static_cast<uint32_t>(groups[2].rank) << 8);

    } else if (groups[0].count == 2 && groups[1].count == 2) {
        // Two Pair
        value = (2u << 24)
              | (static_cast<uint32_t>(groups[0].rank) << 16)
              | (static_cast<uint32_t>(groups[1].rank) << 12)
              | (static_cast<uint32_t>(groups[2].rank) << 8);

    } else if (groups[0].count == 2) {
        // One Pair
        value = (1u << 24)
              | (static_cast<uint32_t>(groups[0].rank) << 16)
              | (static_cast<uint32_t>(groups[1].rank) << 12)
              | (static_cast<uint32_t>(groups[2].rank) << 8)
              | (static_cast<uint32_t>(groups[3].rank) << 4);

    } else {
        // High Card
        int sorted[5];
        for (int i = 0; i < 5; ++i) sorted[i] = ranks[i];
        std::sort(sorted, sorted + 5, std::greater<int>());
        value = 0;
        for (int i = 0; i < 5; ++i) {
            value |= (static_cast<uint32_t>(sorted[i]) << (16 - i * 4));
        }
    }

    return {value};
}

// ── 7-Card Evaluation ───────────────────────────────────────

// Precomputed C(7,5) = 21 combinations of 5 indices from 7.
static constexpr int COMBO_7_5[21][5] = {
    {0,1,2,3,4}, {0,1,2,3,5}, {0,1,2,3,6}, {0,1,2,4,5}, {0,1,2,4,6},
    {0,1,2,5,6}, {0,1,3,4,5}, {0,1,3,4,6}, {0,1,3,5,6}, {0,1,4,5,6},
    {0,2,3,4,5}, {0,2,3,4,6}, {0,2,3,5,6}, {0,2,4,5,6}, {0,3,4,5,6},
    {1,2,3,4,5}, {1,2,3,4,6}, {1,2,3,5,6}, {1,2,4,5,6}, {1,3,4,5,6},
    {2,3,4,5,6},
};

HandRank HandEvaluator::evaluate7(const uint8_t cards[7]) const {
    HandRank best{0};
    uint8_t sub[5];
    for (auto& combo : COMBO_7_5) {
        for (int i = 0; i < 5; ++i) sub[i] = cards[combo[i]];
        HandRank r = evaluate5(sub);
        if (r > best) best = r;
    }
    return best;
}

// ── General Evaluation ──────────────────────────────────────

HandRank HandEvaluator::evaluate(const uint8_t* cards, int n) const {
    assert(n >= 5 && n <= 7);
    if (n == 5) return evaluate5(cards);
    if (n == 7) return evaluate7(cards);

    // n == 6: C(6,5) = 6 combinations
    HandRank best{0};
    uint8_t sub[5];
    for (int skip = 0; skip < 6; ++skip) {
        int j = 0;
        for (int i = 0; i < 6; ++i) {
            if (i != skip) sub[j++] = cards[i];
        }
        HandRank r = evaluate5(sub);
        if (r > best) best = r;
    }
    return best;
}

// ── Compare ─────────────────────────────────────────────────

int HandEvaluator::compare(
    const uint8_t hand_a[2], const uint8_t hand_b[2],
    const uint8_t* board, int board_len
) const {
    uint8_t all_a[7], all_b[7];
    all_a[0] = hand_a[0]; all_a[1] = hand_a[1];
    all_b[0] = hand_b[0]; all_b[1] = hand_b[1];
    for (int i = 0; i < board_len; ++i) {
        all_a[2 + i] = board[i];
        all_b[2 + i] = board[i];
    }
    int total = 2 + board_len;
    HandRank ra = evaluate(all_a, total);
    HandRank rb = evaluate(all_b, total);
    if (ra > rb) return 1;
    if (ra < rb) return -1;
    return 0;
}

// ── HandRank::toString ──────────────────────────────────────

std::string HandRank::toString() const {
    return categoryToString(category());
}

} // namespace poker
