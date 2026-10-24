# self-tuning-lam-annealing-cpp

Cicirello (2021) 自整定 Lam 退火策略的 C++ 实现。

论文: [DOI:10.3390/app11219828](https://doi.org/10.3390/app11219828) | [Cicirello Website](https://www.cicirello.org/publications/cicirello2021applsci.html)

## Self-Tuning Lam 退火策略

Self-Tuning Lam 是 Cicirello (2021) 在 Modified Lam 基础上提出的一种自适应模拟退火策略。

传统 SA 的初始温度、降温速率等超参数通常需要针对问题、实例和运行长度预先调参。Self-Tuning Lam 在运行开头设置一段调温阶段执行随机游走，然后用这段数据在线确定四个超参数：接受率 EMA 的初值和 EMA 学习率 $\alpha$、初始温度 $T_0$、温度变化率 $\beta$。随后按 Lam 的理想接受率轨迹退火，并根据目标接受率与 EMA 值动态校准温度。其核心贡献是让自适应退火的行为对代价尺度和运行长度不敏感，并在许多情况下带来更好的解。

其优势主要有：无需预先调参、快速跟踪目标行为、探索与开发的平衡更稳定、适用面广等。

## 项目简介

Self-Tuning Lam 退火策略的独立 C++ 实现，并非论文作者的官方版本。目标函数按最小化处理。

项目采用 header-only 设计，只依赖 C++17 标准库。

## 环境要求

- 支持 C++17 或更高标准的 C++ 编译器
- 构建示例或作为 CMake 子项目时，CMake >= 3.16

## 快速使用

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

参考示例 [`examples/onemax/onemax_copy.cc`](examples/onemax/onemax_copy.cc)。

## 集成方法

直接包含头文件 [`include/stlam/self_tuning_lam.h`](include/stlam/self_tuning_lam.h) 以接入现有工程，或使用 CMake。

### 作为 CMake 依赖

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

## 使用方法

<!-- 更多细节请参考 API 文档。 -->

### 求解器

```cpp
// 实例化问题。问题对象必须比求解器活得更久。
MyProblem problem;

// 构建求解器。模板参数：具体问题类型。参数：问题对象，以及随机数生成器的种子。
stlam::SelfTuningLam<MyProblem> annealer(problem, 42);

// 自整定并求解。参数：迭代次数的预算。
const stlam::ScoredSolution<RawSolution> best = annealer.Run(100000);
```

### 问题类型

`stlam` 为不同的问题类型提供了多种基类。

#### 邻态：快照副本

适用于“候选解占用空间较小”的情况。

```cpp
struct MyProblem : public stlam::CopyNeighborProblem<MyProblem> {
  // 类型 Solution 存储候选解以及它的代价 (即需要最小化的目标，下文称 cost)。
  using Solution = stlam::ScoredSolution<RawSolution>;

  // 生成一个初始候选解，并计算它的 cost。
  Solution RandomSolution(stlam::Rng& rng) const;

  // 将 current 移动到邻态，同步更新 cost。
  void RandomNeighbor(Solution& current, stlam::Rng& rng) const;
};
```

#### 邻态：增量移动与撤销

适用于“候选解不适合深拷贝，但转移快速且可逆”的情况。

```cpp
struct MyProblem : public stlam::DeltaNeighborProblem<MyProblem> {
  // 类型 Solution 存储候选解以及它的代价 (即需要最小化的目标，下文称 cost)。
  using Solution = stlam::ScoredSolution<RawSolution>;

  // 生成一个初始候选解，并计算它的 cost。
  Solution RandomSolution(stlam::Rng& rng) const;

  // 将 current 移动到邻态，同步更新 cost。返回一个 Token 用于记录移动。
  T ApplyRandomMove(Solution& current, stlam::Rng& rng) const;

  // 根据 Token 回退一次移动，同步回退 cost。
  void UndoMove(Solution& current, T&& token) const;
};
```

#### 自定义

适用于“高级状态”或“自定义运行时逻辑”的情况。

```cpp
struct MyProblem : public stlam::ProblemBase {
  // 类型 Solution 存储候选解以及它的代价 (即需要最小化的目标，下文称 cost)。
  using Solution = stlam::ScoredSolution<RawSolution>;

  // 生成一个初始候选解，并计算它的 cost。
  Solution RandomSolution(stlam::Rng& rng) const;

  // 将 current 移动到邻态，同步更新 cost。返回一个 Token 用于记录移动。
  T Propose(Solution& current, stlam::Rng& rng) const;

  // 用户可在 TuneAccept/Accept/Reject 内实现状态更新、数据统计等自定义逻辑。

  // 调温阶段随机游走，始终接受 Propose。可选。
  void TuneAccept(Solution& current, T&& token) const;

  // 退火阶段接受 Propose。可选。
  void Accept(Solution& current, T&& token) const;

  // 退火阶段拒绝 Propose。根据 Token 回退一次移动，同步回退 cost。
  void Reject(Solution& current, T&& token) const;
};
```

## 示例

- OneMax: 翻转 0/1 序列中的某一位，最大化序列中 1 的个数。
  - [`examples/onemax/onemax_copy.cc`](examples/onemax/onemax_copy.cc): 候选复制实现。
  - [`examples/onemax/onemax_delta.cc`](examples/onemax/onemax_delta.cc): 候选增量更新/回退实现。

### 构建示例

```bash
cmake -B build
cmake --build build -j
./build/onemax_copy   # OneMax 复制实现
./build/onemax_delta  # OneMax 增量实现
```

## 许可与引用

MIT License.

若在学术工作中使用，请引用：

Vincent A. Cicirello. *Self-Tuning Lam Annealing: Learning Hyperparameters While Problem Solving*. Applied Sciences, 11(21):9828, 2021. <https://doi.org/10.3390/app11219828>

或使用 [BibTeX](https://www.cicirello.org/publications/cicirello2021applsci.bib)。
