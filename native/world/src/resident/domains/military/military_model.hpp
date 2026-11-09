#include "resident/domains/military/military_command_contract.hpp"
#include "resident/domains/military/military_terms.hpp"
// 军政、远征、会战与围城的既有typed权威；跨域人物与钱粮只借共同服务访问。
#pragma once
#include <memory>
#include "resident/kernel/world_timeline.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "military_travel_end_source.hpp"
#include "siege_integration_model.hpp"
#include "military_officer_model.hpp"
#include "military_field_command_model.hpp"
#include "military_recovery_model.hpp"
#include "named_military_custody_history_model.hpp"
#include "military_garrison_rest_model.hpp"
#include "military_supply_attrition_model.hpp"
#include "military_rout_model.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "siege_model.hpp"
#include "military_siege_lifecycle.hpp"
#include "battle_world_model.hpp"
#include "battle_request_transition.hpp"
#include "occupation_model.hpp"
#include "training_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
namespace hanmo::resident {
struct FieldBattleActorOutcomeContext;
class NamedMilitaryCustodyReleaseRead;
struct OfficeRecord { StableId id,holder_actor_id; bool enabled=false; StableId appointment_id;  std::vector<StableId> field_order; };
struct GarrisonRecord { StableId id; std::vector<StableId> unit_ids,prisoner_ids; Integer last_supply_tick=0;  std::vector<StableId> field_order; };

struct MilitaryUnitRecord {
    StableId id,source_garrison_id,organization_id,garrison_id,army_id,mobilizing_action_id,commander_actor_id,troop_type_id;
    std::vector<StableId> member_ids,fit,wounded,dead,captured,deserted;
    std::vector<std::pair<StableId,StableId>> captors,retainer_sources;
    Integer morale=70;
    std::optional<Integer> training,fatigue;
    std::optional<StableId> training_action_id;
 std::vector<StableId> field_order; };
struct ArmyRecord {
    StableId id,organization_id,commander_actor_id;
    std::vector<StableId> member_actor_ids,unit_ids;
    StableId inventory_id,wallet_id,order_id,garrison_id,status="active";
    Integer last_supply_tick=0;
    std::vector<RetainerRecord> retainers; std::vector<StableId> prisoner_ids;
    StableId formation_source; std::optional<StableId> return_garrison_id;
 std::vector<StableId> field_order; };
// Exact authority annex payloads belong to these original rows only. The legacy
// military codecs deliberately retain their unchanged physical field sequence.


struct MilitaryCampaignAcceptanceBuildView {
 const WorldMoment& accepted_at;SourceRefBuildView actual_acceptance_source;
 const InteractionContext& original_context;MomentOwnerSourceBuildView office_appointment_source;
};
struct MilitaryOfficeResignationBuildView {
 const WorldMoment& resigned_at;SourceRefBuildView actual_resignation_source;
 const OfficeTerms& original_terms;const InteractionContext& original_context;
 MomentOwnerSourceBuildView office_appointment_source;
};


struct MilitaryCampaignAcceptanceTime1 {
 CheckpointMemoryCharge copied_fields;
 WorldMoment accepted_at;SourceRef actual_acceptance_source;
 InteractionContext original_context;MomentOwnerSource office_appointment_source;
 MilitaryCampaignAcceptanceTime1()=default;
 explicit MilitaryCampaignAcceptanceTime1(const MilitaryCampaignAcceptanceBuildView&);
 MilitaryCampaignAcceptanceTime1(const MilitaryCampaignAcceptanceTime1&);
 MilitaryCampaignAcceptanceTime1(MilitaryCampaignAcceptanceTime1&& other):MilitaryCampaignAcceptanceTime1(static_cast<const MilitaryCampaignAcceptanceTime1&>(other)){}
 MilitaryCampaignAcceptanceTime1& operator=(MilitaryCampaignAcceptanceTime1 other)noexcept{swap(other);return *this;}
 void swap(MilitaryCampaignAcceptanceTime1&)noexcept;
};
struct MilitaryOfficeResignationTime1 {
 CheckpointMemoryCharge copied_fields;
 WorldMoment resigned_at;SourceRef actual_resignation_source;OfficeTerms original_terms;
 InteractionContext original_context;MomentOwnerSource office_appointment_source;
 MilitaryOfficeResignationTime1()=default;
 explicit MilitaryOfficeResignationTime1(const MilitaryOfficeResignationBuildView&);
 MilitaryOfficeResignationTime1(const MilitaryOfficeResignationTime1&);
 MilitaryOfficeResignationTime1(MilitaryOfficeResignationTime1&& other):MilitaryOfficeResignationTime1(static_cast<const MilitaryOfficeResignationTime1&>(other)){}
 MilitaryOfficeResignationTime1& operator=(MilitaryOfficeResignationTime1 other)noexcept{swap(other);return *this;}
 void swap(MilitaryOfficeResignationTime1&)noexcept;
};
struct CampaignObjectiveResult { StableId kind; Integer tick=0; std::optional<StableId> source_id;  std::vector<StableId> field_order; };
struct CampaignRecord {
    StableId id,template_id,actor_id,office_id,garrison_id,issuer_role_id,issuer_organization_id,status="accepted",army_id;
    Integer accepted_tick=0,deadline_tick=0,last_tick=0;
    std::optional<CampaignObjectiveResult> objective_result;
    StableId settlement_id,arrival_fact_id,return_receipt_id,recipient_army_id;
    std::optional<StableId> return_garrison_id,return_wait_reason;
 std::vector<StableId> field_order;
 std::optional<MilitaryCampaignAcceptanceTime1> exact_acceptance; };
// 实际到达军令目标后才登记，沿原fact前缀和同阶段序分配身份。
struct CampaignArrivalFact { StableId id,campaign_id,army_id,actor_id; Integer tick=0; StableId stronghold_id; std::vector<StableId> field_order; };
struct StrongholdRecord { StableId id,owner_organization_id,occupation_id,siege_id;  std::vector<StableId> field_order; };


struct MilitaryActionResult { std::optional<bool> ok; StableId office_id,army_id,order_id;  std::vector<StableId> field_order; };
struct MilitaryActionFact {
    StableId id,command,actor_id; Integer start_tick=0,completed_tick=-1;
    StableId phase="started"; bool success=false;
    MilitaryTerms terms;
    std::optional<InteractionContext> context;
    std::optional<MilitaryActionResult> result;
 std::vector<StableId> field_order; };
struct MilitarySupplyFact {
    StableId id,organization_id,ownership_id,army_id,garrison_id;
    Integer tick=0; StableId item_id; std::vector<StableId> mouth_ids;
    Integer required=0,consumed=0,shortage=0;
    // 实际供养当下的唯一来源：正在执行的群旅，或本期原 Opening。
    std::vector<StableId> cause_fact_ids;
 std::vector<StableId> field_order; };


// 既有军令收尾与营地装卸命令，参数保留各自原存在位而不混入battle指令。


struct MilitaryReturnAuthority { StableId kind,id;FieldOrder field_order; };
struct MilitaryLogisticsTerms { StableId army_id,actor_id,garrison_id,source_garrison_id,ownership_id,place_id,organization_id;std::optional<MilitaryReturnAuthority> authority;std::optional<std::vector<std::pair<StableId,Integer>>> goods;std::optional<Integer> money;std::optional<StableId> source_inventory_id,source_wallet_id;FieldOrder field_order; };
struct MilitaryLogisticsResult { std::optional<bool> ok;StableId army_id,order_id,receipt_id;std::optional<std::vector<MilitaryActorChange>> actor_changes;FieldOrder field_order; };
struct MilitaryLogisticsFact { StableId id,command,actor_id,phase="started";Integer start_tick=0,completed_tick=-1;bool success=false;MilitaryLogisticsTerms terms;std::optional<MilitaryLogisticsResult> result;FieldOrder field_order; };
struct MilitaryLogisticsAction { StableId action_id;MilitaryLogisticsTerms terms;ArmyLogistics request;FieldOrder field_order; };
struct MilitaryReturnFact { StableId id,army_id,order_id,garrison_id,source_garrison_id,ownership_id,actor_id;MilitaryReturnAuthority authority;Integer tick=0;std::vector<std::pair<StableId,Integer>> goods;Integer money=0;std::vector<StableId> unit_ids,returned_retainers,lost_retainers,prisoner_ids;std::vector<MilitaryActorChange> actor_changes;FieldOrder field_order; };
struct MilitaryMusterFact { StableId id,actor_id,estate_id,order_id;std::vector<StableId> actor_ids;Integer tick=0;FieldOrder field_order; };
struct MilitaryResignationFact { StableId id,actor_id,office_id;Integer tick=0;FieldOrder field_order;std::optional<MilitaryOfficeResignationTime1> exact_resignation; };
struct MilitaryDeliveryFact { StableId id,campaign_id,army_id,actor_id;Integer tick=0;StableId stronghold_id,recipient_army_id,item_id;Integer quantity=0;StableId from_inventory_id,to_inventory_id;Owner source_owner,recipient_owner;FieldOrder field_order; };
struct CampaignSettlementFact { StableId id,campaign_id,army_id,actor_id;Integer tick=0;StableId reason;std::optional<CampaignObjectiveResult> objective_result;StableId return_receipt_id;std::optional<InteractionContext> report_context;FieldOrder field_order; };
struct MilitaryDeliveryQuote { StableId source_inventory_id,destination_inventory_id;Owner source_owner,recipient_owner;StableId item_id;Integer quantity=0; };
struct MilitaryPledgeQuote { StableId estate_id,order_id;std::vector<StableId> actor_ids; };

struct MilitaryActionPayload { StableId action_id; MilitaryTerms terms; std::variant<OfficeAccept,ArmyForm> request; std::optional<InteractionContext> context;  std::vector<StableId> field_order; };
struct MilitaryState {
    // 独立具名看管附段只存原俘获期限历史，不复制当前 prisoner_ids 占用。
    NamedMilitaryCustodyHistory named_custody;
    // 仅显式Military8写入；旧1..7不追加空恢复段。
    MilitaryRecoveryState recovery;
    MilitaryGarrisonRestState garrison_rest;
    MilitarySupplyAttritionState supply_attrition;
    MilitaryRoutState rout;
    // 仅显式Military7追加，指挥交接不另建Army或改原形成事实。
    std::vector<MilitaryFieldCommandFact> field_commands;
    // 仅显式调任profile写Military6，旧段不增加空序列。
    std::vector<MilitaryOfficerAssignmentFact> officer_assignments;
    // 请求事实只在显式request_policy/1开启；旧军事段不写存在位。
    std::vector<MilitaryRequestReceipt> request_facts;
    std::vector<MilitaryBattleRecord> battles;
    std::vector<SiegeRecord> sieges;
    MilitarySiegeEngineeringState siege_engineering;
    MilitarySiegeLifecycleState siege_lifecycle;
    std::vector<MilitaryTrainingFact> training_facts;
    std::vector<MilitaryTrainingAction> training_actions;
    std::vector<MilitaryMarchFact> march_facts;
    std::vector<MilitaryOccupationFact> occupation_facts;
    std::vector<MilitaryReliefFact> relief_facts;
    std::vector<SiegeMaterialFact> siege_material_facts;
    std::vector<MilitaryLogisticsAction> logistics_actions;
    std::vector<MilitaryLogisticsFact> logistics_facts;
    std::vector<MilitaryReturnFact> return_facts;
    std::vector<MilitaryMusterFact> muster_facts;
    std::vector<MilitaryResignationFact> resignation_facts;
    std::vector<MilitaryDeliveryFact> delivery_facts;
    std::vector<CampaignSettlementFact> settlement_facts;
    std::vector<MilitaryContactFact> contact_facts;
    std::vector<MilitaryBattleFact> battle_facts;
    std::vector<MilitaryCasualtyFact> casualty_facts;
    std::vector<OfficeRecord> offices;
    std::vector<GarrisonRecord> garrisons;
    std::vector<MilitaryUnitRecord> military_units;
    std::vector<ArmyRecord> armies;
    std::vector<CampaignRecord> campaigns;
    std::vector<CampaignArrivalFact> arrival_facts;
    std::vector<StrongholdRecord> strongholds;
    std::vector<MilitaryActionPayload> actions;
    std::vector<MilitaryActionFact> action_facts;
    std::vector<MilitarySupplyFact> supply_facts;
    SiegeIntegrationState siege_integration;
    std::vector<MilitaryIntegrationFact> integration_facts;
};
struct EquipmentDefinition { StableId item_id; Integer quantity_per_person=0; };
struct MilitaryUnitDefinition { StableId id,commander_actor_id,troop_type_id; Integer initial_soldiers=0; std::vector<EquipmentDefinition> equipment;std::optional<Integer> initial_training,initial_fatigue;std::optional<StableId> display_name;std::optional<StableId> population_source_id; };
struct GarrisonDefinition { StableId id,organization_id,place_id,inventory_id,wallet_id,food_item_id; Integer army_capacity=0,ration_per_person=0; std::vector<MilitaryUnitDefinition> units;StableId display_name;std::optional<StableId> training_wallet_id,training_drill_id;std::optional<Integer> training_fatigue_per_tick; };
struct OfficeDefinition { StableId id,organization_id,garrison_id,appointing_role_id,command_scope; };
struct MilitaryOrderDefinition { StableId id,office_id,garrison_id,issuer_role_id,report_role_id,target_stronghold_id,objective_kind; Integer deadline_ticks=0; StableId recipient="garrison",recipient_organization_id,item_id; Integer quantity=0;StableId display_name; };
struct StrongholdDefinition { StableId id,garrison_id,settlement_id,topology_id;std::vector<StableId> public_inventory_ids,public_wallet_ids; };
struct MilitaryPlan { StableId actor_id,office_id,order_id; std::vector<StableId> unit_ids; Integer food_quantity=0,money=0; };
struct MilitaryCatalog {
    // 未显式选择时没有期限缺省值；作者参数必须由锁定目录实际读取。
    std::optional<NamedMilitaryCustodyPolicy> named_custody_policy;
    std::optional<MilitarySiegeEngineeringPolicy> siege_engineering_policy;
    std::optional<MilitaryRecoveryPolicy> recovery_policy;
    std::optional<MilitaryGarrisonRestPolicy> garrison_rest_policy;
    std::optional<MilitarySupplyAttritionPolicy> supply_attrition_policy;
    std::optional<MilitaryRoutRetreatPolicy> rout_retreat_policy;
    std::optional<MilitaryCommandShockRule> command_shock_policy;
    std::optional<MilitaryCombatConditionRule> combat_condition_policy;
    std::optional<MilitaryCommandedDamageRule> commanded_damage_policy;
    std::optional<MilitaryCommandResourcesRule> command_resources_policy;
    std::optional<MilitarySequentialRoundRule> sequential_round_policy;
    std::optional<MilitaryFieldTerrainPolicy> field_terrain_policy;
    std::optional<MilitaryFieldCommandPolicy> field_command_policy;
    std::optional<MilitaryOfficerAssignmentPolicy> officer_assignment_policy;
    std::optional<MilitaryRequestPolicy> request_policy;
    bool military=false,march_condition_model=false;
    StableId scenario_id;
    std::vector<OfficeDefinition> offices;
    std::vector<GarrisonDefinition> garrisons;
    std::vector<MilitaryOrderDefinition> orders;
    std::vector<StrongholdDefinition> strongholds;
    std::vector<MilitaryPlan> plans;
    std::vector<StableId> troop_type_ids;
    MilitaryBattleCatalog battle_catalog;
    std::vector<SiegeTopology> siege_topologies;
    std::vector<DrillDefinition> drill_challenges;
    std::vector<StableId> item_ids;
    std::vector<std::pair<StableId,StableId>> estate_display_names;
    SiegeIntegrationCatalog siege_integration;
};
class TravelServices;
struct NavalState;
class MilitarySiegeEngineeringAccess;
class MilitaryActorConditionWrite;
class CurrentMomentRead;
class CommonStore;
struct MilitaryAuthorityTimeContext;
struct MilitaryActionConditionContext;
struct MilitaryBattleLifecycleWriteContext;
class OriginalEstateDetachmentReleaseSink;
struct MilitaryAccess {
    // 新条件账只借同候选实际组写后服务；旧未启用为空，无永久Military格式变化。
    std::shared_ptr<MilitaryActorConditionWrite> actor_conditions;
    // 完整同候选服务寿命内借用；显式17缺此读写门时拒绝，不退回旧无来源围城。
    std::shared_ptr<MilitarySiegeEngineeringAccess> siege_engineering;
    // 只在真实contact_install同候选严格单步绑定；缺读口必须拒绝，不借当前技能兜底。
    std::function<Outcome<MilitaryCommandedDamageState>(const MilitaryBattleRecord&)> prepare_commanded_damage;
    // 同contact栈读取原兵法；缺读口拒绝新入场，不允许今日技艺兜底。
    std::function<Outcome<MilitaryCommandResourcesState>(const MilitaryBattleRecord&)> prepare_command_resources;
    // 只在同contact候选读取已验证唯一习得账，不能借今天列表回填历史。
    std::function<Outcome<MilitaryTacticEntryState>(const MilitaryBattleRecord&)> prepare_learned_tactics;
    const MilitaryTacticLearningState* tactic_learning_state=nullptr;
    // 原Common接口不暴露完整学习账；World同栈薄桥回到唯一军事认定函数。
    std::function<Outcome<MilitaryTacticPracticeCompletion>(const StableId&,const MilitaryTacticPracticeClaim&)> preview_tactic_practice;
    std::function<Outcome<MilitaryTacticPracticeReceipt>(const StableId&,const MilitaryTacticPracticeClaim&)> apply_tactic_practice;
    const CommonCatalog* common_catalog=nullptr;
    const TravelCatalog* travel_catalog=nullptr;
    std::function<Outcome<Integer>(const StableId&,const StableId&)> return_route_cost;
    std::function<Status(const StableId&,const StableId&)> preview_travel;
    std::function<std::optional<Integer>(const StableId&,const StableId&)> actor_skill;
    std::function<std::optional<Integer>(const StableId&,const StableId&)> actor_attribute;
    std::function<std::optional<RngStream>(const StableId&)> rng_stream;
    const TravelState* travel_state=nullptr;
    const TravelServices* travel_services=nullptr;
    std::function<Integer()> next_instance_id;
    std::function<bool(const StableId&)> fact_exists;
    std::function<Outcome<std::vector<StableId>>(const StableId&,const StableId&,const std::vector<StableId>&)> preview_estate_muster;
    std::function<Status(const StableId&,const StableId&,const std::vector<StableId>&,const StableId&)> apply_estate_muster;
    std::function<Status(const StableId&,const StableId&,const StableId&,OriginalEstateDetachmentReleaseSink*)> release_lost_retainer;
    Integer military_return_detachments_schema=0;

