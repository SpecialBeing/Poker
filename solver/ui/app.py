"""GTO Solver Web UI built with Streamlit."""

import streamlit as st
import numpy as np
import pandas as pd
import plotly.graph_objects as go
import plotly.express as px

st.set_page_config(
    page_title='GTO Poker Solver',
    page_icon='♠',
    layout='wide',
    initial_sidebar_state='expanded',
)

RANKS = 'AKQJT98765432'
SUITS = '♠♥♦♣'
SUIT_COLORS = {'♠': '#1a1a2e', '♥': '#e74c3c', '♦': '#3498db', '♣': '#27ae60'}


def render_card(rank: str, suit: str) -> str:
    color = SUIT_COLORS.get(suit, '#000')
    return f'<span style="color:{color};font-weight:bold;font-size:1.2em">{rank}{suit}</span>'


def make_range_grid(strategies: dict, action: str = 'raise') -> pd.DataFrame:
    """Create a 13x13 grid for the preflop range chart."""
    grid = np.zeros((13, 13))
    labels = [['' for _ in range(13)] for _ in range(13)]

    for i, r1 in enumerate(RANKS):
        for j, r2 in enumerate(RANKS):
            if i == j:
                hand = f'{r1}{r2}'
            elif i < j:
                hand = f'{r1}{r2}s'
            else:
                hand = f'{r2}{r1}o'

            if hand in strategies:
                grid[i][j] = strategies[hand].get(action, 0) * 100
            labels[i][j] = hand

    return grid, labels


def plot_range_chart(strategies: dict, action: str = 'raise', title: str = 'Range') -> go.Figure:
    """Create a heatmap visualization of a preflop range."""
    grid, labels = make_range_grid(strategies, action)

    text = [[f'{labels[i][j]}<br>{grid[i][j]:.0f}%' for j in range(13)] for i in range(13)]

    fig = go.Figure(data=go.Heatmap(
        z=grid,
        text=text,
        texttemplate='%{text}',
        textfont={'size': 9},
        colorscale=[
            [0, '#1a1a2e'],
            [0.3, '#16213e'],
            [0.5, '#0f3460'],
            [0.7, '#e94560'],
            [1.0, '#ff6b6b'],
        ],
        zmin=0, zmax=100,
        showscale=True,
        colorbar=dict(title='%', ticksuffix='%'),
    ))

    fig.update_layout(
        title=dict(text=title, font=dict(size=16)),
        xaxis=dict(
            tickmode='array', tickvals=list(range(13)), ticktext=list(RANKS),
            side='top', title='',
        ),
        yaxis=dict(
            tickmode='array', tickvals=list(range(13)), ticktext=list(RANKS),
            autorange='reversed', title='',
        ),
        width=600, height=600,
        margin=dict(l=40, r=40, t=60, b=40),
        paper_bgcolor='#0e1117',
        plot_bgcolor='#1a1a2e',
        font=dict(color='#fafafa'),
    )

    return fig


