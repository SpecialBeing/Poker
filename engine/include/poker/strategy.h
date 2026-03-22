/// @file strategy.h
/// @brief Abstract strategy interface and CFR+ strategy design.
///
/// This file defines the extensible strategy hierarchy for the solver.
///
/// ┌──────────────────┐
/// │    Strategy       │  abstract base — any policy that maps info-sets to actions
/// └───────┬──────────┘
///         │
///    ┌────┴────┐
///    │         │
/// CFRStrategy  FixedStrategy
///    │              (for testing: always fold, always call, etc.)
///    │
///    ├── can have different BetSizingConfig
///    ├── can be trained for N iterations
///    ├── exposes per-iteration diffs
///    └── can be frozen (locked) for cross-strategy comparison
///
/// ── Key Design Decisions ──────────────────────────────────────────
///
/// 1. BetSizingConfig is PART OF the strategy, not the game.
///    Two strategies with different bet sizes produce different game trees.
///
/// 2. Strategies are compared via simulation:
///    Play strategy A (as P1) vs strategy B (as P2), average the EV.
///    When A's bet sizes ≠ B's, the acting player uses THEIR bet sizes.
///    The opponent maps to the "closest available" response action.
///
/// 3. Each CFR iteration produces an IterationDiff — what changed at
///    each information set. This enables the user to inspect convergence.
///
/// ── Future Extensions ─────────────────────────────────────────────
///
/// - Deep CFR: neural network function approximation for info-set values
/// - MCCFR variants: external sampling, outcome sampling
/// - Warm-starting: initialize from a simpler strategy

#pragma once

#include "common.h"
#include "game_state.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace poker {

/// Distribution over actions at a decision node.
struct ActionDistribution {
    std::vector<Action> actions;
    std::vector<double> probabilities;  // same length as actions, sums to 1

    /// Sample one action according to the distribution.
    Action sample(double uniform_random) const;

    std::string toString() const;
};

/// What changed in one info-set after a CFR iteration.
struct InfoSetDiff {
    std::string key;
    std::vector<double> old_strategy;
    std::vector<double> new_strategy;
    std::vector<double> regrets;        // current cumulative regrets
    double strategy_change;             // L1 distance between old and new
};

/// Summary of one CFR+ iteration.
struct IterationResult {
    int iteration_number;
    double exploitability_estimate;     // -1 if not computed
    int info_sets_visited;
    int info_sets_updated;
    std::vector<InfoSetDiff> top_changes;  // info sets with largest strategy change
};

/// Summary of a training run.
struct TrainingResult {
    int total_iterations;
    double final_exploitability;
    int total_info_sets;
    double elapsed_seconds;
};

// ═══════════════════════════════════════════════════════════════
//  Abstract Strategy
// ═══════════════════════════════════════════════════════════════

/// Abstract base class for any poker strategy.
///
/// A strategy maps (game state + player's hole cards) → action distribution.
/// Different subclasses can have different bet sizing, training methods, etc.
class Strategy {
public:
    virtual ~Strategy() = default;

    /// Get the action distribution at a decision point.
    /// @param state   current game state
    /// @param hand    the acting player's hole cards (2 cards)
    virtual ActionDistribution getStrategy(
        const GameState& state,
        const uint8_t hand[2]
    ) const = 0;

    /// Get the bet sizing config this strategy was built with.
    virtual const BetSizingConfig& getBetSizing() const = 0;

    /// Human-readable name for this strategy.
    virtual std::string name() const = 0;

    /// Save strategy to disk.
    virtual void save(const std::string& path) const = 0;

    /// Load strategy from disk.
    virtual void load(const std::string& path) = 0;
};

// ═══════════════════════════════════════════════════════════════
//  CFR+ Strategy (trainable)
// ═══════════════════════════════════════════════════════════════

/// A strategy trained via CFR+ (Counterfactual Regret Minimization Plus).
///
/// Stores per-info-set cumulative regrets and cumulative strategy.
/// The average strategy converges to Nash equilibrium as iterations → ∞.
///
/// Supports:
///   - Training for N iterations with per-iteration callbacks
///   - Freezing: lock the strategy so it stops updating (for matchups)
///   - Inspection: look up what the strategy does at any node
///   - Comparison: play against another strategy and measure EV
class CFRStrategy : public Strategy {
public:
    /// @param bet_config  bet sizing to use (determines the game tree)
    /// @param stack_bb    effective stack in big blinds
    /// @param name        human-readable name (e.g. "Strategy_A_quarter_half_pot")
    CFRStrategy(BetSizingConfig bet_config, double stack_bb, std::string name);

    // ── Strategy interface ──────────────────────────────────

    ActionDistribution getStrategy(
        const GameState& state,
        const uint8_t hand[2]
    ) const override;

    const BetSizingConfig& getBetSizing() const override { return bet_config_; }
    std::string name() const override { return name_; }
    void save(const std::string& path) const override;
    void load(const std::string& path) override;

    // ── Training ────────────────────────────────────────────

    /// Run one CFR+ iteration. Returns details of what changed.
    IterationResult iterate();

    /// Run N iterations with optional progress callback.
    /// Callback receives the IterationResult after each iteration.
    using ProgressCallback = std::function<void(const IterationResult&)>;
    TrainingResult train(int num_iterations, ProgressCallback cb = nullptr);

    /// Number of iterations completed so far.
    int iterationsDone() const { return iterations_done_; }

    /// Freeze: stop updating, use current average strategy.
    void freeze() { frozen_ = true; }
    bool isFrozen() const { return frozen_; }

    // ── Analysis ────────────────────────────────────────────

    /// Approximate exploitability (how far from Nash equilibrium).
    double computeExploitability() const;

    /// Total number of information sets discovered.
    int numInfoSets() const;

private:
    BetSizingConfig bet_config_;
    double stack_bb_;
    std::string name_;
    int iterations_done_ = 0;
    bool frozen_ = false;

    /// Per-info-set data: regrets and accumulated strategy.
    struct InfoSetData {
        int num_actions;
        std::vector<double> cumulative_regret;
        std::vector<double> cumulative_strategy;
        std::vector<Action> actions;

        /// Regret-matching: derive current strategy from regrets.
        std::vector<double> currentStrategy() const;

        /// Average strategy (converges to Nash).
        std::vector<double> averageStrategy() const;
    };

    std::unordered_map<std::string, InfoSetData> info_sets_;

    /// Build the info-set key for a player at a given state.
    std::string makeInfoSetKey(const GameState& state, const uint8_t hand[2]) const;

    /// Get or create info-set data.
    InfoSetData& getInfoSet(const std::string& key, const std::vector<Action>& actions);
};

// ═══════════════════════════════════════════════════════════════
//  Strategy Comparison
// ═══════════════════════════════════════════════════════════════

/// Compare two strategies head-to-head by simulating hands.
///
/// @param a          first strategy (plays as P1)
/// @param b          second strategy (plays as P2)
/// @param num_hands  number of hands to simulate
/// @param stack_bb   effective stack in big blinds
/// @return EV of strategy A in bb/hand (positive = A is better)
///
/// When strategies have different bet sizes, the acting player uses
/// THEIR bet sizes. The responding player maps to the closest action
/// in their own action space.
double compareStrategies(
    const Strategy& a, const Strategy& b,
    int num_hands, double stack_bb
);

} // namespace poker
