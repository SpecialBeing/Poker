"""CFR+ (Counterfactual Regret Minimization Plus) solver.

This implements the core GTO solving algorithm. CFR+ is a variant of CFR
that clips cumulative regrets to zero, converging faster in practice.

For tractability, we solve heads-up postflop spots with card abstraction.
"""

from __future__ import annotations

from collections import defaultdict
from typing import Dict, List, Optional, Tuple

import numpy as np
from tqdm import tqdm

from solver.core.hand_eval import HandEvaluator
from solver.core.constants import NUM_CARDS
from solver.engine.game_tree import (
    GameTreeBuilder, GameState, GameConfig, Action, ActionWithSize, Street,
)
from solver.engine.abstraction import CardAbstraction


class InfoSet:
    """Information set: what a player knows (their cards + public history)."""

    __slots__ = ('key', 'num_actions', 'cumulative_regret', 'cumulative_strategy', 'reach_sum')

    def __init__(self, key: str, num_actions: int):
        self.key = key
        self.num_actions = num_actions
        self.cumulative_regret = np.zeros(num_actions, dtype=np.float64)
        self.cumulative_strategy = np.zeros(num_actions, dtype=np.float64)
        self.reach_sum = 0.0

    def get_strategy(self) -> np.ndarray:
        """Get current strategy via regret matching."""
        positive = np.maximum(self.cumulative_regret, 0)
        total = positive.sum()
        if total > 0:
            return positive / total
        return np.ones(self.num_actions) / self.num_actions

    def get_average_strategy(self) -> np.ndarray:
        """Get the average strategy (converges to Nash equilibrium)."""
        total = self.cumulative_strategy.sum()
        if total > 0:
            return self.cumulative_strategy / total
        return np.ones(self.num_actions) / self.num_actions


