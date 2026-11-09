// 公职新职责只沿原成员episode和真实回应序重建，业务域须先核自己的精确事实头。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
class OrganizationAuthorityClock;
// 各明确消费域分别约束允许边界；不能传任意事实或任意历史序号。
enum class PoliticalMemberHistoryUse {
  shop_licensing,
  governance_continuation,
  political_succession,
  political_head_handover,
  political_successor_round,
  political_term_handover,
  political_office_reception,
  political_governance_agency,
  political_subordinate_appointment,
  political_dismissal
};
Status validate_political_historical_member_duty(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const ActingAuthority &, Integer tick, const StableId &actual_fact,
    PoliticalMemberHistoryUse, const DynamicStepInput *phase = nullptr,
    const OrganizationAuthorityClock *clock = nullptr,
    const WorldMoment *actual_boundary = nullptr);
}
