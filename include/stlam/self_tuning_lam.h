/// @file
/// @brief Self-Tuning Lam annealing schedule.

#ifndef STLAM_SELF_TUNING_LAM_H
#define STLAM_SELF_TUNING_LAM_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>

/// @brief Self-Tuning Lam annealing.
namespace stlam {

/// @brief Random engine used by the library.
using Rng = std::mt19937_64;

/// @brief A raw solution with its cached cost.
///
/// The objective is minimized. The `solution` member stores the RawSolution;
/// implementations must update it and `cost` together.
/// @tparam RawSolution Candidate type; must be default-constructible.
template <typename RawSolution>
struct ScoredSolution {
  RawSolution solution{};  ///< Problem-specific candidate.
  double cost = 0.0;       ///< Cached objective value; lower is better.
};

/// @brief Base interface for user-defined optimization problems.
///
/// Serves as a type constraint for `SelfTuningLam`. Derived classes must
/// implement the following public interface:
///
/// - `using Solution = stlam::ScoredSolution<RawSolution>;`:
///   Problem-specific scored solution type.
///
/// - `Solution RandomSolution(Rng&) const`:
///   Returns a valid initial candidate with its cost correctly evaluated.
///
/// - `Token Propose(Solution&, Rng&) const`:
///   Mutates the solution in-place to generate a neighbor and returns an
///   opaque undo token. `Token` is implementation-defined: it may be a saved
///   solution, a move index, or any value that can undo the move.
///
/// - `void Reject(Solution&, Token&&) const`:
///   Restores the solution to its pre-Propose state using the token.
///
/// - `void Accept(Solution&, Token&&) const` (optional):
///   Consumes the token after an accepted move. Defaults to a no-op.
///
/// - `void TuneAccept(Solution&, Token&&) const` (optional):
///   Consumes the token during the temperature tuning. Defaults to a no-op.
///
/// `Propose` returns one token per call. It is passed to exactly one of
/// `Reject`, `Accept`, or `TuneAccept`, and must not be used afterwards.
/// `RandomSolution` must set `cost`; `Propose` and `Reject` must keep
/// `Solution::solution` and `Solution::cost` synchronized.
class ProblemBase {
 public:
  /// @brief Scored solution type used by the problem.
  /// @note Defaults to `void`; derived problems must provide this type.
  using Solution = void;

  /// @brief Creates a random initial solution with its cost set.
  /// @note Deleted here; derived problems must provide this method.
  auto RandomSolution(Rng&) const = delete;

  /// @brief Applies a random move and returns its undo token.
  /// @tparam S ScoredSolution type.
  template <typename S>
  auto Propose(S&, Rng&) const = delete;

  /// @brief Consumes a token during temperature tuning.
  /// @tparam S ScoredSolution type.
  /// @tparam T Undo-token type.
  /// @note Defaults to a no-op.
  template <typename S, typename T>
  void TuneAccept(S&, T&&) const {}

  /// @brief Consumes a token when accepted during annealing.
  /// @tparam S ScoredSolution type.
  /// @tparam T Undo-token type.
  /// @note Defaults to a no-op.
  template <typename S, typename T>
  void Accept(S&, T&&) const {}

  /// @brief Consumes a token when rejected and restores the solution.
  /// @tparam S ScoredSolution type.
  /// @tparam T Undo-token type.
  /// @note Deleted here; derived problems must provide this method.
  template <typename S, typename T>
  void Reject(S&, T&&) const = delete;

 protected:
  /// @brief Protected to prevent deletion through this base.
  ~ProblemBase() = default;
};

/// @brief Implementation details; not part of the public API.
namespace internal {

/// @brief Detects `ScoredSolution` specializations.
/// @tparam T Candidate solution type.
template <typename>
inline constexpr bool is_scored_solution_v = false;

/// @brief Detects `ScoredSolution` specializations.
/// @tparam RawS Candidate solution type.
template <typename RawS>
inline constexpr bool is_scored_solution_v<ScoredSolution<RawS>> = true;

}  // namespace internal

/// @brief The Self-Tuning Lam annealing schedule.
///
/// Tunes the phase length, EMA rate, initial acceptance rate, and temperature
/// schedule (Algorithms 3-6), then anneals while tracking the target Lam
/// acceptance rate (Algorithm 7).
///
/// @tparam Problem Problem type derived from `ProblemBase`.
/// @see https://doi.org/10.3390/app11219828
template <typename Problem>
class SelfTuningLam {
  static_assert(std::is_base_of_v<ProblemBase, Problem>,
                "Problem must derive from stlam::ProblemBase");
  static_assert(internal::is_scored_solution_v<typename Problem::Solution>,
                "Problem::Solution must be ScoredSolution<RawSolution>");

