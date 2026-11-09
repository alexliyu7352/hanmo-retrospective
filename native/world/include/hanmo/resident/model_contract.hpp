// 常驻行旅第一版的共享读取/写入边界；仅声明现有规则需要的类型，不暴露ECS句柄。
#pragma once
#include "hanmo/projection_types.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <string_view>

namespace hanmo::resident {
struct ActorFoundationCatalog;
struct ExactTimeProfile;
struct WorldTimelineState;
struct ActionElapsedStore;
struct FiscalSettlementCatalog;
struct FiscalSettlementState;
struct FiscalTaxpayerContinuityState;
struct PublicReliefCatalog;
struct PublicReliefState;
using StableId = std::string;
using Integer = std::int64_t;
enum class DynamicProfile { disabled, local_society_v1 };
inline constexpr const char *dynamic_ruleset_id =
    "hanmo.native.local_society/1";
// 字段序只是冷边界元数据，随typed记录移动；不参与owner业务相等或作为第二份字段值。
using FieldOrder = std::vector<std::string>;
using WorldId = std::array<std::uint8_t, 16>;
inline constexpr std::uint32_t contract_version = 2;

// 世界身份保存在检查点；运行代次只在成功替换世界后变化，不能替代world_id。
struct WorldIdentity {
  WorldId world_id{};
  Integer execution_epoch = 0, revision = 0, tick = 0;
};
// 权威位置是整数；暂态地形标量运算按原binary64顺序，屏幕Vector2不回写此处。
struct PointI {
  Integer x = 0, y = 0;
};
// Borrowed original first-copy inputs only; not alternate stored locations.
struct SettlementLocationBuildView {
  std::string_view settlement_id, place_id;
};
struct InArmyLocationBuildView {
  std::string_view army_id;
};
struct SettlementLocation {
  StableId settlement_id, place_id;
};
struct WildernessLocation {
  StableId map_id, origin;
  PointI point;
};
struct FollowingLocation {
  StableId party_id;
};
struct InArmyLocation {
  StableId army_id;
};
// 旧路网可以无损识别；本profile未支持时须明确报能力错误，不能偷偷换成地形旅行。
struct OnRouteLocation {
  StableId edge_id, origin;
  Integer progress = 0;
};
struct CarriedLocation {
  enum class CarrierKind { transport_unit, vessel, fleet };
  CarrierKind carrier_kind = CarrierKind::transport_unit;
  StableId carrier_id;
};
using Location =
    std::variant<SettlementLocation, WildernessLocation, FollowingLocation,
                 InArmyLocation, OnRouteLocation, CarriedLocation>;

// 普通领域拒绝保旧code/path；未实现能力与内部故障必须中止整候选，不能当NPC普通拒绝。
enum class ErrorKind { rule, unsupported_capability, internal };
// 原安全离城拒绝的有限说明仍为typed；不把任意错误字典留进候选。
struct RuleError {
  Error original;
  ErrorKind kind = ErrorKind::rule;
  StableId phase, domain, command, source_id;
  std::optional<StableId> army_id{}, reason{}, limit{}, feasibility{};
  std::optional<Integer> maximum{}, new_total{}, required_level{},
      actual_level{};
  std::optional<StableId> skill_id{}, actor_id{}, battle_id{};
  bool battle_id_after_path = false;
};
// 结果不带部分成功；各域复用同一错误分类，不自造另一种fallback协议。
template <class T> struct Outcome {
  bool ok = false;
  T value{};
  RuleError error{};
};
struct Done {};
using Status = Outcome<Done>;

// 读取端口中的对象由唯一候选拥有；开放但只读的兼容字段由A保全，不塞整世界字典。
struct CompatibilityFields;
struct Owner {
  enum class Kind { actor, organization, task, household };
  Kind kind = Kind::actor;
  StableId id;
  std::shared_ptr<const CompatibilityFields> extra;
  FieldOrder field_order{};
};
struct ActorCoreView {
  StableId id, action_id, wallet_id, inventory_id;
  bool alive = false, available = false;
  Integer hp = 0, fatigue = 0;
  Location location;
};
// 行动头由A唯一拥有；各kind的typed参数分别在所属域，以同一action_id关联。
struct ActionCoreView {
  StableId id, kind, actor_id;
  std::vector<StableId> participants;
  Integer start_tick = 0, remaining_ticks = 0;
};
// 读版本为可重建缓存标记，不是世界ID或新存档字段；每次候选相关写入即失效。
struct ResourceKey {
  enum class Kind { actor, wallet, inventory, action, party, army };
  Kind kind = Kind::actor;
  StableId id;
};
struct ReadVersion {
  ResourceKey resource;
  std::uint64_t version = 0;
};
// 旧字符串构造只代表action；不同种类即使ID相同也不是同一预留。
struct ReservationRef {
  enum class Kind { action, contract, org_activity, domain_activity };
  Kind kind = Kind::action;
  StableId id, domain_id, activity_kind;
  ReservationRef() = default;
  ReservationRef(StableId action) : id(std::move(action)) {}
  ReservationRef(const char *action) : id(action) {}
  ReservationRef(Kind source_kind, StableId source_id,
                 StableId owner_domain = {}, StableId activity = {})
      : kind(source_kind), id(std::move(source_id)),
        domain_id(std::move(owner_domain)), activity_kind(std::move(activity)) {
  }
  bool empty() const { return id.empty(); }
  void clear() {
    kind = Kind::action;
    id.clear();
    domain_id.clear();
    activity_kind.clear();
  }
  bool operator==(const ReservationRef &other) const {
    return kind == other.kind && domain_id == other.domain_id &&
           activity_kind == other.activity_kind && id == other.id;
  }
  bool operator!=(const ReservationRef &other) const {
    return !(*this == other);
  }
  bool operator<(const ReservationRef &other) const {
    if (kind != other.kind)
      return kind < other.kind;
    if (domain_id != other.domain_id)
      return domain_id < other.domain_id;
    if (activity_kind != other.activity_kind)
      return activity_kind < other.activity_kind;
    return id < other.id;
  }
};
struct Reservation {
  ReservationRef action_id;
  Integer amount = 0;
};
struct WalletView {
  StableId id;
  Owner owner;
  Integer balance = 0;
  std::vector<Reservation> reserved;
  FieldOrder field_order{};
};
// 精确选批不携带权限或版本；这些只由真实recipe的owners和expected_versions给出。
struct CargoSelection {
  StableId inventory_id, lot_id;
  Integer quantity = 0;
};
struct LotPropertiesRef {
  StableId properties_id;
};
struct LotView {
  StableId id, item_id;
  ReservationRef reserved_by;
  Integer quantity = 0;
  Owner owner;
  std::shared_ptr<const CompatibilityFields> extra;
  FieldOrder field_order{};
  std::optional<StableId> item_instance_id;
  std::optional<LotPropertiesRef> lot_properties_ref;
};
struct InventoryView {
  StableId id;
  Owner owner;
  Integer capacity = 0, revision = 0;
  std::vector<LotView> lots;
  FieldOrder field_order{};
};

// 缺ID返回nullptr；借用只在当前未发生写入的phase片段有效。所有列表保原序，不隐式排序。
class CommonRead {
public:
  virtual ~CommonRead() = default;
  // 当前空间占用优先于冻结 Location；旧 profile 的有限读者保持未绑定。
  virtual Outcome<bool> field_battle_bound(const StableId &) const {
    return {true, false, {}};
  }
  virtual const ActorCoreView *actor(const StableId &) const = 0;
  virtual const ActionCoreView *action(const StableId &) const = 0;
  virtual const WalletView *wallet(const StableId &) const = 0;
  virtual const InventoryView *inventory(const StableId &) const = 0;
  // 活动军团成员优先、再party；合法独行必返回本人单元素列表，军俘不算行动成员。
  virtual const std::vector<StableId> *members(const StableId &) const = 0;
  virtual const std::vector<StableId> &actor_order() const = 0;
  virtual const std::vector<StableId> &action_order() const = 0;
  virtual Outcome<Integer>
  inventory_weight(const StableId &inventory_id) const = 0;
  virtual ReadVersion read_version(const ResourceKey &) const = 0;
  virtual Outcome<Integer> spendable(const StableId &wallet_id) const = 0;
  virtual Integer tick() const = 0;
  virtual Integer revision() const = 0;
};

// A定义实际资源recipe及actor变更；原子应用同一候选，业务规则归领域，不允许分域独立余额。
struct ResourceRecipe;
struct ActorMutation;
struct ActionMutation;
struct FactDraft;
// Actual transient ID output: one final string, never a second counter or
// ledger.
struct ProducedStableIdBuildView {
  std::string_view prefix, decimal;
};
class CommonStore;
class ProducedStableId {
  CheckpointMemoryCharge copied_fields_; // first: survives value_ destruction
  StableId value_;
  explicit ProducedStableId(const ProducedStableIdBuildView &);
  friend class CommonStore;

public:
  ProducedStableId() noexcept = default;
  ProducedStableId(const ProducedStableId &) = delete;
  ProducedStableId &operator=(const ProducedStableId &) = delete;
  ProducedStableId(ProducedStableId &&) noexcept = default;
  ProducedStableId &operator=(ProducedStableId &&) = delete;
  const StableId &id() const noexcept { return value_; }
};
class CommonWrite : public CommonRead {
public:
  virtual Status apply_resources(const ResourceRecipe &) = 0;
  virtual Status apply_actor(const ActorMutation &) = 0;
  virtual Status apply_action(const ActionMutation &) = 0;
  virtual Status append_fact(const FactDraft &) = 0;
  virtual Status append_fact_borrowed(std::string_view id,
                                      std::string_view kind,
                                      std::string_view domain) = 0;
  virtual Outcome<StableId> next_id(const StableId &prefix) = 0;
  virtual Outcome<ProducedStableId> next_owned_id(std::string_view prefix) = 0;
  virtual Outcome<Integer> random_bounded(const StableId &stream,
                                          Integer inclusive_min,
                                          Integer inclusive_max) = 0;
};

// 各域真实状态由对应lane定义、clone、校验和codec；这里不复制字段或形成第五个权威所有者。
struct LocalEconomyState;
struct LocalEconomyCatalog;
struct OrganizationState;
struct OrganizationCatalog;
struct OrganizationLifecycleCatalog;
struct AffairsState;
struct AffairsCatalog;
struct DirectorState;
struct DirectorCatalog;
struct DiplomacyState;
struct DiplomacyCatalog;
struct PoliticsGovernanceState;
struct PoliticsGovernanceCatalog;
struct PoliticalGovernanceContinuationState;
struct GovernanceInvitationReplyState;
struct PoliticalHeadHandoverState;
struct PoliticalHeadHandoverCatalog;
struct PoliticalSuccessorRoundState;
struct PoliticalSuccessorRoundCatalog;
struct PoliticalTermHandoverState;
struct PoliticalTermHandoverCatalog;
struct PoliticalGovernanceAgencyState;
struct PoliticalGovernanceAgencyCatalog;
struct PoliticalOfficeReceptionState;
struct PoliticalOfficeReceptionCatalog;
struct PoliticalOfficeResignationState;
struct PoliticalOfficeResignationCatalog;
struct PoliticalSubordinateAppointmentState;
struct PoliticalSubordinateAppointmentCatalog;
struct PoliticalDismissalSourceState;
struct PoliticalDismissalSourceCatalog;
struct PoliticalSubordinateAppointmentIntentCatalog;
struct PoliticalSuccessionState;
struct PoliticalSuccessionCatalog;
struct PoliticsState;
struct PoliticsCatalog;
struct PoliticalFiscalState;
struct PoliticalFiscalCatalog;
struct PopulationMilitaryState;
struct PopulationMilitaryCatalog;
struct PlayerPlanState;
struct PersonalState;
struct PersonalCatalog;
struct GrowthState;
struct GrowthCatalog;
struct GrowthRecoveryCatalog;
struct GrowthRecoveryState;
struct GrowthTacticLessonCatalog;
struct GrowthTacticLessonState;
struct EnvoyTaskState;
struct GovernanceTaskState;
struct TaskState;
struct TaskCatalog;
struct TaskPersonalAdvanceCatalog;
struct FamilyState;
struct FamilyCatalog;
struct NavalAssetState;
struct NavalAssetCatalog;
struct NavalState;
struct NavalCatalog;
struct MilitaryTacticLearningState;
struct TravelState;
struct MilitaryState;
struct InteractionState;
struct SocialState;
struct EstateState;
struct CommonCatalog;
struct TravelCatalog;
struct MilitaryCatalog;
struct InteractionCatalog;
struct SocialCatalog;
struct EstateCatalog;
// 静态目录域由各owner只解码一次；共享const所有权使任何查询借用不悬空。
struct CatalogIdentity {
  StableId content_id, content_version, content_digest, ruleset_id,
      schema_sha256;
  std::optional<StableId> pack_set_digest;
  std::vector<std::pair<StableId, StableId>> module_versions;
  StableId artifact_sha256;
};
// 冷目录只记录world视图模板是否明确存在/非法；不保存JSON、展开模板或进入checkpoint。
enum class WorldTemplateMapping { absent, present, invalid };
struct HouseholdProvisionManagementCatalog;
struct HouseholdProvisionManagementState;
struct ProductionDisruptionCatalog;
struct ProductionDisruptionState;
struct PoliticalTakeoverCatalog;
struct PoliticalTakeoverState;
struct EconomicTakeoverExecutionState;
struct FamilyDeathPolicy;
struct FamilyDeathState;
struct LegacyControlConsentCatalog;
struct FamilyLegacyState;
struct FamilyBequestPolicy;
struct FamilyBequestState;
struct TreatyPassageCatalog;
struct TreatyPassageState;
struct FamilyInheritancePolicy;
struct FamilyInheritanceState;
struct PoliticalFieldAppointmentCatalog;
struct PoliticalFieldAppointmentState;
struct PoliticalMilitaryAppointmentCatalog;
struct PoliticalMilitaryAppointmentState;
struct MarriageSourceCatalog;
struct FamilyMarriageState;
struct ApprenticeshipCatalog;
struct FamilyApprenticeshipState;
struct DiscipleControlCatalog;
struct DiscipleControlState;
struct BiologicalControlCatalog;
struct BiologicalControlState;
struct CollateralControlCatalog;
struct CollateralControlState;
struct VoluntaryPathCatalog;
struct VoluntaryPathState;
struct ChildCareReliefCatalog;
struct ChildCareReliefState;
struct BirthArrangementCatalog;
struct DependentProvisionCatalog;
struct FamilyBirthState;
struct FamilyLineageCatalog;
struct EstateEmploymentCatalog;
struct EstateEmploymentState;
struct EconomicSupplyPolicyCatalog;
struct EconomicSupplyPolicyState;
struct PoliticalSupplyAdoptionState;
struct OccupationFiscalCatalog;
struct OccupationFiscalState;
struct OccupationFiscalSettlementState;
struct TaskPersonalCancellationCatalog;
struct TaskPersonalAcceptanceCatalog;
struct TaskPersonalReportCatalog;
struct TaskPersonalMessageCatalog;
struct TaskMessageReturnCatalog;
struct TaskMessageNoticeCatalog;
struct TaskExtensionCatalog;
struct PersonalAppointmentCatalog;
struct ContactInquiryCatalog;
struct VisitReplyCatalog;
struct VisitAppointmentCatalog;
struct VisitAppointmentIntentCatalog;
struct VisitAppointmentAttendanceCatalog;
struct BornPersonalDialogueCatalog;
struct KnownPersonalDialogueCatalog;
struct VisitReplyIntentCatalog;
struct PrivateShopLiquidationCatalog;
struct PrivateShopLiquidationState;
struct PrivateShopHouseholdCustodyCatalog;
struct PrivateShopCommandResultsCatalog;
struct PrivateShopCommandResultsState;
struct PrivateShopDailyRentCatalog;
struct PrivateShopDailyRentState;
struct PrivateShopCatalog;
struct PrivateShopState;
struct PoliticalShopLicensingCatalog;
struct PoliticalShopLicensingState;
struct PoliticalShopLicensingReceiptState;
struct PoliticalShopRegistrySuccessionCatalog;
struct PoliticalShopRegistrySuccessionState;
struct GrowthMarketHistoryCatalog;
struct GrowthMarketHistoryState;
struct GrowthPersonalMasteryInitialCatalog;
struct HouseholdPurchaseCatalog;
struct HouseholdRetailCatalog;
struct ActorSocialOccupancyInitialCatalog;
struct GrowthPermanentCatalog;
struct GrowthPermanentState;
struct LifeAgeCatalog;
struct LifeAgeState;
class CareRootCapabilitySelection;
struct ControlContinuationCatalog;
struct ControlContinuationState;
struct EconomicWorkCatalog;
struct EconomicWorkSourcesState;
struct CompleteExactTimeRuntime;
struct DynamicMilitaryActionCatalog2;
struct MilitarySpatialDefinitionCatalog;
struct MilitarySessionDefinitionCatalog;
struct MilitaryDutyCatalog;
struct ItemServiceCatalog;
struct RepairPolicyCatalog;
struct RepairServiceRuleCatalog;
struct CraftRecipeCatalog;
struct EconomicFacilityCatalog;
struct ProductionFacilityConditionsCatalog;
struct GovernanceWorkFeeCatalog;
struct PublicBudgetPolicyCatalog;
struct PoliticalTreatyTermsCatalog;
struct TreatyAuthorityMappingCatalog;
struct GovernanceProgramCatalog;
struct FieldBattleCatalog;
class AuthoredInteractionFlags;
struct PublicSupplyInspectionCatalog;
struct PoliticalRelationsCatalog;
struct FieldGroupPassageCatalog;
struct PreparedCatalog {
  // 只由批准的青川 Field fixture 选择，非一般全图许可。
  std::shared_ptr<const FieldGroupPassageCatalog> field_group_passage;
  // P3 原作者目录独立持有；未选不产生中立回退。
  std::shared_ptr<const PoliticalRelationsCatalog> political_relations;
  // 唯一原条约目录和显式作者权源映射；按同一锁身份保活，不复制运行态。
  std::shared_ptr<const PoliticalTreatyTermsCatalog> political_treaty_terms;
  std::shared_ptr<const TreatyAuthorityMappingCatalog> treaty_authority_mapping;
  // 明确选择的精确时间首链；空指针严格保legacy，不由存档缺字段猜新profile。
  std::shared_ptr<const ExactTimeProfile> exact_time;
  std::shared_ptr<const CompleteExactTimeRuntime> complete_exact_time_runtime;
  std::shared_ptr<const ActorFoundationCatalog> actor_foundation;
  std::shared_ptr<const GrowthPermanentCatalog> growth_permanent;
  std::shared_ptr<const LifeAgeCatalog> life_age_checks;
  // 只由最终正式 prepare 形成，未选 Care 的旧目录保持 null。
  std::shared_ptr<const CareRootCapabilitySelection>
      care_root_capability_selection;
  std::shared_ptr<const ControlContinuationCatalog> control_continuation;
  std::shared_ptr<const DynamicMilitaryActionCatalog2> dynamic_military_actions;
  // 原 Military 物理定义只随最终目录锁持有，不成为第二份世界状态。
  // 缺字段保持 null；有空表也不代表实际 definition 请求可成功。
  std::shared_ptr<const MilitarySpatialDefinitionCatalog>
      military_spatial_definitions;
  // 原 Session 五槽定义独立拥有，不从 geometry、timings 或当前态推导。
  // 缺字段仍为 null；这里只保存 immutable 输入，不保存会话进度。
  std::shared_ptr<const MilitarySessionDefinitionCatalog>
      military_session_definitions;
  std::shared_ptr<const MilitaryDutyCatalog> military_duty_catalog;
  std::shared_ptr<const ItemServiceCatalog> item_services;
  std::shared_ptr<const RepairPolicyCatalog> repair_policies;
  std::shared_ptr<const RepairServiceRuleCatalog> repair_service_rules;
  std::shared_ptr<const CraftRecipeCatalog> craft_recipes;
  std::shared_ptr<const EconomicFacilityCatalog> economic_facilities;
  std::shared_ptr<const ProductionFacilityConditionsCatalog>
      production_facility_conditions;
  std::shared_ptr<const GovernanceWorkFeeCatalog> governance_work_fees;
  // 只由显式政治预算选型持有作者政策；不从旧责任或当前容器反造。
  std::shared_ptr<const PublicBudgetPolicyCatalog> public_budget_policies;
  std::shared_ptr<const EconomicWorkCatalog> production_cycles;
  bool private_transport_funding_selected = false;
  // 仅显式作者选择启用知识实际 Common 切点，旧 Social 字节不升级。
  bool social_knowledge_common_cut_selected = false;
  bool actor_social_occupancy_selected = false;
  std::shared_ptr<const ActorSocialOccupancyInitialCatalog>
      actor_social_occupancy_initial;
  CatalogIdentity identity;
  std::shared_ptr<const PoliticalTakeoverCatalog> political_takeover;
  bool task_takeover_execution_enabled = false;
  bool task_static_assets_enabled = false;
  std::shared_ptr<const TaskExtensionCatalog> task_extensions;
  std::shared_ptr<const PersonalAppointmentCatalog> personal_appointments;
  std::shared_ptr<const ContactInquiryCatalog> contact_inquiries;
  std::shared_ptr<const VisitReplyCatalog> visit_message_replies;
  std::shared_ptr<const VisitAppointmentCatalog> visit_appointment_invitations;
  std::shared_ptr<const VisitAppointmentIntentCatalog>
      visit_appointment_intents;
  std::shared_ptr<const VisitAppointmentAttendanceCatalog>
      visit_appointment_attendance;
  bool visit_message_receipts_enabled = false;
  std::shared_ptr<const VisitReplyIntentCatalog> visit_reply_intents;
  std::shared_ptr<const BornPersonalDialogueCatalog> born_personal_dialogues;
  std::shared_ptr<const KnownPersonalDialogueCatalog> known_personal_dialogues;
  bool task_message_handoffs_enabled = false;
  Integer task_message_handoff_version = 0;
  std::shared_ptr<const TaskMessageNoticeCatalog> task_message_notices;
  std::shared_ptr<const TaskMessageReturnCatalog> task_message_returns;
  std::shared_ptr<const TaskPersonalMessageCatalog> task_personal_messages;
  std::shared_ptr<const TaskPersonalReportCatalog> task_personal_reports;
  std::shared_ptr<const TaskPersonalAcceptanceCatalog>
      task_personal_acceptances;
  std::shared_ptr<const TaskPersonalCancellationCatalog>
      task_personal_cancellations;
  std::shared_ptr<const TreatyPassageCatalog> treaty_passage;
  std::shared_ptr<const PoliticalMilitaryAppointmentCatalog>
      political_military_appointments;
  std::shared_ptr<const PoliticalFieldAppointmentCatalog>
      political_field_appointments;
  std::shared_ptr<const OccupationFiscalCatalog> occupation_fiscal;
  bool occupation_fiscal_settlement_enabled = false;
  bool occupation_fiscal_needs_enabled = false;
  bool fiscal_taxpayer_continuity_enabled = false;
  DynamicProfile dynamic_profile = DynamicProfile::disabled;
  // 只有所选情景显式开启地方生活时使用新增持久段，旧目录不随包的能力并集升级。
  bool regional_life = false, named_membership = false, player_plans = false,
       governance_tasks_enabled = false, task_offer_responses = false,
       envoy_tasks_enabled = false, party_invitations_enabled = false;
  // 原bool保既有调用；新版本只从所选情景显式profile派生。
  Integer task_offer_response_version = 0;
  std::shared_ptr<const LocalEconomyCatalog> local_economy;
  std::shared_ptr<const HouseholdPurchaseCatalog> household_purchases;
  std::shared_ptr<const HouseholdRetailCatalog> household_retail_settlements;
  Integer selected_household_version = 0;
  std::shared_ptr<const PopulationMilitaryCatalog> population_military;
  std::shared_ptr<const OrganizationCatalog> organization;
  std::shared_ptr<const OrganizationLifecycleCatalog> organization_lifecycle;
  std::shared_ptr<const GovernanceProgramCatalog> governance_program_catalog;
  std::shared_ptr<const AffairsCatalog> affairs;
  std::shared_ptr<const DirectorCatalog> director;
  std::shared_ptr<const CommonCatalog> common;
  std::shared_ptr<const TravelCatalog> travel;
  std::shared_ptr<const MilitaryCatalog> military;
  std::shared_ptr<const FieldBattleCatalog> field_battle;
  // 显式作者告示关联；未选时不从同处货源猜测目录。
  std::shared_ptr<const PublicSupplyInspectionCatalog> public_supply_inspection;
  Integer selected_military_occupation_schema = 1;
  std::shared_ptr<const NavalCatalog> naval;
  std::shared_ptr<const InteractionCatalog> interaction;
  // 仅显式单prefix边界/无flag
  // writer选型：保原data.flags与最终完整锁，非第二世界账。
  std::shared_ptr<const AuthoredInteractionFlags> immutable_interaction_flags;
  std::shared_ptr<const SocialCatalog> social;
  std::shared_ptr<const EstateCatalog> estate;
  std::shared_ptr<const TaskCatalog> tasks;
  std::shared_ptr<const TaskPersonalAdvanceCatalog> task_personal_advances;
  std::shared_ptr<const FamilyCatalog> family;
  std::shared_ptr<const FamilyDeathPolicy> family_life;
  std::shared_ptr<const LegacyControlConsentCatalog> family_legacy;
  std::shared_ptr<const FamilyBequestPolicy> family_bequest;
  std::shared_ptr<const FamilyInheritancePolicy> family_inheritance;
  std::shared_ptr<const MarriageSourceCatalog> family_marriage;
  std::shared_ptr<const ApprenticeshipCatalog> family_apprenticeship;
  std::shared_ptr<const DiscipleControlCatalog> family_disciple_legacy;
  std::shared_ptr<const BiologicalControlCatalog> family_biological_legacy;
  std::shared_ptr<const CollateralControlCatalog> family_collateral_legacy;
  std::shared_ptr<const VoluntaryPathCatalog> family_voluntary_paths;
  std::shared_ptr<const BirthArrangementCatalog> family_birth;
  std::shared_ptr<const DependentProvisionCatalog> dependent_provision_catalog;
  std::shared_ptr<const ChildCareReliefCatalog> family_care_relief;
  std::shared_ptr<const FamilyLineageCatalog> family_lineage;
  std::shared_ptr<const EstateEmploymentCatalog> estate_employment;
  bool economic_supply_policies_enabled = false;
  std::shared_ptr<const EconomicSupplyPolicyCatalog> economic_supply_policies;
  std::shared_ptr<const PersonalCatalog> personal;
  std::shared_ptr<const GrowthCatalog> growth;
  // 只读Installed掌握度初态，随目录共享；不参与旧Growth/checkpoint形状。
  std::shared_ptr<const GrowthPersonalMasteryInitialCatalog>
      personal_mastery_initial;
  std::shared_ptr<const GrowthRecoveryCatalog> personal_recovery;
  std::shared_ptr<const GrowthTacticLessonCatalog> growth_tactic_lessons;
  std::shared_ptr<const GrowthMarketHistoryCatalog> growth_market_history;
  std::shared_ptr<const PrivateShopCatalog> private_shops;
  std::shared_ptr<const PrivateShopHouseholdCustodyCatalog>
      private_shop_household_custody;
  std::shared_ptr<const PrivateShopDailyRentCatalog> private_shop_daily_rent;
  std::shared_ptr<const PrivateShopCommandResultsCatalog>
      private_shop_command_results;
  std::shared_ptr<const PrivateShopLiquidationCatalog>
      private_shop_liquidations;
  std::shared_ptr<const PoliticalShopLicensingCatalog> political_shop_licensing;
  bool political_shop_licensing_receipts = false;
  bool political_governance_continuations = false;
  bool political_governance_invitation_replies = false;
  std::shared_ptr<const PoliticalHeadHandoverCatalog> political_head_handovers;
  std::shared_ptr<const PoliticalSuccessorRoundCatalog>
      political_successor_rounds;
  std::shared_ptr<const PoliticalTermHandoverCatalog> political_term_handovers;
  std::shared_ptr<const PoliticalOfficeReceptionCatalog>
      political_office_receptions;
  std::shared_ptr<const PoliticalOfficeResignationCatalog>
      political_office_resignations;
  std::shared_ptr<const PoliticalSubordinateAppointmentCatalog>
      political_subordinate_appointments;
  std::shared_ptr<const PoliticalDismissalSourceCatalog>
      political_dismissal_sources;
  std::shared_ptr<const PoliticalSubordinateAppointmentIntentCatalog>
      political_subordinate_appointment_intents;
  std::shared_ptr<const PoliticalGovernanceAgencyCatalog>
      political_governance_agencies;
  std::shared_ptr<const PoliticalSuccessionCatalog> political_successions;
  std::shared_ptr<const PoliticalShopRegistrySuccessionCatalog>
      political_shop_registry_successions;
  bool task_shop_orders_enabled = false;
  bool task_shop_order_contacts_enabled = false;
  std::shared_ptr<const DiplomacyCatalog> diplomacy;
  std::shared_ptr<const PoliticsGovernanceCatalog> politics_governance;
  std::shared_ptr<const PoliticsCatalog> politics;
  std::shared_ptr<const PoliticalFiscalCatalog> political_fiscal;
  std::shared_ptr<const FiscalSettlementCatalog> fiscal_settlement;
  std::shared_ptr<const PublicReliefCatalog> public_relief;
  std::shared_ptr<const HouseholdProvisionManagementCatalog>
      household_provision_management;
  std::shared_ptr<const ProductionDisruptionCatalog> production_disruptions;
  std::vector<std::pair<StableId, WorldTemplateMapping>> place_world_templates;
  WorldTemplateMapping world_template = WorldTemplateMapping::absent;
};
struct InitialWorldSeed;
struct PreparedDomainOp;
struct CodecVisitor;
struct WorldRead {
  const CommonRead &common;
  const TravelState &travel;
  const MilitaryState &military;
  const InteractionState &interaction;
  const SocialState &social;
  const EstateState &estate;
  const TaskState &tasks;
  const FamilyState &family;
  const PersonalState &personal;
  const GrowthState &growth;
  const LocalEconomyState *local_economy = nullptr;
  const OrganizationState *organization = nullptr;
  const AffairsState *affairs = nullptr;
  const DirectorState *director = nullptr;
  const PopulationMilitaryState *population_military = nullptr;
  const PoliticsState *politics = nullptr;
  const PoliticalFiscalState *political_fiscal = nullptr;
  const FiscalSettlementState *fiscal_settlement = nullptr;
  const PoliticsGovernanceState *politics_governance = nullptr;
  const GovernanceTaskState *governance_tasks = nullptr;
  const PublicReliefState *public_relief = nullptr;
  const DiplomacyState *diplomacy = nullptr;
  const EnvoyTaskState *envoy_tasks = nullptr;
  const HouseholdProvisionManagementState *household_provision_management =
      nullptr;
  const ProductionDisruptionState *production_disruptions = nullptr;
};
struct Txn {
  CommonWrite &common;
  TravelState &travel;
  MilitaryState &military;
  InteractionState &interaction;
  SocialState &social;
  EstateState &estate;
  TaskState &tasks;
  FamilyState &family;
  PersonalState &personal;
  GrowthState &growth;
  LocalEconomyState *local_economy = nullptr;
  OrganizationState *organization = nullptr;
  AffairsState *affairs = nullptr;
  DirectorState *director = nullptr;
  PopulationMilitaryState *population_military = nullptr;
  PoliticsState *politics = nullptr;
  PoliticalFiscalState *political_fiscal = nullptr;
  FiscalSettlementState *fiscal_settlement = nullptr;
  PoliticsGovernanceState *politics_governance = nullptr;
  GovernanceTaskState *governance_tasks = nullptr;
  PublicReliefState *public_relief = nullptr;
  DiplomacyState *diplomacy = nullptr;
  EnvoyTaskState *envoy_tasks = nullptr;
  HouseholdProvisionManagementState *household_provision_management = nullptr;
  ProductionDisruptionState *production_disruptions = nullptr;
};
} // namespace hanmo::resident
