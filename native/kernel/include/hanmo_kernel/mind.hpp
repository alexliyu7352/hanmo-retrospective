// 推理演算（设计第 8、14 节）：心智可替换。内核自带习惯；细想、小模型、沙盒的 Python 实验版都实现同一接口。
// 拿主意可以多核并行：心智只能读所给的输入，不许改共享状态；同一输入必须给出同一决定。
#pragma once

#include "fact.hpp"

namespace hanmo::kernel {

// 交给心智的输入：只有此人所知。
struct DecideInput {
  const KnownView *known = nullptr;
  Rng rng;
  HalfDay now = 0;
};

// 心智的决定。action.motive 写明依据了哪几条记忆；什么也不做时 action.verb 为 None。
struct Decision {
  Action action;
  bool deliberated = false;  // 是细想出来的，还是照习惯
};

// 一个候选的估值：哪一招、对着谁、值多少。
struct ScoredCandidate {
  RecipeId recipe;
  Ref target;
  float value = 0;
};

// 决策记录：每次细想的处境、各招估值、选了哪招。沙盒用它训练小模型（训练与使用的输入都只来自所知）。
struct DecisionRecord {
  HalfDay when = 0;
  ActorId actor;
  Span<float> situation;            // 处境特征
  Span<ScoredCandidate> candidates;
  int32_t chosen = -1;              // 在 candidates 中的位置；-1 表示什么也不做
  float idle_value = 0;
  const char *mind = nullptr;       // 用的是哪个心智
};

// 收决策记录的一方。拿主意是并行的，实现者自己负责线程安全。
class DecisionRecorder {
 public:
  virtual ~DecisionRecorder() = default;
  virtual void record(const DecisionRecord &) = 0;
};

class Mind {
 public:
  virtual ~Mind() = default;

  virtual const char *name() const = 0;
  // 路由：这半日要不要拿主意。不拿就照手上的打算或习惯继续。小模型的路由就实现在这里。
  virtual bool wants_to_think(const DecideInput &) const = 0;
  // 拿主意。recorder 为空时不记录。
  virtual Decision decide(const DecideInput &, DecisionRecorder *recorder) const = 0;
};

// 反射：被打了躲开、看见贼就喊。当场处理，不等下一个半日，不细想。只凭刚得知的那条记忆。
class Reflex {
 public:
  virtual ~Reflex() = default;
  // 有反应时写进 out 并返回真。
  virtual bool react(const KnownView &, const Memory &just_learned, Action &out) const = 0;
};

}  // namespace hanmo::kernel
