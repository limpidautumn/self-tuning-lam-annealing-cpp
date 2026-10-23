#ifndef STLAM_SELF_TUNING_LAM_H
#define STLAM_SELF_TUNING_LAM_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <tuple>
#include <utility>

namespace stlam {

using Rng = std::mt19937_64;

// Raw solution with cached cost. Caller must keep them in sync.
template <typename RawSolution>
struct ScoredSolution {
  RawSolution solution{};
  double cost = 0.0;
};

// Common contract for problem implementations. The `raw_solution` and `cost` of
// a `Solution` must always remain synchronized.
// Implementations should first define:
//   using Solution = stlam::ScoredSolution<RawSolution>;
// All template parameters S below must be Solution (a ScoredSolution).
class ProblemBase {
 public:
  // Returns a random initial solution with its cost set.
  auto RandomSolution(Rng&) const = delete;

  // Applies a random move and returns its undo token.
  template <typename S>
  auto Propose(S&, Rng&) const = delete;

  // Consumes Propose's result for a tuning-phase move.
  template <typename S, typename T>
  void TuneAccept(S&, T&&) const {}

  // Consumes Propose's result after an accepted move.
  template <typename S, typename T>
  void Accept(S&, T&&) const {};

  // Consumes Propose's result after a rejected move and restores the state.
  template <typename S, typename T>
  void Reject(S&, T&&) const = delete;

 protected:
  ~ProblemBase() = default;
};

namespace internal {
template <typename>
inline constexpr bool is_scored_solution_v = false;
template <typename RawS>
inline constexpr bool is_scored_solution_v<ScoredSolution<RawS>> = true;
}  // namespace internal

template <typename Problem>
class SelfTuningLam {
  static_assert(std::is_base_of_v<ProblemBase, Problem>,
                "Problem must derive from stlam::ProblemBase");
  static_assert(internal::is_scored_solution_v<typename Problem::Solution>,
                "Problem::Solution must be ScoredSolution<RawSolution>");

 public:
  using Solution = typename Problem::Solution;

  SelfTuningLam(const Problem& problem, std::uint64_t seed)
      : problem_(problem), rng_(seed) {}

  // Algorithm 7. Runs the Self-Tuning Lam Annealing schedule and return the
  // optimal solution found. `evaluations` is clamped to at least 100 so the
  // tuning phase is non-empty.
  [[nodiscard]] Solution Run(std::size_t evaluations) {
    // Self-tuning
    const std::size_t n = std::max(evaluations, kMinEvaluations);
    const std::size_t m = TuningLength(n);
    const double alpha = TuneAlpha(n);
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
      const double c0 = s.cost;
      auto p = problem_.Propose(s, rng_);
      const double& c1 = s.cost;
      if (c1 <= c0 || uniform_(rng_) < std::exp(-(c1 - c0) / temperature)) {
        problem_.Accept(s, std::move(p));
        accept_rate = (1.0 - alpha) * accept_rate + alpha;
        Consider(s);
      } else {
        problem_.Reject(s, std::move(p));
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

      if (accept_rate > lam_rate) {
        temperature *= beta;
      } else {
        temperature /= beta;
      }
    }

    return std::move(best_);
  }

