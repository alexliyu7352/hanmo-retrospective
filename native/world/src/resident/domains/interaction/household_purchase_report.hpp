// 唯一计划买方的原消息接口：借同 World 来源，区分完整冷验与非递归时间投影。
// One actual local disclosure to the buyer of one already accepted plan.
#pragma once
#include "household_purchase_report_model.hpp"
#include "report_disclosure.hpp"
#include "resident/domains/local_economy/household_cohort_environment.hpp"
#include "resident/domains/social/actor_report_knowledge.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
namespace hanmo::resident {
struct InteractionState;
struct HouseholdPurchaseDisclosureHistorySources;
class ProjectionWriter;
class FieldBattleExitPlacementRead;
Outcome<TellHouseholdPurchaseMarketReport>
decode_tell_household_purchase_market_report(const BoundaryValue &);
Status emit_tell_household_purchase_market_report_fields(
    ProjectionWriter &, const TellHouseholdPurchaseMarketReport &);
struct HouseholdPurchaseReportContext {
  const CommonStore &common;
  const InteractionState &interaction;
  const HouseholdPurchaseContext &purchase;
  const HouseholdCohortEnvironmentContext &population;
  const ActorSpatialHeadSourceReadContext &actors;
  const ReportDisclosureReadContext &disclosure;
  const ActorReportKnowledgeContext &controller_knowledge;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
  const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr;
  // 显式 Face3 必借真实历史，不用今日角色/身体反证过去告知。
  const HouseholdPurchaseDisclosureHistorySources *controller_history = nullptr;
  // 真Field入场/释放尾借原S1完整history；缺源不能把旧anchor当原接触。
  const FieldBattleExitPlacementRead *field_exits = nullptr;
};
struct HouseholdPurchaseReportView {
  const HouseholdPurchaseReportDisclosure *delivery = nullptr;
  const HouseholdPurchasePlan *plan = nullptr;
  const Report *report = nullptr;
  const SupplyObservation *observation = nullptr;
};
// Actor comes from the authenticated Envelope. M and receiver are never
// payload. Failure abandons the enclosing atomic World candidate.
// 与 writer 共用完整原 pre-ID 算法；不分配 ID、不写事实，不缓存准入结果。
Status validate_tell_household_purchase_market_report_input(
    const HouseholdPurchaseReportContext &,
    const StableId &authenticated_controller,
    const TellHouseholdPurchaseMarketReport &);
// 真命令结果直接借本候选 Common 事实；拥有 ID 的预留贯穿完整写入。
Outcome<const FactRecord *>
tell_household_purchase_market_report_fact_in_candidate(
    CommonStore &, InteractionState &, const HouseholdPurchaseReportContext &,
    const StableId &authenticated_controller,
    const TellHouseholdPurchaseMarketReport &);
Outcome<StableId> tell_household_purchase_market_report_in_candidate(
    CommonStore &, InteractionState &, const HouseholdPurchaseReportContext &,
    const StableId &authenticated_controller,
    const TellHouseholdPurchaseMarketReport &);
// Full original proof, including historical sender knowledge and real contact.
Outcome<HouseholdPurchaseReportView>
read_household_purchase_report_source(const HouseholdPurchaseReportContext &,
                                      const SourceRef &, Integer schema = 1);
// 同一真实源的有限保全入口；所有借用只在同步 callback 内有效。
// typed 行不伪造 SourceRef/schema，中央 mapper 必须使用它们原拥有表的身份。
// 回调不得修改根；任一失败须丢弃整批输出。控制者历史另走已有两种 sink。
enum class HouseholdPurchaseReportProvenanceRole {
  actual_disclosure,
  original_observation,
  original_permission,
  actor_location,
  population_location
};
class HouseholdPurchaseReportProvenanceSink {
public:
  virtual ~HouseholdPurchaseReportProvenanceSink() = default;
  virtual Status household(const HouseholdPurchaseContext &,
                           const HouseholdPurchasePlan &,
                           const HouseholdPurchasePolicy &,
                           const PopulationSelection &,
                           const LaborAllocation &) = 0;
  virtual Status social(const SocialState &, const Report &,
                        const ActorReportKnowledgeReceiptView &) = 0;
  // 原 reader 的完整检查投影含 count sources（可不同于 revision 的 owner）。
  // 此投影仅同步借用；其中 SourceRef 没有 schema 时不得自行补默认版本。
  virtual Status population(const HouseholdCohortEnvironmentContext &,
                            const CivilianPopulationRef &,
                            const PopulationRevisionView &) = 0;
  virtual Status population_revision(const PopulationTransportState &,
                                     const PopulationRevisionSource &,
                                     const MomentOwnerSource &,
                                     bool before_actual_disclosure) = 0;
  virtual Status source(HouseholdPurchaseReportProvenanceRole,
                        const SourceRef &, Integer actual_schema) = 0;
};
Status visit_household_purchase_report_provenance(
    const HouseholdPurchaseReportContext &, const SourceRef &,
    Integer actual_schema, HouseholdPurchaseReportProvenanceSink &);
// Only this accepted plan can consume the disclosed report; no generic
// knowledge.
Outcome<std::optional<HouseholdPurchaseReportView>>
read_known_household_purchase_report(const HouseholdPurchaseReportContext &,
                                     const SourceRef &plan,
                                     const StableId &inventory,
                                     const StableId &site, const StableId &item,
                                     const WorldMoment &now);
Status validate_household_purchase_report_sources(
    const HouseholdPurchaseReportContext &);
// Nonrecursive P10 time/body read; not a substitute for the full proof above.
Outcome<const HouseholdPurchaseReportDisclosure *>
read_original_household_purchase_report_source(const CommonStore &,
                                               const InteractionState &,
                                               const SourceRef &,
                                               Integer schema = 1);
Outcome<ExactOwnerFactRead>
project_household_purchase_report_source(const CommonStore &,
                                         const InteractionState &,
                                         const SourceRef &, Integer schema = 1);
} // namespace hanmo::resident
