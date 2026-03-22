#include "poker/range.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>

namespace poker {

Range::Range() {
    weights_.fill(0.0);
}

void Range::setAll(double weight) {
    weights_.fill(weight);
}

void Range::setCombo(Card c1, Card c2, double weight) {
    int idx = holeCardIndex(c1, c2);
    weights_[idx] = weight;
}

void Range::setExact(Card c1, Card c2) {
    weights_.fill(0.0);
    setCombo(c1, c2, 1.0);
}

void Range::setHand(const std::string& hand_str, double weight) {
    auto combos = expandHand(hand_str);
    for (auto [c1, c2] : combos) {
        int idx = holeCardIndex(Card(c1), Card(c2));
        weights_[idx] = weight;
    }
}

double Range::getWeight(Card c1, Card c2) const {
    return weights_[holeCardIndex(c1, c2)];
}

int Range::numCombos() const {
    int n = 0;
    for (double w : weights_) if (w > 0) ++n;
    return n;
}

double Range::totalWeight() const {
    double s = 0;
    for (double w : weights_) s += w;
    return s;
}

void Range::removeDead(const std::vector<Card>& dead) {
    uint64_t mask = 0;
    for (auto c : dead) mask |= (1ULL << c.id());

    for (uint8_t a = 0; a < NUM_CARDS; ++a) {
        if (mask & (1ULL << a)) {
            for (uint8_t b = a + 1; b < NUM_CARDS; ++b) {
                weights_[holeCardIndex(Card(a), Card(b))] = 0;
            }
            for (uint8_t b = 0; b < a; ++b) {
                weights_[holeCardIndex(Card(b), Card(a))] = 0;
            }
        }
    }
}

void Range::forEachCombo(std::function<void(uint8_t, uint8_t, double)> fn) const {
    for (uint8_t b = 1; b < NUM_CARDS; ++b) {
        for (uint8_t a = 0; a < b; ++a) {
            int idx = b * (b - 1) / 2 + a;
            if (weights_[idx] > 0) {
                fn(a, b, weights_[idx]);
            }
        }
    }
}

// ── Range String Parsing ────────────────────────────────────

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

void Range::parse(const std::string& range_str) {
    std::stringstream ss(range_str);
    std::string token;

    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) continue;

        double weight = 1.0;
        auto colon = token.find(':');
        if (colon != std::string::npos) {
            weight = std::stod(token.substr(colon + 1));
            token = token.substr(0, colon);
        }

        auto dash = token.find('-');
        if (dash != std::string::npos) {
            // Range like "QQ-TT" or "ATs-A7s"
            std::string hi_str = token.substr(0, dash);
            std::string lo_str = token.substr(dash + 1);

            if (hi_str.size() == 2 && hi_str[0] == hi_str[1] &&
                lo_str.size() == 2 && lo_str[0] == lo_str[1]) {
                // Pair range: "QQ-TT"
                int hi_rank = static_cast<int>(charToRank(hi_str[0]));
                int lo_rank = static_cast<int>(charToRank(lo_str[0]));
                if (hi_rank < lo_rank) std::swap(hi_rank, lo_rank);
                for (int r = lo_rank; r <= hi_rank; ++r) {
                    std::string h(1, RANK_CHARS[r]);
                    h += RANK_CHARS[r];
                    setHand(h, weight);
                }
            }
            // Could extend for "ATs-A7s" etc. — left for future.
        } else {
            setHand(token, weight);
        }
    }
}

// ── Expand Canonical Hand ───────────────────────────────────

std::vector<std::pair<uint8_t, uint8_t>>
Range::expandHand(const std::string& hand_str) const {
    std::vector<std::pair<uint8_t, uint8_t>> combos;
    if (hand_str.size() < 2) return combos;

    int r1 = static_cast<int>(charToRank(hand_str[0]));
    int r2 = static_cast<int>(charToRank(hand_str[1]));

    if (hand_str.size() == 2) {
        // Pocket pair (e.g. "AA") or ambiguous — treat as pair if same rank
        if (r1 == r2) {
            for (int s1 = 0; s1 < NUM_SUITS; ++s1) {
                for (int s2 = s1 + 1; s2 < NUM_SUITS; ++s2) {
                    combos.push_back({
                        static_cast<uint8_t>(r1 * 4 + s1),
                        static_cast<uint8_t>(r2 * 4 + s2)
                    });
                }
            }
        } else {
            // All 16 combos (suited + offsuit)
            for (int s1 = 0; s1 < NUM_SUITS; ++s1) {
                for (int s2 = 0; s2 < NUM_SUITS; ++s2) {
                    combos.push_back({
                        static_cast<uint8_t>(r1 * 4 + s1),
                        static_cast<uint8_t>(r2 * 4 + s2)
                    });
                }
            }
        }
    } else {
        char qualifier = hand_str[2];
        if (qualifier == 's') {
            // Suited: same suit
            for (int s = 0; s < NUM_SUITS; ++s) {
                combos.push_back({
                    static_cast<uint8_t>(r1 * 4 + s),
                    static_cast<uint8_t>(r2 * 4 + s)
                });
            }
        } else if (qualifier == 'o') {
            // Offsuit: different suit
            for (int s1 = 0; s1 < NUM_SUITS; ++s1) {
                for (int s2 = 0; s2 < NUM_SUITS; ++s2) {
                    if (s1 != s2) {
                        combos.push_back({
                            static_cast<uint8_t>(r1 * 4 + s1),
                            static_cast<uint8_t>(r2 * 4 + s2)
                        });
                    }
                }
            }
        }
    }
    return combos;
}

// ── Grid Display ────────────────────────────────────────────

std::string Range::toGrid() const {
    // 13×13 grid: rows = first rank (A at top), cols = second rank
    // Upper triangle = suited, diagonal = pairs, lower triangle = offsuit
    std::ostringstream os;
    os << std::fixed << std::setprecision(0);
    os << "     ";
    for (int c = NUM_RANKS - 1; c >= 0; --c) {
        os << "  " << RANK_CHARS[c] << "  ";
    }
    os << "\n";

    for (int r = NUM_RANKS - 1; r >= 0; --r) {
        os << "  " << RANK_CHARS[r] << "  ";
        for (int c = NUM_RANKS - 1; c >= 0; --c) {
            std::string hand_str;
            if (r == c) {
                // Pair
                hand_str = std::string(1, RANK_CHARS[r]) + RANK_CHARS[c];
            } else if (r > c) {
                // r > c → suited (upper triangle in standard chart)
                hand_str = std::string(1, RANK_CHARS[r]) + RANK_CHARS[c] + "s";
            } else {
                // r < c → offsuit
                hand_str = std::string(1, RANK_CHARS[c]) + RANK_CHARS[r] + "o";
            }

            auto combos = expandHand(hand_str);
            double avg = 0;
            for (auto [c1, c2] : combos) {
                avg += weights_[holeCardIndex(Card(c1), Card(c2))];
            }
            if (!combos.empty()) avg /= combos.size();

            os << std::setw(4) << (avg * 100) << "%";
        }
        os << "\n";
    }
    return os.str();
}

} // namespace poker
