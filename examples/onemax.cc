#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>

#include "stlam/self_tuning_lam.h"

constexpr std::size_t kProblemScale = 256;
constexpr std::size_t kNumberIterations = 1000;
const std::uint64_t seed = 42;

// Problem
template <std::size_t size>
class OneMax {
 public:
  using Solution = std::bitset<size>;
  explicit OneMax() = default;

  Solution RandomSolution(std::mt19937_64& rng) const {
    Solution s;
    for (std::size_t base = 0; base < size; base += 64)
      s |= Solution(rng()) << base;
    return s;
  }

  Solution RandomNeighbor(const Solution& current, std::mt19937_64& rng) const {
    Solution neighbor = current;
    std::uniform_int_distribution<std::size_t> dist(0, size - 1);
    const auto index = dist(rng);
    neighbor[index].flip();
    return neighbor;
  }

  double Cost(const Solution& solution) const {
    return -static_cast<double>(solution.count());
  }
};

int main() {
  using Problem = OneMax<kProblemScale>;
  Problem problem;
  stlam::SelfTuningLam<Problem> annealer(problem, seed);
  const auto result = annealer.Run(kNumberIterations);
  std::cout << "best cost: " << result.cost << '\n';
  return 0;
}
