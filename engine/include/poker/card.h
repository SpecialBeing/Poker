/// @file card.h
/// @brief Card and Deck classes with compact integer encoding.
///
/// Encoding: card_id = rank * 4 + suit
///   rank: 0 = 2, 1 = 3, ..., 12 = Ace
///   suit: 0 = clubs, 1 = diamonds, 2 = hearts, 3 = spades

#pragma once

#include "common.h"
#include <array>
#include <random>
#include <string>
#include <vector>
#include <cassert>

namespace poker {

class Card {
    uint8_t id_;   // 0–51

public:
    constexpr Card() : id_(0) {}
    constexpr explicit Card(uint8_t id) : id_(id) { }
    constexpr Card(Rank r, Suit s)
        : id_(static_cast<uint8_t>(r) * 4 + static_cast<uint8_t>(s)) {}

    /// Parse "As", "Td", "2c", etc.
    static Card fromString(const std::string& s);

    constexpr uint8_t id()   const { return id_; }
    constexpr Rank    rank() const { return static_cast<Rank>(id_ / 4); }
    constexpr Suit    suit() const { return static_cast<Suit>(id_ % 4); }
    constexpr int     rankInt() const { return id_ / 4; }
    constexpr int     suitInt() const { return id_ % 4; }

    std::string toString() const;

    constexpr bool operator==(Card o) const { return id_ == o.id_; }
    constexpr bool operator!=(Card o) const { return id_ != o.id_; }
    constexpr bool operator< (Card o) const { return id_ <  o.id_; }
};

// ── Deck ────────────────────────────────────────────────────

class Deck {
    std::array<uint8_t, NUM_CARDS> cards_;
    int size_ = NUM_CARDS;   // number of live cards
    int top_  = 0;           // next card to deal
    std::mt19937 rng_;

public:
    /// Construct a full 52-card deck.
    explicit Deck(uint32_t seed = std::random_device{}());

    /// Construct a deck with some cards already removed (dead cards).
    Deck(const std::vector<Card>& dead, uint32_t seed = std::random_device{}());

    void shuffle();
    Card deal();
    int  remaining() const { return size_ - top_; }

    /// Reset to full 52-card deck.
    void reset();

    /// Reset with dead cards removed.
    void reset(const std::vector<Card>& dead);
};

// ── Free Functions ──────────────────────────────────────────

/// Parse a string like "AsKh" or "As Kh" into a vector of Cards.
std::vector<Card> parseCards(const std::string& s);

/// Compute the canonical index for an unordered pair {c1, c2} in [0, 1325].
/// This maps every 2-card combo to a unique index without regard to order.
int holeCardIndex(Card c1, Card c2);

} // namespace poker
