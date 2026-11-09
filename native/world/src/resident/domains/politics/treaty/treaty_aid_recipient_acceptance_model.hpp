// 原本人同意的持久值型；根拥有体不因此包含 S4/S5 历史读取上下文。
#pragma once
#include "treaty_resource_signature2.hpp"
namespace hanmo::resident {
struct TreatyAidRecipientAcceptance {
  StableId id, actor_id, term_id;
  MomentOwnerSource proposal;
  TreatyResourceKnowledge knowledge;
  // 本人接受进入自己的原私产容器时为空；否则必须显式原 Steward 事实，
  // 不借采购授权或推定家户成员身份。
  std::optional<SourceRef> household_steward_source;
  WorldMoment accepted_at;
};
} // namespace hanmo::resident
