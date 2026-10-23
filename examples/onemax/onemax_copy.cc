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

// Copy-neighbor approach: Propose snapshots the whole Solution, then mutates
// it. Reject restores the snapshot. RandomNeighbor must recompute cost.
class OneMax : public stlam::CopyNeighborProblem<OneMax> {
 private:
  using RawSolution = std::bitset<kProblemScale>;

 public:
  using Solution = stlam::ScoredSolution<RawSolution>;

  Solution RandomSolution(stlam::Rng& rng) const {
    RawSolution s;
    for (std::size_t base = 0; base < kProblemScale; base += 64)
      s |= RawSolution(rng()) << base;
    const double c = Cost(s);
    return {std::move(s), c};
  }

  void RandomNeighbor(Solution& current, stlam::Rng& rng) const {
    std::uniform_int_distribution<std::size_t> dist(0, kProblemScale - 1);
    const std::size_t index = dist(rng);
    current.solution[index].flip();
    current.cost = Cost(current.solution);
  }

 private:
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
