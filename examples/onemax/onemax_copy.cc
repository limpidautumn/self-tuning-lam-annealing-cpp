/// @file
/// @brief OneMax solved with copy-and-restore neighbors.

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <utility>

#include "stlam/self_tuning_lam.h"

/// @brief Number of bits in each candidate.
constexpr std::size_t kProblemScale = 256;
/// @brief Requested evaluation budget.
constexpr std::size_t kNumberIterations = 1000;
/// @brief Seed for the random number generator.
const std::uint64_t seed = 42;
/// @brief Cost contribution of one set bit.
const double kBitContribution = -1.0;

/// @brief OneMax problem using copy-neighbor moves.
///
/// Minimizes the negative number of set bits. `RandomNeighbor` recomputes the
/// cost from scratch after each bit flip.
class OneMax : public stlam::CopyNeighborProblem<OneMax> {
 private:
  /// @brief A bitset represents a candidate.
  using RawSolution = std::bitset<kProblemScale>;

 public:
  /// @brief Scored candidate type.
  using Solution = stlam::ScoredSolution<RawSolution>;

  /// @brief Creates a random bitset and computes its cost.
  /// @param rng Random engine.
  /// @return Random solution with its cost set.
  Solution RandomSolution(stlam::Rng& rng) const {
    RawSolution s;
    for (std::size_t base = 0; base < kProblemScale; base += 64)
      s |= RawSolution(rng()) << base;
    const double c = Cost(s);
    return {std::move(s), c};
  }

  /// @brief Flips one random bit and recomputes the full cost.
  /// @param current Solution to mutate.
  /// @param rng Random engine.
  void RandomNeighbor(Solution& current, stlam::Rng& rng) const {
    std::uniform_int_distribution<std::size_t> dist(0, kProblemScale - 1);
    const std::size_t index = dist(rng);
    current.solution[index].flip();
    current.cost = Cost(current.solution);
  }

 private:
  /// @brief Computes the cost of `s`.
  ///
  /// The cost is `kBitContribution` times the number of set bits, so minimizing
  /// it maximizes the number of ones.
  ///
  /// @param s Candidate to score.
  /// @return `kBitContribution * s.count()` as a `double`.
  double Cost(const RawSolution& s) const {
    return kBitContribution * static_cast<double>(s.count());
  }
};

int main() {
  OneMax problem;
  stlam::SelfTuningLam<OneMax> annealer(problem, seed);
  const auto result = annealer.Run(kNumberIterations);
  std::cout << "best cost: " << result.cost << '\n';
  return 0;
}
