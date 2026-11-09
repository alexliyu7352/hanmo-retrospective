#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
// authored首片的来源和值类型；当前身体只在Common，其他kind未接入时明确拒绝。
#pragma once
#include "condition_refs.hpp"
#include "hanmo/resident/world_moment.hpp"
#include "resident/domains/family/birth_fact_ref.hpp"
#include "resident/domains/family/character_creation.hpp"
#include "resident/domains/family/death_ref.hpp"
#include "resident/domains/family/family_life_exact_types.hpp"
#include "resident/domains/family/population_origin_ref.hpp"
#include "resident/domains/growth/growth_capability_application.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/model/common_model.hpp"
#include <array>
namespace hanmo::resident {
// 锁内具名定义的完整身份；版本和摘要均参与比较，不以显示名字冒规则。

// 原ScenarioOriginRef是冷初态来源，不伪装成已发生Common事实。

struct ActorStartAgeBasisRef {
  StableId actor_id;
};
// Origin原五支年龄引用联合保tag；此首片只实现ActorStart，不吞其他来源。
struct CreationAgeBasisRef {
  StableId registration_result_id;
};
struct PopulationRegistrationAgeBasisRef {
  StableId registration_result_id;
};
struct LegacyAgeBasisRef {
  StableId legacy_actor_id;
};
struct CharacterAgeBasisRef {
  enum class Kind {
    actor_start,
    birth,
    creation,
    population_registration,
    verified_legacy
  };
  Kind kind = Kind::actor_start;
  ActorStartAgeBasisRef actor_start;
  BirthFactRef birth;
  CreationAgeBasisRef creation;
  PopulationRegistrationAgeBasisRef population;
  LegacyAgeBasisRef legacy;
};

// 原At保旧精度tag；本首片新值只准ExactMoment，不能从整数补历史offset。

// 尚未实现的业务表必须真正为空；codec遇非空即Unsupported，绝不吞字段假成功。
struct UnimplementedActorFoundationRecord {};
using UnimplementedActorFoundationRows =
    std::vector<UnimplementedActorFoundationRecord>;
// 未实现的原顶层表仍保持OrderedTable容器合同，不能以空数组占同版JSON。
using UnimplementedActorFoundationTable =
    OrderedTable<UnimplementedActorFoundationRecord>;
// 声明整岁保完整来源；ExactBirth字段保留但首片没有真实出生reader，显式拒绝。
struct AuthoredActorAgeBasis {
  enum class Kind { exact_birth, declared_age_at_start };
  Kind kind = Kind::declared_age_at_start;
  Integer birth_tick = 0;
  StableId birth_source_ref;
  Integer years = 0, as_of_tick = 0, calendar_year_ordinal = 0;
  ScenarioOriginRef source;
};
// actor_start_context/3 only: the seed points to the original creation
// producer. Registration/Allocation readers inspect their own independent
// inputs; they never recurse through initial_condition_seed_ref to establish
// the same seed.
struct CreatedConditionSeedSource {
  StableId creation_result_ref;
};
struct BirthConditionSeedSource {
  BirthFactRef birth_record_ref;
  StableId allocation_source_ref;
};
struct NamedConditionSeedSource {
  PopulationOriginRef population_origin_ref;
  StableId registration_result_ref, allocation_source_ref;
};
using InitialConditionSeedSource =
    std::variant<ScenarioOriginRef, CreatedConditionSeedSource,
                 BirthConditionSeedSource, NamedConditionSeedSource>;
struct InitialConditionSeed {
  StableId seed_key, actor_id;
  InitialConditionSeedSource source;
  Integer hp = 0, fatigue = 0, injury = 0;
  UnimplementedActorFoundationRows episodes;
  ActorFoundationRuleRef activity_limit_profile;
};
struct ScenarioOriginRecord {
  ScenarioOriginRef ref;
  ActorFoundationRuleRef construction_profile_ref;
};
struct ActorAgeBasisRecord {
  StableId actor_id;
  ScenarioOriginRef scenario_origin_ref;
  AuthoredActorAgeBasis age_basis;
};
// 此片只接AtPlace；其他原StartPlacement tag保留编号但没有可伪装的空payload。
struct AuthoredStartPlacement {
  enum class Kind {
    at_place,
    on_personal_journey,
    with_party,
    with_force,
    on_vessel,
    in_custody
  };
  Kind kind = Kind::at_place;
  StableId place_ref;
};
struct ActorDomainBinding {
  StableId actor_id;
  ScenarioOriginRef scenario_origin_ref;
  ActorFoundationRuleRef identity_definition_ref;
  ActorStartAgeBasisRef age_basis_ref;
  std::vector<InitialConditionSeedRef> condition_seed_refs;
  AuthoredStartPlacement placement_ref;
  std::optional<StableId> activity_ref, custody_ref;
  std::vector<StableId> service_refs, office_refs, property_business_refs,
      initial_knowledge_refs;
  ActorFoundationRuleRef appearance_source_ref;
};
// 初始化结果只引用本次冷源；未实施其他领域构造，不能加入任意字符串证明。
struct ActorConstructionReceipt {
  ScenarioOriginRef scenario_origin_ref;
  ActorFoundationRuleRef construction_profile_ref;
  std::vector<StableId> constructed_actor_ids;
  std::vector<InitialConditionSeedRef> initial_source_refs;
  StableId initialization_result_ref;
};
struct ActorStartContextStateV2 {
  // scenario_origins的key就是记录内完整ScenarioOriginRef，不能换成actor裸字符串。
  std::vector<ScenarioOriginRecord> scenario_origins;
  OrderedTable<ActorAgeBasisRecord> actor_age_bases;
  OrderedTable<InitialConditionSeed> initial_condition_seeds;
  OrderedTable<ActorDomainBinding> actor_domain_bindings;
  OrderedTable<ActorConstructionReceipt> construction_receipts;
};
struct AuthoredCharacterSource {
  ScenarioOriginRef actor_start_context_ref;
  StableId authored_actor_id;
  ActorFoundationRuleRef prototype_ref;
};
struct CreatedCharacterSource {
  StableId creation_result_ref;
};
struct BornCharacterSource {
  BirthFactRef birth_record_ref;
  std::vector<StableId> biological_parent_refs;
  std::optional<StableId> innate_result_ref;
};
struct PopulationCharacterSource {
  PopulationOriginRef population_origin_ref;
  StableId registration_result_ref, allocation_source_ref;
};
struct VerifiedLegacyCharacterSource {
  StableId legacy_kind;
  CatalogIdentity legacy_catalog_identity;
  std::vector<StableId> legacy_source_fact_refs;
  ActorFoundationRuleRef migration_rule_ref;
};
using CharacterSourceV2 =
    std::variant<AuthoredCharacterSource, CreatedCharacterSource,
                 BornCharacterSource, PopulationCharacterSource,
                 VerifiedLegacyCharacterSource>;
struct AuthoredInitialCapabilityBasis {
  CatalogIdentity catalog_identity;
  ActorFoundationRuleRef prototype_ref;
};
struct CreatedInitialCapabilityBasis {
  StableId creation_result_ref;
};
struct BirthInitialCapabilityBasis {
  BirthFactRef birth_record_ref;
  StableId allocation_source_ref;
};
struct PopulationInitialCapabilityBasis {
  PopulationOriginRef population_origin_ref;
  StableId allocation_source_ref;
};
struct LegacyInitialCapabilityBasis {
  StableId legacy_origin_kind;
  std::vector<StableId> legacy_source_fact_refs;
  CatalogIdentity legacy_catalog_identity;
};
using InitialCapabilityBasisV2 =
    std::variant<AuthoredInitialCapabilityBasis, CreatedInitialCapabilityBasis,
                 BirthInitialCapabilityBasis, PopulationInitialCapabilityBasis,
                 LegacyInitialCapabilityBasis>;
struct CharacterOriginRecordV2 {
  enum class Kind {
    authored,
    created,
    born,
    named_from_population,
    verified_legacy
  };
  StableId actor_id;
  Kind origin_kind = Kind::authored;
  std::optional<StableId> origin_fact_ref;
  ActorFoundationAt created_at;
  CharacterAgeBasisRef age_basis_ref;
  std::optional<ScenarioOriginRef> actor_start_context_ref;
  CharacterSourceV2 source;
  InitialCapabilityBasisV2 initial_capability_basis;
  InitialConditionSeedRef initial_condition_seed_ref;
  ActorFoundationRuleRef provenance_policy_ref;
};
struct CreationRegistrationRef {
  StableId registration_result_id;
};
using RegistrationAgeSourceRef =
    std::variant<CreationRegistrationRef, PopulationOriginRef>;
struct DeclaredAgeAtRegistration {
  Integer years = 0, as_of_tick = 0;
  ActorFoundationRuleRef calendar_profile_ref;
  Integer calendar_year_ordinal = 0;
  RegistrationAgeSourceRef source_ref;
};
struct RegistrationExactBirth {
  Integer birth_tick = 0;
  BirthFactRef source_ref;
};
using RegistrationAgeBasis =
    std::variant<DeclaredAgeAtRegistration, RegistrationExactBirth>;
struct CreatedRegistrationSource {
  CharacterCreationDraft draft;
  ActorFoundationRuleRef creation_rule_ref;
};
struct NamedRegistrationSource {
  PopulationOriginRef population_origin_ref;
  StableId allocation_source_ref;
  ActorFoundationRuleRef registration_rule_ref;
};
struct RegistrationSourceV2 {
  StableId registration_result_id, actor_id;
  enum class Kind { created, named_population };
  Kind registration_kind = Kind::created;
  ActorFoundationAt registered_at;
  RegistrationAgeBasis age_basis;
  std::variant<CreatedRegistrationSource, NamedRegistrationSource> source;
  InitialConditionSeedRef initial_condition_seed_ref;
};
struct AllocationPrototypeInput {
  ActorFoundationRuleRef prototype_ref;
};
struct AllocationInnateParentInput {
  StableId parent_actor_id, family_id;
  std::optional<BirthFactRef> parent_birth_ref;
};
struct AllocationPopulationDefinitionInput {
  ActorFoundationRuleRef naming_definition_ref;
};
using CapabilityAllocationInputRef =
    std::variant<AllocationPrototypeInput, AllocationInnateParentInput,
                 AllocationPopulationDefinitionInput>;
struct BirthInnateSelectionRef {
  BirthFactRef birth_ref;
  StableId family_id;
};
struct PopulationSelectionResultRef {
  PopulationOriginRef origin_ref;
  StableId selection_result_id;
  Integer source_ordinal = 0;
};
using CapabilityAllocationSelectionRef =
    std::variant<BirthInnateSelectionRef, PopulationSelectionResultRef>;
struct InitialCapabilityAllocation {
  StableId allocation_result_id, actor_id;
  enum class Kind { birth, named_population };
  Kind allocation_kind = Kind::birth;
  std::variant<BirthFactRef, PopulationOriginRef> original_source_ref;
  ActorFoundationRuleRef allocation_rule_ref;
  ActorAttributes base_attributes;
  ActorSkills initial_skill_xp;
  std::vector<CapabilityAllocationInputRef> selected_input_refs;
  std::vector<CapabilityAllocationSelectionRef> actual_selection_result_refs;
  ActorFoundationAt applied_at;
};
struct CharacterOriginRecordsStateV1 {
  OrderedTable<CharacterOriginRecordV2> origins;
  OrderedTable<RegistrationSourceV2> registration_sources;
  OrderedTable<InitialCapabilityAllocation> allocation_sources;
};
// 原两实验profile仅authored
// bytes；完整新profile由锁定Catalog唯一选择，外部请求不可传此选择。

// Bounded prefix of the original applied_receipts order, not Common order
// and not an epoch proving all later payload fields have been populated.


// 原RestProvided完整实际服务来源；原有LifeActivity就是履约来源，没有另发假事实。
struct OriginalRestProvidedCause {
  StableId action_id, rest_kind, provider_actor_id, site_id;
  Integer provided_half_days = 0;
  StableId service_fulfillment_ref;
};
struct RestConditionBusinessKey {
  StableId action_id, actor_id;
  Integer completion_ordinal = 1;
};
// 真实群体运动不伪Action；唯一单位与其原关闭source共同决定一次用力。
struct TravelConditionBusinessKey {
  StableId unit_ref, actor_id, exertion_source_id;
};
struct TravelExertionConditionCause {
  StableId exertion_source_id, actor_id;
};
// wire仍是原MilitaryCasualty/LifeAgeBoundary/EnvironmentalSea闭合cause；C++共享终止载荷。
struct MilitaryCasualtyConditionCause {
  enum class Disposition { wounded, dead };
  Disposition disposition = Disposition::wounded;
  StableId casualty_fact_id, battle_id, actor_id;
  WorldMoment occurrence_at;
  Integer source_schema = 2, source_ordinal = 0;
};
struct LifeTerminationConditionCause {
  LifeCauseRef4 source;
  Integer source_schema = 0, source_ordinal = 0;
};
struct LifeTerminationConditionBusinessKey {
  Integer cause_tag = 4;
  StableId source_id, actor_id;
  Integer source_ordinal = 0;
};
// 原S4实际完整学习/训练结果；真实提供窗由同Growth FSL/S0历史反读。
struct WorkOrStudyConditionCause {
  enum class Kind {
    study_lesson,
    training_completion,
    service_completion,
    craft_gate
  };
  Kind kind = Kind::study_lesson;
  StableId action_id, actor_id;
  Integer ordinal = 0;
};
struct WorkOrStudyConditionBusinessKey {
  StableId action_id, actor_id;
  Integer ordinal = 0;
};
// P4.3 ProvisionShortfall, explicit Condition3 tag6. Only the original
// military Supply2 branch is implemented; its numbers and M remain S2-owned.
struct ProvisionShortfallConditionCause {
  SourceRef original_supply_ref;
  Integer source_schema = 2, source_ordinal = 0;
  StableId actor_id;
};
struct ProvisionShortfallConditionBusinessKey {
  enum class Purpose { military_supply_shortage };
  SourceRef original_supply_ref;
  Integer source_schema = 2, source_ordinal = 0;
  StableId actor_id;
  Purpose purpose = Purpose::military_supply_shortage;
};
// The independent row resolves the real battle/action business key. Full cold
// coverage also rejects two application rows that cite the same original key.
struct CharacterInjuryConditionBusinessKeyV1 {
  CharacterInjurySourceRefV1 source_ref;
  StableId actor_id;
};
// Original Gather's completed FWP/2 receipt, never a Growth activity alias.
struct GatherFatigueConditionCauseV1 { StableId receipt_id; };
struct GatherFatigueConditionBusinessKeyV1 { StableId receipt_id, actor_id; };
using CharacterConditionCause =
    std::variant<TravelExertionConditionCause, OriginalRestProvidedCause,
                 LifeTerminationConditionCause, WorkOrStudyConditionCause,
                 MilitaryCasualtyConditionCause,
                 ProvisionShortfallConditionCause, CharacterInjurySourceRefV1,
                 GatherFatigueConditionCauseV1>;
using CharacterConditionBusinessKey =
    std::variant<TravelConditionBusinessKey, RestConditionBusinessKey,
                 LifeTerminationConditionBusinessKey,
                 WorkOrStudyConditionBusinessKey,
                 ProvisionShortfallConditionBusinessKey,
                 CharacterInjuryConditionBusinessKeyV1, GatherFatigueConditionBusinessKeyV1>;
struct ConditionScalarChange {
  enum class Field { hp, fatigue, injury, alive };
  Field field = Field::hp;
  Integer before = 0, after = 0;
};
// INJ-C2 / V4.2: immutable effect values belong only to the selected original
// result. Current contributions below retain their identities, never a second
// attribute balance. The first adapter is explicitly deterministic.
struct InjuryAttributeReduction {
  StableId attribute_id;
  ActorFoundationRuleRef reducer_ref;
  StableId unit_id;
  Integer amount = 0;
};
struct InjuryActivityRestriction {
  ActorFoundationRuleRef registered_purpose, restriction_ref;
};
struct RecoverableInjuryEffect {
  ActorFoundationRuleRef original_recovery_gate_ref;
};
struct PermanentInjuryEffect {};
struct InjuryEffectItem {
  StableId effect_key;
  std::variant<InjuryAttributeReduction, InjuryActivityRestriction>
      effect_payload;
  std::variant<RecoverableInjuryEffect, PermanentInjuryEffect> retention =
      PermanentInjuryEffect{};
  ActorFoundationRuleRef original_effect_rule_ref;
};
struct SelectedInjuryResult {
  enum class Kind { not_an_injury, no_additional_effect, applied };
  Kind kind = Kind::not_an_injury;
  ActorFoundationRuleRef injury_rule_ref, result_choice_ref;
  // No actual random/decision producer has yet been admitted. Deterministic
  // selections leave this null; a non-null source must not pass on shape alone.
  std::optional<MomentOwnerSource> actual_decision_source_ref;
  std::vector<InjuryEffectItem> effect_items;
};
struct ConditionReceiptV2 {
  StableId result_id, actor_id;
  CharacterConditionBusinessKey business_key = RestConditionBusinessKey{};
  CharacterConditionCause cause = OriginalRestProvidedCause{};
  ActorFoundationRuleRef rule_ref;
  ActorFoundationAt effective_at;
  Integer condition_revision_before = 0, condition_revision_after = 0;
  std::vector<ConditionScalarChange> scalar_changes;
  SelectedInjuryResult selected_injury_result;
  std::vector<StableId> created_effect_keys, recovery_resolution_refs;
  std::optional<StableId> life_transition_ref;
  std::vector<StableId> capacity_transition_refs;
  // Explicit character_condition_extensions/3 only.
  std::optional<GrowthCapabilityApplication> capability_application;
};
// Existing CapacityTransitionV2 slot. Physical 1/2/3 retain the untagged
// military cause; explicit /4 adds actual original Condition/purpose outputs.
struct MilitaryAvailabilityCauseV2 {
  SourceRef casualty_source;
  Integer source_schema = 2, source_ordinal = 0;
};
struct ConditionPurposeCapacityCause {
  ConditionReceiptRefV2 condition_receipt_ref;
};
// 显式物理6才准入：term反读真实Field应用；军域不固化availability副本。
struct NamedMilitaryCaptureAvailabilityCause {
  SourceRef term_source;
  Integer source_schema = 1;
  std::optional<StableId> predecessor_transition_id;
};
struct NamedMilitaryReleaseAvailabilityCause {
  SourceRef release_source;
  Integer source_schema = 1;
  StableId capture_transition_id;
};
using CapacityTransitionCauseV2 =
    std::variant<MilitaryAvailabilityCauseV2, ConditionPurposeCapacityCause,
                 NamedMilitaryCaptureAvailabilityCause,
                 NamedMilitaryReleaseAvailabilityCause>;
struct CapacityTransitionV2 {
  StableId transition_id, actor_id;
  ActorFoundationRuleRef purpose;
  bool from = false, to = false;
  ActorFoundationAt effective_at;
  std::optional<ActorFoundationAt> expected_recovery_at;
  CapacityTransitionCauseV2 cause_ref = MilitaryAvailabilityCauseV2{};
};
struct RecoveryResolutionRefV2 {
  StableId result_id;
};
using ConditionEffectOriginRef =
    std::variant<ConditionReceiptRefV2, InitialConditionSeedRef>;
enum class ConditionEffectState { active, reduced, released };
struct ConditionEffectIdentity {
  StableId actor_id, episode_id;
  ConditionEffectOriginRef origin_result;
  StableId effect_key;
};
struct ConditionEpisodeV2 {
  StableId episode_id, actor_id;
  ActorFoundationRuleRef condition_rule_ref;
  CharacterConditionSourceRef origin;
  ActorFoundationAt started_at;
  ActorFoundationRuleRef stage_ref;
  Integer stage_revision = 0;
  std::optional<Integer> next_calendar_due;
  std::optional<std::variant<RecoveryResolutionRefV2, DeathRef>>
      terminal_source_ref;
};
struct ConditionEffectContribution {
  ConditionEffectIdentity identity;
  ConditionEffectState current_state = ConditionEffectState::active;
  std::optional<RecoveryResolutionRefV2> last_resolution_ref;
};
// Only an actual RestProvided receipt is addressable in this first mapping.
// TreatmentCredit and condition_due require their original typed producers.
struct RestProvidedRecoverySource {
  ConditionReceiptRefV2 condition_receipt_ref;
};
struct RecoveryEffectTarget {
  ConditionEffectIdentity effect_identity;
  ActorFoundationRuleRef gate_ref;
  ConditionEffectState state_before = ConditionEffectState::active,
                       state_after = ConditionEffectState::active;
  std::optional<Integer> remaining_effect_amount;
};
struct RecoveryResolutionV2 {
  StableId result_id, actor_id;
  std::variant<RestProvidedRecoverySource> actual_recovery_source;
  ActorFoundationRuleRef rule_ref;
  ActorFoundationAt effective_at;
  std::vector<RecoveryEffectTarget> target_effects;
  Integer condition_revision_before = 0, condition_revision_after = 0;
};
struct CharacterConditionExtensionsStateV1 {
  OrderedTable<ActorConditionHead> actor_condition_heads;
  OrderedTable<ConditionEpisodeV2> episodes;
  OrderedTable<ConditionReceiptV2> applied_receipts;
  OrderedTable<ConditionEffectContribution> effect_contributions;
  OrderedTable<RecoveryResolutionV2> recovery_resolutions;
  OrderedTable<CapacityTransitionV2> capacity_transitions;
  UnimplementedActorFoundationTable combat_return_bindings, instant_limits;
};
struct AuthoredActorFoundationState {
  ActorStartContextStateV2 actor_start_context;
  CharacterOriginRecordsStateV1 character_origin_records;
  CharacterConditionExtensionsStateV1 character_condition_extensions;
};
// 锁定作者模板是不可变初值来源，绝不反向从当前人物拼出。
struct AuthoredActorFoundationDefinition {
  StableId actor_id;
  ScenarioOriginRef scenario_origin_ref;
  ActorFoundationRuleRef identity_definition_ref;
  bool alive = true, available = true;
  AuthoredActorAgeBasis age_basis;
  InitialConditionSeed condition;
  AuthoredStartPlacement placement;
  ActorFoundationRuleRef appearance_source_ref;
  ActorAttributes attributes;
  ActorSkills skills;
};
// 锁内原规则参数与有限purpose绑定；仅content定义，不产生世界能力账。
struct CharacterNormalWorkEfficiencyRule {
  ActorFoundationRuleRef rule_ref;
  std::array<Integer, 3> fatigue_cutoffs{};
  std::array<Integer, 4> fatigue_caps{}, injury_caps{};
};
struct CharacterPurposeBinding {
  enum class Reducer {
    normal_work,
    expression,
    personal_training,
    family_care,
    trade_participation
  };
  ActorFoundationRuleRef purpose_ref, rule_ref;
  Reducer reducer = Reducer::normal_work;
};
// 原 Military wounded
// 标量规则的显式、确定性无附加效果选择；不是所有伤源的默认选择。
struct OriginalMilitaryWoundRule {
  ActorFoundationRuleRef rule_ref, injury_rule_ref, result_choice_ref;
  StableId original_casualty_rule_ref;
  bool selected_applied = false;
};
struct InjuryCapabilityMapEntry {
  ActorFoundationRuleRef injury_rule_ref, result_choice_ref;
  StableId original_casualty_rule_ref;
  enum class CauseAdapter { military_wounded, field_segment_wounded, personal_defeat };
  CauseAdapter cause_adapter = CauseAdapter::military_wounded;
  enum class Selection { deterministic };
  Selection selection = Selection::deterministic;
  ActorFoundationRuleRef stage_ref;
  std::vector<InjuryEffectItem> effect_items;
};
struct InjuryCapabilityMap {
  Integer version = 1;
  std::vector<InjuryCapabilityMapEntry> entries;
};
// Explicit authored selection for real source1 battle adapters. No fallback
// from an absent row to NoAdditionalEffect; legacy dynamic2 stays separate.
struct OriginalBattleInjuryRuleV1 {
  InjuryCapabilityMapEntry::CauseAdapter cause_adapter =
      InjuryCapabilityMapEntry::CauseAdapter::field_segment_wounded;
  ActorFoundationRuleRef rule_ref, injury_rule_ref, result_choice_ref;
  bool selected_applied = false;
};
struct OriginalLongRestConditionRule {
  ActorFoundationRuleRef rule_ref;
  bool personal_injury_recovery = false;
};
struct ActorFoundationCatalog {
  CatalogIdentity identity;
  StableId scenario_id;
  ActorFoundationRuleRef construction_profile, provenance_policy, rest_rule,
      rest_purpose, provider_purpose;
  OrderedTable<AuthoredActorFoundationDefinition> actors;
  // 显式选中原S1单位规则后才准用力写入；旧rest-only目录仍不获得该能力。
  std::optional<ActorFoundationRuleRef> travel_rule, work_or_study_rule,
      study_capability_rule;
  std::optional<ActorFoundationRuleRef> military_death_rule,
      natural_age_death_rule, sea_death_rule;
  std::optional<OriginalMilitaryWoundRule> original_military_wound_rule;
  // Content-only mapping in this original catalog, never a World root.
  std::optional<InjuryCapabilityMap> injury_capability_map;
  std::vector<OriginalBattleInjuryRuleV1> original_battle_injury_rules;
  // Compiled-only original feed() +4 rule, selected together with Condition3.
  std::optional<ActorFoundationRuleRef> original_military_supply_shortage_rule;
  // Same selected original recovery module; never a request-controlled rule
  // switch.
  std::optional<OriginalLongRestConditionRule> original_long_rest_rule;
  std::optional<CharacterNormalWorkEfficiencyRule> normal_work_efficiency_rule;
  OrderedTable<CharacterPurposeBinding> capability_purpose_bindings;
  // 指向PreparedCatalog已准入的原Growth目录，不复制条款或产生第二经营账。
  std::shared_ptr<const struct GrowthCatalog> original_growth;
};
// 冻结身体仅是原业务一次使用的before源，非可变人物表。
struct FrozenBodyScalars {
  bool alive = false;
  Integer hp = 0, fatigue = 0, injury = 0;
  std::vector<StableId> field_order;
};
struct ActorCapabilityResult {
  bool allowed = false;
  Integer efficiency_bp = 0;
  std::vector<CharacterConditionSourceRef> limiting_condition_refs;
  ActorFoundationRuleRef rule_ref;
  WorldMoment observed_at;
};
// FH 原冻结 effect 引用深值；只保当次采用证据，不复制效果载荷或授予恢复权。
struct FrozenConditionEffectBinding {
  ConditionEffectOriginRef origin_result;
  StableId effect_key;
  ConditionEffectState current_state = ConditionEffectState::active;
  std::optional<RecoveryResolutionRefV2> last_resolution_ref;
  std::vector<StableId> field_order;
};
// FH 原冻结 episode 输入；生命周期、所需完整集合和规则归属仍由原读取者验证。
struct FrozenActiveConditionInput {
  StableId episode_id;
  ActorFoundationRuleRef condition_rule_ref, stage_ref;
  Integer stage_revision = 0;
  CharacterConditionSourceRef origin;
  std::vector<FrozenConditionEffectBinding> effect_bindings;
  std::vector<StableId> field_order;
};
struct FrozenFacilityCapabilityInputs {
  StableId actor_id;
  ActorFoundationRuleRef purpose_ref;
  WorldMoment captured_at;
  ActorConditionHead condition_head_snapshot;
  FrozenBodyScalars body_scalars;
  std::vector<FrozenActiveConditionInput> active_condition_inputs;
  std::vector<ActorFoundationRuleRef> selected_rule_refs;
  ActorCapabilityResult result;
  std::vector<StableId> field_order;
};
} // namespace hanmo::resident
