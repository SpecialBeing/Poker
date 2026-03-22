"""Game tree representation for No-Limit Hold'em.

The tree has three node types:
  - ActionNode: a player chooses fold/call/raise
  - ChanceNode: community cards are dealt
  - TerminalNode: hand is over, payoffs are determined
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Dict, List, Optional, Tuple


class Action(Enum):
    FOLD = auto()
    CHECK = auto()
    CALL = auto()
    BET = auto()    # covers both bet and raise
    ALL_IN = auto()

    def __repr__(self) -> str:
        return self.name


@dataclass
class ActionWithSize:
    """An action paired with its bet size (0 for fold/check/call)."""
    action: Action
    size: float = 0.0

    def __repr__(self) -> str:
        if self.action in (Action.BET, Action.ALL_IN):
            return f'{self.action.name}({self.size:.0f})'
        return self.action.name

    def __hash__(self) -> int:
        return hash((self.action, round(self.size, 2)))

    def __eq__(self, other) -> bool:
        if not isinstance(other, ActionWithSize):
            return NotImplemented
        return self.action == other.action and abs(self.size - other.size) < 0.01


class Street(Enum):
    PREFLOP = 0
    FLOP = 1
    TURN = 2
    RIVER = 3


@dataclass
class GameConfig:
    """Configuration for the game being solved."""
    num_players: int = 6
    stack_size: float = 100.0   # in big blinds
    small_blind: float = 0.5
    big_blind: float = 1.0
    ante: float = 0.0
    # Bet sizing as fractions of pot: e.g. [0.33, 0.5, 0.75, 1.0]
    bet_sizes_flop: List[float] = field(default_factory=lambda: [0.33, 0.67, 1.0])
    bet_sizes_turn: List[float] = field(default_factory=lambda: [0.5, 0.75, 1.0])
    bet_sizes_river: List[float] = field(default_factory=lambda: [0.5, 0.75, 1.0])
    raise_sizes_preflop: List[float] = field(default_factory=lambda: [2.5, 3.0])
    all_in_threshold: float = 0.67  # go all-in if raise > this fraction of remaining stack

    def bet_sizes_for_street(self, street: Street) -> List[float]:
        if street == Street.PREFLOP:
            return self.raise_sizes_preflop
        elif street == Street.FLOP:
            return self.bet_sizes_flop
        elif street == Street.TURN:
            return self.bet_sizes_turn
        return self.bet_sizes_river


class NodeType(Enum):
    ACTION = auto()
    CHANCE = auto()
    TERMINAL = auto()


@dataclass
class GameState:
    """Mutable state of a hand in progress."""
    street: Street = Street.PREFLOP
    pot: float = 0.0
    bets: List[float] = field(default_factory=list)      # current-round bet per player
    stacks: List[float] = field(default_factory=list)
    folded: List[bool] = field(default_factory=list)
    all_in: List[bool] = field(default_factory=list)
    current_player: int = 0
    num_actions_this_round: int = 0
    last_raiser: int = -1
    board: List[int] = field(default_factory=list)
    history: List[Tuple[int, ActionWithSize]] = field(default_factory=list)

    def active_players(self) -> List[int]:
        return [i for i in range(len(self.folded)) if not self.folded[i]]

    def players_in_hand(self) -> int:
        return sum(1 for f in self.folded if not f)

    def max_bet(self) -> float:
        return max(self.bets) if self.bets else 0.0

    def to_call(self, player: int) -> float:
        return self.max_bet() - self.bets[player]

    def copy(self) -> GameState:
        return GameState(
            street=self.street,
            pot=self.pot,
            bets=self.bets.copy(),
            stacks=self.stacks.copy(),
            folded=self.folded.copy(),
            all_in=self.all_in.copy(),
            current_player=self.current_player,
            num_actions_this_round=self.num_actions_this_round,
            last_raiser=self.last_raiser,
            board=self.board.copy(),
            history=self.history.copy(),
        )


class GameTreeBuilder:
    """Builds the game tree for CFR traversal.

    For the initial version, we focus on heads-up postflop spots
    (the most tractable and useful case). Preflop 6-max ranges
    are handled via a separate preflop chart system.
    """

    def __init__(self, config: GameConfig):
        self.config = config

    def get_available_actions(self, state: GameState) -> List[ActionWithSize]:
        """Return legal actions for the current player."""
        player = state.current_player
        actions = []

        to_call = state.to_call(player)
        can_raise = state.stacks[player] > to_call

        if to_call > 0:
            actions.append(ActionWithSize(Action.FOLD))
            actions.append(ActionWithSize(Action.CALL, min(to_call, state.stacks[player])))
        else:
            actions.append(ActionWithSize(Action.CHECK))

        if can_raise:
            if state.street == Street.PREFLOP:
                for mult in self.config.raise_sizes_preflop:
                    raise_to = max(state.max_bet() * mult, state.max_bet() + self.config.big_blind)
                    raise_amount = raise_to - state.bets[player]
                    if raise_amount < state.stacks[player] * self.config.all_in_threshold:
                        actions.append(ActionWithSize(Action.BET, raise_amount))
            else:
                pot_after_call = state.pot + to_call * 2
                for frac in self.config.bet_sizes_for_street(state.street):
                    bet_size = pot_after_call * frac
                    if bet_size < state.stacks[player] * self.config.all_in_threshold:
                        if to_call > 0:
                            raise_to = state.max_bet() + bet_size
                            raise_amount = raise_to - state.bets[player]
                            actions.append(ActionWithSize(Action.BET, raise_amount))
                        else:
                            actions.append(ActionWithSize(Action.BET, bet_size))

            # Always include all-in option
            all_in_amount = state.stacks[player]
            actions.append(ActionWithSize(Action.ALL_IN, all_in_amount))

        return actions

    def apply_action(self, state: GameState, action: ActionWithSize) -> GameState:
        """Apply an action and return the new state."""
        new_state = state.copy()
        player = new_state.current_player

        if action.action == Action.FOLD:
            new_state.folded[player] = True

        elif action.action == Action.CHECK:
            pass

        elif action.action == Action.CALL:
            call_amount = min(action.size, new_state.stacks[player])
            new_state.bets[player] += call_amount
            new_state.stacks[player] -= call_amount
            if new_state.stacks[player] <= 0:
                new_state.all_in[player] = True

        elif action.action in (Action.BET, Action.ALL_IN):
            bet_amount = min(action.size, new_state.stacks[player])
            new_state.bets[player] += bet_amount
            new_state.stacks[player] -= bet_amount
            new_state.last_raiser = player
            if new_state.stacks[player] <= 0:
                new_state.all_in[player] = True

        new_state.history.append((player, action))
        new_state.num_actions_this_round += 1

        # Advance to next active player
        new_state.current_player = self._next_player(new_state)

        return new_state

    def is_round_over(self, state: GameState) -> bool:
        """Check if the current betting round is complete."""
        if state.players_in_hand() <= 1:
            return True

        active_not_allin = [
            i for i in range(len(state.folded))
            if not state.folded[i] and not state.all_in[i]
        ]

        if not active_not_allin:
            return True

        if state.num_actions_this_round < len(active_not_allin):
            return False

        # All active players have acted and bets are matched
        max_bet = state.max_bet()
        return all(
            state.bets[i] == max_bet or state.folded[i] or state.all_in[i]
            for i in range(len(state.bets))
        )

    def advance_street(self, state: GameState) -> GameState:
        """Move to the next street, collecting bets into pot."""
        new_state = state.copy()
        new_state.pot += sum(new_state.bets)
        new_state.bets = [0.0] * len(new_state.bets)
        new_state.num_actions_this_round = 0
        new_state.last_raiser = -1

        if new_state.street == Street.PREFLOP:
            new_state.street = Street.FLOP
        elif new_state.street == Street.FLOP:
            new_state.street = Street.TURN
        elif new_state.street == Street.TURN:
            new_state.street = Street.RIVER

        # Postflop: action starts from first active player after dealer
        active = new_state.active_players()
        if active:
            new_state.current_player = active[0]

        return new_state

    def is_terminal(self, state: GameState) -> bool:
        """Check if the hand is over."""
        if state.players_in_hand() <= 1:
            return True
        if state.street == Street.RIVER and self.is_round_over(state):
            return True
        return False

    def _next_player(self, state: GameState) -> int:
        n = len(state.folded)
        p = (state.current_player + 1) % n
        while state.folded[p] or state.all_in[p]:
            p = (p + 1) % n
            if p == state.current_player:
                break
        return p

    def create_initial_state(
        self,
        num_players: int = 2,
        street: Street = Street.PREFLOP,
        pot: float = 0.0,
        stacks: Optional[List[float]] = None,
        board: Optional[List[int]] = None,
    ) -> GameState:
        """Create an initial game state for solving."""
        if stacks is None:
            stacks = [self.config.stack_size] * num_players

        state = GameState(
            street=street,
            pot=pot,
            bets=[0.0] * num_players,
            stacks=stacks.copy(),
            folded=[False] * num_players,
            all_in=[False] * num_players,
            current_player=0,
            board=board or [],
        )

        if street == Street.PREFLOP and num_players >= 2:
            # Post blinds
            sb_idx = num_players - 2 if num_players > 2 else 0
            bb_idx = num_players - 1 if num_players > 2 else 1
            state.bets[sb_idx] = self.config.small_blind
            state.stacks[sb_idx] -= self.config.small_blind
            state.bets[bb_idx] = self.config.big_blind
            state.stacks[bb_idx] -= self.config.big_blind
            state.current_player = 0 if num_players > 2 else sb_idx

        return state
