// 内核的基本类型：编号、时刻、引用、拒绝原因、只读视图、随机数。
// 全是可按位复制的小结构，存档时直接写表。内核不懂任何玩法，种类一律用编号，由内容和包登记。
#pragma once

#include <cstddef>
#include <cstdint>

namespace hanmo::kernel {

// 内核接口版本：包的清单写明它要求的版本；接口一改就加一，旧包要跟着改。
constexpr uint32_t kAbiVersion = 1;

// 世界时刻：第几个半日。半日是世界的时间刻度，验证和存档都在半日边界。
using HalfDay = uint32_t;

// 各表的编号。值为 0 表示「没有」。
struct ActorId { uint32_t v = 0; };
struct PlaceId { uint32_t v = 0; };
struct ItemKindId { uint16_t v = 0; };
struct RelationKindId { uint16_t v = 0; };
struct ClaimKindId { uint16_t v = 0; };
struct FactKindId { uint16_t v = 0; };
struct PackageId { uint16_t v = 0; };
struct RecipeId { uint32_t v = 0; };
struct ProcessId { uint32_t v = 0; };
struct FactId { uint64_t v = 0; };

// 某人记忆里的一条：谁的记忆、第几格。记忆被遗忘后这格会被别的记忆占用。
struct MemoryId {
  ActorId holder;
  uint32_t slot = 0;
};

// 指向任一表中的一行：行动者、地方、物品种类、事实，或某个包自己的表（由该包解释 id）。
enum class Table : uint8_t { None, Actor, Place, ItemKind, Fact, PackageRow };

struct Ref {
  uint32_t id = 0;
  Table table = Table::None;
  PackageId package;
};

// 拒绝原因：内核的基本几种；PackageRule 时由 package 指明哪个包、detail 由该包解释。
enum class RejectCode : uint16_t {
  None,
  NoSuchActor,
  NotPresent,   // 不在场、够不着
  NotHeld,      // 手上没有
  NotEnough,    // 数量不够
  Busy,         // 正在做别的事
  Incapable,    // 身体或年纪做不了
  Refused,      // 对方不答应
  PackageRule,  // 包的规则不许
};

struct Reject {
  RejectCode code = RejectCode::None;
  PackageId package;
  uint32_t detail = 0;

  // 没有被拒绝。
  bool ok() const { return code == RejectCode::None; }
};

// 半日推进的各段（第 4 节），包可以在指定的段被调用。
enum class Phase : uint8_t { Processes, Decide, Execute, Gossip, Body, Forget, Validate, Save };

// 只读的连续视图（C++17 没有 std::span）。视图在下一次改动世界之前有效。
template <class T>
struct Span {
  const T *data = nullptr;
  size_t size = 0;

  const T *begin() const { return data; }
  const T *end() const { return data + size; }
};

// 可重现的随机数：由世界种子、半日、行动者推出。同一存档、同一输入，结果相同。
struct Rng {
  uint64_t state = 0;

  // 下一个 64 位随机数。
  uint64_t next();
  // [0, 1) 之间的随机数。
  double unit();
};

}  // namespace hanmo::kernel
