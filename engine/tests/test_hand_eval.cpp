#include <gtest/gtest.h>
#include "poker/hand_evaluator.h"
#include <chrono>
#include <iostream>

using namespace poker;

class HandEvalTest : public ::testing::Test {
protected:
    HandEvaluator eval;

    // Helper: build 5-card hand from rank/suit pairs
    static void makeHand(uint8_t out[5],
                         Rank r0, Suit s0, Rank r1, Suit s1,
                         Rank r2, Suit s2, Rank r3, Suit s3,
                         Rank r4, Suit s4) {
        out[0] = Card(r0, s0).id();
        out[1] = Card(r1, s1).id();
        out[2] = Card(r2, s2).id();
        out[3] = Card(r3, s3).id();
        out[4] = Card(r4, s4).id();
    }
};

// ── Category Detection ──────────────────────────────────────

TEST_F(HandEvalTest, DetectsStraightFlush) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::SPADES, Rank::KING, Suit::SPADES,
             Rank::QUEEN, Suit::SPADES, Rank::JACK, Suit::SPADES,
             Rank::TEN, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::STRAIGHT_FLUSH);
}

TEST_F(HandEvalTest, DetectsFourOfAKind) {
    uint8_t h[5];
    makeHand(h, Rank::NINE, Suit::SPADES, Rank::NINE, Suit::HEARTS,
             Rank::NINE, Suit::DIAMONDS, Rank::NINE, Suit::CLUBS,
             Rank::ACE, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::FOUR_OF_A_KIND);
}

TEST_F(HandEvalTest, DetectsFullHouse) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::SPADES, Rank::ACE, Suit::HEARTS,
             Rank::ACE, Suit::DIAMONDS, Rank::KING, Suit::SPADES,
             Rank::KING, Suit::HEARTS);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::FULL_HOUSE);
}

TEST_F(HandEvalTest, DetectsFlush) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::HEARTS, Rank::JACK, Suit::HEARTS,
             Rank::EIGHT, Suit::HEARTS, Rank::FIVE, Suit::HEARTS,
             Rank::TWO, Suit::HEARTS);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::FLUSH);
}

TEST_F(HandEvalTest, DetectsStraight) {
    uint8_t h[5];
    makeHand(h, Rank::NINE, Suit::SPADES, Rank::EIGHT, Suit::HEARTS,
             Rank::SEVEN, Suit::DIAMONDS, Rank::SIX, Suit::CLUBS,
             Rank::FIVE, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::STRAIGHT);
}

TEST_F(HandEvalTest, DetectsWheelStraight) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::SPADES, Rank::TWO, Suit::HEARTS,
             Rank::THREE, Suit::DIAMONDS, Rank::FOUR, Suit::CLUBS,
             Rank::FIVE, Suit::HEARTS);
    auto rank = eval.evaluate5(h);
    EXPECT_EQ(rank.category(), HandCategory::STRAIGHT);
}

TEST_F(HandEvalTest, DetectsThreeOfAKind) {
    uint8_t h[5];
    makeHand(h, Rank::SEVEN, Suit::SPADES, Rank::SEVEN, Suit::HEARTS,
             Rank::SEVEN, Suit::DIAMONDS, Rank::KING, Suit::CLUBS,
             Rank::TWO, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::THREE_OF_A_KIND);
}

TEST_F(HandEvalTest, DetectsTwoPair) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::SPADES, Rank::ACE, Suit::HEARTS,
             Rank::KING, Suit::DIAMONDS, Rank::KING, Suit::CLUBS,
             Rank::QUEEN, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::TWO_PAIR);
}

TEST_F(HandEvalTest, DetectsPair) {
    uint8_t h[5];
    makeHand(h, Rank::TEN, Suit::SPADES, Rank::TEN, Suit::HEARTS,
             Rank::ACE, Suit::DIAMONDS, Rank::KING, Suit::CLUBS,
             Rank::QUEEN, Suit::SPADES);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::PAIR);
}

