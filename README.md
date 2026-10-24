# self-tuning-lam-annealing-cpp

A C++ implementation of Cicirello's (2021) Self-Tuning Lam annealing strategy.

Paper: [DOI:10.3390/app11219828](https://doi.org/10.3390/app11219828) | [Cicirello's Website](https://www.cicirello.org/publications/cicirello2021applsci.html)

## Self-Tuning Lam Annealing Strategy

Self-Tuning Lam is an adaptive simulated annealing strategy proposed by Cicirello (2021) that builds on Modified Lam.

The hyperparameters of conventional simulated annealing (SA), such as the initial temperature and cooling rate, usually need to be tuned in advance for each problem, instance, and run length. Self-Tuning Lam sets up a tuning phase at the beginning of the run to perform a random walk, and then uses the data from this phase to determine four hyperparameters online: the initial value of the acceptance-rate EMA and the EMA learning rate $\alpha$, the initial temperature $T_0$, and the rate of temperature change $\beta$. It then anneals according to Lam's ideal acceptance-rate trajectory and dynamically adjusts the temperature based on the target acceptance rate and the EMA value. Its core contribution is making the behavior of adaptive annealing insensitive to cost scale and run length, and producing better solutions in many cases.

Its main advantages include no need for prior parameter tuning, rapid tracking of the target behavior, a more stable balance between exploration and exploitation, and broad applicability.

## Project Overview

This is an independent C++ implementation of the Self-Tuning Lam annealing strategy; it is not an official version by the paper's author. The objective function is minimized.

The project is header-only and depends only on the C++17 standard library.

## Requirements

- A C++ compiler supporting C++17 or later
- CMake >= 3.16 when building the examples or using the project as a CMake subproject

## Quick Start

```cpp
#include "stlam/self_tuning_lam.h"

struct MyProblem : public stlam::CopyNeighborProblem<MyProblem> {
  using Solution = stlam::ScoredSolution<RawSolution>;
  Solution RandomSolution(stlam::Rng&) const;
  void RandomNeighbor(Solution&, stlam::Rng&) const;
};

int main() {
  MyProblem problem;
  stlam::SelfTuningLam<MyProblem> annealer(problem, /*seed=*/42);
  const auto best = annealer.Run(/*evaluations=*/100000);
}
```

See the example [`examples/onemax/onemax_copy.cc`](examples/onemax/onemax_copy.cc).

## Integration

Include the header [`include/stlam/self_tuning_lam.h`](include/stlam/self_tuning_lam.h) directly to integrate the library into an existing project, or use CMake.

### As a CMake Dependency

#### add_subdirectory

```cmake
add_subdirectory(third_party/self-tuning-lam-annealing-cpp)
target_link_libraries(my_app PRIVATE stlam::stlam)
```

#### FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  stlam
  GIT_REPOSITORY https://github.com/limpidautumn/self-tuning-lam-annealing-cpp.git
  GIT_TAG        <targetVersion>
)
FetchContent_MakeAvailable(stlam)
target_link_libraries(my_app PRIVATE stlam::stlam)
```

## Usage

<!-- For more details, refer to the API documentation. -->

### Solver

```cpp
// Instantiate the problem. The problem object must outlive the solver.
MyProblem problem;

// Construct the solver. Template parameter: the concrete problem type.
// Arguments: the problem object and the seed for the random number generator.
stlam::SelfTuningLam<MyProblem> annealer(problem, 42);

// Self-tune and solve. Argument: the iteration budget.
const stlam::ScoredSolution<RawSolution> best = annealer.Run(100000);
```

### Problem Types

`stlam` provides several base classes for different problem types.

#### Neighboring State: Snapshot Copy

Suitable when candidate solutions occupy little memory.

```cpp
struct MyProblem : public stlam::CopyNeighborProblem<MyProblem> {
  // The Solution type stores the candidate solution and its cost
  // (i.e., the objective to be minimized, referred to below as cost).
  using Solution = stlam::ScoredSolution<RawSolution>;

  // Generate an initial candidate solution and compute its cost.
  Solution RandomSolution(stlam::Rng& rng) const;

  // Move current to a neighboring state and update cost in sync.
  void RandomNeighbor(Solution& current, stlam::Rng& rng) const;
};
```

#### Neighboring State: Incremental Move and Undo

Suitable when candidate solutions do not lend themselves to deep copying, but moves are fast and reversible.

```cpp
struct MyProblem : public stlam::DeltaNeighborProblem<MyProblem> {
  // The Solution type stores the candidate solution and its cost
  // (i.e., the objective to be minimized, referred to below as cost).
  using Solution = stlam::ScoredSolution<RawSolution>;

  // Generate an initial candidate solution and compute its cost.
  Solution RandomSolution(stlam::Rng& rng) const;

  // Move current to a neighboring state and update cost in sync.
  // Return a Token that records the move.
  T ApplyRandomMove(Solution& current, stlam::Rng& rng) const;

  // Undo one move according to the Token and roll back cost in sync.
  void UndoMove(Solution& current, T&& token) const;
};
```

#### Custom

Suitable for scenarios that involve advanced state or custom runtime logic.

```cpp
struct MyProblem : public stlam::ProblemBase {
  // The Solution type stores the candidate solution and its cost
  // (i.e., the objective to be minimized, referred to below as cost).
  using Solution = stlam::ScoredSolution<RawSolution>;

  // Generate an initial candidate solution and compute its cost.
  Solution RandomSolution(stlam::Rng& rng) const;

  // Move current to a neighboring state and update cost in sync.
  // Return a Token that records the move.
  T Propose(Solution& current, stlam::Rng& rng) const;

  // You can implement custom logic in TuneAccept/Accept/Reject,
  // such as state updates and data statistics.

  // Random walk during the tuning phase; Propose is always accepted. Optional.
  void TuneAccept(Solution& current, T&& token) const;

  // Accept Propose during the annealing phase. Optional.
  void Accept(Solution& current, T&& token) const;

  // Reject Propose during the annealing phase. Undo one move according to the
  // Token and roll back cost in sync.
  void Reject(Solution& current, T&& token) const;
};
```

## Examples

- OneMax: flip one bit in a 0/1 sequence to maximize the number of 1s in the sequence.
  - [`examples/onemax/onemax_copy.cc`](examples/onemax/onemax_copy.cc): candidate copy implementation.
  - [`examples/onemax/onemax_delta.cc`](examples/onemax/onemax_delta.cc): incremental candidate update/undo implementation.

### Building the Examples

```bash
cmake -B build
cmake --build build -j
./build/onemax_copy   # OneMax copy implementation
./build/onemax_delta  # OneMax incremental implementation
```

## License and Citation

MIT License.

If you use this software in academic work, please cite:

Vincent A. Cicirello. *Self-Tuning Lam Annealing: Learning Hyperparameters While Problem Solving*. Applied Sciences, 11(21):9828, 2021. <https://doi.org/10.3390/app11219828>

Or use [BibTeX](https://www.cicirello.org/publications/cicirello2021applsci.bib).