class CFRSolver:
    """CFR+ solver for heads-up postflop spots."""

    def __init__(
        self,
        config: GameConfig,
        num_buckets: int = 10,
        ev_mode: str = 'chip',  # 'chip' or 'icm'
    ):
        self.config = config
        self.tree_builder = GameTreeBuilder(config)
        self.abstraction = CardAbstraction(num_buckets)
        self.evaluator = HandEvaluator()
        self.info_sets: Dict[str, InfoSet] = {}
        self.ev_mode = ev_mode
        self.iterations_done = 0

    def _get_info_set(self, key: str, num_actions: int) -> InfoSet:
        if key not in self.info_sets:
            self.info_sets[key] = InfoSet(key, num_actions)
        return self.info_sets[key]

    def _make_info_set_key(
        self,
        player: int,
        hole_cards: Tuple[int, int],
        board: List[int],
        history: List[Tuple[int, ActionWithSize]],
    ) -> str:
        """Create a unique key for this information set."""
        bucket = self.abstraction.get_bucket(hole_cards, board)
        history_str = '|'.join(f'{p}:{a}' for p, a in history)
        return f'P{player}:B{bucket}:{history_str}'

    def _terminal_payoff(
        self,
        state: GameState,
        hands: List[Tuple[int, int]],
    ) -> np.ndarray:
        """Calculate payoffs at a terminal node. Returns array of payoffs per player."""
        num_players = len(hands)
        total_pot = state.pot + sum(state.bets)
        payoffs = np.zeros(num_players)

        # How much each player has invested
        invested = [self.config.stack_size - state.stacks[i] for i in range(num_players)]

        active = state.active_players()

        if len(active) == 1:
            winner = active[0]
            payoffs[winner] = total_pot - invested[winner]
            for i in range(num_players):
                if i != winner:
                    payoffs[i] = -invested[i]
        else:
            # Showdown
            scores = {}
            for i in active:
                scores[i] = self.evaluator.evaluate(list(hands[i]) + state.board)

            best_score = max(scores.values())
            winners = [i for i, s in scores.items() if s == best_score]
            share = total_pot / len(winners)

            for i in range(num_players):
                if i in winners:
                    payoffs[i] = share - invested[i]
                else:
                    payoffs[i] = -invested[i]

        return payoffs

    def _cfr_recursive(
        self,
        state: GameState,
        hands: List[Tuple[int, int]],
        reach_probs: np.ndarray,
        iteration: int,
    ) -> np.ndarray:
        """Recursive CFR+ traversal. Returns expected payoff for each player."""
        if self.tree_builder.is_terminal(state):
            return self._terminal_payoff(state, hands)

        if self.tree_builder.is_round_over(state):
            if state.street == Street.RIVER:
                return self._terminal_payoff(state, hands)
            new_state = self.tree_builder.advance_street(state)
            return self._cfr_recursive(new_state, hands, reach_probs, iteration)

        player = state.current_player
        actions = self.tree_builder.get_available_actions(state)
        num_actions = len(actions)

        info_key = self._make_info_set_key(
            player, hands[player], state.board, state.history,
        )
        info_set = self._get_info_set(info_key, num_actions)
        strategy = info_set.get_strategy()

        action_payoffs = np.zeros((num_actions, len(hands)))
        node_payoff = np.zeros(len(hands))

        for i, action in enumerate(actions):
            new_state = self.tree_builder.apply_action(state, action)
            new_reach = reach_probs.copy()
            new_reach[player] *= strategy[i]

            action_payoffs[i] = self._cfr_recursive(
                new_state, hands, new_reach, iteration,
            )
            node_payoff += strategy[i] * action_payoffs[i]

        # Update regrets (CFR+: clip to zero)
        opp_reach = np.prod(reach_probs) / (reach_probs[player] + 1e-30)
        for i in range(num_actions):
            regret = action_payoffs[i][player] - node_payoff[player]
            info_set.cumulative_regret[i] = max(
                0, info_set.cumulative_regret[i] + opp_reach * regret
            )

        # Accumulate strategy weighted by reach probability
        info_set.cumulative_strategy += reach_probs[player] * strategy

        return node_payoff

    def solve(
        self,
        state: GameState,
        hands: List[Tuple[int, int]],
        num_iterations: int = 1000,
        show_progress: bool = True,
    ) -> Dict[str, np.ndarray]:
        """Run CFR+ for the given number of iterations.

        Returns dict mapping info set keys to average strategies.
        """
        num_players = len(hands)
        iterator = range(num_iterations)
        if show_progress:
            iterator = tqdm(iterator, desc='CFR+ Solving')

        for t in iterator:
            reach_probs = np.ones(num_players)
            self._cfr_recursive(state, hands, reach_probs, t)
            self.iterations_done += 1

        result = {}
        for key, info_set in self.info_sets.items():
            result[key] = info_set.get_average_strategy()

        return result

    def get_strategy_for_hand(
        self,
        player: int,
        hole_cards: Tuple[int, int],
        board: List[int],
        history: List[Tuple[int, ActionWithSize]],
    ) -> Optional[Dict[str, float]]:
        """Look up the solved strategy for a specific situation."""
        info_key = self._make_info_set_key(player, hole_cards, board, history)
        if info_key not in self.info_sets:
            return None

        info_set = self.info_sets[info_key]
        avg_strategy = info_set.get_average_strategy()

        # We need the actions list to label them
        state = self._reconstruct_state(history, board)
        if state is None:
            return None

        actions = self.tree_builder.get_available_actions(state)
        return {str(a): float(avg_strategy[i]) for i, a in enumerate(actions)}

    def _reconstruct_state(
        self,
        history: List[Tuple[int, ActionWithSize]],
        board: List[int],
    ) -> Optional[GameState]:
        """Reconstruct game state from action history."""
        state = self.tree_builder.create_initial_state(
            num_players=2,
            street=Street.FLOP if len(board) >= 3 else Street.PREFLOP,
            board=board,
        )
        for player, action in history:
            state = self.tree_builder.apply_action(state, action)
            if self.tree_builder.is_round_over(state):
                state = self.tree_builder.advance_street(state)
        return state


