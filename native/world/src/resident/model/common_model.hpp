// 常驻唯一共同状态：有类型记录保原插入序，索引仅用于定位，不保存第二份业务真值。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "hanmo/resident/model_contract.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include "hanmo/resident/checkpoint_memory.hpp"

namespace hanmo::resident {
// 有序键表的key顺序即原Dictionary顺序；删除不交换尾元素，更新不重插入。
template <class T> class OrderedTable {
public:
  using Row = std::pair<StableId, T>;
  OrderedTable() = default;
  OrderedTable(const OrderedTable &other) : OrderedTable() {
    auto next = copy_selected_rows(other.size(), [&](auto &&emit) {
      for (const auto &row : other.rows_)
        emit(row.first, row.second);
    });
    *this = std::move(next);
  }
  OrderedTable(OrderedTable &&) = default;
  OrderedTable &operator=(const OrderedTable &other) {
    if (this != &other) {
      OrderedTable copy(other);
      *this = std::move(copy);
    }
    return *this;
  }
  OrderedTable &operator=(OrderedTable &&other) noexcept {
    if (this != &other) {
      this->~OrderedTable();
      new (this) OrderedTable(std::move(other));
    }
    return *this;
  }
  const std::vector<Row> &rows() const { return rows_; }
  std::vector<Row> &rows() { return rows_; }
  const std::vector<StableId> &order() const { return order_; }
  const T *find(const StableId &id) const {
    auto i = index_.find(id);
    return i == index_.end() ? nullptr : &rows_[i->second].second;
  }
  T *find(const StableId &id) {
    auto i = index_.find(id);
    return i == index_.end() ? nullptr : &rows_[i->second].second;
  }
  // 借用原行后再在私有候选表复制；不以按值实参提前改动来源。
  bool insert(const StableId &id, const T &row) {
    if (index_.count(id))
      return false;
    if (size() == SIZE_MAX)
      throw std::bad_alloc();
    auto next = copy_selected_rows(size() + 1, [&](auto &&emit) {
      for (const auto &original : rows_)
        emit(original.first, original.second);
      emit(id, row);
    });
    *this = std::move(next);
    return true;
  }
  bool insert(const StableId &id, T &&row) {
    return insert(id, static_cast<const T &>(row));
  }
  // Rollback restores the exact original position, not an append-equivalent
  // order. The same owning copy boundary covers any fallible T copy.
  bool insert_at(std::size_t position, const StableId &id, const T &row) {
    if (index_.count(id) || position > size())
      return false;
    if (size() == SIZE_MAX)
      throw std::bad_alloc();
    auto next = copy_selected_rows(size() + 1, [&](auto &&emit) {
      for (std::size_t n = 0; n <= size(); ++n) {
        if (n == position)
          emit(id, row);
        if (n < size())
          emit(rows_[n].first, rows_[n].second);
      }
    });
    *this = std::move(next);
    return true;
  }
  bool insert_at(std::size_t position, const StableId &id, T &&row) {
    return insert_at(position, id, static_cast<const T &>(row));
  }
  void set(const StableId &id, const T &row) {
    auto found = index_.find(id);
    if (found == index_.end()) {
      insert(id, row);
      return;
    }
    // 同槽更新不替换整表；仍先完成普通拥有复制，再作原精确字段移动。
    // token 只检查当前 job 生命周期，不再给行另建内存账或租约树。
    CheckpointMemoryCharge active(0);
    T fresh(row);
    rows_[found->second].second = std::move(fresh);
  }
  void set(const StableId &id, T &&row) {
    set(id, static_cast<const T &>(row));
  }
  bool erase(const StableId &id) {
    auto found = index_.find(id);
    if (found == index_.end())
      return false;
    const auto position = found->second;
    auto next = copy_selected_rows(size() - 1, [&](auto &&emit) {
      for (std::size_t n = 0; n < size(); ++n)
        if (n != position)
          emit(rows_[n].first, rows_[n].second);
    });
    *this = std::move(next);
    return true;
  }
  std::size_t size() const { return rows_.size(); }
  bool empty() const { return rows_.empty(); }
  // 仅私有 decoder 调用：真实 rows 整体移入，派生索引和顺序从原键构造。
  bool adopt_checkpoint_rows(std::vector<Row> &&rows) {
    OrderedTable next;
    const auto count = rows.size();
    next.checkpoint_rows_lifetime_ = CheckpointMemoryCharge(0);
    next.order_.reserve(count);
    next.index_.reserve(count);
    next.rows_ = std::move(rows);
    for (std::size_t n = 0; n < count; ++n) {
      const auto &id = next.rows_[n].first;
      if (!next.index_.emplace(id, n).second)
        return false;
      next.order_.push_back(id);
    }
    *this = std::move(next);
    return true;
  }
  // 原owner显式typed转移允许真实分数分配失败；私有候选失败后一律整体丢弃。
  // 新 std::vector 壳在源 allocator 旧壳仍活时分配；只通过原 typed 回调迁移行。
  template <class Transfer>
  Status adopt_checkpoint_rows(CheckpointVector<Row> &&rows,
                               Transfer &&transfer) {
    const auto failure = [](const char *code) -> Status {
      return {
          false,
          {},
          {{code, "checkpoint/table"}, ErrorKind::internal, {}, {}, {}, {}}};
    };
    const auto current = current_checkpoint_memory(),
               original = rows.get_allocator().resource();
    if (bool(current) != bool(original) ||
        (current && !current->shares_job(*original)))
      return failure("CheckpointDecodedTableJobMismatch");
    const auto count = rows.size();
    if (count > SIZE_MAX / sizeof(Row))
      throw std::bad_alloc();
    CheckpointMemoryCharge shell_lifetime(0);
    std::vector<Row> moved;
    moved.reserve(count);
    for (auto &row : rows) {
      const auto before = moved.size(), capacity = moved.capacity();
      const auto *allocation = moved.data();
      auto status = transfer(moved, row);
      if (!status.ok)
        return status;
      if (moved.size() != before + 1 || moved.capacity() != capacity ||
          moved.data() != allocation)
        return failure("CheckpointDecodedTransferShapeMismatch");
    }
    OrderedTable next;
    if (!next.adopt_checkpoint_rows(std::move(moved)))
      return failure("DuplicateCheckpointTableKey");
    next.checkpoint_rows_lifetime_ = std::move(shell_lifetime);
    *this = std::move(next);
    return {true, {}, {}};
  }
  // 无分配move行的原简口；可抛原值必须由owner明确选上面的typed回调。
  bool adopt_checkpoint_rows(CheckpointVector<Row> &&rows) {
    static_assert(std::is_nothrow_move_constructible_v<Row>,
                  "checkpoint table requires explicit fallible typed transfer");
    return adopt_checkpoint_rows(
               std::move(rows),
               [](auto &destination, Row &original) -> Status {
                 destination.emplace_back(std::move(original));
                 return {true, {}, {}};
               })
        .ok;
  }

private:
  // 在原表旁完成全部普通拥有复制；失败不发布半张表，也不重排既有行。
  template <class Enumerate>
  static OrderedTable copy_selected_rows(std::size_t count,
                                         Enumerate &&enumerate) {
    if (count > SIZE_MAX / sizeof(Row) || count > SIZE_MAX / sizeof(StableId))
      throw std::bad_alloc();
    OrderedTable next;
    next.checkpoint_rows_lifetime_ = CheckpointMemoryCharge(0);
    next.rows_.reserve(count);
    next.order_.reserve(count);
    next.index_.reserve(count);
    enumerate([&](const StableId &id, const T &row) {
      const auto position = next.rows_.size();
      next.rows_.emplace_back(id, row);
      next.order_.push_back(id);
      if (!next.index_.emplace(id, position).second)
        throw std::logic_error("DuplicateCheckpointTableKey");
    });
    if (next.rows_.size() != count)
      throw std::logic_error("CheckpointTableCopyShapeMismatch");
    return next;
  }
  // 当前 job 的取消／首错身份先声明后销毁；真实字段仍由普通 C++ 容器拥有。
  CheckpointMemoryCharge checkpoint_rows_lifetime_;
  std::vector<Row> rows_;
  std::vector<StableId> order_;
  std::unordered_map<
      StableId, std::size_t, std::hash<StableId>, std::equal_to<StableId>,
      CheckpointAllocator<std::pair<const StableId, std::size_t>>>
      index_;
};
// 当前profile的owner/lot只允许原严格字段；不存在任意extras或JSON兜底。
struct CompatibilityFields {
  enum class Profile { strict_original };
  Profile profile = Profile::strict_original;
};
// 外观是共同人物内显式、不可变的兼容内容；未知字段必须拒绝，不能静默吞掉。
struct AppearanceFeatures {
  StableId face_shape, eyes, brows, nose, mouth, hair, beard, skin_palette,
      build;
  bool present = false;
  std::vector<StableId> field_order;
};
struct AppearanceProfile {
  Integer format_version = 1;
  StableId generator_version, generation_seed, attire_style, source;
  AppearanceFeatures base_features;
  std::vector<StableId> parent_sources;
  std::vector<StableId> field_order;
};
struct ActorAttributes {
  Integer administration = 0, charisma = 0, intellect = 0, leadership = 0,
          martial = 0;
};
// 六项完整目标技艺使用显式可选块；旧人物仍只持原十项，不能靠读取自动补出新经验。
struct ActorExtendedSkills {
  Integer naval = 0, espionage = 0, fortification = 0, agriculture = 0,
          medicine = 0, forging = 0;
  std::vector<StableId> field_order;
};
struct ActorSkills {
  Integer archery = 0, cavalry = 0, commerce = 0, debate = 0, infantry = 0,
          martial = 0, negotiation = 0, rites = 0, tactics = 0, transport = 0;
  std::optional<ActorExtendedSkills> extended;
};
struct ActorCombat {
  StableId weapon_id, heart_id;
  std::optional<StableId> armor_id;
  OrderedTable<Integer> learned;
};
struct ActorRecord : ActorCoreView {
  Integer merit = 0, injury = 0;
  std::optional<Integer> birth_tick, created_tick;
  std::optional<StableId> prototype_id, display_name;
  ActorAttributes attributes;
  ActorSkills skills;
  ActorCombat combat;
  std::shared_ptr<const AppearanceProfile> appearance;
  std::vector<StableId> field_order, location_order, attributes_order,
      skills_order, combat_order;
};
struct CompanionTerms {
  StableId role_id;
  Integer cost_per_tick = 0;
  std::vector<StableId> field_order;
};
struct PartyRecord {
  StableId id, leader_id;
  std::vector<StableId> members;
  OrderedTable<CompanionTerms> terms;
  Integer created_tick = 0;
  std::vector<StableId> field_order;
};
struct RoleRecord {
  StableId actor_id;
  bool enabled = false;
  std::vector<StableId> field_order;
};
// 事实全局登记只保存引用头；每一payload仅在所属typed域内保存一次。
struct FactRecord {
  StableId id, kind, domain;
  std::vector<StableId> field_order;
};
struct RngStream {
  Integer seed = 0, state = 0;
  std::vector<StableId> field_order;
};
struct HistoryRecord {
  Integer tick = 0;
  StableId kind, actor_id, result_id;
  std::vector<StableId> field_order;
};
struct CommitResult;
struct WorldMetadata {
  StableId ruleset_id, content_id, content_version, content_digest,
      engine_build, rng_backend, scenario_id, player_actor_id;
};
struct CommonState {
  WorldIdentity identity;
  WorldMetadata metadata;
  Integer next_instance_id = 1;
  std::vector<StableId> root_field_order, battle_order;
  OrderedTable<std::vector<StableId>> action_field_order;
  OrderedTable<ActorRecord> actors;
  OrderedTable<ActionCoreView> actions;
  OrderedTable<PartyRecord> parties;
  OrderedTable<RoleRecord> roles;
  OrderedTable<WalletView> wallets;
  OrderedTable<InventoryView> inventories;
  OrderedTable<FactRecord> facts;
  OrderedTable<RngStream> rng;
  OrderedTable<Integer> request_sequences;
  OrderedTable<std::shared_ptr<const CommitResult>> results;
  std::vector<HistoryRecord> history;
};
struct ItemDefinition {
  StableId id;
  Integer weight = 0, base_price = 0;
  bool weapon = false, armor = false;
};
struct PlaceDefinition {
  StableId id, settlement_id;
  std::vector<StableId> passages;
};
struct RoleDefinition {
  StableId id, initial_actor, place_id;
  std::optional<StableId> organization_id, dialogue_id;
  std::vector<StableId> services;
  Integer companion_cost_per_tick = 6;
  // Bound only from the actual scenario.initial row used by the original
  // native seeder. An absent authored source is not an enabled role.
  std::optional<bool> initial_enabled;
};
struct RequiredContainer {
  Owner owner;
  Integer capacity = 0;
};
// 表现只保作者名字键及两列表现引用；缺name_key仍取空键，不复制作者整个人物。
struct ActorNameDefinition {
  StableId name_key;
  StableId presentation_profile_id{}, portrait_kind{};
};
struct CommonCatalog {
  OrderedTable<ItemDefinition> items;
  OrderedTable<PlaceDefinition> places;
  OrderedTable<RoleDefinition> roles;
  std::vector<StableId> organization_ids, actor_ids;
  OrderedTable<RequiredContainer> required_wallets, required_inventories;
  OrderedTable<std::optional<Integer>> actor_initial_birth;
  OrderedTable<StableId> personal_skill_kinds;
  OrderedTable<ActorNameDefinition> actor_names;
  OrderedTable<std::string> texts;
  bool extended_skills = false;
  // 只由显式出生profile启用；实际动态人物仍须完整typed出生身份先验。
  bool family_birth_identity_enabled = false;
  // 仅由锁内 Common 有限物理选择赋值；原资源/位置 reader
  // 不按当前payload猜能力。
  bool item_instances_enabled = false, lot_properties_enabled = false;
  // X/1 is a static original recipe capability, not a Common wire axis.
  bool selected_resource_operations_enabled = false;
  bool domain_activity_reservations_enabled = false,
       carrier_location_enabled = false;
  // 仅从显式新市场profile的正规Installed初态派生；随catalog克隆保留，不能靠缺借用关闭保护。
  std::vector<StableId> protected_market_inventory_ids;
  // 仅完整新个人战profile后续显式选择；默认关闭，克隆不能因缺服务关闭已选保护。
  bool personal_loadout_protection_enabled = false;
  // 仅完整新携货profile后续选中；未绑定同候选纯准备服务时资源原语必须拒绝。
  bool private_shop_freight_protection_enabled = false;
};
// 每个候选通过新resolver借用C军队状态，clone后禁止保留旧candidate的指针。
class MembershipResolver {
public:
  virtual ~MembershipResolver() = default;
  virtual bool field_battle_binding_ready() const { return false; }
  virtual Outcome<bool> field_battle_bound(const StableId &) const {
    return {true, false, {}};
  }
  virtual const std::vector<StableId> *
  active_army_members(const StableId &actor_id) const = 0;
  virtual const StableId *army_leader(const StableId &actor_id) const = 0;
};
// 任务产权只借同候选实际任务表；没有绑定时不能把任意task ID当有效所有者。
class TaskOwnerRead {
public:
  virtual ~TaskOwnerRead() = default;
  virtual bool task_exists(const StableId &) const = 0;
};
// 动态身份的创建事实/原型关联由家庭唯一域证明，共同数值及资产校验仍留在store。
class FamilyIdentityRead {
public:
  virtual ~FamilyIdentityRead() = default;
  virtual Status validate_dynamic_actor(const CommonRead &,
                                        const ActorRecord &) const = 0;
};
// 社会与庄园的占用规则读同候选领域；不能把未知的战斗/会话当作空集合。
class ActivityRead {
public:
  virtual ~ActivityRead() = default;
  virtual bool in_open_battle(const StableId &) const = 0;
  virtual bool in_interaction(const StableId &,
                              const StableId &allowed_speaker = {}) const = 0;
  // 拒接新职事不撤销已冻结合同的收货责任；缺省域没有此额外服务承诺。
  virtual bool has_existing_contract_service(const StableId &provider_actor_id,
                                             const StableId &role_id) const {
    (void)provider_actor_id;
    (void)role_id;
    return false;
  }
};
// 各domain的typed seed由owner定义；组合者必须各域validate后才能唯一发布。
struct AuthoredActorFoundationState;
struct CharacterInjurySourcesStateV1;
struct SocialPeriodSources;
struct CardAcquisitionState;
struct ProfileDeliveryOutbox;
struct FacilityClaimsState;
struct MovementExactState;
struct SpatialCarrierState;
struct GroupMovementExecutionState;
struct TransportRegistrationState;
struct EconomicTransportExpenseState;
struct PrivateShopHouseholdCustodyState;
struct PrivateShopOperationSourcesState;
struct PrivateShopOperationCapabilitiesState;
struct PublicSupplyInspectionState;
struct FieldBattleWorldState;
struct MilitaryExactState;
struct MilitaryActionTimeState;
struct PopulationTransportState;
struct HouseholdPurchaseState;
struct HouseholdRetailState;
struct PrivateTransportFundingState;
struct ItemInstancesState;
struct LotPropertiesState;
struct CraftJobsState;
struct OrganizationRuntime;
struct EconomicObligationSourcesState;
struct GovernanceProgramState;
struct EconomicFacilitySourcesState;
struct ProductionFacilityConditionsState;
struct GovernanceWorkExpenseState;
struct PoliticalBudgetState;
struct PoliticalTreatyTermsState;
struct BudgetExecutionState;
struct PoliticalRelationsState;
struct PoliticalPassageDecisionState;
struct SafeExitState;
struct InitialWorldSeed {
  std::shared_ptr<const PoliticalPassageDecisionState> political_passage;
  std::shared_ptr<const SafeExitState> political_safe_exit;
  std::shared_ptr<const PoliticalRelationsState> political_relations;
  std::shared_ptr<const CardAcquisitionState> card_acquisitions;
  std::shared_ptr<const ProfileDeliveryOutbox> profile_deliveries;
  std::shared_ptr<const SocialPeriodSources> social_period_sources;
  // 两段成对存在；新局由真实安装生成，冷恢复不补开期或从旧ticks推历史。
  std::shared_ptr<const AuthoredActorFoundationState> actor_foundation;
  std::shared_ptr<const CharacterInjurySourcesStateV1> character_injury_sources;
  std::shared_ptr<const GrowthPermanentState> growth_permanent;
  std::shared_ptr<const LifeAgeState> life_age_checks;
  std::shared_ptr<const ControlContinuationState> control_continuation;
  std::shared_ptr<const EconomicWorkSourcesState> production_cycles;
  std::shared_ptr<const WorldTimelineState> world_timeline;
  std::shared_ptr<const ActionElapsedStore> action_elapsed;
  std::shared_ptr<const FacilityClaimsState> facility_claims;
  std::shared_ptr<const MilitaryExactState> military_exact;
  std::shared_ptr<const MilitaryActionTimeState> military_action_time;
  std::shared_ptr<const FieldBattleWorldState> field_battle;
  std::shared_ptr<const PublicSupplyInspectionState> public_supply_inspection;
  std::shared_ptr<const ItemInstancesState> item_instances;
  std::shared_ptr<const LotPropertiesState> lot_properties;
  std::shared_ptr<const CraftJobsState> craft_jobs;
  std::shared_ptr<const EconomicFacilitySourcesState>
      economic_facility_capacity_sources;
  std::shared_ptr<const ProductionFacilityConditionsState>
      production_facility_conditions;
  std::shared_ptr<const GovernanceWorkExpenseState> governance_work_expense;
  // 原政治授权与 S5 实际责任分根持有，只有完整选型才能成对安装。
  std::shared_ptr<const PoliticalBudgetState> political_budgets;
  std::shared_ptr<const PoliticalTreatyTermsState> political_treaty_terms;
  std::shared_ptr<const BudgetExecutionState> budget_execution;
  std::shared_ptr<const OrganizationRuntime> organization_runtime;
  std::shared_ptr<const GovernanceProgramState> governance_programs;
  std::shared_ptr<const EconomicObligationSourcesState>
      economic_obligation_sources;
  std::shared_ptr<const PopulationTransportState> population_transport;
  std::shared_ptr<const HouseholdPurchaseState> household_purchases;
  std::shared_ptr<const HouseholdRetailState> household_retail_settlements;
  // 三个固定物理段共同持有的真实typed子状态；未选取保持absent。
  std::shared_ptr<const MovementExactState> movement_exact;
  std::shared_ptr<const SpatialCarrierState> spatial_carriers;
  std::shared_ptr<const GroupMovementExecutionState> group_movement_executions;
  std::shared_ptr<const TransportRegistrationState> transport_registrations;
  std::shared_ptr<const EconomicTransportExpenseState>
      economic_transport_expenses;
  std::shared_ptr<const PrivateTransportFundingState> private_transport_funding;

