#include "poker/equity_calculator.h"
#include <thread>
#include <mutex>
#include <random>
#include <algorithm>
#include <numeric>
#include <cassert>

namespace poker {

EquityCalculator::EquityCalculator(int num_threads)
    : num_threads_(num_threads > 0 ? num_threads
                                   : static_cast<int>(std::thread::hardware_concurrency()))
{
    if (num_threads_ < 1) num_threads_ = 1;
}

// ── Internal: get remaining cards ───────────────────────────

static std::vector<uint8_t> getRemainingCards(uint64_t dead_mask) {
    std::vector<uint8_t> remaining;
    remaining.reserve(52);
    for (int i = 0; i < NUM_CARDS; ++i) {
        if (!(dead_mask & (1ULL << i))) remaining.push_back(i);
    }
    return remaining;
}

static uint64_t buildDeadMask(const uint8_t hand_a[2], const uint8_t hand_b[2],
                               const std::vector<uint8_t>& board) {
    uint64_t mask = 0;
    mask |= (1ULL << hand_a[0]) | (1ULL << hand_a[1]);
    mask |= (1ULL << hand_b[0]) | (1ULL << hand_b[1]);
    for (auto c : board) mask |= (1ULL << c);
    return mask;
}

// ── Exhaustive Board Enumeration ────────────────────────────

EquityResult EquityCalculator::enumerateBoards(
    const uint8_t hand_a[2], const uint8_t hand_b[2],
    const std::vector<uint8_t>& board,
    const std::vector<uint8_t>& remaining
) const {
    int cards_needed = 5 - static_cast<int>(board.size());
    assert(cards_needed >= 0 && cards_needed <= 5);

    uint64_t wins = 0, losses = 0, ties = 0;
    int n = static_cast<int>(remaining.size());

    // Build base hand arrays (hole cards + known board)
    uint8_t base_a[7], base_b[7];
    base_a[0] = hand_a[0]; base_a[1] = hand_a[1];
    base_b[0] = hand_b[0]; base_b[1] = hand_b[1];
    for (size_t i = 0; i < board.size(); ++i) {
        base_a[2 + i] = board[i];
        base_b[2 + i] = board[i];
    }
    int base_len = 2 + static_cast<int>(board.size());

    if (cards_needed == 0) {
        // Full board known — just evaluate once
        HandRank ra = evaluator_.evaluate(base_a, base_len);
        HandRank rb = evaluator_.evaluate(base_b, base_len);
        if (ra > rb) wins = 1;
        else if (ra < rb) losses = 1;
        else ties = 1;

    } else if (cards_needed == 1) {
        // River card only
        for (int i = 0; i < n; ++i) {
            base_a[base_len] = remaining[i];
            base_b[base_len] = remaining[i];
            HandRank ra = evaluator_.evaluate(base_a, base_len + 1);
            HandRank rb = evaluator_.evaluate(base_b, base_len + 1);
            if (ra > rb) ++wins;
            else if (ra < rb) ++losses;
            else ++ties;
        }

    } else if (cards_needed == 2) {
        // Turn + river
        for (int i = 0; i < n; ++i) {
            base_a[base_len] = remaining[i];
            base_b[base_len] = remaining[i];
            for (int j = i + 1; j < n; ++j) {
                base_a[base_len + 1] = remaining[j];
                base_b[base_len + 1] = remaining[j];
                HandRank ra = evaluator_.evaluate(base_a, base_len + 2);
                HandRank rb = evaluator_.evaluate(base_b, base_len + 2);
                if (ra > rb) ++wins;
                else if (ra < rb) ++losses;
                else ++ties;
            }
        }

    } else if (cards_needed == 5) {
        // Preflop: enumerate all C(n,5) boards — can be huge, use multi-threading
        // For n=48: C(48,5) = 1,712,304 — manageable single-threaded
        std::mutex mtx;
        uint64_t total_w = 0, total_l = 0, total_t = 0;

        auto worker = [&](int start_i, int end_i) {
            uint8_t la[7], lb[7];
            la[0] = hand_a[0]; la[1] = hand_a[1];
            lb[0] = hand_b[0]; lb[1] = hand_b[1];
            uint64_t lw = 0, ll = 0, lt = 0;

            for (int i = start_i; i < end_i; ++i) {
                la[2] = remaining[i]; lb[2] = remaining[i];
                for (int j = i + 1; j < n; ++j) {
                    la[3] = remaining[j]; lb[3] = remaining[j];
                    for (int k = j + 1; k < n; ++k) {
                        la[4] = remaining[k]; lb[4] = remaining[k];
                        for (int l = k + 1; l < n; ++l) {
                            la[5] = remaining[l]; lb[5] = remaining[l];
                            for (int m = l + 1; m < n; ++m) {
                                la[6] = remaining[m]; lb[6] = remaining[m];
                                HandRank ra = evaluator_.evaluate(la, 7);
                                HandRank rb = evaluator_.evaluate(lb, 7);
                                if (ra > rb) ++lw;
                                else if (ra < rb) ++ll;
                                else ++lt;
                            }
                        }
                    }
                }
            }
            std::lock_guard<std::mutex> lock(mtx);
            total_w += lw; total_l += ll; total_t += lt;
        };

        // Distribute work across threads
        std::vector<std::thread> threads;
        int chunk = std::max(1, n / num_threads_);
        for (int t = 0; t < num_threads_; ++t) {
            int lo = t * chunk;
            int hi = (t == num_threads_ - 1) ? n : (t + 1) * chunk;
            if (lo >= n) break;
            threads.emplace_back(worker, lo, hi);
        }
        for (auto& t : threads) t.join();

        wins = total_w; losses = total_l; ties = total_t;

    } else {
        // cards_needed == 3 or 4: enumerate C(n, cards_needed)
        // For simplicity, handle 3 (flop → 3 remaining) — rarely called in practice
        // because we'd already know the flop.
        // Generic recursive enumeration:
        std::function<void(int, int, uint8_t*, uint8_t*)> enumerate;
        enumerate = [&](int depth, int start, uint8_t* a, uint8_t* b) {
            if (depth == cards_needed) {
                HandRank ra = evaluator_.evaluate(a, 7);
                HandRank rb = evaluator_.evaluate(b, 7);
                if (ra > rb) ++wins;
                else if (ra < rb) ++losses;
                else ++ties;
                return;
            }
            for (int i = start; i < n; ++i) {
                a[base_len + depth] = remaining[i];
                b[base_len + depth] = remaining[i];
                enumerate(depth + 1, i + 1, a, b);
            }
        };
        enumerate(0, 0, base_a, base_b);
    }

    uint64_t total = wins + losses + ties;
    if (total == 0) return {0.5, 0, 0, 0, 0};
    return {
        (wins + ties * 0.5) / total,
        static_cast<double>(wins) / total,
        static_cast<double>(ties) / total,
        static_cast<double>(losses) / total,
        total
    };
}

// ── Hand vs Hand ────────────────────────────────────────────

EquityResult EquityCalculator::handVsHand(
    Card h1a, Card h1b, Card h2a, Card h2b,
    const std::vector<Card>& board
) const {
    uint8_t ha[2] = {h1a.id(), h1b.id()};
    uint8_t hb[2] = {h2a.id(), h2b.id()};
    std::vector<uint8_t> board_ids;
    for (auto c : board) board_ids.push_back(c.id());

    uint64_t dead = buildDeadMask(ha, hb, board_ids);
    auto remaining = getRemainingCards(dead);

    return enumerateBoards(ha, hb, board_ids, remaining);
}

// ── Hand vs Range ───────────────────────────────────────────

EquityResult EquityCalculator::handVsRange(
    Card h1a, Card h1b,
    const Range& villain_range,
    const std::vector<Card>& board,
    int monte_carlo_samples
) const {
    uint8_t hero[2] = {h1a.id(), h1b.id()};
    std::vector<uint8_t> board_ids;
    for (auto c : board) board_ids.push_back(c.id());

    uint64_t hero_board_mask = (1ULL << hero[0]) | (1ULL << hero[1]);
    for (auto c : board_ids) hero_board_mask |= (1ULL << c);

    double total_eq = 0;
    double total_weight = 0;
    uint64_t total_matchups = 0;

    villain_range.forEachCombo([&](uint8_t vc1, uint8_t vc2, double w) {
        // Skip combos that conflict with hero or board
        if ((hero_board_mask >> vc1) & 1) return;
        if ((hero_board_mask >> vc2) & 1) return;

        uint8_t villain[2] = {vc1, vc2};
        uint64_t dead = hero_board_mask | (1ULL << vc1) | (1ULL << vc2);
        auto remaining = getRemainingCards(dead);

        if (monte_carlo_samples > 0) {
            // Monte Carlo for this matchup
            int cards_needed = 5 - static_cast<int>(board_ids.size());
            std::mt19937 rng(vc1 * 100 + vc2);
            int wins = 0, total = 0;

            int samples = std::max(1, monte_carlo_samples / std::max(1, villain_range.numCombos()));
            uint8_t base_a[7], base_b[7];
            base_a[0] = hero[0]; base_a[1] = hero[1];
            base_b[0] = vc1;     base_b[1] = vc2;
            for (size_t i = 0; i < board_ids.size(); ++i) {
                base_a[2 + i] = board_ids[i];
                base_b[2 + i] = board_ids[i];
            }
            int base_len = 2 + static_cast<int>(board_ids.size());

            for (int s = 0; s < samples; ++s) {
                // Shuffle and pick cards_needed
                std::shuffle(remaining.begin(), remaining.end(), rng);
                for (int k = 0; k < cards_needed; ++k) {
                    base_a[base_len + k] = remaining[k];
                    base_b[base_len + k] = remaining[k];
                }
                HandRank ra = evaluator_.evaluate(base_a, 7);
                HandRank rb = evaluator_.evaluate(base_b, 7);
                if (ra > rb) wins += 2;
                else if (ra == rb) wins += 1;
                total += 2;
            }
            total_eq += w * static_cast<double>(wins) / total;
            total_matchups += samples;
        } else {
            // Exhaustive
            auto result = enumerateBoards(hero, villain, board_ids, remaining);
            total_eq += w * result.equity;
            total_matchups += result.matchups_evaluated;
        }
        total_weight += w;
    });

    if (total_weight < 1e-9) return {0.5, 0, 0, 0, 0};
    double eq = total_eq / total_weight;
    return {eq, eq, 0, 1.0 - eq, total_matchups};
}

// ── Range vs Range ──────────────────────────────────────────

EquityResult EquityCalculator::rangeVsRange(
    const Range& range_a, const Range& range_b,
    const std::vector<Card>& board,
    int monte_carlo_samples
) const {
    std::vector<uint8_t> board_ids;
    for (auto c : board) board_ids.push_back(c.id());
    uint64_t board_mask = 0;
    for (auto c : board_ids) board_mask |= (1ULL << c);

    // Collect all valid combo pairs
    struct MatchupInfo { uint8_t a1, a2, b1, b2; double weight; };
    std::vector<MatchupInfo> matchups;
    double total_weight = 0;

    range_a.forEachCombo([&](uint8_t a1, uint8_t a2, double wa) {
        if ((board_mask >> a1) & 1) return;
        if ((board_mask >> a2) & 1) return;
        uint64_t a_mask = (1ULL << a1) | (1ULL << a2);

        range_b.forEachCombo([&](uint8_t b1, uint8_t b2, double wb) {
            if ((board_mask >> b1) & 1) return;
            if ((board_mask >> b2) & 1) return;
            if ((a_mask >> b1) & 1) return;
            if ((a_mask >> b2) & 1) return;

            double w = wa * wb;
            matchups.push_back({a1, a2, b1, b2, w});
            total_weight += w;
        });
    });

    if (matchups.empty()) return {0.5, 0, 0, 0, 0};

    // Monte Carlo: sample matchups weighted by their probability
    std::mt19937 rng(42);
    double eq_sum = 0;
    uint64_t total_evals = 0;
    int cards_needed = 5 - static_cast<int>(board_ids.size());

    for (int s = 0; s < monte_carlo_samples; ++s) {
        // Pick a random matchup (uniform for now; weighting improves accuracy)
        auto& m = matchups[rng() % matchups.size()];

        uint64_t dead = board_mask | (1ULL << m.a1) | (1ULL << m.a2)
                                   | (1ULL << m.b1) | (1ULL << m.b2);
        auto remaining = getRemainingCards(dead);

        std::shuffle(remaining.begin(), remaining.end(), rng);

        uint8_t all_a[7] = {m.a1, m.a2};
        uint8_t all_b[7] = {m.b1, m.b2};
        for (size_t i = 0; i < board_ids.size(); ++i) {
            all_a[2 + i] = board_ids[i];
            all_b[2 + i] = board_ids[i];
        }
        for (int k = 0; k < cards_needed; ++k) {
            all_a[2 + board_ids.size() + k] = remaining[k];
            all_b[2 + board_ids.size() + k] = remaining[k];
        }

        HandRank ra = evaluator_.evaluate(all_a, 7);
        HandRank rb = evaluator_.evaluate(all_b, 7);
        if (ra > rb) eq_sum += 1.0;
        else if (ra == rb) eq_sum += 0.5;
        ++total_evals;
    }

    double eq = eq_sum / total_evals;
    return {eq, eq, 0, 1.0 - eq, total_evals};
}

// ── All-In Street-by-Street Equity ──────────────────────────

StreetEquities EquityCalculator::allInEquityByStreet(
    Card h1a, Card h1b, Card h2a, Card h2b,
    const std::vector<Card>& known_board
) const {
    StreetEquities result;

    // Start with whatever we know and fill in equity at each stage
    auto board_ids = [&]() {
        std::vector<Card> v;
        for (auto c : known_board) v.push_back(c);
        return v;
    }();

    // Preflop equity (if board is empty)
    if (known_board.empty()) {
        auto eq = handVsHand(h1a, h1b, h2a, h2b, {});
        result.preflop = eq.equity;
    }

    // Flop equity (if board has >= 3 cards)
    if (known_board.size() >= 3) {
        std::vector<Card> flop(known_board.begin(), known_board.begin() + 3);
        auto eq = handVsHand(h1a, h1b, h2a, h2b, flop);
        result.flop = eq.equity;
    }

    // Turn equity (if board has >= 4 cards)
    if (known_board.size() >= 4) {
        std::vector<Card> turn(known_board.begin(), known_board.begin() + 4);
        auto eq = handVsHand(h1a, h1b, h2a, h2b, turn);
        result.turn = eq.equity;
    }

    // River equity (if board has 5 cards)
    if (known_board.size() >= 5) {
        auto eq = handVsHand(h1a, h1b, h2a, h2b, known_board);
        result.river = eq.equity;
    }

    return result;
}

} // namespace poker
