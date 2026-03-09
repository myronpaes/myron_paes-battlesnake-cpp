# Battlesnake: C++ Monte Carlo Tree Search Engine

## Overview

This Battlesnake engine is a high-performance cybernetic brain designed to dominate the arena through probabilistic forecasting and spatial control. This architecture treats board survival as a strict resource optimization problem, executing thousands of simulated futures per turn to maximize the return on investment for every movement decision.

## Languages & System Architecture

The system is built for absolute execution speed, bypassing the standard latency limitations of interpreted languages.

* **C++ (C++17):** The core engine is written entirely in compiled C++ to eliminate Garbage Collection pauses and Global Interpreter Lock (GIL) bottlenecks, allowing for maximum hardware utilization.
* **Single-Header Libraries:** Utilizes `cpp-httplib` for multithreaded web serving and `nlohmann/json` for high-speed payload parsing.
* **Docker Containerization:** Deployed via a custom `gcc:13` Docker image utilizing aggressive `-O3` compiler optimizations and `-pthread` multithreading to ensure sub-millisecond API response times.

## Statistical Methodologies

### 1. Monte Carlo Tree Search (MCTS)

Rather than relying on brittle, hardcoded heuristics, the engine leverages an MCTS algorithm to statistically sample thousands of potential future board states within a strict 500-millisecond timeout window. The engine executes rapid rollouts of parallel game universes, backpropagating the survival rates up the decision tree to weight the most mathematically viable paths.

### 2. Upper Confidence Bound Applied to Trees (UCB1)

To navigate the multi-armed bandit problem inherent in branching game states, the engine utilizes the UCB1 formula. This elegantly balances the *exploitation* of known winning paths with the *exploration* of unproven, potentially dominant strategies.

$$UCB1 = \frac{w_i}{n_i} + c \sqrt{\frac{\ln(N)}{n_i}}$$

* $w_i$: Total accumulated win score of the current node.
* $n_i$: Total number of times this specific node has been visited.
* $N$: Total visits to the parent node.
* $c$: Exploration parameter, mathematically calibrated to $\sqrt{2}$.

### 3. Voronoi Spatial Evaluation

When a simulated rollout reaches a terminal depth, the engine quantifies the strategic value of the board using a high-speed Breadth-First Search (BFS) mapped to a flat 1D Boolean array. This generates a Voronoi diagram of the grid, calculating the exact percentage of cells the engine can reach before any enemy snake. The higher the spatial control ratio, the higher the statistical value of the node.

### 4. Deterministic Precision Rounding

Floating-point variations across different CPU threads can severely corrupt MCTS probability branches over thousands of parallel simulations. To maintain absolute mathematical fidelity, every heuristic and probabilistic calculation is subjected to a strict fast-math clamp. The engine strictly applies standard rounding rules (0-4 rounds down, 5+ rounds up) to precisely 5 decimal places.

$$f(x) = \frac{\lfloor x \times 100000 + 0.5 \rfloor}{100000}$$
