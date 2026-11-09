// 世界状态的表（设计第 1、10 节）：平的表，互相只用编号引用，不存指针。
// 这里只放内核自己的数据；包的数据在包里，由包自己存（见 package.hpp）。
#pragma once

#include <array>

#include "types.hpp"

namespace hanmo::kernel {

// 行动者的种类：一个人是人数 1；军队、流民、一里百姓是人数 N 的群体；势力、官府、宗族是人数 0 的组织。
enum class ActorKind : uint8_t { Person, Group, Organization };

// 身体：饥饿、精力、伤。群体存人均值。规则由我们定，只求简单。
struct Body {
  int32_t hunger = 0;
  int32_t energy = 0;
  int32_t injury = 0;
  bool alive = true;
};

// 行动者。具名人物挂在某个群体里时，随群体移动、受伤、阵亡。
struct Actor {
  ActorId id;
  ActorKind kind = ActorKind::Person;
  uint32_t headcount = 1;
  PlaceId place;
  Body body;
  ActorId group;      // 挂在哪个群体里；0 表示独立
  uint32_t name = 0;  // 字串表编号
};

constexpr size_t kTiers = 6;    // 群体的等阶数；各阶的含义由包定（例：新兵、老兵、精兵）
constexpr size_t kOrigins = 8;  // 来源最多记几个里，其余并入最后一格「他处」

// 来源：来自哪个里（里也是群体）、多少人。阵亡时由此知道死讯该传回哪里。
struct Origin {
  ActorId community;
  uint32_t count = 0;
};

// 群体附加：各等阶人数之和等于 headcount。经验跟人走（分出时带等阶），东西留在编制（见 Holding）。
struct GroupInfo {
  ActorId id;
  std::array<uint32_t, kTiers> tiers{};
  int32_t morale = 0;
  ActorId head;
  std::array<Origin, kOrigins> origins{};
};

// 东西：持有者与所有者分开，被拿走仍是原主的。持有者可以是行动者，也可以是地方（粮仓、战场）。
// 群体的装备就是群体持有的东西，分出去的人不带走。
struct Holding {
  Ref holder;
  ActorId owner;
  ItemKindId kind;
  int64_t qty = 0;
};

// 物品种类：由内容登记。内核只用到「吃了顶多少」与「占多少地方」，其余含义由包定。
struct ItemKind {
  ItemKindId id;
  uint32_t name = 0;
  int32_t food_value = 0;
  int32_t bulk = 0;
};

// 地方与道路：路程以半日计。
struct Place {
  PlaceId id;
  uint32_t name = 0;
};

struct Route {
  PlaceId from;
  PlaceId to;
  uint32_t half_days = 1;
};

// 关系：谁对谁、哪种（由内容或包登记）、数值。亲属、上下级、情分都在这里。
struct Relation {
  ActorId from;
  ActorId to;
  RelationKindId kind;
  int32_t value = 0;
};

// 持续过程：旅行、做工、学习、造物。到期或被打断时由内核通知所属的包。
struct Process {
  ProcessId id;
  ActorId actor;
  PackageId owner;   // 0 表示内核自己的（例：旅行）
  uint32_t kind = 0; // 由 owner 解释
  HalfDay start = 0;
  HalfDay end = 0;
  Ref target;
  int64_t progress = 0;
};

// 真值的只读视图：只给包算规则和调试用。心智与玩家界面不许读，它们只能用 KnownView。
class WorldView {
 public:
  virtual ~WorldView() = default;

  virtual HalfDay now() const = 0;
  virtual Span<Actor> actors() const = 0;
  // 群体附加；不是群体返回空。
  virtual const GroupInfo *group(ActorId) const = 0;
  virtual Span<Holding> holdings_of(Ref holder) const = 0;
  virtual Span<ItemKind> item_kinds() const = 0;
  virtual Span<Place> places() const = 0;
  virtual Span<Route> routes() const = 0;
  virtual Span<Relation> relations_of(ActorId) const = 0;
  // 此刻在这个地方的行动者（地方索引，感知分发也用它）。
  virtual Span<ActorId> present_at(PlaceId) const = 0;
  virtual Span<Process> processes_of(ActorId) const = 0;
};

}  // namespace hanmo::kernel
