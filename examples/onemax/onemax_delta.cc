#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <utility>

#include "stlam/self_tuning_lam.h"

constexpr std::size_t kProblemScale = 256;
constexpr std::size_t kNumberIterations = 1000;
const std::uint64_t seed = 42;
const double kBitContribution = -1.0;

// Delta-neighbor approach: Propose applies a move and returns an undo token.
// Reject calls UndoMove(token). Moves update cost incrementally.
class OneMax : public stlam::DeltaNeighborProblem<OneMax> {
 private:
  using RawSolution = std::bitset<kProblemScale>;

 public:
  using Solution = stlam::ScoredSolution<RawSolution>;

  Solution RandomSolution(stlam::Rng& rng) const {
    RawSolution s;
    for (std::size_t base = 0; base < kProblemScale; base += 64)
      s |= RawSolution(rng()) << base;
    const double c = kBitContribution * static_cast<double>(s.count());
    return {std::move(s), c};
  }

  std::size_t ApplyRandomMove(Solution& current, stlam::Rng& rng) const {
    std::uniform_int_distribution<std::size_t> dist(0, kProblemScale - 1);
    const std::size_t index = dist(rng);
    Flip(current, index);
    return index;
  }

  void UndoMove(Solution& current, const std::size_t& index) const {
    Flip(current, index);
  }

 private:
  void Flip(Solution& s, std::size_t i) const {
    auto b = s.solution[i];
    if (b == true) {
      s.cost -= kBitContribution;
      b = false;
    } else if (b == false) {
      s.cost += kBitContribution;
      b = true;
    }
  }
};

int main() {
  OneMax problem;
  stlam::SelfTuningLam<OneMax> annealer(problem, seed);
  const auto result = annealer.Run(kNumberIterations);
  std::cout << "best cost: " << result.cost << '\n';
  return 0;
}
