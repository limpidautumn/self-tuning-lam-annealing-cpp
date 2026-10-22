#ifndef STLAM_SELF_TUNING_LAM_H
#define STLAM_SELF_TUNING_LAM_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <tuple>

namespace stlam {

template <typename Problem>
class SelfTuningLam {
 public:
  using Solution = typename Problem::Solution;
  struct Result {
    Solution solution;
    double cost;
  };

  explicit SelfTuningLam(const Problem& problem, std::uint64_t seed)
      : problem_(problem), rng_(seed) {}

  [[nodiscard]] Result Run(std::size_t evaluations) {
    assert(evaluations >= 100);

    // Self-tuning
    const std::size_t& n = evaluations;
    std::size_t m = TuningLength(n);
    double alpha = TuneAlpha(n);
    double accept_rate = InitialAcceptanceRate(n);
    auto [s, temperature, beta] = TuneTemperature(n, m, accept_rate);

    double m0 = accept_rate - 0.44;
    const double m1 = std::pow(560.0, -1.0 / (0.15 * static_cast<double>(n)));
    const double m2 = std::pow(440.0, -1.0 / (0.35 * static_cast<double>(n)));
    double lam_rate = accept_rate;

    const std::size_t early_end =
        static_cast<std::size_t>(0.15 * static_cast<double>(n));
    const std::size_t late_start =
        static_cast<std::size_t>(0.65 * static_cast<double>(n));

    for (std::size_t i = m + 1; i <= n; ++i) {
      Result s1 = MakeResult(problem_.RandomNeighbor(s.solution, rng_));

      const double dc = s1.cost - s.cost;
      if (dc <= 0 || uniform_(rng_) < std::exp(-dc / temperature)) {
        s = std::move(s1);
        accept_rate = (1.0 - alpha) * accept_rate + alpha;
        Consider(s);
      } else {
        accept_rate *= 1.0 - alpha;
      }

      if (i <= early_end) {
        m0 = m0 * m1;
        lam_rate = 0.44 + m0;
      } else if (i > late_start) {
        lam_rate *= m2;
      } else {
        lam_rate = 0.44;
      }

      if (accept_rate > lam_rate)
        temperature *= beta;
      else
        temperature /= beta;
    }

    return std::move(best_);
  }

 private:
  Result MakeResult(Solution s) {
    double c = problem_.Cost(s);
    return Result{std::move(s), c};
  }

  // Algorithm 3.
  static std::size_t TuningLength(std::size_t n) {
    return (n >= 10000u) ? n / 1000 : n / 100;
  }

  // Algorithm 4.
  static double TuneAlpha(std::size_t n) {
    return std::min(2.0 / (0.01 * static_cast<double>(n) + 1.0), 0.2);
  }

  // Algorithm 5.
  static double InitialAcceptanceRate(std::size_t n) {
    return (n >= 10000u) ? kLambdaAt0p001 : kLambdaAt0p01;
  }

  // Algorithm 6.
  using Tuned = std::tuple<Result, double, double>;
  Tuned TuneTemperature(std::size_t n, std::size_t m, double initial_rate) {
    const bool long_run = n >= 10000u;
    const double& zeta1 = long_run ? kZetaAt0p001 : kZetaAt0p01;
    const double& zeta2 = long_run ? kZetaAt0p002 : kZetaAt0p02;
    const double& lambda2 = long_run ? kLambdaAt0p002 : kLambdaAt0p02;

    double sum = 0.0;           // SumOfDifferences in paper
    std::size_t diff = 0;       // k in paper
    std::size_t non_worse = 0;  // d in paper
    Result s = MakeResult(problem_.RandomSolution(rng_));
    best_ = s;  // Initialize the optimal solution and cost.
    for (std::size_t i = 1; i <= m; ++i) {
      Result s1 = MakeResult(problem_.RandomNeighbor(s.solution, rng_));
      if (s1.cost != s.cost) {
        sum += std::abs(s1.cost - s.cost);
        diff += 1;
      }
      if (s1.cost <= s.cost) {
        non_worse += 1;
      }
      s = std::move(s1);
      Consider(s);
    }

    const double delta_c = diff != 0 ? sum / static_cast<double>(diff) : 1.0;
    const double gamma = static_cast<double>(non_worse) /
                         static_cast<double>(m + (non_worse == m));
    const double& r = lambda2;
    double t0, beta;

    if (gamma < initial_rate) {
      const double l0 = std::log((initial_rate - gamma) / (1.0 - gamma));
      t0 = -delta_c / l0;
      if (gamma < r) {
        const double lr = std::log((r - gamma) / (1.0 - gamma));
        beta = std::pow(l0 / lr, 1.0 / static_cast<double>(m));
      } else {
        beta = std::pow(-zeta2 * l0, 1.0 / static_cast<double>(m));
      }
    } else {
      t0 = delta_c * zeta1;
      beta = std::pow(zeta2 / zeta1, 1.0 / static_cast<double>(m));
    }

    return {std::move(s), t0, beta};
  }

  void Consider(const Result& r) {
    if (r.cost < best_.cost) best_ = r;
  }

  const Problem& problem_;
  std::mt19937_64 rng_;
  std::uniform_real_distribution<double> uniform_{0.0, 1.0};
  Result best_;

  // Table 1.
  static constexpr double kLambdaAt0p001 = 0x1.f427ebf8a5f9bp-1;  // λ_0.001
  static constexpr double kLambdaAt0p002 = 0x1.e8cd18520b1eap-1;  // λ_0.002
  static constexpr double kLambdaAt0p01 = 0x1.9d51639c5d11dp-1;   // λ_0.01
  static constexpr double kLambdaAt0p02 = 0x1.5c998e7720c01p-1;   // λ_0.02
  static constexpr double kZetaAt0p001 = 0x1.41a6997657820p-2;    // ζ_0.001
  static constexpr double kZetaAt0p002 = 0x1.0afd324bcab8ap-2;    // ζ_0.002
  static constexpr double kZetaAt0p01 = 0x1.84df5607ce4e2p-3;     // ζ_0.01
  static constexpr double kZetaAt0p02 = 0x1.6303fb1d89265p-3;     // ζ_0.02
};

}  // namespace stlam

#endif