TEST_F(HandEvalTest, DetectsHighCard) {
    uint8_t h[5];
    makeHand(h, Rank::ACE, Suit::SPADES, Rank::JACK, Suit::HEARTS,
             Rank::EIGHT, Suit::DIAMONDS, Rank::FIVE, Suit::CLUBS,
             Rank::TWO, Suit::HEARTS);
    EXPECT_EQ(eval.evaluate5(h).category(), HandCategory::HIGH_CARD);
}

// ── Ordering ────────────────────────────────────────────────

TEST_F(HandEvalTest, StraightFlushBeatsQuads) {
    uint8_t sf[5], quads[5];
    makeHand(sf, Rank::NINE, Suit::HEARTS, Rank::EIGHT, Suit::HEARTS,
             Rank::SEVEN, Suit::HEARTS, Rank::SIX, Suit::HEARTS,
             Rank::FIVE, Suit::HEARTS);
    makeHand(quads, Rank::ACE, Suit::SPADES, Rank::ACE, Suit::HEARTS,
             Rank::ACE, Suit::DIAMONDS, Rank::ACE, Suit::CLUBS,
             Rank::KING, Suit::SPADES);
    EXPECT_GT(eval.evaluate5(sf), eval.evaluate5(quads));
}

TEST_F(HandEvalTest, HigherPairBeatsLowerPair) {
    uint8_t aa[5], kk[5];
    makeHand(aa, Rank::ACE, Suit::SPADES, Rank::ACE, Suit::HEARTS,
             Rank::FIVE, Suit::DIAMONDS, Rank::THREE, Suit::CLUBS,
             Rank::TWO, Suit::HEARTS);
    makeHand(kk, Rank::KING, Suit::SPADES, Rank::KING, Suit::HEARTS,
             Rank::QUEEN, Suit::DIAMONDS, Rank::JACK, Suit::CLUBS,
             Rank::TEN, Suit::HEARTS);
    EXPECT_GT(eval.evaluate5(aa), eval.evaluate5(kk));
}

TEST_F(HandEvalTest, PairBeatsHighCard) {
    uint8_t pair[5], high[5];
    makeHand(pair, Rank::TWO, Suit::SPADES, Rank::TWO, Suit::HEARTS,
             Rank::THREE, Suit::DIAMONDS, Rank::FOUR, Suit::CLUBS,
             Rank::FIVE, Suit::HEARTS);
    makeHand(high, Rank::ACE, Suit::SPADES, Rank::KING, Suit::HEARTS,
             Rank::QUEEN, Suit::DIAMONDS, Rank::JACK, Suit::CLUBS,
             Rank::NINE, Suit::HEARTS);
    EXPECT_GT(eval.evaluate5(pair), eval.evaluate5(high));
}

TEST_F(HandEvalTest, WheelLosesToSixHighStraight) {
    uint8_t wheel[5], six_high[5];
    makeHand(wheel, Rank::ACE, Suit::SPADES, Rank::TWO, Suit::HEARTS,
             Rank::THREE, Suit::DIAMONDS, Rank::FOUR, Suit::CLUBS,
             Rank::FIVE, Suit::HEARTS);
    makeHand(six_high, Rank::TWO, Suit::SPADES, Rank::THREE, Suit::HEARTS,
             Rank::FOUR, Suit::DIAMONDS, Rank::FIVE, Suit::CLUBS,
             Rank::SIX, Suit::HEARTS);
    EXPECT_LT(eval.evaluate5(wheel), eval.evaluate5(six_high));
}

TEST_F(HandEvalTest, KickerBreaksTie) {
    // Pair of aces, king kicker vs pair of aces, queen kicker
    uint8_t ak[5], aq[5];
    makeHand(ak, Rank::ACE, Suit::SPADES, Rank::ACE, Suit::HEARTS,
             Rank::KING, Suit::DIAMONDS, Rank::FIVE, Suit::CLUBS,
             Rank::THREE, Suit::HEARTS);
    makeHand(aq, Rank::ACE, Suit::DIAMONDS, Rank::ACE, Suit::CLUBS,
             Rank::QUEEN, Suit::SPADES, Rank::FIVE, Suit::HEARTS,
             Rank::THREE, Suit::DIAMONDS);
    EXPECT_GT(eval.evaluate5(ak), eval.evaluate5(aq));
}

