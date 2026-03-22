#include "poker/card.h"
#include <algorithm>
#include <stdexcept>

namespace poker {

// ── Card ────────────────────────────────────────────────────

Card Card::fromString(const std::string& s) {
    if (s.size() < 2) throw std::invalid_argument("Card string too short: " + s);
    return Card(charToRank(s[0]), charToSuit(s[1]));
}

std::string Card::toString() const {
    return std::string(1, rankToChar(rank())) + suitToChar(suit());
}

// ── Deck ────────────────────────────────────────────────────

Deck::Deck(uint32_t seed) : rng_(seed) {
    reset();
}

Deck::Deck(const std::vector<Card>& dead, uint32_t seed) : rng_(seed) {
    reset(dead);
}

void Deck::shuffle() {
    std::shuffle(cards_.begin(), cards_.begin() + size_, rng_);
    top_ = 0;
}

Card Deck::deal() {
    if (top_ >= size_) throw std::runtime_error("Deck exhausted");
    return Card(cards_[top_++]);
}

void Deck::reset() {
    size_ = NUM_CARDS;
    top_ = 0;
    for (int i = 0; i < NUM_CARDS; ++i) cards_[i] = i;
}

void Deck::reset(const std::vector<Card>& dead) {
    uint64_t mask = 0;
    for (auto c : dead) mask |= (1ULL << c.id());

    size_ = 0;
    top_ = 0;
    for (int i = 0; i < NUM_CARDS; ++i) {
        if (!(mask & (1ULL << i))) {
            cards_[size_++] = i;
        }
    }
}

// ── Free Functions ──────────────────────────────────────────

std::vector<Card> parseCards(const std::string& s) {
    std::vector<Card> result;
    std::string clean;
    for (char c : s) {
        if (c != ' ') clean += c;
    }
    for (size_t i = 0; i + 1 < clean.size(); i += 2) {
        result.push_back(Card::fromString(clean.substr(i, 2)));
    }
    return result;
}

int holeCardIndex(Card c1, Card c2) {
    int a = c1.id(), b = c2.id();
    if (a > b) std::swap(a, b);
    return b * (b - 1) / 2 + a;
}

// ── Action ──────────────────────────────────────────────────

std::string Action::toString() const {
    switch (type) {
        case ActionType::FOLD:   return "Fold";
        case ActionType::CHECK:  return "Check";
        case ActionType::CALL:   return "Call";
        case ActionType::BET:    return "Bet(" + std::to_string(size) + ")";
        case ActionType::ALL_IN: return "All-In";
    }
    return "?";
}

} // namespace poker
