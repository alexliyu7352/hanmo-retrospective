// 有限决策仅保存责任、复评和公平次序；命令交中央重报价并在同一候选中采用。
#pragma once
#include "director_membership_policy.hpp"
#include "director_subject2_model.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_response_model.hpp"
#include "resident/domains/politics/politics_decision_model.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/model/regional_life_contract.hpp"
#include "resident/storage/common_codec.hpp"
#include <functional>
namespace hanmo::resident {
class EconomicSupplyPolicyRead;
struct AffairsState;
struct PersonalAppointmentAttendanceCatalog;
struct PersonalAppointmentAttendanceRead;
struct VisitReceptionCatalog;
struct VisitReceptionRead;
struct OrganizationState;
struct OrganizationCatalog;
struct TaskResponsibilityDirectorContext;
struct SocialState;
struct DirectorMembershipContext;
struct DirectorPoliticalContext;
struct DirectorDiplomaticContext;
struct FamilyDirectorPostProposalContext5;
struct TaskState;
struct TaskCatalog;
struct GrowthCatalog;
struct TravelCatalog;
struct InteractionState;

// 首批计划模板闭合，未知模板不能以任意脚本或隐藏世界查询替代。
enum class DirectorTemplate {
  maintain_commitment,
  replenish_supply,
  fulfill_contract,
  organize_activity,
  await_adjustment,
  private_livelihood,
  political_career,
  diplomatic_response,
  diplomatic_delegation,
  treaty_ratification,
  military_appointment = 10,
  field_appointment = 11,
  political_supply_renewal = 12,
  visit_reception = 13,
  shop_registry_acceptance = 14,
  personal_appointment_attendance = 15,
  governance_commission = 16,
  governance_agency = 17,
  office_resignation_response = 18,
  economic_household = 19,
  economic_employee = 20,
  commerce_operator = 21,
  estate_steward = 22,
  craft_operator = 23,
  institutional_economy = 24,
  political_affairs = 25,
  sect_affairs = 26,
  hideout_affairs = 27,
  fleet_affairs = 28,
  life_growth_care = 29,
  academy_teaching = 30,
  medicine_supply = 31,
  clinical_service = 32,
  scholarship_work = 33,
  mediation_service = 34,
  arcane_craft_ritual = 35,
  expedition_field = 36
};
const char *director_template_id(DirectorTemplate) noexcept;
enum class PlanPhase {
  selecting,
  accepting,
  sourcing,
  purchasing,
  delivering,
  reporting,
  waiting,
  completed
};

// 主体策略中的阈值均是作者给出的开发参数；私人信任不替代组织成员状态。
struct DirectorPolicy {
  StableId organization_id;
  std::vector<DirectorTemplate> templates;
  Integer request_arrear_after_ticks = 1, decline_work_after_ticks = 2,
          leave_after_ticks = 4;
  std::vector<StableId> report_source_keys;
  std::optional<PrivateLivelihoodPolicy> private_livelihood;
  std::optional<StableId> named_membership_policy_id;
  std::vector<NamedRecruitmentPost> recruitment_posts;
  std::optional<PoliticalCareerPolicy> political_career;
  std::optional<DiplomaticResponsePolicy> diplomatic_response;
  std::optional<StableId> diplomatic_delegation_policy_id;
  std::optional<StableId> supply_renewal_intent_id;
  std::optional<StableId> shop_registry_intent_id;
  std::optional<StableId> governance_intent_id;
  std::optional<StableId> governance_agency_intent_id;
  std::optional<StableId> office_resignation_response_intent_id;
};
// 新登记政策只决定复评主体，不授予业务权限、年龄资格或组织职权。
enum class DirectorAdmissionKind {
  all_actors,
  organization_members,
  household_cohorts,
  approved_organizations
};
// Selected immutable content only; complete runtime composition still requires
// each real route.
struct DirectorSubjectsProfile2 {
  bool enabled = false;
  std::vector<StableId> selected_template_ids;
};
struct DirectorAdmission {
  StableId policy_id, organization_id;
  DirectorAdmissionKind kind = DirectorAdmissionKind::all_actors;
  // 新自主成人模板须显式声明成年门限；登记本身不授业务资格。
  Integer minimum_age_years = 0;
  std::vector<StableId> excluded_actor_ids;
};
// 主体退出不删除既有责任；epoch阻止旧登记提案借新的准入继续提交。
enum class DirectorLifecycle { active, paused, retired };
struct DirectorRegistration {
  StableId actor_id, policy_id, admission_id, reason;
  Integer epoch = 1, entered_tick = 0, changed_tick = 0;
  DirectorLifecycle lifecycle = DirectorLifecycle::active;
  bool obligations_only = false;
};
struct TaskAuthoredContinuationCatalog;
struct PoliticalSupplyRenewalCatalog;
struct PoliticalShopRegistryIntentCatalog;
struct PoliticalGovernanceIntentCatalog;
struct PoliticalGovernanceAgencyIntentCatalog;
// 只有明确Agency2/intent1选择才开放17；旧完整调用缺省关闭。
struct DirectorGovernanceAgencyProfile {
  bool enabled = false;
};
struct PoliticalOfficeResignationResponseCatalog;
struct DirectorOfficeResignationResponseProfile {
  bool enabled = false;
};
// 只有原政治治理/Task/公共赈济组合可选安民意图。
struct DirectorGovernanceIntentProfile {
  bool enabled = false;
};
struct DirectorShopRegistryProfile {
  bool enabled = false;
};
struct DirectorSupplyRenewalProfile {
  bool economic_supply_pair = false;
};
struct DirectorMilitaryAppointmentProfile {
  bool enabled = false;
  bool field_enabled = false;
};
struct DirectorCatalog {
  std::shared_ptr<const PoliticalGovernanceIntentCatalog> governance_intents;
  std::shared_ptr<const PoliticalGovernanceAgencyIntentCatalog>
      governance_agency_intents;
  std::shared_ptr<const PoliticalOfficeResignationResponseCatalog>
      office_resignation_responses;
  // 仅已验证独立补任意图目录投影，非持久visitor字段。
  bool subordinate_appointments = false;
  std::shared_ptr<const PoliticalShopRegistryIntentCatalog>
      shop_registry_intents;
  std::shared_ptr<const PersonalAppointmentAttendanceCatalog>
      personal_appointment_attendance;
  std::shared_ptr<const VisitReceptionCatalog> visit_reception;
  std::shared_ptr<const PoliticalSupplyRenewalCatalog> supply_renewals;
  bool military_appointments = false;
  bool field_appointments = false;
  Integer review_interval_ticks = 1, retry_delay_ticks = 1,
          decision_budget = 16, candidates_per_subject = 16;
  OrderedTable<DirectorPolicy> subjects;
  bool regional_life = false, named_membership = false, politics = false;
  bool diplomatic_response = false, diplomatic_delegation = false,
       treaty_passage = false;
  // 仅新显式本人选择政策由受信目录启用；旧政策继续拒绝NPC主体与玩家重叠。
  bool exclude_selected_player = false;
  // 只有显式registry_version=1启用，旧subjects目录/冷布局保持不变。
  bool dynamic_subjects = false;
  bool subjects2 = false;
  bool command_acceptance_bindings = false; // explicit physical director/6 only
  OrderedTable<DirectorPolicy> policies;
  OrderedTable<DirectorAdmission> admissions;
  // 仅显式作者续行已受领责任；空值保旧非采购等待，不生成新的任务或知识。
  std::shared_ptr<const TaskAuthoredContinuationCatalog>
      authored_task_continuation;
};

// 同一主体同一边界至多采用一次决定，拒绝也持久化下次复评，不能免费重抽。
struct ReviewSchedule {
  SubjectRef subject;
  Integer next_tick = 0, decision_seq = 0, waiting_since = 0,
          last_decision_tick = -1;
  std::vector<StableId> wake_reasons;
  StableId last_outcome;
};
struct FinitePlan {
  StableId id, template_id, goal_ref;
  SubjectRef subject;
  Integer step_index = 0, next_review_tick = 0;
  PlanPhase phase = PlanPhase::selecting;
  StableId retry_reason;
  std::optional<StableId> current_action_id, org_activity_id;
  std::vector<StableId> commitment_ids, known_report_ids;
  std::optional<LivelihoodIntent> livelihood;
  std::optional<StableId> pending_life_action_id, last_life_fact_id;
  // 仅动态登记版保存原策略来源；归档不再驱动业务或钉住临时活动。
  StableId policy_id;
};
struct DirectorState {
  std::optional<DirectorSubjects2State> subject2;
  OrderedTable<ReviewSchedule> schedules;
  OrderedTable<FinitePlan> plans;
  Integer fairness_cursor = 0, last_boundary_tick = -1;
  bool regional_life = false, named_membership = false;
  bool dynamic_subjects = false;
  OrderedTable<DirectorRegistration> registrations;
  OrderedTable<FinitePlan> archived_plans;
};

// 已知值保留观察/获知时刻与来源；这不是另一个可支出的库存或工资账本。
struct KnownSupply {
  StableId report_id, fact_id, organization_id, site_id, inventory_id, item_id,
      source_key;
  Integer quantity = 0, target_amount = 0, source_revision = 0,
          observation_tick = 0, learned_tick = 0;
  std::optional<Integer> valid_until_tick;
};
struct KnownArrear {
  StableId report_id, fact_id, arrear_id, organization_id, member_id;
  Integer outstanding = 0, due_tick = 0, observation_tick = 0, learned_tick = 0;
};
struct KnownCommitment {
  StableId id, kind, status, source_fact_id;
  Integer due_tick = 0;
};
// 服务值仅投影作者已知条款及当场正式报价，不携带远地provider状态。
struct KnownServiceOption {
  KnownServiceRef source;
  StableId provider_actor_id, refusal_reason;
  ErrorKind refusal_kind = ErrorKind::rule;
  Integer duration = 0, cost = 0, wage = 0, hp_gain = 0, fatigue_delta = 0;
};
struct KnownSubjectView {
  SubjectRef subject;
  Integer tick = 0, revision = 0;
  StableId actor_id, place_id, settlement_id, current_action_id;
  bool alive = false, available = false, interaction_busy = false;
  std::vector<StableId> locally_occupied_actor_ids;
  std::vector<KnownSupply> supplies;
  std::vector<KnownArrear> arrears;
  std::vector<KnownCommitment> commitments;
  std::vector<KnownServiceOption> services;
  std::vector<StableId> known_report_ids, legacy_fact_ids, known_offer_ids,
      known_need_ids, known_source_keys;
};

// 采用元信息由本域拥有，中央仅在真实命令成功或普通拒绝之后原子登记。
struct DirectorTransition {
  Integer expected_revision = 0, decision_seq = 0, next_review_tick = 0,
          next_step_index = 0;
  StableId template_id, goal_ref;
  PlanPhase phase = PlanPhase::selecting;
  std::optional<StableId> current_action_id;
  std::vector<StableId> known_report_ids, commitment_ids;
  // 本次无命令仍须解释实际等待原因，采用后写入既有计划retry_reason。
  StableId wait_reason;
  std::optional<LivelihoodIntent> livelihood;
  std::optional<StableId> pending_life_action_id, last_life_fact_id;
  Integer registration_epoch = 0;
};

// 投影只借当前候选；旧记忆与offer发现保留原所有者，调用方不能取得未获知资源。
KnownSubjectView
known_subject_view(const CommonStore &, const SocialState &,
                   const OrganizationRead &, const StableId &actor_id,
                   const InteractionState *interaction = nullptr);
Outcome<DirectorState> make_initial_director(const CommonStore &,
                                             const DirectorCatalog &);
// 动态主体只从当前共同身份与真实组织成员解析，失效时保持原冻结责任。
const DirectorPolicy *director_policy(const DirectorState &,
                                      const DirectorCatalog &,
                                      const StableId &);
bool director_subject_active(const DirectorState &, const StableId &);
// Only a fresh, successful original diplomatic reply preview may cross
// conversation occupancy.
using DirectorConversationResponse =
    std::function<Outcome<bool>(const StableId &, const DirectorPolicy &)>;
Status reconcile_director_registry(const CommonStore &, DirectorState &,
                                   const DirectorCatalog &,
                                   const OrganizationRead &, const TaskState &,
                                   const ActivityRead &);
Status
reconcile_director_registry(const CommonStore &, DirectorState &,
                            const DirectorCatalog &, const OrganizationRead &,
                            const TaskState &, const ActivityRead &,
                            const DirectorConversationResponse &response);
Status validate_director_registry(const CommonStore &, const DirectorState &,
                                  const DirectorCatalog &);
Outcome<std::vector<DecisionProposal>>
propose_director(const CommonStore &, const DirectorState &,
                 const DirectorCatalog &, const KnownSubjectView &,
                 const OrganizationRead &, const AffairsState &, Integer tick);
// 完整履约提案显式借用真实合同/服务目录；无借用的入口只生成受控成员响应或等待。
Outcome<std::vector<DecisionProposal>>
propose_director(const CommonStore &, const DirectorState &,
                 const DirectorCatalog &, const KnownSubjectView &,
                 const OrganizationRead &, const AffairsState &,
                 const TaskState &, const TaskCatalog &, const CommonCatalog &,
                 const GrowthCatalog &, const TravelCatalog &,
                 const OrganizationState &, const OrganizationCatalog &,
                 Integer tick, const GrowthAccess *growth_access = nullptr,
                 const GrowthLifeRead *life = nullptr,
                 const DirectorMembershipContext *membership = nullptr,
                 const DirectorPoliticalContext *politics = nullptr);
Outcome<std::vector<DecisionProposal>> propose_director(
    const CommonStore &, const DirectorState &, const DirectorCatalog &,
    const KnownSubjectView &, const OrganizationRead &, const AffairsState &,
    const TaskState &, const TaskCatalog &, const CommonCatalog &,
    const GrowthCatalog &, const TravelCatalog &, const OrganizationState &,
    const OrganizationCatalog &, Integer tick,
    const GrowthAccess *growth_access, const GrowthLifeRead *life,
    const DirectorMembershipContext *membership,
    const DirectorPoliticalContext *politics,
    const DirectorDiplomaticContext *diplomacy,
    const EconomicSupplyPolicyRead *issued_supply = nullptr,
    const TaskResponsibilityDirectorContext *task_responsibility = nullptr,
    const VisitReceptionRead *visit_reception = nullptr,
    const PersonalAppointmentAttendanceRead *appointment_attendance = nullptr,
    const FamilyDirectorPostProposalContext5 *family_post = nullptr);
Status record_director_result(DirectorState &, const DecisionProposal &,
                              const DecisionOutcome &, Integer tick);
// 排序只返回有限待复评主体；持久waiting_since优先，轮转游标打破同级竞争。
Outcome<std::vector<StableId>> director_due_subjects(const DirectorState &,
                                                     const DirectorCatalog &,
                                                     Integer tick);
// 真实责任先按履约级别/期限排序，忙碌人物的原行动自行推进，不占本轮普通评估预算。
Outcome<std::vector<StableId>>
director_due_subjects(const CommonStore &, const DirectorState &,
                      const DirectorCatalog &, const TaskState &, Integer tick);
// 新选择政策的原玩家由已验证FamilyState.founder传入；初始化空值沿当前玩家，旧政策忽略此值。
Status validate_director(const CommonStore &, const DirectorState &,
                         const DirectorCatalog &,
                         const GrowthLifeRead *life = nullptr,
                         const StableId &initial_player_id = {});
Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue &,
                                                 bool regional_life = false,
                                                 bool named_membership = false,
                                                 bool politics = false);
Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue &,
                                                 bool regional_life,
                                                 bool named_membership,
                                                 bool politics, bool diplomacy,
                                                 Integer diplomacy_version = 1);