    StableId player_actor_id;Integer battle_seed=0;
    std::function<bool(const StableId&)> in_open_battle;
    const InteractionState* interaction=nullptr;
    const InteractionCatalog* interaction_catalog=nullptr;
    const InteractionAccess* interaction_access=nullptr;
    std::function<Location(const StableId&)> resolve_location;
    std::function<bool(const StableId&)> in_party;
    std::function<Integer(const StableId&)> item_weight,actor_injury;
    std::function<std::optional<RetainerRecord>(const StableId&,const StableId&)> retainer_source;
    // 人口来源分支由同候选适配器证明；退籍后仍保留旧口粮回执的匿名身份。
    std::function<Outcome<std::vector<StableId>>(const StableId&,const StableId&)> population_roster;
    std::function<bool(const StableId&,Integer)> population_historical_token;
    std::function<bool(const StableId&,const StableId&,const StableId&,Integer)> population_training_member;
    // 仅报告真实实船占用，不把暂不可航或撤职误作已离船。
    std::function<bool(const StableId&)> army_embarked;
    // 仅显式Naval2的已验证事实链提供，旧profile为空且继续拒绝新败军/真人俘虏状态。
    std::function<std::optional<StableId>(const StableId&)> naval_custodian;
    std::function<std::optional<StableId>(const StableId&)> naval_defeat_source;
    // 显式责任模块在同一World候选内完成占领前捕获、唯一资产应用及责任登记。
    // required为真而服务缺失时，必须在分配占领ID或转权之前拒绝。
    bool occupation_application_required=false;
    // Explicit military_occupation/2; same-candidate trusted phase clock only.
    Integer occupation_source_schema=1;
    const CurrentMomentRead* occupation_clock=nullptr;
    const CommonStore* occupation_common=nullptr;
    std::function<Outcome<OccupationEffect>(const StableId&)> apply_occupation;
    // 只借同候选原历史的具名轮次前名册；不得传任意作者字典或另存一份兵员。
    std::function<Outcome<std::vector<MilitaryUnitRecord>>(const std::vector<StableId>&,Integer,const StableId&)> rosters_before;
    // 真实死亡等退出只终止原旅行Action；领域回执再交原计划/对白完成门。
    std::function<Status(const StableId&)> end_journey;
    // 指挥来源和政治专项签受均来自同一候选，缺口不能降级为一般军令。
    std::function<Outcome<MilitaryUnitCommandObservation>(const StableId&)> unit_command_source;
    const MilitaryOfficerAppointmentRead* officer_appointments=nullptr;
    // 野外现场来源与专项政治权源独立于驻营口，均须借同一完整候选。
    std::function<Outcome<MilitaryFieldCommandObservation>(MilitaryFieldCommandKind,const StableId&,const StableId&)> field_command_source;
    const MilitaryFieldCommandAppointmentRead* field_appointments=nullptr;
    std::function<Outcome<MilitaryArmyCommandBinding>(const StableId&)> fleet_command_source;
    std::function<Outcome<MilitaryArmyCommandBinding>(const StableId&,const StableId&)> army_command_before;
    const NavalState* naval_state=nullptr;
    // 与原事实边界纯读同源，当前已知不可重放的占用必须在报价前拒绝。
    std::function<Status(const StableId&)> field_actor_occupation_clear;
    // 仅在本次完整野外交接事实已落本候选后同步同军活动Fleet当前指挥者。
    std::function<Status(const MilitaryFieldCommandFact&)> apply_field_fleet_binding;
    // 原field_exit_source已核出的真实伤亡头，只传同栈原因，不在回调内伪造到达。
    MilitaryTravelEndWithSource end_journey_with_source;
    // Explicit military_authority_time/1 selection, never inferred from rows.
    Integer military_authority_time_schema=0;
    const MilitaryAuthorityTimeContext* authority_time=nullptr;
    Integer military_action_resources_schema=0;
    Integer military_action_conditions_schema=0;
    Integer military_training_completion_time_schema=0;
    Integer military_formation_reservations_schema=0;
    const MilitaryActionConditionContext* action_conditions=nullptr;
    // Explicit adjunct selection. A real candidate scope/lease owns the clock
    // and context; this is independent of the occupation schema and its clock.
    Integer military_battle_lifecycle_schema=0;
    const MilitaryBattleLifecycleWriteContext* battle_lifecycle=nullptr;
    // 只借同 World 三原根；中央原 owning scope 的 alias 负责被借根寿命，非另一份账。
    std::shared_ptr<const FieldBattleActorOutcomeContext> field_battle_actor_outcomes;
    // 仅真正关闭历史可解除当前名单；原 captured/captors 不因释放被重写。
    std::shared_ptr<const NamedMilitaryCustodyReleaseRead> named_custody_releases;
    // 完整 World 一律借真实锁目录，包含未选 Field 的情形；世界运行身份不是内容选择。
    const CatalogIdentity* actual_catalog_identity=nullptr;
};
struct MilitaryProvisions { StableId food_item_id; Integer mouth_count=0,required=0,available=0,full_ticks=0,shortage=0; };
// 同源只读军粮报价供有限视图使用；不扣粮、不分配ID、不接受任意观察者权限。
Outcome<MilitaryProvisions> preview_army_supply(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const StableId& army_id);
struct MilitaryQuote { std::optional<MilitaryTacticPracticeCompletion> tactic_practice; std::optional<MilitaryFieldCommandQuote> field_command; std::optional<MilitaryOfficerAssignmentQuote> officer_assignment; std::optional<MilitaryRequestReceipt> request_receipt;bool request_replayed=false;std::optional<MilitaryTrainingTerms> training_terms;std::optional<Integer> training_cost,expected_training_gain,provided_half_days;StableId action_id;std::optional<DrillState> drill_before,drill_after; std::optional<MilitaryLogisticsTerms> logistics_terms;std::optional<MilitaryDeliveryQuote> delivery;std::optional<MilitaryPledgeQuote> pledge;std::optional<OfficeTerms> resignation; StableId battle_id,siege_id;MilitaryOrders battle_orders;std::optional<SiegeCreateRequest> siege_request;std::optional<SiegeResolution> siege_resolution; std::optional<MilitaryProvisions> provisions; Integer duration=0; std::vector<StableId> participants; std::optional<MilitaryTerms> terms; std::optional<InteractionContext> context; StableId template_id,recipient_army_id,campaign_id; };
// 军令只提出这一项确定行程；中央在同一候选内调用B，不建立另一提交点。
struct MilitaryJourneyRequest { StableId army_id,destination_settlement_id; std::vector<StableId> field_order;std::optional<StableId> return_garrison_id; };
struct MilitaryEffect { std::optional<MilitaryRequestAnswer> request_answer;StableId action_id;std::optional<MilitaryTrainingReceipt> training_receipt; std::vector<StableId> completed_receipt_ids; StableId receipt_id,fact_id,source_id;std::optional<StableId> return_wait_reason; StableId battle_id,siege_id;std::optional<bool> plan_submitted;std::vector<MilitaryActorChange> actor_changes;std::vector<MilitaryJourneyRequest> journey_requests; bool success=true; StableId code,office_id,army_id,order_id,campaign_id; std::optional<bool> details_ok,already_completed; std::optional<MilitaryJourneyRequest> journey_request;std::optional<SiegeIntegrationPropose> integration_request;
 // 本次真实终止产生的伤亡头顺序，仅热返回，不加入原effect回执/存档访客。
 std::vector<StableId> casualty_source_ids; };
const ArmyRecord* active_army_for_actor(const MilitaryState&,const StableId&);
Outcome<MilitaryQuote> preview_military(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryCommand&,const StableId& own_action={});
Status start_military_action(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,const MilitaryCommand&,const MilitaryQuote&);
Outcome<MilitaryEffect> finish_military_action(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&);
Outcome<MilitaryEffect> apply_military_instant(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryCommand&);
Status military_supply_step(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick);
Status campaign_step(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const TravelCatalog&,Integer finish_tick);
// 军团宏观位置只读真实统帅，城市或同地图精确城市点均算已在目标。
bool army_at_settlement(const CommonRead&,const MilitaryState&,const TravelCatalog&,const StableId& army_id,const StableId& settlement_id);
Status validate_military_action(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,bool finishing=false);
Status validate_military_state(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Status visit_military_checkpoint(CodecVisitor&,MilitaryState&,Integer occupation_source_schema=1);
// unsupported错误携带确切阶段/命令/来源；调用方必须丢弃整候选，绝不按NPC普通拒绝继续。
Status military_capability_failure(const StableId& phase,const StableId& kind,const StableId& source);
}