  std::shared_ptr<const MilitaryTacticLearningState> military_tactic_learning;
  std::shared_ptr<const TreatyPassageState> treaty_passage;
  std::shared_ptr<const PoliticalMilitaryAppointmentState>
      political_military_appointments;
  std::shared_ptr<const PoliticalFieldAppointmentState>
      political_field_appointments;
  std::shared_ptr<const OccupationFiscalState> occupation_fiscal;
  std::shared_ptr<const OccupationFiscalSettlementState>
      occupation_fiscal_settlement;
  std::shared_ptr<const FiscalTaxpayerContinuityState>
      fiscal_taxpayer_continuity;
  std::shared_ptr<const PoliticalTakeoverState> political_takeover;
  std::shared_ptr<const EconomicTakeoverExecutionState>
      economic_takeover_execution;
  std::shared_ptr<const FamilyDeathState> family_life;
  std::shared_ptr<const FamilyLegacyState> family_legacy;
  std::shared_ptr<const VoluntaryPathState> family_voluntary_paths;
  std::shared_ptr<const FamilyBequestState> family_bequest;
  std::shared_ptr<const FamilyInheritanceState> family_inheritance;
  std::shared_ptr<const FamilyMarriageState> family_marriage;
  std::shared_ptr<const FamilyApprenticeshipState> family_apprenticeship;
  std::shared_ptr<const DiscipleControlState> family_disciple_legacy;
  std::shared_ptr<const BiologicalControlState> family_biological_legacy;
  std::shared_ptr<const CollateralControlState> family_collateral_legacy;
  std::shared_ptr<const FamilyBirthState> family_birth;
  std::shared_ptr<const ChildCareReliefState> child_care_relief;
  std::shared_ptr<const EstateEmploymentState> estate_employment;
  std::shared_ptr<const EconomicSupplyPolicyState> economic_supply_policies;
  std::shared_ptr<const PoliticalSupplyAdoptionState>
      political_supply_adoptions;
  std::shared_ptr<const GrowthMarketHistoryState> growth_market_history;
  std::shared_ptr<const GrowthRecoveryState> personal_recovery;
  std::shared_ptr<const GrowthTacticLessonState> growth_tactic_lessons;
  std::shared_ptr<const PrivateShopState> private_shops;
  std::shared_ptr<const PrivateShopOperationSourcesState>
      private_shop_operation_sources;
  std::shared_ptr<const PrivateShopOperationCapabilitiesState>
      private_shop_operation_capabilities;
  std::shared_ptr<const PrivateShopHouseholdCustodyState>
      private_shop_household_custody;
  std::shared_ptr<const PrivateShopDailyRentState> private_shop_daily_rent;
  std::shared_ptr<const PrivateShopCommandResultsState>
      private_shop_command_results;
  std::shared_ptr<const PrivateShopLiquidationState> private_shop_liquidations;
  std::shared_ptr<const PoliticalShopLicensingState> political_shop_licensing;
  std::shared_ptr<const PoliticalShopLicensingReceiptState>
      political_shop_licensing_receipts;
  std::shared_ptr<const PoliticalGovernanceContinuationState>
      political_governance_continuations;
  std::shared_ptr<const GovernanceInvitationReplyState>
      governance_invitation_replies;
  std::shared_ptr<const PoliticalHeadHandoverState> political_head_handovers;
  std::shared_ptr<const PoliticalSuccessorRoundState>
      political_successor_rounds;
  std::shared_ptr<const PoliticalTermHandoverState> political_term_handovers;
  std::shared_ptr<const PoliticalOfficeReceptionState>
      political_office_receptions;
  std::shared_ptr<const PoliticalOfficeResignationState>
      political_office_resignations;
  std::shared_ptr<const PoliticalSubordinateAppointmentState>
      political_subordinate_appointments;
  std::shared_ptr<const PoliticalDismissalSourceState>
      political_dismissal_sources;
  std::shared_ptr<const PoliticalGovernanceAgencyState>
      political_governance_agencies;
  std::shared_ptr<const PoliticalSuccessionState> political_successions;
  std::shared_ptr<const PoliticalShopRegistrySuccessionState>
      political_shop_registry_successions;
  std::shared_ptr<const HouseholdProvisionManagementState>
      household_provision_management;
  std::shared_ptr<const ProductionDisruptionState> production_disruptions;
  std::shared_ptr<const NavalState> naval;
  std::shared_ptr<const EnvoyTaskState> envoy_tasks;
  std::shared_ptr<const DiplomacyState> diplomacy;
  std::shared_ptr<const PoliticsGovernanceState> politics_governance;
  std::shared_ptr<const GovernanceTaskState> governance_tasks;
  std::shared_ptr<const PoliticsState> politics;
  std::shared_ptr<const PoliticalFiscalState> political_fiscal;
  std::shared_ptr<const FiscalSettlementState> fiscal_settlement;
  std::shared_ptr<const PublicReliefState> public_relief;
  CommonState common;
  std::shared_ptr<const TravelState> travel;
  std::shared_ptr<const MilitaryState> military;
  std::shared_ptr<const InteractionState> interaction;
  std::shared_ptr<const SocialState> social;
  std::shared_ptr<const EstateState> estate;
  std::shared_ptr<const TaskState> tasks;
  std::shared_ptr<const FamilyState> family;
  std::shared_ptr<const PersonalState> personal;
  std::shared_ptr<const GrowthState> growth;
  std::shared_ptr<const PopulationMilitaryState> population_military;
  std::shared_ptr<const PlayerPlanState> player_plan;
  std::shared_ptr<const LocalEconomyState> local_economy;
  std::shared_ptr<const OrganizationState> organization;
  std::shared_ptr<const AffairsState> affairs;
  std::shared_ptr<const DirectorState> director;
};
// owner业务相等比较完整显式值kind/id；字段序只属边界元数据。
inline bool owner_equal(const Owner &a, const Owner &b) {
  return a.kind == b.kind && a.id == b.id;
}
// 工厂只构造错误值；普通规则失败保留旧码，能力/内部错误不能按NPC continue处理。
inline Status common_failure(const StableId &code, const StableId &path = {},
                             ErrorKind kind = ErrorKind::rule) {
  return {false, {}, {{code, path}, kind, {}, {}, {}, {}}};
}
inline Status common_success() { return {true, {}, {}}; }
} // namespace hanmo::resident
