// FSL§2固定同版完整槽；Growth八种原F行动共用原owner边界。
#pragma once
#include "life_actual_command_binding.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "growth_result_refs.hpp"
#include "growth_skill_qualification_v2.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/family/death_ref.hpp"
namespace hanmo::resident {
// 原owner嵌入字段直接以稳定键和selector定位，不分配虚构terms/费用事实。
struct LifeOwnedFieldRef {SourceRef owner_ref;StableId field_selector;};
// 真实锁内作者字段；首片控制源精确指scenario.player_actor并反验FamilyCatalog初始founder。
struct LifeCatalogFieldRef {CatalogIdentity catalog_identity;StableId scenario_id,entity_id,field_selector;};
using LifeOriginalRef=std::variant<SourceRef,LifeOwnedFieldRef,LifeCatalogFieldRef,ScenarioOriginRef>;
// 尚未接入的原字段仍占固定槽；非空必拒，不能存任意BoundaryValue绕typed协议。
struct UnimplementedLifeFixedValue {};
using UnimplementedLifeFixedRows=std::vector<UnimplementedLifeFixedValue>;
// G+64 keeps this original slot and writes its explicit finite source tag.
// Older G+32 is still the untagged Catalog field and cannot encode a dynamic controller.
struct LifePlayerActorRoute {
 LifeOriginalRef original_control_source_ref=LifeCatalogFieldRef{};
 LifePlayerActorRoute()=default;
 LifePlayerActorRoute(LifeCatalogFieldRef value):original_control_source_ref(std::move(value)){}
};
struct LifeNpcActorRoute {StableId subject_actor_id;Integer registration_epoch=0,decision_seq=0;std::variant<ActorFoundationRuleRef,LifeCatalogFieldRef> policy_ref;};
using LifeActorRoute=std::variant<LifePlayerActorRoute,LifeNpcActorRoute>;
struct LifeActualCommand {StableId actor_id;LifeActualCommandBinding command_binding;LifeActorRoute route_basis;LifeOriginalRef accepted_business_ref;};
struct LifeAcceptedExecution {SourceRef accepted_source_ref;Integer source_schema=0;ActorFoundationRuleRef execution_rule_ref;StableId actual_initiator_actor_id;};
// FSL§2封闭十种真实输入变化，外source/schema仍须各原owner直接核验。
struct LifeConditionInputChange {ConditionReceiptRefV2 condition_receipt_ref;};
struct LifeDeathInputChange {DeathRef death_ref;};
struct LifeGrowthInputChange {LifeGrowthContributionRef original_result_ref;};
struct LifeActualPositionInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeOccupationInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeFacilityInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeAcceptedTermsInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeResourceInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeKnowledgeInputChange {SourceRef original_source_ref;Integer source_schema=0;};
struct LifeScheduledInputChange {LifeOriginalRef schedule_ref;Integer period_ordinal=0;enum class Boundary{start,end};Boundary boundary=Boundary::start;};
using LifeFixedInputChange=std::variant<LifeConditionInputChange,LifeDeathInputChange,LifeGrowthInputChange,LifeActualPositionInputChange,LifeOccupationInputChange,LifeFacilityInputChange,LifeAcceptedTermsInputChange,LifeResourceInputChange,LifeKnowledgeInputChange,LifeScheduledInputChange>;
struct LifeActualInputChange {LifeFixedInputChange change;ActorFoundationRuleRef transition_rule_ref;};
// 所有原tag保留；本首片仅tag3的原LifeActivity稳定键有真实reader。
struct LifeFixedOutcomeRef {enum class Kind{training,study,rest,life_activity,group_lesson,enrollment_boundary,community_relief,case_observation,treatment_period,argument_grant,arcane_service};Kind kind=Kind::life_activity;StableId id;Integer period_ordinal=0;StableId secondary_ref;};
struct LifeOriginalResultTrigger {LifeFixedOutcomeRef outcome;};
struct LifeFixedCompletionGate {StableId action_ref;LifeCatalogFieldRef original_required_rule_ref;std::optional<SourceRef> last_actual_period_ref;};
struct LifeActualStopTrigger {ActorFoundationRuleRef original_stop_rule_ref;std::variant<LifeActualCommand,LifeFixedInputChange> source;};
struct LifeOriginalTerminalApplication {enum class Disposition{completed,stopped};Disposition disposition=Disposition::completed;std::variant<LifeOriginalResultTrigger,LifeFixedCompletionGate,LifeActualStopTrigger> trigger;std::vector<LifeFixedOutcomeRef> actual_result_refs;std::vector<LifeOriginalRef> remaining_responsibility_refs;FieldOrder field_order;};
using LifeFixedBoundaryCause=std::variant<LifeActualCommand,LifeAcceptedExecution,LifeActualInputChange,LifeOriginalTerminalApplication>;
// 初态位置由真实ScenarioOriginRef证明；后续位置kind待各真实owner接通，不能拿今日位置倒证。
struct LifeFixedActorInputs {StableId actor_id,role;std::vector<FrozenFacilityCapabilityInputs> capability_frames;StableId origin_actor_key;Integer origin_schema=2;std::vector<GrowthSkillQualificationV2> skill_qualification_views;UnimplementedLifeFixedRows effective_attribute_inputs;std::optional<UnimplementedLifeFixedValue> age_view;Location location_snapshot;std::vector<LifeOriginalRef> actual_location_source_refs,participation_source_refs,occupation_source_refs;FieldOrder field_order;};
struct LifeAcceptedPermissionRef {StableId permission_kind;LifeOriginalRef original_ref;Integer source_schema=0;};
struct LifeFacilityUseRef {SourceRef original_facility_use_ref;Integer source_schema=0;std::vector<StableId> actual_claim_ids;};
struct LifeResourceBasisRef {enum class Purpose{fee,wage_reservation,food,material,custody,effect_attachment};Purpose purpose=Purpose::fee;LifeOriginalRef original_source_ref;Integer source_schema=0;};
struct LifeQuietServiceInputs {StableId place_ref;ActorFoundationRuleRef quiet_rule_ref;std::vector<LifeOriginalRef> site_access_source_refs,actual_conflict_source_refs;WorldMoment observed_at;bool is_quiet=false;FieldOrder field_order;};
// Service完整specific；恢复/quiet/trade只在原规则真实读取才有值，本首片三者都null。
struct LifeActivityFixedInputs {StableId activity_id;Integer terms_kind=0;std::optional<LifeCatalogFieldRef> service_role_ref,activity_ref;std::optional<SourceRef> recovery_service_ref;std::optional<LifeQuietServiceInputs> original_quiet_or_rest_basis;std::optional<SourceRef> trade_settlement_ref;FieldOrder field_order;};
struct GrowthFactFixedInputs {StableId terms_action_ref;std::optional<LifeOriginalRef> teaching_or_study_acceptance_ref;LifeCatalogFieldRef original_training_or_study_rule_ref;std::optional<LifeQuietServiceInputs> original_quiet_or_rest_basis;std::vector<SourceRef> actual_learning_or_rest_period_refs;FieldOrder field_order;};
struct LifeFixedSpecificInputs {Integer kind=1;LifeActivityFixedInputs life_activity;GrowthFactFixedInputs growth_fact;};
struct LifeFixedServiceInputs {
 // 普通深字段复制预留随原record存活；精确数值移动仍沿原可抛语义。
 CheckpointCopyLease copy_memory;
 LifeFixedServiceInputs()=default;
 LifeFixedServiceInputs(const LifeFixedServiceInputs&);
 LifeFixedServiceInputs(LifeFixedServiceInputs&&);
 LifeFixedServiceInputs& operator=(const LifeFixedServiceInputs&);
 LifeFixedServiceInputs& operator=(LifeFixedServiceInputs&&);
 void swap(LifeFixedServiceInputs&);
 friend void swap(LifeFixedServiceInputs& a,LifeFixedServiceInputs& b){a.swap(b);}
 std::vector<LifeFixedActorInputs> actors;std::vector<LifeCatalogFieldRef> locked_rule_refs;std::vector<LifeAcceptedPermissionRef> accepted_permission_refs;std::vector<LifeFacilityUseRef> facility_use_refs;std::vector<LifeResourceBasisRef> resource_basis_refs;LifeFixedSpecificInputs specific;FieldOrder field_order;};
enum class GrowthFixedFactKind {training,study,rest};
struct LifeFixedOwnerRef {Integer kind=1;StableId life_activity_id;Integer terms_kind=0;StableId action_id;GrowthFixedFactKind fact_kind=GrowthFixedFactKind::training;};
enum class LifeFixedBoundaryKind {started,suspended,resumed,input_rebound,completed,stopped};
// 唯一原owner边界只有业务来源；没有credited/due/current/第二占用或第二时间累计。
struct LifeFixedServiceBoundary {
 // 普通深字段复制预留随原record存活；精确数值移动仍沿原可抛语义。
 CheckpointCopyLease copy_memory;
 LifeFixedServiceBoundary()=default;
 LifeFixedServiceBoundary(const LifeFixedServiceBoundary&);
 LifeFixedServiceBoundary(LifeFixedServiceBoundary&&);
 LifeFixedServiceBoundary& operator=(const LifeFixedServiceBoundary&);
 LifeFixedServiceBoundary& operator=(LifeFixedServiceBoundary&&);
 void swap(LifeFixedServiceBoundary&);
 friend void swap(LifeFixedServiceBoundary& a,LifeFixedServiceBoundary& b){a.swap(b);}
 StableId boundary_id;Integer source_schema=1;StableId action_ref,action_kind;LifeFixedOwnerRef owner_ref;Integer boundary_ordinal=0;std::optional<StableId> previous_boundary_ref;LifeFixedBoundaryKind kind=LifeFixedBoundaryKind::started;WorldMoment occurred_at;LifeFixedBoundaryCause cause;LifeFixedServiceInputs input_snapshot;std::optional<LifeFixedOutcomeRef> original_outcome_ref;FieldOrder field_order;};
} // 字段authority固定fcafe62e的life-fixed-service-source-contract§2及0524 owner登记，不生成额外协议。