 public:
  /// @brief Solution type used by `Problem`.
  using Solution = typename Problem::Solution;

  /// @brief Creates an annealer for `problem`.
  /// @param problem Problem instance; stored by reference and must outlive this
  /// annealer.
  /// @param seed Seed for the internal random engine.
  SelfTuningLam(const Problem& problem, std::uint64_t seed)
      : problem_(problem), rng_(seed) {}

  /// @brief Algorithm 7. Runs the schedule.
  ///
  /// The first M evaluations form the tuning walk; the remaining
  /// evaluations run the annealing loop.
  ///
  /// @param evaluations Evaluation budget; clamped to at least 100.
  /// @return Best solution found.
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
  /// @brief Output of `TuneTemperature`.
  struct Tuned {
    Solution solution;  ///< Solution at the end of the tuning walk.
    double t0;          ///< Initial temperature.
    double beta;        ///< Multiplicative temperature change per step.
  };

  /// @brief Algorithm 3. Computes the tuning-phase length M.
  /// @param n Run length N.
  static std::size_t TuningLength(std::size_t n) {
    return (n >= 10000u) ? n / 1000 : n / 100;
  }

  /// @brief Algorithm 4. Computes the EMA learning rate alpha.
  /// @param n Run length N.
  static double TuneAlpha(std::size_t n) {
    return std::min(2.0 / (0.01 * static_cast<double>(n) + 1.0), 0.2);
  }

  /// @brief Algorithm 5. Selects the initial acceptance rate.
  /// @param n Run length N.
  static double InitialAcceptanceRate(std::size_t n) {
    return (n >= 10000u) ? kLambdaAt0p001 : kLambdaAt0p01;
  }