def main():
    # Sidebar
    with st.sidebar:
        st.title('♠ GTO Solver')
        st.markdown('---')

        mode = st.radio('Mode', ['Cash Game', 'Tournament (Poker Now)'], index=0)

        st.markdown('---')
        st.subheader('Settings')

        if mode == 'Cash Game':
            stack_size = st.slider('Stack Size (BB)', 20, 200, 100)
            sb = 0.5
            bb = 1.0
        else:
            starting_stack = st.number_input('Starting Stack', value=10000, step=1000)
            num_players_alive = st.number_input('Players Remaining', 2, 6, 6)
            blind_level = st.selectbox('Blind Level', [
                '25/50', '50/100', '75/150', '100/200',
                '150/300', '200/400', '300/600', '500/1000',
            ])
            sb, bb = map(float, blind_level.split('/'))

            st.subheader('Stacks')
            stacks = []
            for i in range(num_players_alive):
                s = st.number_input(f'Player {i+1}', value=starting_stack, step=100, key=f'stack_{i}')
                stacks.append(float(s))

            payouts_str = st.text_input('Payouts (%)', '50, 30, 20')
            payouts = [float(x.strip()) for x in payouts_str.split(',')]

        st.markdown('---')
        solver_iters = st.slider('Solver Iterations', 100, 10000, 2000, 100)

    # Main content
    tab1, tab2, tab3 = st.tabs(['📊 Preflop Ranges', '🎯 Postflop Solver', '📈 ICM Analysis'])

    # ─── Tab 1: Preflop Ranges ──────────────────────────────────
    with tab1:
        st.header('Preflop Range Solver')

        col1, col2 = st.columns([1, 2])

        with col1:
            position = st.selectbox('Position', ['UTG', 'HJ', 'CO', 'BTN', 'SB', 'BB'])
            action_view = st.selectbox('Action', ['raise', 'call', 'fold'])

            if st.button('🔄 Solve Preflop Ranges', type='primary', use_container_width=True):
                with st.spinner('Running CFR+ preflop solver...'):
                    if mode == 'Cash Game':
                        from solver.modes.cash import CashGameConfig, CashGameSolver
                        cfg = CashGameConfig(stack_size_bb=stack_size)
                        solver = CashGameSolver(cfg)
                        result = solver.solve_preflop(num_iterations=solver_iters, show_progress=False)
                    else:
                        from solver.modes.tournament import TournamentConfig, TournamentSolver
                        cfg = TournamentConfig(
                            current_stacks=stacks,
                            blinds=(sb, bb),
                            payouts=payouts,
                        )
                        tsolver = TournamentSolver(cfg)
                        result_data = tsolver.get_icm_adjusted_ranges(
                            hero_position=0,
                            num_iterations=solver_iters,
                            show_progress=False,
                        )
                        result = {}
                        for pos in ['UTG', 'HJ', 'CO', 'BTN', 'SB', 'BB']:
                            result[pos] = {
                                'strategies': result_data['strategies'].get(pos, {}),
                            }

                    st.session_state['preflop_result'] = result
                    st.success(f'Solved in {solver_iters} iterations!')

        with col2:
            if 'preflop_result' in st.session_state:
                result = st.session_state['preflop_result']
                if position in result:
                    strats = result[position]
                    if isinstance(strats, dict) and 'strategies' in strats:
                        strats = strats['strategies']

                    fig = plot_range_chart(strats, action_view, f'{position} — {action_view.title()} Range')
                    st.plotly_chart(fig, use_container_width=True)

                    # Summary stats
                    total_action = sum(
                        s.get(action_view, 0) for s in strats.values()
                    ) / len(strats) * 100
                    st.metric(f'{action_view.title()} Frequency', f'{total_action:.1f}%')

    # ─── Tab 2: Postflop Solver ─────────────────────────────────
    with tab2:
        st.header('Postflop Spot Solver')

        col1, col2 = st.columns(2)

        with col1:
            st.subheader('Setup')
            hero_cards = st.text_input('Hero Cards (e.g. AsKh)', 'AsKh')
            villain_cards = st.text_input('Villain Cards (e.g. QdJd)', 'QdJd')
            board_input = st.text_input('Board (e.g. Tc9h2s)', 'Tc9h2s')
            pot_size = st.number_input('Pot Size (BB)', value=6.0, step=0.5)

            hero_stack = st.number_input('Hero Stack (BB)', value=97.0, step=1.0)
            villain_stack = st.number_input('Villain Stack (BB)', value=97.0, step=1.0)

        with col2:
            st.subheader('Bet Sizing')
            flop_sizes = st.multiselect('Flop Bet Sizes (pot fraction)',
                                         [0.25, 0.33, 0.5, 0.67, 0.75, 1.0, 1.5],
                                         default=[0.33, 0.67, 1.0])
            turn_sizes = st.multiselect('Turn Bet Sizes (pot fraction)',
                                         [0.25, 0.33, 0.5, 0.67, 0.75, 1.0, 1.5],
                                         default=[0.5, 0.75, 1.0])
            river_sizes = st.multiselect('River Bet Sizes (pot fraction)',
                                          [0.25, 0.33, 0.5, 0.67, 0.75, 1.0, 1.5, 2.0],
                                          default=[0.5, 0.75, 1.0])

        if st.button('🎯 Solve Postflop Spot', type='primary', use_container_width=True):
            with st.spinner('Running CFR+ postflop solver...'):
                try:
                    from solver.modes.cash import CashGameConfig, CashGameSolver
                    cfg = CashGameConfig(
                        stack_size_bb=max(hero_stack, villain_stack),
                        bet_sizes_flop=flop_sizes,
                        bet_sizes_turn=turn_sizes,
                        bet_sizes_river=river_sizes,
                    )
                    solver = CashGameSolver(cfg)
                    result = solver.solve_postflop(
                        hero_cards=hero_cards,
                        villain_cards=villain_cards,
                        board=board_input,
                        pot_bb=pot_size,
                        stacks_bb=[hero_stack, villain_stack],
                        num_iterations=solver_iters,
                        show_progress=False,
                    )

                    st.session_state['postflop_result'] = result
                    st.success(f'Solved! {result["num_info_sets"]} information sets explored.')
                except Exception as e:
                    st.error(f'Error: {e}')

        if 'postflop_result' in st.session_state:
            result = st.session_state['postflop_result']
            st.subheader('Strategy')

            strategies = result['strategies']
            if strategies:
                strat_data = []
                for key, avg_strat in strategies.items():
                    parts = key.split(':')
                    player = parts[0] if parts else '?'
                    bucket = parts[1] if len(parts) > 1 else '?'
                    strat_str = ', '.join(f'{v:.1%}' for v in avg_strat)
                    strat_data.append({
                        'Player': player,
                        'Bucket': bucket,
                        'Strategy': strat_str,
                    })

                df = pd.DataFrame(strat_data)
                st.dataframe(df, use_container_width=True)

                # Action distribution chart
                if strat_data:
                    first_key = list(strategies.keys())[0]
                    first_strat = strategies[first_key]
                    action_labels = [f'Action {i}' for i in range(len(first_strat))]

                    fig = go.Figure(data=[go.Bar(
                        x=action_labels,
                        y=first_strat * 100,
                        marker_color=['#e74c3c', '#2ecc71', '#3498db', '#f39c12', '#9b59b6'][:len(first_strat)],
                    )])
                    fig.update_layout(
                        title='Strategy Distribution',
                        yaxis_title='Frequency (%)',
                        paper_bgcolor='#0e1117',
                        plot_bgcolor='#1a1a2e',
                        font=dict(color='#fafafa'),
                    )
                    st.plotly_chart(fig, use_container_width=True)

    # ─── Tab 3: ICM Analysis ────────────────────────────────────
    with tab3:
        st.header('ICM Analysis')

        if mode != 'Tournament (Poker Now)':
            st.info('Switch to Tournament mode in the sidebar to use ICM analysis.')
        else:
            col1, col2 = st.columns(2)

            with col1:
                st.subheader('Stack Distribution')
                fig = go.Figure(data=[go.Bar(
                    x=[f'Player {i+1}' for i in range(len(stacks))],
                    y=stacks,
                    marker_color=px.colors.qualitative.Set2[:len(stacks)],
                )])
                fig.update_layout(
                    yaxis_title='Chips',
                    paper_bgcolor='#0e1117',
                    plot_bgcolor='#1a1a2e',
                    font=dict(color='#fafafa'),
                )
                st.plotly_chart(fig, use_container_width=True)

            with col2:
                st.subheader('ICM Equity')

                if st.button('📈 Calculate ICM', use_container_width=True):
                    from solver.modes.tournament import ICMCalculator
                    icm = ICMCalculator(payouts)
                    equities = icm.calculate_equity(stacks)

                    st.session_state['icm_result'] = equities

                if 'icm_result' in st.session_state:
                    equities = st.session_state['icm_result']

                    fig = go.Figure(data=[go.Pie(
                        labels=[f'Player {i+1}' for i in range(len(equities))],
                        values=equities,
                        hole=0.4,
                        marker=dict(colors=px.colors.qualitative.Set2[:len(equities)]),
                    )])
                    fig.update_layout(
                        paper_bgcolor='#0e1117',
                        font=dict(color='#fafafa'),
                    )
                    st.plotly_chart(fig, use_container_width=True)

                    # ICM vs Chip EV comparison
                    total_chips = sum(stacks)
                    total_payout = sum(payouts)

                    comparison_data = []
                    for i in range(len(stacks)):
                        chip_pct = stacks[i] / total_chips * 100
                        icm_pct = equities[i] / total_payout * 100
                        comparison_data.append({
                            'Player': f'Player {i+1}',
                            'Chips': f'{stacks[i]:,.0f}',
                            'Chip %': f'{chip_pct:.1f}%',
                            'ICM Equity': f'{equities[i]:.2f}',
                            'ICM %': f'{icm_pct:.1f}%',
                            'Pressure': f'{"High" if icm_pct < chip_pct - 2 else "Low"}',
                        })

                    st.dataframe(pd.DataFrame(comparison_data), use_container_width=True)

            # Push/Fold chart for short stacks
            st.markdown('---')
            st.subheader('Push/Fold Chart')

            hero_pos = st.selectbox('Hero Position', list(range(len(stacks))),
                                     format_func=lambda x: f'Player {x+1} ({stacks[x]:,.0f} chips, {stacks[x]/bb:.0f}BB)')

            if st.button('Calculate Push/Fold', use_container_width=True):
                from solver.modes.tournament import TournamentConfig, TournamentSolver
                cfg = TournamentConfig(
                    current_stacks=stacks,
                    blinds=(sb, bb),
                    payouts=payouts,
                )
                tsolver = TournamentSolver(cfg)
                pf_result = tsolver.get_icm_adjusted_ranges(
                    hero_position=hero_pos,
                    num_iterations=solver_iters,
                    show_progress=False,
                )

                if pf_result.get('push_fold'):
                    push_fold = pf_result['push_fold']

                    # Build grid
                    grid = np.zeros((13, 13))
                    for i, r1 in enumerate(RANKS):
                        for j, r2 in enumerate(RANKS):
                            if i == j:
                                hand = f'{r1}{r2}'
                            elif i < j:
                                hand = f'{r1}{r2}s'
                            else:
                                hand = f'{r2}{r1}o'
                            grid[i][j] = 100 if push_fold.get(hand) == 'PUSH' else 0

                    fig = go.Figure(data=go.Heatmap(
                        z=grid,
                        colorscale=[[0, '#1a1a2e'], [1, '#e94560']],
                        zmin=0, zmax=100,
                        showscale=False,
                    ))

                    labels_text = [['' for _ in range(13)] for _ in range(13)]
                    for i, r1 in enumerate(RANKS):
                        for j, r2 in enumerate(RANKS):
                            if i == j:
                                labels_text[i][j] = f'{r1}{r2}'
                            elif i < j:
                                labels_text[i][j] = f'{r1}{r2}s'
                            else:
                                labels_text[i][j] = f'{r2}{r1}o'

                    fig.update_traces(
                        text=labels_text,
                        texttemplate='%{text}',
                        textfont={'size': 9},
                    )
                    fig.update_layout(
                        title=f'Push/Fold — Player {hero_pos+1} ({stacks[hero_pos]/bb:.0f}BB)',
                        xaxis=dict(tickmode='array', tickvals=list(range(13)), ticktext=list(RANKS), side='top'),
                        yaxis=dict(tickmode='array', tickvals=list(range(13)), ticktext=list(RANKS), autorange='reversed'),
                        width=600, height=600,
                        paper_bgcolor='#0e1117',
                        plot_bgcolor='#1a1a2e',
                        font=dict(color='#fafafa'),
                    )
                    st.plotly_chart(fig, use_container_width=True)

                    push_count = sum(1 for v in push_fold.values() if v == 'PUSH')
                    st.metric('Push Range', f'{push_count}/{len(push_fold)} hands ({push_count/len(push_fold)*100:.1f}%)')
                    st.metric('ICM Pressure', f'{pf_result["icm_pressure"]:.2%}')


if __name__ == '__main__':
    main()
