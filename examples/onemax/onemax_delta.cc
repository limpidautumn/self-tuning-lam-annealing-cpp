/// @file
/// @brief OneMax solved with apply-and-undo neighbors.

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

/// @brief OneMax problem using delta-neighbor moves.
///
/// Minimizes the negative number of set bits. Each move flips one bit and
/// updates the cost incrementally; the returned bit index is also the undo
/// token.
class OneMax : public stlam::DeltaNeighborProblem<OneMax> {
 private:
  /// @brief A bitset represents a candidate.
  using RawSolution = std::bitset<kProblemScale>;

 public:
  /// @brief Scored candidate type.
  using Solution = stlam::ScoredSolution<RawSolution>;

  /// @brief Creates a random candidate and computes its cost.
  /// @param rng Random engine.
  /// @return Random solution with its cost set.
  Solution RandomSolution(stlam::Rng& rng) const {
    RawSolution s;
    for (std::size_t base = 0; base < kProblemScale; base += 64)
      s |= RawSolution(rng()) << base;
    const double c = kBitContribution * static_cast<double>(s.count());
    return {std::move(s), c};
  }

  /// @brief Flips one random bit, updates the cached cost, and returns its
  /// index as the undo token.
  /// @param current Solution to mutate.
  /// @param rng Random engine.
  /// @return Index of the flipped bit.
  std::size_t ApplyRandomMove(Solution& current, stlam::Rng& rng) const {
    std::uniform_int_distribution<std::size_t> dist(0, kProblemScale - 1);
    const std::size_t index = dist(rng);
    Flip(current, index);
    return index;
  }

  /// @brief Flips the bit at `index` back, restoring the previous cost.
  /// @param current Solution to restore.
  /// @param index Index returned by `ApplyRandomMove`.
  void UndoMove(Solution& current, std::size_t&& index) const {
    Flip(current, index);
  }

 private:
  /// @brief Flips bit `i` and adjusts the cached cost.
  /// @param s Solution to mutate.
  /// @param i Index of the bit to flip.
  void Flip(Solution& s, std::size_t i) const {
    if (s.solution[i]) {
      s.cost -= kBitContribution;
    } else {
      s.cost += kBitContribution;
    }
    s.solution[i].flip();
  }
};

/// @brief Runs the delta-neighbor OneMax example.
int main() {
  OneMax problem;
  stlam::SelfTuningLam<OneMax> annealer(problem, seed);
  const auto result = annealer.Run(kNumberIterations);
  std::cout << "best cost: " << result.cost << '\n';
  return 0;
}
