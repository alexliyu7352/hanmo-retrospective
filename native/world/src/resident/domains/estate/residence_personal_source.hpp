// 原本人表达与成功命令封口；不替代 host 准入住权或 S0 床位采用。
#pragma once
#include "residence_service_contract.hpp"
#include "resident/domains/actor_foundation/character_expression.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
namespace hanmo::resident {
struct ResidencePersonalContext {
  const CommonStore &common;
  const CharacterConditionSources &body;
  const ProfileControlEligibilitySources &control;
  const CurrentMomentRead &clock;
};
Outcome<WorldMoment> read_current_residence_personal_actor(
    const ResidencePersonalContext &, const StableId &actual_actor,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
// 只核本 owner 真结果与原 S0 成功封口相互对应；历史身体/获知仍须上层
// 原 source reader 同时核，不能把此函数当完整同意/历史控制授权。
Status validate_residence_original_command_result(
    const CommonStore &, const SourceRef &actual_owner_source,
    const SourceRef &actual_command_source, const StableId &actual_actor,
    const StableId &estate_id, const WorldMoment &actual_moment);
} // namespace hanmo::resident
