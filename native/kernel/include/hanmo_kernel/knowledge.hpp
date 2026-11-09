// 信息网（设计第 6、6.1 节）：真相只在事实表；所知每人一份，由内核在看见、说话、闲谈、读信、推断时写入。
// 心智和玩家界面只能读自己那份（KnownView），从不读真值；NPC 不订阅事件。
#pragma once

#include "world.hpp"

namespace hanmo::kernel {

// 怎么知道的。
enum class Source : uint8_t { Saw, Lived, Told, Notice, Letter, Inferred, Upbringing };

// 「说的是什么」：种类、关于谁、牵涉的另一方、值、那件事发生的时刻（不是听说的时刻）。
struct Claim {
  ClaimKindId kind;
  Ref about;
  Ref other;
  int64_t value = 0;
  HalfDay when = 0;
};

// 内核自带的所知种类；包从 kFirstPackageClaim 起登记自己的（价格、战况、任免、悬赏……）。
namespace claim {
constexpr ClaimKindId Whereabouts{1};  // 某人在哪
constexpr ClaimKindId Alive{2};        // 某人死活
constexpr ClaimKindId Holds{3};        // 某人或某处有多少某物
constexpr ClaimKindId DidTo{4};        // 谁对谁做了什么（value 为动词）
constexpr ClaimKindId KnowsRecipe{5};  // 会某一招（教养、学来的）
constexpr ClaimKindId Intends{6};      // 某人许诺、威胁要做什么
constexpr uint16_t kFirstPackageClaim = 100;
}  // namespace claim

// 记忆里的一条：内容自带，单独成立；事实过期删掉了，记忆照样在。
struct Memory {
  Claim claim;
  Source source = Source::Saw;
  uint8_t hops = 0;        // 传了几手；亲眼为 0
  uint16_t credence = 0;   // 信几分，0 到 1000
  uint16_t salience = 0;   // 有多要紧：决定遗忘的先后与闲谈先说哪条
  ActorId told_by;         // 谁告诉我的（只知道上一手）
  FactId fact;             // 对应的真相；编造的为 0（无源）。只给内核用，心智读不到
  bool fresh = true;       // 新来的：用来唤醒细想，拿完主意后清掉
};

// 舆论：一个里或一个地方对某人的所知汇总。名声不是属性值，就是这个。
struct Opinion {
  int32_t good = 0;
  int32_t bad = 0;
  uint32_t heard_by = 0;
};

// 心智与玩家界面唯一的查询口：只看此人的所知、自己的身体与手上的东西，不碰真值。
class KnownView {
 public:
  virtual ~KnownView() = default;

  virtual ActorId self() const = 0;
  virtual HalfDay now() const = 0;
  // 看法索引：关于某人某事，我最新相信的那条；不知道返回空。
  virtual const Memory *belief(ClaimKindId, Ref about) const = 0;
  // 全部记忆（上限几百条）。
  virtual Span<Memory> memories() const = 0;
  // 上次拿主意之后新来的记忆。
  virtual Span<MemoryId> fresh() const = 0;
  // 我所在的里怎么看某人（我听得到的乡里舆论）。
  virtual Opinion opinion(ActorId about) const = 0;
  // 自己的身体与手上的东西是亲知的。
  virtual const Body &my_body() const = 0;
  virtual Span<Holding> my_holdings() const = 0;
};

}  // namespace hanmo::kernel