Outcome<DirectorCatalog>
decode_director_catalog(const BoundaryValue &, bool regional_life,
                        bool named_membership, bool politics, bool diplomacy,
                        Integer diplomacy_version, bool treaty_passage);
Outcome<DirectorCatalog>
decode_director_catalog(const BoundaryValue &, bool regional_life,
                        bool named_membership, bool politics, bool diplomacy,
                        Integer diplomacy_version,
                        DirectorMilitaryAppointmentProfile);
Outcome<DirectorCatalog>
decode_director_catalog(const BoundaryValue &, bool regional_life,
                        bool named_membership, bool politics, bool diplomacy,
                        Integer diplomacy_version, bool treaty_passage,
                        DirectorMilitaryAppointmentProfile);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile,
    std::shared_ptr<const VisitReceptionCatalog> visit_reception = nullptr,
    bool personal_appointment_attendance_enabled = false);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile, std::shared_ptr<const VisitReceptionCatalog>,
    DirectorShopRegistryProfile,
    bool personal_appointment_attendance_enabled = false);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile, std::shared_ptr<const VisitReceptionCatalog>,
    DirectorShopRegistryProfile, bool personal_appointment_attendance_enabled,
    DirectorGovernanceIntentProfile);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile, std::shared_ptr<const VisitReceptionCatalog>,
    DirectorShopRegistryProfile, bool personal_appointment_attendance_enabled,
    DirectorGovernanceIntentProfile, DirectorGovernanceAgencyProfile);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile, std::shared_ptr<const VisitReceptionCatalog>,
    DirectorShopRegistryProfile, bool personal_appointment_attendance_enabled,
    DirectorGovernanceIntentProfile, DirectorGovernanceAgencyProfile,
    DirectorOfficeResignationResponseProfile);
Outcome<DirectorCatalog> decode_director_catalog(
    const BoundaryValue &, bool regional_life, bool named_membership,
    bool politics, bool diplomacy, Integer diplomacy_version,
    bool treaty_passage, DirectorMilitaryAppointmentProfile,
    DirectorSupplyRenewalProfile, std::shared_ptr<const VisitReceptionCatalog>,
    DirectorShopRegistryProfile, bool personal_appointment_attendance_enabled,
    DirectorGovernanceIntentProfile, DirectorGovernanceAgencyProfile,
    DirectorOfficeResignationResponseProfile, const DirectorSubjectsProfile2 &);
Outcome<DirectorState> decode_director_seed(const BoundaryValue &,
                                            bool regional_life = false,
                                            bool named_membership = false,
                                            bool dynamic_subjects = false);
BoundaryValue encode_director_state(const DirectorState &);
Status visit_director_checkpoint(CodecVisitor &, DirectorState &,
                                 bool regional_life = false,
                                 bool named_membership = false,
                                 bool dynamic_subjects = false);
} // namespace hanmo::resident
