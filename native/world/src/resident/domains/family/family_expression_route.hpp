// 原Family表达路由纯DTO；NPC只借已审真实Actor admission SourceRef。
#pragma once
#include "profile_controller_source.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// Family 的 NPC route 保存原 SubjectRef，不用 FSL 的 actor 字符串布局替换。
// Player 分支直接持原控制来源有限联合，不增加 metadata.player 权限捷径。
struct FamilyPlayerExpressionRoute {
  ProfileControllerSource original_control_source_ref;
};
struct FamilyNpcExpressionRoute {
  SubjectRef subject;
  Integer registration_epoch = 0, decision_seq = 0;
  // 原Director5 adoption的actor_admission来源，不虚构该policy的独立version。
  SourceRef policy_ref;
};
using FamilyExpressionRoute =
    std::variant<FamilyPlayerExpressionRoute, FamilyNpcExpressionRoute>;

} // namespace hanmo::resident