class PreflopSolver:
    """Solve preflop ranges for 6-max using simplified CFR.

    Rather than solving the full preflop tree (which is massive for 6 players),
    we use a bucket-based approach: hands are grouped by canonical type (169 types),
    and we solve for open-raise, 3-bet, and call frequencies from each position.
    """

    def __init__(self, config: GameConfig):
        self.config = config
        self.abstraction = CardAbstraction()
        self.positions = ['UTG', 'HJ', 'CO', 'BTN', 'SB', 'BB']
        # Strategy: position -> canonical hand -> {action: probability}
        self.strategies: Dict[str, Dict[str, Dict[str, float]]] = {}

    def _init_strategies(self):
        from solver.engine.abstraction import canonical_hole_cards, canonical_to_str
        hands = canonical_hole_cards()
        for pos in self.positions:
            self.strategies[pos] = {}
            for r1, r2, suited in hands:
                hand_str = canonical_to_str(r1, r2, suited)
                self.strategies[pos][hand_str] = {
                    'fold': 1.0 / 3,
                    'call': 1.0 / 3,
                    'raise': 1.0 / 3,
                }

    def solve_preflop(self, num_iterations: int = 5000, show_progress: bool = True):
        """Solve preflop opening ranges using regret matching.

        Simplified model: each position independently decides to fold/call/raise,
        with payoffs based on equity realization against a simplified opponent model.
        """
        from solver.engine.abstraction import canonical_hole_cards, canonical_to_str

        self._init_strategies()
        hands = canonical_hole_cards()

        # Regret accumulators: position -> hand -> action -> cumulative regret
        regrets: Dict[str, Dict[str, Dict[str, float]]] = {}
        strat_sum: Dict[str, Dict[str, Dict[str, float]]] = {}

        for pos in self.positions:
            regrets[pos] = {}
            strat_sum[pos] = {}
            for r1, r2, suited in hands:
                hand_str = canonical_to_str(r1, r2, suited)
                regrets[pos][hand_str] = {'fold': 0.0, 'call': 0.0, 'raise': 0.0}
                strat_sum[pos][hand_str] = {'fold': 0.0, 'call': 0.0, 'raise': 0.0}

        iterator = range(num_iterations)
        if show_progress:
            iterator = tqdm(iterator, desc='Preflop Solving')

        for _ in iterator:
            for pos_idx, pos in enumerate(self.positions):
                for r1, r2, suited in hands:
                    hand_str = canonical_to_str(r1, r2, suited)

                    # Current strategy from regret matching
                    reg = regrets[pos][hand_str]
                    pos_reg = {a: max(0, v) for a, v in reg.items()}
                    total = sum(pos_reg.values())
                    if total > 0:
                        strategy = {a: v / total for a, v in pos_reg.items()}
                    else:
                        strategy = {'fold': 1/3, 'call': 1/3, 'raise': 1/3}

                    # Simplified payoff model based on hand strength and position
                    from solver.core.constants import RANK_TO_INT
                    r1_int = RANK_TO_INT[r1]
                    r2_int = RANK_TO_INT[r2]
                    bucket = self.abstraction.preflop_bucket(
                        r1_int * 4, r2_int * 4 + (0 if suited else 1)
                    )
                    strength = (bucket + 0.5) / 10.0

                    position_bonus = pos_idx * 0.02
                    equity = min(1.0, strength + position_bonus)

                    # Payoffs (in BB)
                    payoff_fold = -self.config.big_blind if pos in ('SB', 'BB') else 0.0
                    payoff_call = (equity * 2 - 1) * 3.0 * self.config.big_blind
                    payoff_raise = (equity * 2 - 1) * 6.0 * self.config.big_blind

                    # Expected value
                    ev = (strategy['fold'] * payoff_fold +
                          strategy['call'] * payoff_call +
                          strategy['raise'] * payoff_raise)

                    # Update regrets (CFR+)
                    for a, p in [('fold', payoff_fold), ('call', payoff_call), ('raise', payoff_raise)]:
                        regrets[pos][hand_str][a] = max(0, regrets[pos][hand_str][a] + (p - ev))
                        strat_sum[pos][hand_str][a] += strategy[a]

        # Average strategies
        for pos in self.positions:
            for r1, r2, suited in hands:
                hand_str = canonical_to_str(r1, r2, suited)
                ss = strat_sum[pos][hand_str]
                total = sum(ss.values())
                if total > 0:
                    self.strategies[pos][hand_str] = {a: v / total for a, v in ss.items()}

    def get_range(self, position: str, action: str = 'raise', threshold: float = 0.5) -> List[str]:
        """Get hands that exceed the threshold for a given action."""
        if position not in self.strategies:
            return []
        result = []
        for hand, strat in self.strategies[position].items():
            if strat.get(action, 0) >= threshold:
                result.append(hand)
        return result

    def get_opening_range_pct(self, position: str) -> float:
        """Get the total opening (raise) percentage for a position."""
        if position not in self.strategies:
            return 0.0
        total = 0.0
        count = 0
        for hand, strat in self.strategies[position].items():
            total += strat.get('raise', 0) + strat.get('call', 0)
            count += 1
        return total / count * 100 if count > 0 else 0.0
