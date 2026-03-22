#include <gtest/gtest.h>
#include "poker/card.h"

using namespace poker;

// ── Card Encoding ───────────────────────────────────────────

TEST(Card, AceOfSpadesHasId51) {
    Card c(Rank::ACE, Suit::SPADES);
    EXPECT_EQ(c.id(), 51);
}

TEST(Card, TwoOfClubsHasId0) {
    Card c(Rank::TWO, Suit::CLUBS);
    EXPECT_EQ(c.id(), 0);
}

TEST(Card, IdEncodesRankTimesFourPlusSuit) {
    // Verify encoding for every card
    for (int r = 0; r < NUM_RANKS; ++r) {
        for (int s = 0; s < NUM_SUITS; ++s) {
            Card c(static_cast<Rank>(r), static_cast<Suit>(s));
            EXPECT_EQ(c.id(), r * 4 + s);
            EXPECT_EQ(c.rankInt(), r);
            EXPECT_EQ(c.suitInt(), s);
        }
    }
}

// ── Card Parsing ────────────────────────────────────────────

TEST(Card, FromStringParsesAceOfSpades) {
    Card c = Card::fromString("As");
    EXPECT_EQ(c.rank(), Rank::ACE);
    EXPECT_EQ(c.suit(), Suit::SPADES);
}

TEST(Card, FromStringParsesTenOfClubs) {
    Card c = Card::fromString("Tc");
    EXPECT_EQ(c.rank(), Rank::TEN);
    EXPECT_EQ(c.suit(), Suit::CLUBS);
}

TEST(Card, FromStringParsesDeuce) {
    Card c = Card::fromString("2d");
    EXPECT_EQ(c.rank(), Rank::TWO);
    EXPECT_EQ(c.suit(), Suit::DIAMONDS);
}

TEST(Card, ToStringRoundTrips) {
    for (int id = 0; id < NUM_CARDS; ++id) {
        Card c(static_cast<uint8_t>(id));
        Card parsed = Card::fromString(c.toString());
        EXPECT_EQ(c, parsed) << "Round-trip failed for card id " << id;
    }
}

TEST(Card, FromStringThrowsOnTooShort) {
    EXPECT_THROW(Card::fromString("A"), std::invalid_argument);
}

// ── Card Comparison ─────────────────────────────────────────

TEST(Card, EqualityOperator) {
    EXPECT_EQ(Card::fromString("As"), Card::fromString("As"));
    EXPECT_NE(Card::fromString("As"), Card::fromString("Ah"));
}

TEST(Card, LessThanFollowsId) {
    EXPECT_LT(Card::fromString("2c"), Card::fromString("As"));
    EXPECT_LT(Card::fromString("2c"), Card::fromString("2d"));
}

// ── parseCards ──────────────────────────────────────────────

TEST(ParseCards, ParsesMultipleCardsNoSpaces) {
    auto cards = parseCards("AsKhTc");
    ASSERT_EQ(cards.size(), 3u);
    EXPECT_EQ(cards[0], Card::fromString("As"));
    EXPECT_EQ(cards[1], Card::fromString("Kh"));
    EXPECT_EQ(cards[2], Card::fromString("Tc"));
}

TEST(ParseCards, ParsesWithSpaces) {
    auto cards = parseCards("As Kh Tc");
    ASSERT_EQ(cards.size(), 3u);
    EXPECT_EQ(cards[0], Card::fromString("As"));
}

TEST(ParseCards, EmptyStringReturnsEmpty) {
    auto cards = parseCards("");
    EXPECT_TRUE(cards.empty());
}

// ── holeCardIndex ───────────────────────────────────────────

TEST(HoleCardIndex, SymmetricInCardOrder) {
    Card a = Card::fromString("As");
    Card k = Card::fromString("Kh");
    EXPECT_EQ(holeCardIndex(a, k), holeCardIndex(k, a));
}

TEST(HoleCardIndex, AllIndicesUniqueAndInRange) {
    std::set<int> seen;
    for (uint8_t a = 0; a < NUM_CARDS; ++a) {
        for (uint8_t b = a + 1; b < NUM_CARDS; ++b) {
            int idx = holeCardIndex(Card(a), Card(b));
            EXPECT_GE(idx, 0);
            EXPECT_LT(idx, NUM_HOLE_COMBOS);
            seen.insert(idx);
        }
    }
    EXPECT_EQ(static_cast<int>(seen.size()), NUM_HOLE_COMBOS);
}

// ── Deck ────────────────────────────────────────────────────

TEST(Deck, FullDeckHas52Cards) {
    Deck d(42);
    EXPECT_EQ(d.remaining(), 52);
}

TEST(Deck, DealReducesRemaining) {
    Deck d(42);
    d.shuffle();
    d.deal();
    EXPECT_EQ(d.remaining(), 51);
}

TEST(Deck, DeckWithDeadCardsHasFewerCards) {
    std::vector<Card> dead = {Card::fromString("As"), Card::fromString("Kh")};
    Deck d(dead, 42);
    EXPECT_EQ(d.remaining(), 50);
}

TEST(Deck, ShuffleProducesDifferentOrders) {
    Deck d1(42);
    d1.shuffle();
    std::vector<uint8_t> order1;
    for (int i = 0; i < 5; ++i) order1.push_back(d1.deal().id());

    Deck d2(99);
    d2.shuffle();
    std::vector<uint8_t> order2;
    for (int i = 0; i < 5; ++i) order2.push_back(d2.deal().id());

    EXPECT_NE(order1, order2);
}