TEST_F(HandEvalTest, IdenticalHandsTie) {
    // Same ranks, different suits → should tie
    uint8_t h1[5], h2[5];
    makeHand(h1, Rank::ACE, Suit::SPADES, Rank::KING, Suit::HEARTS,
             Rank::QUEEN, Suit::DIAMONDS, Rank::JACK, Suit::CLUBS,
             Rank::NINE, Suit::SPADES);
    makeHand(h2, Rank::ACE, Suit::HEARTS, Rank::KING, Suit::DIAMONDS,
             Rank::QUEEN, Suit::CLUBS, Rank::JACK, Suit::SPADES,
             Rank::NINE, Suit::HEARTS);
    EXPECT_EQ(eval.evaluate5(h1), eval.evaluate5(h2));
}

// ── 7-Card Evaluation ───────────────────────────────────────

TEST_F(HandEvalTest, SevenCardFindsRoyalFlush) {
    uint8_t h[7] = {
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::KING, Suit::SPADES).id(),
        Card(Rank::QUEEN, Suit::SPADES).id(),
        Card(Rank::JACK, Suit::SPADES).id(),
        Card(Rank::TEN, Suit::SPADES).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
        Card(Rank::THREE, Suit::DIAMONDS).id(),
    };
    EXPECT_EQ(eval.evaluate7(h).category(), HandCategory::STRAIGHT_FLUSH);
}

TEST_F(HandEvalTest, SevenCardFindsHiddenFullHouse) {
    // Board: A K K 7 2, hand: A A → full house AAA KK
    uint8_t h[7] = {
        Card(Rank::ACE, Suit::HEARTS).id(),
        Card(Rank::ACE, Suit::DIAMONDS).id(),
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::KING, Suit::HEARTS).id(),
        Card(Rank::KING, Suit::DIAMONDS).id(),
        Card(Rank::SEVEN, Suit::CLUBS).id(),
        Card(Rank::TWO, Suit::SPADES).id(),
    };
    EXPECT_EQ(eval.evaluate7(h).category(), HandCategory::FULL_HOUSE);
}

TEST_F(HandEvalTest, CompareReturnsCorrectResults) {
    uint8_t aa[2] = {Card(Rank::ACE, Suit::SPADES).id(), Card(Rank::ACE, Suit::HEARTS).id()};
    uint8_t kk[2] = {Card(Rank::KING, Suit::SPADES).id(), Card(Rank::KING, Suit::HEARTS).id()};
    uint8_t board[5] = {
        Card(Rank::TWO, Suit::CLUBS).id(),
        Card(Rank::FIVE, Suit::DIAMONDS).id(),
        Card(Rank::EIGHT, Suit::HEARTS).id(),
        Card(Rank::JACK, Suit::SPADES).id(),
        Card(Rank::THREE, Suit::CLUBS).id(),
    };
    EXPECT_EQ(eval.compare(aa, kk, board, 5), 1);   // AA wins
    EXPECT_EQ(eval.compare(kk, aa, board, 5), -1);   // KK loses
}

// ── Performance ─────────────────────────────────────────────

TEST_F(HandEvalTest, Benchmark_1M_FiveCardEvals) {
    uint8_t h[5] = {
        Card(Rank::ACE, Suit::SPADES).id(),
        Card(Rank::ACE, Suit::HEARTS).id(),
        Card(Rank::FIVE, Suit::DIAMONDS).id(),
        Card(Rank::THREE, Suit::CLUBS).id(),
        Card(Rank::TWO, Suit::HEARTS).id(),
    };
    auto start = std::chrono::high_resolution_clock::now();
    volatile uint32_t sink = 0;
    for (int i = 0; i < 1000000; ++i) {
        h[4] = static_cast<uint8_t>(i % 40);
        sink += eval.evaluate5(h).value;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double evals_per_sec = 1e6 / ms * 1000;
    std::cout << "[BENCHMARK] 1M 5-card evals: " << ms << " ms ("
              << evals_per_sec / 1e6 << "M evals/sec)" << std::endl;
    EXPECT_LT(ms, 500) << "5-card evaluator too slow";
}
