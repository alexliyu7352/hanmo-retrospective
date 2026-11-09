// 因果网（设计第 5 节）：事实表是唯一的真相，按半日分段；起因分「促成」与「动机」两种。
#pragma once

#include "action.hpp"

namespace hanmo::kernel {

// 保存级别：日常只留半日汇总；一般存 30 天；要事长存并进史书。
// 一件要事产生时，内核把它往上几层的起因一并升为要事。
enum class FactClass : uint8_t { Routine, Normal, Major };

// 事实种类：内核动作的种类编号等于动词编号；内核结算的从 900 起；包登记的从 1000 起。
namespace fact_kind {
constexpr FactKindId Died{900};
constexpr FactKindId Born{901};
constexpr FactKindId Wounded{902};
constexpr FactKindId Recovered{903};
constexpr FactKindId ProcessEnded{904};
constexpr uint16_t kFirstPackageFact = 1000;
}  // namespace fact_kind

struct Fact {
  FactId id;
  HalfDay when = 0;
  FactKindId kind;
  FactClass cls = FactClass::Normal;
  ActorId actor;
  Ref target;
  Ref object;
  int64_t qty = 0;
  PlaceId place;
  PackageId package;  // 包事实属于哪个包
};

enum class CauseKind : uint8_t {
  Produced,  // 促成：物理上由哪件事造成（打 → 伤；断粮 → 饿死）
  Motive,    // 动机：行动者当时相信什么（可能是假的，链会经过谣言连到编谣言的人）
};

// 一条起因。动机存一份当时所知的副本，所以那条记忆后来被忘了，因果仍讲得出。
struct Cause {
  FactId fact;
  CauseKind kind = CauseKind::Produced;
  FactId because;     // 促成：哪件事；被删的事实改指最近的保留祖先
  Claim believed;     // 动机：他当时相信什么
  ActorId told_by;    // 动机：是谁告诉他的（亲见为 0）
};

// 因果查询：给包、调试，以及经主角所知过滤后的玩家界面用。
class FactView {
 public:
  virtual ~FactView() = default;

  // 已过期删掉的返回空，界面显示「已不可考」。
  virtual const Fact *get(FactId) const = 0;
  virtual Span<Cause> causes(FactId) const = 0;
  virtual Span<FactId> consequences(FactId) const = 0;
  virtual Span<Fact> during(HalfDay) const = 0;
};

}  // namespace hanmo::kernel
