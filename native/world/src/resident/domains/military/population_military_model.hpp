// 预备人口与既有军籍的有限来源合同；保存绑定和收据，不另存可用兵力或钱货。
#pragma once
#include "field_battle/field_spatial_view.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/politics/takeover/politics_occupation_contract.hpp"
namespace hanmo::resident {
// 名册分类沿原军事权威；released只用于永久来源史，不是新增作战状态。
enum class PopulationSoldierStatus {
  fit,
  wounded,
  dead,
  captured,
  deserted,
  released
};
// 服役许可独立于训练许可；作者明确给出接收组织、营地、有效期和同时服役人数上限。
struct PopulationServiceConsent {
  StableId id, cohort_id, organization_id, garrison_id;
  Integer from_tick = 0, through_tick = 0, max_active_count = 0;
};
struct PopulationMilitarySource {
  StableId id, organization_id, organization_office_id, military_office_id,
      garrison_id, unit_id;
  StableId reserve_cohort_id, serving_cohort_id, wounded_cohort_id,
      captive_cohort_id, displaced_cohort_id;
  StableId home_site_id, household_group_id, service_consent_source_id;
  std::vector<StableId> wage_obligation_ids;
  Integer capacity = 0, initial_training = 0, initial_fatigue = 0,
          initial_morale = 70;
};
// 接收许可逐一绑定原军籍、接收营地、独立服役同意和新工资义务。
struct PopulationMilitaryIntegrationPolicy {
  StableId id, source_id, receiving_organization_id, receiving_garrison_id,
      service_consent_source_id;
  std::vector<StableId> wage_obligation_ids, ration_obligation_ids;
  StableId target_stronghold_id;
};
struct PopulationMilitaryCatalog {
  bool enabled = false;
  OrderedTable<PopulationServiceConsent> consents;
  OrderedTable<PopulationMilitarySource> sources;
  OrderedTable<PopulationMilitaryIntegrationPolicy> integration_policies;
  // Author selector only: absent/0 retains legacy siege capability-derived 1/2.
  // The JSON author decoder never accepts explicit 0.
  Integer selected_version = 0;
};
// 训练和服役分别授权；请求数量、劳力与军籍slot不从展示文字反推。
struct PopulationEnrollCommand {
  ActingAuthority authority;
  StableId source_id;
  Integer count = 0, labor_count = 0;
  std::vector<StableId> release_allocation_ids;
  FieldOrder field_order;
};
struct PopulationReleaseCommand {
  ActingAuthority authority;
  StableId enrollment_id;
  FieldOrder field_order;
};
using PopulationMilitaryCommand =
    std::variant<PopulationEnrollCommand, PopulationReleaseCommand>;
struct PopulationMilitaryTerms {
  StableId source_id, organization_id, unit_id, garrison_id, place_id,
      household_group_id;
  StableId reserve_cohort_id, serving_cohort_id, actor_id, military_office_id,
      military_appointment_id;
  AuthorityProof authority;
  Integer count = 0, labor_count = 0;
  std::vector<StableId> member_ids, release_allocation_ids;
  StableId enrollment_id;
};
struct PopulationMilitaryQuote {
  Integer duration = 1;
  std::vector<StableId> participants;
  PopulationMilitaryTerms terms;
};
// 永久军籍身份不会因退籍从历史中擦除；每个token始终归属于原家户及劳力资格。
struct PopulationSoldierOrigin {
  StableId member_id, source_cohort_id, household_group_id;
  bool labor_eligible = false;
};
struct PopulationEnrollment {
  StableId id, source_id, action_id, unit_id, actor_id;
  Integer completed_tick = 0;
  PopulationMilitaryTerms terms;
  std::vector<PopulationSoldierOrigin> members;
};
struct PopulationMilitaryAction {
  StableId action_id, kind;
  PopulationMilitaryCommand request;
  PopulationMilitaryTerms terms;
};
struct PopulationMilitaryActionFact {
  StableId id, kind, actor_id, phase = "started", code, receipt_id;
  Integer started_tick = 0, completed_tick = -1;
  bool success = false;
  PopulationMilitaryCommand request;
  PopulationMilitaryTerms terms;
};
// from/to是实际人口转移或死亡sink；成员分类只能由登记/退籍动作或原军事伤亡收据证明。
struct PopulationMilitaryMovement {
  StableId member_id, enrollment_id, from_cohort_id, to_cohort_id,
      captor_army_id;
  PopulationSoldierStatus before = PopulationSoldierStatus::fit,
                          after = PopulationSoldierStatus::fit;
};
struct PopulationMilitaryFact {
  StableId id, kind, source_id, action_id, military_receipt_id;
  Integer tick = 0;
  std::vector<PopulationMilitaryMovement> movements;
  std::vector<StableId> population_receipt_ids;
  // Physical /3 tail. Legacy source rows retain absence; tick is only the
  // old population receipt projection, never a reconstructed exact moment.
  std::optional<WorldMoment> occurred_at;
  Integer military_source_schema = 1;
};
// 续接不移动人数；只追加接收许可和旧新工资责任的实际交接来源。
struct PopulationMilitaryIntegrationFact {
  StableId id, military_receipt_id, policy_id, source_id, unit_id, from_army_id,
      to_army_id, service_consent_source_id;
  Integer tick = 0;
  std::vector<StableId> member_ids, old_wage_obligation_ids,
      new_wage_obligation_ids;
  std::vector<Integer> next_due_ticks, wage_remainders,
      receiving_wage_remainders_before;
  std::vector<StableId> old_ration_obligation_ids, new_ration_obligation_ids,
      ration_provision_binding_ids;
  std::vector<Integer> ration_next_due_ticks, ration_remainders,
      receiving_ration_remainders_before;
};
struct PopulationMilitaryState {
  OrderedTable<PopulationEnrollment> enrollments;
  OrderedTable<PopulationMilitaryAction> actions;
  OrderedTable<PopulationMilitaryActionFact> action_facts;
  OrderedTable<PopulationMilitaryFact> facts;
  OrderedTable<PopulationMilitaryIntegrationFact> integration_facts;
};
struct PopulationMilitaryEffects {
  std::vector<StableId> receipt_ids, fact_ids;
};
struct PopulationMilitaryResult {
  bool success = false;
  StableId code, receipt_id, enrollment_id, unit_id;
};
// 死亡与责任覆盖是验证原始收据后取得的只读投影，不能持久化为另一份人口台账。
struct PopulationMilitaryLoss {
  StableId fact_id, military_receipt_id, member_id, cohort_id;
  Integer tick = 0;
  bool labor_eligible = false;
};
// 军供读值保持实际位置分支；在战不是冻结 OnRoute，也不新增持久人口账。
using PopulationMilitarySupplyLocation =
    std::variant<Location, FieldBattleSpatialView>;
struct PopulationMilitaryProvision {
  StableId source_id, cohort_id, member_id, army_id, garrison_id, inventory_id;
  PopulationSoldierStatus status = PopulationSoldierStatus::fit;
  bool labor_eligible = false;
  PopulationMilitarySupplyLocation location;
};
struct PopulationContinuity {
  std::vector<PopulationSoldierOrigin> origins;
  std::vector<PopulationMilitaryFact> events;
  std::vector<PopulationMilitaryLoss> losses;
  std::vector<PopulationMilitaryProvision> provisions;
};
// 所有引用必须来自同一候选；上下文不拥有状态，也不得越过中央事务发布。
struct FieldBattleWorldState;
struct NavalState;
struct MilitaryExactState;
struct PopulationTransportState;
struct ProductionPopulationChangeContext;
class CurrentMomentRead;
class DynamicMilitaryLandConsequenceSources;
// 回调只借当前同候选的政治原占领；空函数仅用于未启用该作者模块的旧profile。
using PopulationRecruitmentOccupationRead =
    std::function<Outcome<std::optional<PoliticalRecruitmentOccupation>>(
        const StableId &)>;
using PopulationRecruitmentOccupationHistoryRead =
    std::function<Outcome<std::optional<PoliticalRecruitmentOccupation>>(
        const StableId &, Integer, const StableId &)>;
struct PopulationMilitaryReadContext {
  const CommonStore &common;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const MilitaryState &military;
  const MilitaryCatalog &military_catalog;
  const PopulationMilitaryState &state;
  const PopulationMilitaryCatalog &catalog;
  // 仅内部半日阶段可指定下一tick；冷准入留空并严格核当前已提交时刻。
  std::optional<Integer> phase_finish_tick;
  const NavalState *naval_combat = nullptr;
  PopulationRecruitmentOccupationRead recruitment_occupation;
  PopulationRecruitmentOccupationHistoryRead recruitment_occupation_before;
  Integer selected_version = 1;
  const MilitaryExactState *military_exact = nullptr;
  const CurrentMomentRead *actual_clock = nullptr;
  const DynamicMilitaryLandConsequenceSources *dynamic_land_sources = nullptr;
  bool actual_integration = false;
  const PopulationTransportState *population_transport = nullptr;
  // 同 World 的实际 Field 根；只借用，不以空 legacy Battle 替代。
  const FieldBattleWorldState *field_battle = nullptr;
};
struct PopulationMilitaryWriteContext {
  CommonStore &common;
  LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  MilitaryState &military;
  const MilitaryCatalog &military_catalog;
  PopulationMilitaryState &state;
  const PopulationMilitaryCatalog &catalog;
  std::optional<Integer> phase_finish_tick;
  const NavalState *naval_combat = nullptr;
  PopulationRecruitmentOccupationRead recruitment_occupation;
  PopulationRecruitmentOccupationHistoryRead recruitment_occupation_before;
  Integer selected_version = 1;
  const MilitaryExactState *military_exact = nullptr;
  const CurrentMomentRead *actual_clock = nullptr;
  const DynamicMilitaryLandConsequenceSources *dynamic_land_sources = nullptr;
  bool actual_integration = false;
  PopulationTransportState *population_transport = nullptr;
  ProductionPopulationChangeContext *production_inputs = nullptr;
  const FieldBattleWorldState *field_battle = nullptr;
  PopulationMilitaryReadContext read() const {
    return {common,
            economy,
            economy_catalog,
            organization,
            organization_catalog,
            military,
            military_catalog,
            state,
            catalog,
            phase_finish_tick,
            naval_combat,
            recruitment_occupation,
            recruitment_occupation_before,
            selected_version,
            military_exact,
            actual_clock,
            dynamic_land_sources,
            actual_integration,
            population_transport,
            field_battle};
  }
};
// 只读退籍权限沿原任职或唯一真实接收来源解析，不修改原source与家户。
struct PopulationReleaseAuthority {
  StableId organization_id, military_office_id, required_organization_office_id,
      integration_receipt_id;
};
Outcome<PopulationReleaseAuthority> population_military_release_authority(
    const PopulationMilitaryReadContext &, const PopulationMilitarySource &,
    Integer tick, const StableId &before_fact_id = {});
Status validate_population_military_return_source(
    const PopulationMilitaryReadContext &, const StableId &receipt_id);
// 明确政策报价和同候选续接；旧欠款、家户、劳力与死亡不改写。
Status
preview_population_military_integration(const PopulationMilitaryReadContext &,
                                        const SiegeIntegrationProposal &);
Outcome<std::vector<StableId>>
apply_population_military_integration(PopulationMilitaryWriteContext &,
                                      const MilitaryIntegrationFact &);
Status validate_population_military_integrations(
    const PopulationMilitaryReadContext &);
// 新能力检查点2显式写入续接事实，旧检查点1不追加字段。
Status visit_population_military_checkpoint(CodecVisitor &,
                                            PopulationMilitaryState &,
                                            bool integration);
} // namespace hanmo::resident