  /// @brief Algorithm 6. Tunes the initial temperature and temperature change
  /// factor.
  ///
  /// Performs a random walk of `m` moves, then solves for T0 and beta from the
  /// observed cost differences and non-worsening move count.
  ///
  /// @param n Run length N.
  /// @param m Tuning walk length M.
  /// @param initial_rate Initial acceptance rate.
  /// @return Solution at the end of the walk, T0, and beta.
  /// @note Also initializes `best_`; `Consider(s)` updates it during the walk.
  Tuned TuneTemperature(std::size_t n, std::size_t m, double initial_rate) {
    const bool long_run = n >= 10000u;
    const double& zeta1 = long_run ? kZetaAt0p001 : kZetaAt0p01;
    const double& zeta2 = long_run ? kZetaAt0p002 : kZetaAt0p02;
    const double& lambda2 = long_run ? kLambdaAt0p002 : kLambdaAt0p02;

    double sum = 0.0;           // SumOfDifferences in paper
    std::size_t diff = 0;       // k in paper
    std::size_t non_worse = 0;  // d in paper
    Solution s = problem_.RandomSolution(rng_);
    best_ = s;  // Initialize best_ to the initial solution.
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
      problem_.TuneAccept(s, std::move(p));
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

  /// @brief Updates `best_` if `s` has a lower cost.
  void Consider(const Solution& s) {
    if (s.cost < best_.cost) best_ = s;
  }

  /// @brief Minimum evaluation budget; keeps the tuning walk non-empty.
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

/// @brief CRTP helper for problems that use copy-and-restore neighbors.
///
/// `Derived` must provide these public members:
/// - `using Solution = stlam::ScoredSolution<RawSolution>;`
/// - `Solution RandomSolution(Rng&) const`
///   Returns a random initial candidate with `cost` set.
/// - `void RandomNeighbor(Solution&, Rng&) const`
///   Replaces the candidate with a random neighbor and updates `cost`.
///
/// `RandomNeighbor` may recompute `cost` from scratch or update it
/// incrementally, but must leave `solution` and `cost` synchronized.
///
/// `TuneAccept` and `Accept` are inherited no-ops; override them in `Derived`
/// to observe accepted moves. They must remain public.
///
/// @tparam Derived Concrete problem type.
template <typename Derived>
class CopyNeighborProblem : public ProblemBase {
 public:
  /// @brief Deleted; derived classes must implement this method.
  auto RandomSolution(Rng&) const = delete;

  /// @brief Copies `s`, applies a random neighbor, and returns the snapshot.
  /// @tparam S ScoredSolution type.
  /// @param s Solution mutated into a neighbor.
  /// @param rng Random engine.
  /// @return Pre-move snapshot, used by `Reject` to restore `s`.
  template <typename S>
  S Propose(S& s, Rng& rng) const {
    S s0 = s;
    self().RandomNeighbor(s, rng);
    return s0;
  }

  /// @brief Restores `s` from the snapshot `s0`.
  /// @tparam S ScoredSolution type.
  /// @param s Solution to restore.
  /// @param s0 Snapshot returned by `Propose`.
  template <typename S>
  void Reject(S& s, S&& s0) const {
    s = std::move(s0);
  }

  /// @brief Deleted; derived classes must implement this method.
  /// @note Must mutate the solution and update its cost.
  template <typename S>
  void RandomNeighbor(S&, Rng&) const = delete;

 protected:
  /// @brief Protected to prevent deletion through this CRTP base.
  ~CopyNeighborProblem() = default;

 private:
  const Derived& self() const { return static_cast<const Derived&>(*this); }
};

/// @brief CRTP helper for problems that use apply-and-undo neighbors.
///
/// `Derived` must provide these public members:
/// - `using Solution = stlam::ScoredSolution<RawSolution>;`
/// - `Solution RandomSolution(Rng&) const`
///   Returns a random initial candidate with `cost` set.
/// - `T ApplyRandomMove(Solution&, Rng&) const`
///   Applies a random move, updates `cost`, and returns an undo token.
/// - `void UndoMove(Solution&, T&&) const`
///   Reverses the move using the token and restores `cost`.
///
/// `ApplyRandomMove` and `UndoMove` must leave `solution` and `cost`
/// synchronized. The token was forwarded as an rvalue, so `T` may be move-only.
///
/// `TuneAccept` and `Accept` are inherited no-ops; override them in `Derived`
/// to observe accepted moves. They must remain public.
///
/// @tparam Derived Concrete problem type.
template <typename Derived>
class DeltaNeighborProblem : public ProblemBase {
 public:
  /// @brief Deleted; derived classes must implement this method.
  auto RandomSolution(Rng&) const = delete;

  /// @brief Applies a random move and returns its undo token.
  /// @tparam S ScoredSolution type.
  /// @param s Solution to mutate.
  /// @param rng Random engine.
  /// @return An undo token for the mutation.
  template <typename S>
  auto Propose(S& s, Rng& rng) const {
    return self().ApplyRandomMove(s, rng);
  }

  /// @brief Undoes the move represented by the token.
  /// @tparam S ScoredSolution type.
  /// @tparam T Custom type for token.
  /// @param s Solution to restore.
  /// @param t Token returned by `Propose`.
  template <typename S, typename T>
  void Reject(S& s, T&& t) const {
    self().UndoMove(s, std::move(t));
  }

  /// @brief Deleted; derived classes must implement this method.
  /// @note Must mutate the solution and update its cost.
  template <typename S>
  auto ApplyRandomMove(S&, Rng&) const = delete;

  /// @brief Deleted; derived classes must implement this method.
  /// @note Must undo the move and restore the cost.
  template <typename S, typename T>
  void UndoMove(S&, T&&) const = delete;

 protected:
  /// @brief Protected to prevent deletion through this CRTP base.
  ~DeltaNeighborProblem() = default;

 private:
  const Derived& self() const { return static_cast<const Derived&>(*this); }
};

}  // namespace stlam

#endif
