#include <gtest/gtest.h>
#include "poker/range.h"

using namespace poker;

// ── Parsing ─────────────────────────────────────────────────

TEST(Range, ParseAA_Returns6Combos) {
    Range r;
    r.parse("AA");
    EXPECT_EQ(r.numCombos(), 6);  // C(4,2) = 6 ways to pick 2 aces
}

TEST(Range, ParseAKs_Returns4Combos) {
    Range r;
    r.parse("AKs");
    EXPECT_EQ(r.numCombos(), 4);  // 4 suits
}

TEST(Range, ParseAKo_Returns12Combos) {
    Range r;
    r.parse("AKo");
    EXPECT_EQ(r.numCombos(), 12);  // 4*4 - 4 suited = 12
}

TEST(Range, ParseMultipleHands) {
    Range r;
    r.parse("AA, KK, QQ, AKs, AKo");
    EXPECT_EQ(r.numCombos(), 6 + 6 + 6 + 4 + 12);  // 34
}

TEST(Range, ParsePairRange_QQtoTT) {
    Range r;
    r.parse("QQ-TT");
    EXPECT_EQ(r.numCombos(), 6 + 6 + 6);  // QQ, JJ, TT = 18
}

TEST(Range, ParseWithWeight) {
    Range r;
    r.parse("AKs:0.5");
    EXPECT_EQ(r.numCombos(), 4);
    // Each combo should have weight 0.5
    Card as = Card::fromString("As");
    Card ks = Card::fromString("Ks");
    EXPECT_DOUBLE_EQ(r.getWeight(as, ks), 0.5);
}

TEST(Range, ParseEmptyString_NoError) {
    Range r;
    r.parse("");
    EXPECT_EQ(r.numCombos(), 0);
}

// ── Set Operations ──────────────────────────────────────────

TEST(Range, SetAll_1326Combos) {
    Range r;
    r.setAll(1.0);
    EXPECT_EQ(r.numCombos(), NUM_HOLE_COMBOS);
}

TEST(Range, SetAll_TotalWeight) {
    Range r;
    r.setAll(0.5);
    EXPECT_DOUBLE_EQ(r.totalWeight(), NUM_HOLE_COMBOS * 0.5);
}

TEST(Range, SetCombo_SingleCard) {
    Range r;
    Card kd = Card::fromString("Kd");
    Card tc = Card::fromString("3c");
    r.setCombo(kd, tc, 0.75);
    EXPECT_EQ(r.numCombos(), 1);
    EXPECT_DOUBLE_EQ(r.getWeight(kd, tc), 0.75);
    EXPECT_DOUBLE_EQ(r.getWeight(tc, kd), 0.75);  // order shouldn't matter
}

TEST(Range, SetExact_OnlyOneCombo) {
    Range r;
    r.setAll(1.0);  // start with everything
    Card c1 = Card::fromString("Kd");
    Card c2 = Card::fromString("3c");
    r.setExact(c1, c2);
    EXPECT_EQ(r.numCombos(), 1);
    EXPECT_DOUBLE_EQ(r.getWeight(c1, c2), 1.0);
}

// ── Dead Card Removal ───────────────────────────────────────

TEST(Range, RemoveDead_ReducesCombos) {
    Range r;
    r.parse("AA");
    EXPECT_EQ(r.numCombos(), 6);

    r.removeDead({Card::fromString("As")});
    EXPECT_EQ(r.numCombos(), 3);  // 3 combos with As removed
}

TEST(Range, RemoveDead_MultipleCards) {
    Range r;
    r.parse("AA");
    r.removeDead({Card::fromString("As"), Card::fromString("Ah")});
    EXPECT_EQ(r.numCombos(), 1);  // only AdAc left
}

TEST(Range, RemoveDead_EmptyList_NoChange) {
    Range r;
    r.parse("AA");
    r.removeDead({});
    EXPECT_EQ(r.numCombos(), 6);
}

// ── ForEachCombo ────────────────────────────────────────────

TEST(Range, ForEachCombo_VisitsAllNonzero) {
    Range r;
    r.parse("AKs");  // 4 combos
    int count = 0;
    r.forEachCombo([&](uint8_t, uint8_t, double w) {
        EXPECT_GT(w, 0.0);
        ++count;
    });
    EXPECT_EQ(count, 4);
}

TEST(Range, ForEachCombo_EmptyRange_VisitsNothing) {
    Range r;
    int count = 0;
    r.forEachCombo([&](uint8_t, uint8_t, double) { ++count; });
    EXPECT_EQ(count, 0);
}

// ── SetHand ─────────────────────────────────────────────────

TEST(Range, SetHand_PocketPair) {
    Range r;
    r.setHand("77", 1.0);
    EXPECT_EQ(r.numCombos(), 6);
}

TEST(Range, SetHand_Suited) {
    Range r;
    r.setHand("T9s", 1.0);
    EXPECT_EQ(r.numCombos(), 4);
}

TEST(Range, SetHand_Offsuit) {
    Range r;
    r.setHand("T9o", 1.0);
    EXPECT_EQ(r.numCombos(), 12);
}
