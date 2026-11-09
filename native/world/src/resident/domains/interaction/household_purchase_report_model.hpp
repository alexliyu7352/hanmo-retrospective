// 原 S3 计划专属告知事实；不创建全户或通用 cohort 知识账。
// Actual plan-scoped telling source. Never a general cohort KnownReport.
#pragma once
#include "report_disclosure_types.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/family/household_control.hpp"
#include "resident/domains/local_economy/population_source_ref.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
inline constexpr const char *household_purchase_report_source_kind =
    "household_purchase_report_disclosed";
struct HouseholdPurchaseReportDisclosure {
  StableId id, controller_actor_id, cohort_id, report_id;
  SourceRef purchase_plan_ref, accepted_policy_ref, population_selection_ref;
  Integer accepted_policy_revision = 0;
  HouseholdProvisionControlProof controller_authority;
  SourceRef controller_knowledge_source;
  MomentOwnerSource original_observation;
  WorldMoment controller_learned_at, disclosed_at;
  ReportDisclosurePermission permission;
  CivilianPopulationRef actual_population;
  SourceRef population_location_source;
  CommonActualFactPrefix actual_contact_prefix;
};

// A reference to the original S3 delivery, not a copy of Social knowledge.
struct HouseholdPurchaseScopedKnowledgeRef {
  MomentOwnerSource actual_delivery;
  SourceRef purchase_plan_ref;
  StableId cohort_id, report_id;
  WorldMoment learned_at;
};
struct TellHouseholdPurchaseMarketReport {
  SourceRef purchase_plan_ref;
  StableId report_id;
};
} // namespace hanmo::resident
