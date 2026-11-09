// 采购、送信、护卫合同的唯一typed状态；预算和任务产权仍由共同资产表拥有。
#pragma once
#include "extensions/task_extension_state.hpp"
#include "message_returns/task_message_return_state.hpp"
#include "personal_messages/task_personal_message_state.hpp"
#include "personal_reports/task_personal_report_state.hpp"
#include "personal_cancellations/task_personal_cancellation_state.hpp"
#include "personal_acceptances/task_personal_acceptance_state.hpp"
#include "shop_orders/task_shop_order_state.hpp"
#include "static_assets/task_static_asset_state.hpp"
#include "task_offer_response.hpp"
#include "resident/domains/family/death_ref.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "message_notices/task_message_notice_state.hpp"
#include "message_handoffs/task_message_handoff_state.hpp"
#include "task_public_account_state.hpp"
#include "task_personal_advance.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/model/offer_ref.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/social/social_model.hpp"
#include "task_claim_establishment.hpp"
#include "task_quantity_deadline.hpp"
#include "state_need_task.hpp"
#include "work_execution/work_execution_contract.hpp"
#include "work_execution/work_commission_terminal.hpp"
#include "task_transport_agreement.hpp"
#include "objectives/task_objective_evidence.hpp"
namespace hanmo::resident {
struct TaskPersonalAcceptanceServices;struct TaskPersonalReportServices;struct TaskPersonalMessageServices;struct TaskMessageReturnServices;struct TaskMessageNoticeServices;struct TaskExecutionServices;class PoliticalTakeoverRead;
struct TaskGoal { StableId item_id,message_id,receiver_role,receiver_actor,actor_id,destination_place_id,objective_id;Integer quantity=0; };
struct TaskDefinition { StableId id,template_id,accept_role,report_role,funding_wallet,on_unavailable;StableId display_name;Owner issuer;Integer budget=0,reward=0,accept_ticks=0,deadline_ticks=0;TaskGoal goal; };
// 作者委托与动态模板均为不可变目录；运行机会不写回PreparedCatalog。
struct TaskPersonalAcceptanceCatalog;
struct TaskCatalog { OrderedTable<TaskDefinition> offers;OrderedTable<StableId> presentation_names;OrderedTable<TaskDefinition> templates;std::shared_ptr<const TaskPersonalAcceptanceCatalog> personal_acceptances; };
struct SourceBinding { StableId affair_id,need_id,claim_id,contract_id;Integer source_revision=0; };
enum class OfferStatus { published,withdrawn,closed };
// definition是发布时固定的模板实例，quantity仅为合同上限；可接份额始终从Need读取。
struct OfferInstance { StableId id,template_id,source_need_id;Integer need_revision_at_creation=0;Owner issuer;ScopeRef accept_scope,report_scope;StableId funding_wallet_id,reward_wallet_id,terms_version;Integer available_from_tick=0;std::optional<Integer> expires_tick;OfferStatus status=OfferStatus::published;TaskDefinition definition; };
// 冻结条款保留原资金来源和整数结酬政策；剩余采购金不得混入酬劳。
struct FrozenTaskTerms { TaskDefinition definition;Integer agreed_quantity=0,purchase_budget_cap=0,reward_total=0,deadline_tick=0;StableId target_inventory_id,item_id,quantity_unit,source_wallet_id,reward_wallet_id,payee_wallet_id;SubjectRef beneficiary;StableId terms_version,payment_policy="delivered_on_time_pro_rata_v1",unused_budget_policy="return_to_issuer",late_reward_policy="zero",cancellation_policy_id="return_actual_cargo_v1";bool personal_advance_allowed=false; };
enum class ContractStatus { accepted,active,settling,settled };
struct ContractRecord { StableId id;SourceBinding source_binding;Owner issuer;SubjectRef assignee;std::optional<StableId> executor_actor_id,org_activity_id,current_task_id;std::vector<StableId> historical_task_ids;FrozenTaskTerms frozen_terms;ContractStatus status=ContractStatus::accepted;std::optional<StableId> settlement_id;std::vector<StableId> restitution_refs;Integer delivered_quantity=0,eligible_delivered_quantity=0,paid_reward=0,refunded_budget=0,refunded_reward=0;bool execution_stopped=false;StableId stop_reason;std::optional<ActingAuthority> accepted_authority;std::vector<StableId> cargo_disposition_ids;std::optional<TaskQuantityDeadlineClosure> deadline_closure; };
// 一份实际转移对应一个交付事实；Affairs反查该事实的合同来源，不接受调用者改绑。
enum class TaskQuantitySourceKind {legacy_procurement,procurement_exact};
struct DeliveryFact { StableId id,task_id,activity_id,source_action_id,from_inventory_id,to_inventory_id;SubjectRef beneficiary;StableId item_id;Integer quantity=0,completed_tick=0;StableId transfer_fact_id;std::optional<WorldMoment> completed_at;std::optional<SourceRef> original_completion_result_ref;TaskQuantitySourceKind quantity_source_kind=TaskQuantitySourceKind::legacy_procurement; };
struct TaskTransferFact { StableId id,contract_id,task_id,source_action_id,from_inventory_id,to_inventory_id,item_id;Integer quantity=0,completed_tick=0;Owner from_owner,to_owner;StableId purpose;std::optional<WorldMoment> completed_at;std::optional<SourceRef> original_completion_result_ref;TaskQuantitySourceKind quantity_source_kind=TaskQuantitySourceKind::legacy_procurement; };
struct Shipment { StableId id,activity_id,source_inventory_id,cargo_inventory_id,destination_inventory_id;Owner cargo_owner;SubjectRef carrier_ref;StableId route_or_action_id,status;std::vector<StableId> receipt_ids; };
// 路遇货损只有作者明确的固定上限规则；参战胜负不会自动推导全部掠夺或比例。
struct TaskCargoPolicy { StableId id,version="fixed_caps_v1",task_template_id,item_id,outcome="defeat",recipient="encounter_opponent",allocation="oldest_active_contract",unavailable_recipient="no_disposition";Integer transfer_cap=0,destroy_cap=0; };
struct TaskCargoTotals { Integer transferred_quantity=0,destroyed_quantity=0; };
struct TaskCargoLot { StableId lot_id;Integer quantity=0; };
enum class TaskCargoResolution { pending,skipped,applied };
// 入场时冻结实际货物与双方路遇锚点；这里的数量是历史验收依据，不是第二库存。
struct TaskCargoExposure { StableId battle_id,encounter_id,contract_id,task_id,carrier_actor_id,recipient_actor_id,recipient_prototype_id,source_inventory_id,destination_inventory_id;TaskCargoPolicy policy;Location carrier_anchor,recipient_anchor;Integer created_tick=0,source_inventory_revision=0,destination_inventory_revision=0,destination_capacity=0,destination_weight=0,frozen_quantity=0,transfer_quantity=0,destroy_quantity=0;std::vector<TaskCargoLot> lots;TaskCargoResolution resolution=TaskCargoResolution::pending;StableId source_result_id,disposition_fact_id,resolution_reason;Integer resolved_tick=-1; };
// 一个真实个人战结果只产生一次有限处置；转移和销毁分别留数量，均不计入交付奖励。
struct TaskCargoDispositionFact { StableId id,battle_id,encounter_id,source_result_id,contract_id,task_id,carrier_actor_id,recipient_actor_id,from_inventory_id,to_inventory_id,item_id;Owner from_owner,to_owner;Integer tick=0,source_quantity_before=0,source_quantity_after=0,transferred_quantity=0,destroyed_quantity=0; };
// 唯一生产适配器从实际PersonalBattle/PersonalFact/Encounter读取这些字段；命令不能提交它。
struct TaskCargoBattleSource { StableId battle_id,encounter_id,controller_actor_id,opponent_actor_id,opponent_prototype_id;TaskCargoPolicy policy;Location controller_anchor,opponent_anchor;Integer created_tick=0,completed_tick=-1,round=1;StableId phase,outcome,result_id;bool result_applied=false,opponent_survived=false; };
class TaskCargoSourceRead { public:virtual ~TaskCargoSourceRead()=default;virtual Outcome<std::optional<TaskCargoBattleSource>> task_cargo_source(const StableId& battle_id)const=0;virtual std::vector<StableId> task_cargo_sources()const=0; };
// 报价携带来源revision与资金版本，实际采用前必须再次验证；报价本身不分配ID。
struct RuntimeTaskQuote { StableId need_id;Integer need_revision=0,quantity=0;FrozenTaskTerms terms;std::vector<ReadVersion> resource_versions; };
struct EscortProgress { bool departed=false;Integer arrival_tick=-1;std::vector<StableId> battle_ids;StableId failure;Integer failed_tick=-1;FieldOrder field_order{}; };
// S3 tasks/2 finite record choice. /1 never writes these successor fields.
// Exact fields belong to this same Task row; legacy Tick members remain only
// compatibility projections and are not an alternative historical authority.
enum class TaskRecordKind {legacy_quantity,exact_quantity,native_objective};
struct TaskExactQuantityFields {
 SubjectRef assignee;StableId original_acceptance_action_ref;
 StateNeedDeadline deadline_gate;
 std::optional<WorldMoment> objective_completed_at,reported_at;
 std::optional<SourceRef> original_acceptance_result_ref,original_report_result_ref;
};
struct TaskRecord { StableId id,offer_id,assignee,status,budget_wallet_id,settlement_id;Integer serial=0,accepted_tick=0,deadline_tick=0;std::optional<Integer> objective_completed_tick,reported_tick;std::optional<bool> late;std::optional<EscortProgress> escort;FieldOrder field_order{};std::optional<OfferRef> offer_ref;std::optional<StableId> contract_id;std::optional<WorldMoment> accepted_at;TaskRecordKind record_kind=TaskRecordKind::legacy_quantity;std::optional<TaskExactQuantityFields> exact_quantity; };
struct TaskCommand { StableId kind,offer_id;InteractionContext context;FieldOrder field_order{};std::optional<StableId> source_kind;std::optional<OfferRef> offer_ref;std::optional<Integer> need_revision,quantity; };
struct TaskQuote { Integer duration=0;InteractionContext context;std::optional<StableId> task_id;std::optional<Integer> reward;std::optional<RuntimeTaskQuote> runtime;std::optional<Integer> delivery_quantity;bool restitution=false; };
struct TaskAction { StableId offer_id,task_id;InteractionContext context;std::optional<Integer> reward;FieldOrder field_order{};std::optional<StableId> source_kind;std::optional<Integer> delivery_quantity;bool restitution=false; };
struct EscortArrivalFact { StableId task_id,actor_id,escort_actor_id,place_id;Integer tick=0;FieldOrder field_order{}; };
struct TaskEffect { bool success=true;std::optional<StableId> task_id,code,on_unavailable,retry;FieldOrder field_order{}; };
// Immutable source payloads from the actual Task resource writer. They carry no second service progress.
struct TaskExactStartBuildView {
 const StableId& id;const StableId& action_id;const StableId& action_kind;const StableId& actor_id;
 const std::vector<StableId>& participants;const TaskAction& accepted_action;Integer required_half_days;
 const WorldMoment& started_at;const CatalogIdentity& catalog_identity;const TermsRevisionRef& terms_revision;
 const std::vector<SourceRef>& actual_start_sources;
};
struct TaskExactStartSource {
 CheckpointCopyLease copied_fields; // first, actual fresh ordinary copy ownership; not wire
 TaskExactStartSource()=default;
 explicit TaskExactStartSource(const TaskExactStartBuildView&);
 TaskExactStartSource(const TaskExactStartSource&);
 TaskExactStartSource(TaskExactStartSource&&);
 TaskExactStartSource& operator=(const TaskExactStartSource&);
 TaskExactStartSource& operator=(TaskExactStartSource&&);
 void swap(TaskExactStartSource&);
 StableId id,action_id,action_kind,actor_id;std::vector<StableId> participants;TaskAction accepted_action;
 Integer required_half_days=0;WorldMoment started_at;CatalogIdentity catalog_identity;TermsRevisionRef terms_revision;
 std::vector<SourceRef> actual_start_sources;
};
enum class TaskExactDeathRole { assignee,interaction_provider };
struct TaskExactParticipantDeath {DeathRef death_ref;TaskExactDeathRole role=TaskExactDeathRole::assignee;};
struct TaskExactParticipantDeathBuildView {const DeathRef& death_ref;TaskExactDeathRole role;};
struct TaskExactTerminalBuildView {
 const StableId& id;const StableId& start_source_id;const StableId& action_id;
 const WorldMoment& finished_at;const TaskEffect& effect;
 const StableId& task_status_after;const StableId& settlement_id_after;
 const std::vector<SourceRef>& actual_result_sources;FixedTerminalDisposition disposition;
 std::optional<TaskExactParticipantDeathBuildView> participant_death;
};
struct TaskExactTerminalSource {
 CheckpointCopyLease copied_fields; // first, runtime only; never written to wire
 TaskExactTerminalSource()=default;
 explicit TaskExactTerminalSource(const TaskExactTerminalBuildView&);
 TaskExactTerminalSource(const TaskExactTerminalSource&);
 TaskExactTerminalSource(TaskExactTerminalSource&&);
 TaskExactTerminalSource& operator=(const TaskExactTerminalSource&);
 TaskExactTerminalSource& operator=(TaskExactTerminalSource&&);
 void swap(TaskExactTerminalSource&);
 friend void swap(TaskExactTerminalSource&a,TaskExactTerminalSource&b){a.swap(b);}
 StableId id,start_source_id,action_id;WorldMoment finished_at;TaskEffect effect;
 StableId task_status_after,settlement_id_after;std::vector<SourceRef> actual_result_sources;
 FixedTerminalDisposition disposition=FixedTerminalDisposition::completed;
 // Only explicit exact execution2 carries this original interruption cause.
 // The action, task and exact M are the existing terminal/start identities.
 std::optional<TaskExactParticipantDeath> participant_death;
};
struct TaskExactExecutionState {Integer version=1;OrderedTable<TaskExactStartSource> starts;OrderedTable<TaskExactTerminalSource> terminals;};
class OriginalTaskReportRelationWrite;
struct TaskExactLifeSources;
struct TaskExactWrite {ActionElapsedStore& elapsed;const TimeScaleRef& scale;const WorldMoment& at;const CatalogIdentity& catalog_identity;bool defer_adoption=false;std::optional<std::size_t> first_new_fact;OriginalTaskReportRelationWrite* report_relations=nullptr;const TaskExactLifeSources* life=nullptr;};
struct TaskState { std::optional<TaskServiceTerminalState> service_terminals; std::optional<TaskWorkExecutionState> work_execution;std::optional<TaskObjectiveBaseState> objective_base;std::optional<TaskExactExecutionState> exact_execution;std::optional<TaskTransportAgreementState> transport_agreements;std::optional<StateNeedTaskState> state_need_tasks;bool exact_quantities=false;Integer dynamic_tasks_version=2;std::optional<TaskClaimEstablishmentState> claim_establishment; OrderedTable<TaskRecord> tasks;OrderedTable<TaskAction> actions;OrderedTable<EscortArrivalFact> facts;OrderedTable<OfferInstance> offer_instances;OrderedTable<ContractRecord> contracts;OrderedTable<DeliveryFact> deliveries;OrderedTable<TaskTransferFact> transfers;OrderedTable<Shipment> shipments;OrderedTable<TaskCargoExposure> cargo_exposures;OrderedTable<TaskCargoDispositionFact> cargo_dispositions;std::optional<TaskOfferResponseState> offer_responses;std::optional<TaskPersonalAdvanceState> personal_advances;std::optional<TaskPublicAccountExecutionState> takeover_execution;std::optional<TaskMessageHandoffState> message_handoffs;std::optional<TaskMessageNoticeState> message_notices;std::optional<TaskStaticAssetState> static_assets;std::optional<TaskExtensionState> deadline_extensions;
    // 独立returns/1保存；旧Task1/2 visitor不添加空字段。
    std::optional<TaskMessageReturnState> message_returns;std::optional<TaskPersonalMessageState> personal_messages;std::optional<TaskPersonalReportState> personal_reports;std::optional<TaskPersonalCancellationState> personal_cancellations;std::optional<TaskPersonalAcceptanceState> personal_acceptances;std::optional<TaskShopOrderState> shop_orders; };
// 战斗来源由个人战唯一owner投影，任务只保留ID及履约结果，绝不保存第二战斗。
struct TaskProtectedBattle { StableId id,controller_id,outcome;std::vector<StableId> protected_actor_ids;bool applied=false; };
using TaskBattleRead=std::function<std::optional<TaskProtectedBattle>(const StableId&)>;
// 组织与共同预留仅借只读合同注册；不复制合同或钱包责任。
class ContractRead { public:virtual ~ContractRead()=default;virtual const ContractRecord* contract(const StableId&) const=0;virtual bool contract_reservation_exists(const StableId&) const=0; };
struct AffairsState;
struct KnownSubjectView;
// 唯一解析口保证付款、手记、条件及验证均读到接受时冻结的条款。
const TaskDefinition* resolve_offer_definition(const TaskState&,const TaskCatalog&,const OfferRef&);
const TaskDefinition* resolve_task_definition(const TaskState&,const TaskCatalog&,const TaskRecord&);
// 以有界整数长除法计算floor(total*quantity/agreed)，不依赖超出C++17的整数类型。
Outcome<Integer> task_prorata(Integer total,Integer quantity,Integer agreed);
OfferRef task_command_offer(const TaskCommand&);
const TaskDefinition* resolve_known_offer_definition(const TaskState&,const TaskCatalog&,const StableId&);
Outcome<TaskQuote> preview_dynamic_task(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const TaskCommand&,const AffairsState&,const KnownSubjectView*,const TaskExecutionServices* execution=nullptr);
Status start_dynamic_task_action(CommonStore&,TaskState&,const ActionCoreView&,const TaskCommand&,const TaskQuote&,AffairsState&,const TaskExecutionServices* execution=nullptr,const WorldMoment* exact_at=nullptr);
Outcome<TaskEffect> finish_dynamic_task_action(CommonStore&,TaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ActionCoreView&,Integer,AffairsState&,const TaskExecutionServices* execution=nullptr,const WorldMoment* exact_at=nullptr,const TaskExactLifeSources* life=nullptr);
Status cancel_dynamic_task(CommonStore&,TaskState&,const StableId&,AffairsState&,const TaskExecutionServices* execution=nullptr);
bool task_contract_reservation_exists(const TaskState&,const StableId&);
Status visit_dynamic_task_checkpoint(CodecVisitor&,TaskState&);
Outcome<TaskState> decode_dynamic_task_state(const BoundaryValue&,TaskState base={});
BoundaryValue encode_dynamic_task_state(const TaskState&);
Status validate_dynamic_tasks(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const PoliticalTakeoverRead* authority=nullptr,const CurrentMomentRead* actual_clock=nullptr,const TaskExactLifeSources* life=nullptr);
Status reconcile_task_contracts(CommonStore&,TaskState&,AffairsState&,Integer tick,const TaskExecutionServices* execution=nullptr,const DynamicStepInput* original_calendar_phase=nullptr,const TaskQuantityDeadlineContext* quantity_deadline=nullptr,const ExactPeriodContext* original_period=nullptr);
Status settle_task_contract(CommonStore&,TaskState&,const StableId& contract_id,Integer tick,const TaskExecutionServices* execution=nullptr,const DynamicStepInput* original_calendar_phase=nullptr);
// 只接受真实来源ID；政策、目的地和数量均由来源适配器与入场快照决定。
Status validate_task_cargo_policy(const TaskCargoPolicy&);
Outcome<TaskCargoTotals> task_cargo_totals(const TaskState&,const StableId& contract_id);
Outcome<Integer> task_cargo_remaining_quantity(const TaskState&,const StableId& contract_id);
Status freeze_task_cargo_exposure(CommonStore&,TaskState&,const StableId& battle_id,const TaskCargoSourceRead&);
Status apply_task_cargo_result(CommonStore&,TaskState&,const StableId& battle_id,const TaskCargoSourceRead&);
Status validate_task_cargo_integrity(const CommonStore&,const TaskState&);
Status validate_task_cargo_sources(const CommonStore&,const TaskState&,const TaskCargoSourceRead&);
BoundaryValue encode_task_cargo_facts(const TaskState&);
struct GrowthState;
Outcome<BoundaryValue> query_task_journal(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const InteractionState&,const SocialState&,const GrowthState&);
Outcome<TaskCatalog> decode_task_catalog(const BoundaryValue&);
Outcome<TaskState> decode_task_seed(const BoundaryValue&);
Outcome<TaskState> decode_task_seed(const BoundaryValue&,bool exact_quantities);
BoundaryValue encode_task_state(const TaskState&);
Outcome<TaskCommand> decode_task_command(const StableId&,const BoundaryValue&);
BoundaryValue encode_task_command(const TaskCommand&);
BoundaryValue encode_task_quote(const TaskQuote&);
BoundaryValue encode_task_effect(const TaskEffect&);
Outcome<TaskEffect> decode_task_effect(const BoundaryValue&);
Status visit_task_checkpoint(CodecVisitor&,TaskState&);
Status visit_task_effect(CodecVisitor&,TaskEffect&);
const TaskRecord* task_for(const TaskState&,const StableId& actor,const StableId& offer);
bool task_can_retry(const CommonStore&,const TaskState&,const TaskRecord&);
StableId task_display_name(const TaskState&,const TaskCatalog&,const StableId& task);
std::vector<std::pair<StableId,StableId>> task_procurement_payers(const TaskState&,const TaskCatalog&,const StableId& actor);
bool task_procurement_authorized(const TaskState&,const TaskCatalog&,const StableId& actor,const StableId& task,const StableId& item);
StableId task_condition_status(const CommonStore&,const TaskState&,const InteractionState&,const StableId& actor,const StableId& offer);
Outcome<TaskQuote> preview_task(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const TaskCommand&,const AffairsState* affairs=nullptr,const KnownSubjectView* known=nullptr,const TaskExecutionServices* execution=nullptr,const TaskMessageNoticeServices* notices=nullptr,const TaskMessageReturnServices* returns=nullptr);
// 中央先建立行动头及参与者占用；本入口才分配task、建立预算钱包并预留实际钱物。
Status start_task_action(CommonStore&,TaskState&,const TaskCatalog&,const ActionCoreView&,const TaskCommand&,const TaskQuote&,AffairsState* affairs=nullptr,const TaskExecutionServices* execution=nullptr,const TaskMessageReturnServices* returns=nullptr,const TaskPersonalAcceptanceServices* personal_acceptances=nullptr,TaskExactWrite* exact=nullptr);
Outcome<TaskEffect> finish_task_action(CommonStore&,TaskState&,SocialState&,const TaskCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ActionCoreView&,Integer finish_tick,AffairsState* affairs=nullptr,const TaskExecutionServices* execution=nullptr,const TaskMessageNoticeServices* notices=nullptr,const TaskMessageReturnServices* returns=nullptr,const TaskPersonalMessageServices* personal=nullptr,const TaskPersonalReportServices* personal_reports=nullptr,const TaskPersonalAcceptanceServices* personal_acceptances=nullptr,TaskExactWrite* exact=nullptr);
Status cancel_task(CommonStore&,TaskState&,const TaskCatalog&,const CommonCatalog&,const StableId& task_id,AffairsState* affairs=nullptr,const TaskExecutionServices* execution=nullptr);
Status observe_escort_tasks(CommonStore&,TaskState&,const TaskCatalog&);
Status record_task_battle(CommonStore&,TaskState&,const TaskCatalog&,const TaskProtectedBattle&);
Status validate_tasks(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskBattleRead& battles={},const PoliticalTakeoverRead* authority=nullptr);
} // 世界候选是唯一原子边界；失败不可单独发布本域或资产的部分改动。