 private:
  struct Tuned {
    Solution solution;
    double t0, beta;
  };

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
  Tuned TuneTemperature(std::size_t n, std::size_t m, double initial_rate) {
    const bool long_run = n >= 10000u;
    const double& zeta1 = long_run ? kZetaAt0p001 : kZetaAt0p01;
    const double& zeta2 = long_run ? kZetaAt0p002 : kZetaAt0p02;
    const double& lambda2 = long_run ? kLambdaAt0p002 : kLambdaAt0p02;

    double sum = 0.0;           // SumOfDifferences in paper
    std::size_t diff = 0;       // k in paper
    std::size_t non_worse = 0;  // d in paper
    Solution s = problem_.RandomSolution(rng_);
    best_ = s;  // Initialize the optimal solution and cost.
    for (std::size_t i = 1; i <= m; ++i) {
      const double c0 = s.cost;
      auto p = problem_.Propose(s, rng_);
      const double c1 = s.cost;
      if (c1 != c0) {
        sum += std::abs(c1 - c0);
        diff += 1;
      }
      if (c1 <= c0) {
        non_worse += 1;
      }
      problem_.TuneAccept(s, p);
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

  void Consider(const Solution& r) {
    if (r.cost < best_.cost) best_ = r;
  }

  static constexpr std::size_t kMinEvaluations = 100;

  // Table 1.
  static constexpr double kLambdaAt0p001 = 0x1.f427ebf8a5f9bp-1;  // λ_0.001
  static constexpr double kLambdaAt0p002 = 0x1.e8cd18520b1eap-1;  // λ_0.002
  static constexpr double kLambdaAt0p01 = 0x1.9d51639c5d11dp-1;   // λ_0.01
  static constexpr double kLambdaAt0p02 = 0x1.5c998e7720c01p-1;   // λ_0.02
  static constexpr double kZetaAt0p001 = 0x1.41a6997657820p-2;    // ζ_0.001
  static constexpr double kZetaAt0p002 = 0x1.0afd324bcab8ap-2;    // ζ_0.002
  static constexpr double kZetaAt0p01 = 0x1.84df5607ce4e2p-3;     // ζ_0.01
  static constexpr double kZetaAt0p02 = 0x1.6303fb1d89265p-3;     // ζ_0.02

  const Problem& problem_;
  Rng rng_;
  std::uniform_real_distribution<double> uniform_{0.0, 1.0};
  Solution best_;
};

// CRTP base for copy-neighbor problems.
// Derived must define:
//   using Solution = stlam::ScoredSolution<Raw>;
//   Solution RandomSolution(Rng&) const;
//   void RandomNeighbor(Solution&, Rng&) const;  // mutates and updates cost.
// and keep raw_solution/cost synchronized.
template <typename Derived>
class CopyNeighborProblem : public ProblemBase {
 public:
  auto RandomSolution(Rng&) const = delete;

  template <typename S>
  void RandomNeighbor(S&, Rng&) const = delete;

 protected:
  ~CopyNeighborProblem() = default;

 private:
  friend class SelfTuningLam<Derived>;

  template <typename S>
  S Propose(S& s, Rng& rng) const {
    S s0 = s;
    self().RandomNeighbor(s, rng);
    return s0;
  }

  template <typename S, typename T>
  void TuneAccept(S&, T&&) const {}

  template <typename S, typename T>
  void Accept(S&, T&&) const {};

  template <typename S>
  void Reject(S& s, S&& s0) const {
    s = std::move(s0);
  }

  const Derived& self() const { return static_cast<const Derived&>(*this); }
};

// CRTP base for delta-neighbor problems.
// Derived must define:
//   using Solution = stlam::ScoredSolution<Raw>;
//   Solution RandomSolution(Rng&) const;
//   T ApplyRandomMove(Solution&, Rng&) const;  // mutates, updates cost,
//                                              // returns undo token.
//   void UndoMove(Solution&, const T&) const;  // restores state and cost.
// and keep raw_solution/cost synchronized.
template <typename Derived>
class DeltaNeighborProblem : public ProblemBase {
 public:
  auto RandomSolution(Rng&) const = delete;

  template <typename S>
  auto ApplyRandomMove(S&, Rng&) const = delete;

  template <typename S, typename T>
  void UndoMove(S&, const T&) const = delete;

 protected:
  ~DeltaNeighborProblem() = default;

 private:
  friend class SelfTuningLam<Derived>;

  template <typename S>
  auto Propose(S& s, Rng& rng) const {
    return self().ApplyRandomMove(s, rng);
  }

  template <typename S, typename T>
  void TuneAccept(S&, T&&) const {}

  template <typename S, typename T>
  void Accept(S&, T&&) const {};

  template <typename S, typename T>
  void Reject(S& s, T&& t) const {
    self().UndoMove(s, std::move(t));
  }

  const Derived& self() const { return static_cast<const Derived&>(*this); }
};

}  // namespace stlam

#endif
