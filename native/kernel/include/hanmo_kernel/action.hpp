// 基本动作（设计第 2 节）：改变内核状态的唯一入口，相当于系统调用。玩家、NPC、包走同一个口。
// 每个动作先 check（不过就拒绝，世界不变），再 apply（必定成功，并产生事实）。不撤销。
#pragma once

#include <array>

#include "knowledge.hpp"

namespace hanmo::kernel {

// 动词。内核自带 24 个；包登记的自有动词从 kFirstPackageVerb 起编号。
enum class Verb : uint16_t {
  None = 0,
  // 身体
  Walk, Take, Put, Give, Hide, Eat, Work, Rest, Guard, Hit, Care, Mate, Play,
  // 说话
  Tell, Ask, Request, Promise, Threaten, Accept, Refuse, Express,
  // 群体
  Split,  // 从群体分出若干人（指定各阶人数），带等阶走，不带东西
  Merge,  // 并入另一个群体，各阶人数相加
  Name,   // 从群体某阶分出一人，成为具名人物
};
constexpr uint16_t kFirstPackageVerb = 1000;

// 说话的内容：告诉、问的是哪条所知（可以改动、可以编造）；请求、许诺、威胁的是哪个打算。
struct Speech {
  Claim claim;
  bool altered = false;            // 说的人有意改动（说谎）或说错；听的人不知道
  Verb intent = Verb::None;        // 打算让谁做什么
  Ref intent_target;
  int64_t intent_qty = 0;
  HalfDay due = 0;                 // 许诺的期限
};

struct Action {
  ActorId actor;
  Verb verb = Verb::None;
  Ref target;                      // 对谁、去哪
  Ref object;                      // 什么东西、哪个地方
  int64_t qty = 0;                 // 多少；分出时是人数
  uint32_t manner = 0;             // 做法：公开或暗中、姿态（编号由内容定义）
  std::array<uint32_t, kTiers> tiers{};  // 分出时各阶取多少；具名化时取哪一阶
  Speech speech;                   // 说话类动作的内容
  std::array<MemoryId, 4> motive{};      // 动机：做这件事依据的记忆，写进因果网
};

// 提交的结果：被拒绝时世界不变；成功时给出产生的事实。
struct Outcome {
  Reject reject;
  FactId fact;
};

// 预览：纯读，算出会怎样，不写表、不动随机数。
struct PreviewResult {
  Reject reject;
  int64_t qty_effective = 0;  // 实际能做成多少
  HalfDay done_at = 0;        // 要用时间的动作何时做完
};

}  // namespace hanmo::kernel
