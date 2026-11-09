// Internal actual-source consumer. Requests cannot supply damage or repair targets.
#pragma once
#include "item_service.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <string_view>
namespace hanmo::resident {
struct ItemUseRequest {ItemUseSource source;StableId instance_id;Integer expected_instance_version=0;const ReadStamp& source_stamp;};
struct ItemRepairCompletionRequest {SourceRef actual_repair_result;StableId instance_id;Integer expected_instance_version=0;const ReadStamp& source_stamp;};
// Original providers verify real equipment/custody, accepted risk/rule, completed
// work and their immutable result. Historical reads do not ask today's permission.
// They must not call back into S9 condition validation (the result is a consumer).
struct ActualItemUse {
 ItemUseSource source;StableId instance_id;Integer version_before=0;
 SourceRef source_fact_ref,source_activity_ref;WorldMoment occurred_at;
 Integer damage_units=0,authorized_damage_limit=0;std::vector<SourceRef> resource_effect_refs;
};
// Borrowed actual result inputs; the tracked buffer contains views only.
// It is not another result/authority source or persistent resource ledger.
struct ActualItemRepairResourceEffectsBuildView {const CheckpointVector<SourceRefBuildView>& values;};
struct ActualItemRepairCompletionBuildView {
 SourceRefBuildView source_fact_ref,source_activity_ref;
 std::string_view instance_id,repair_policy_id;
 Integer version_before=0,durability_after=0,max_durability_after=0;
 const WorldMoment& occurred_at;
 ActualItemRepairResourceEffectsBuildView resource_effect_refs;
};
struct ActualItemRepairCompletion {
private:
 CheckpointCopyLease memory_; // first; retained through fallible exact movement
public:
 SourceRef source_fact_ref,source_activity_ref;StableId instance_id,repair_policy_id;
 Integer version_before=0,durability_after=0,max_durability_after=0;WorldMoment occurred_at;
 std::vector<SourceRef> resource_effect_refs;
 ActualItemRepairCompletion()=default;
 explicit ActualItemRepairCompletion(const ActualItemRepairCompletionBuildView&);
 ActualItemRepairCompletion(const ActualItemRepairCompletion&)=delete;
 ActualItemRepairCompletion&operator=(const ActualItemRepairCompletion&)=delete;
 ActualItemRepairCompletion(ActualItemRepairCompletion&&);
 ActualItemRepairCompletion&operator=(ActualItemRepairCompletion&&)=delete;
};
struct ItemUseOccurrence {ItemUseSource source;StableId instance_id;};
struct ItemRepairOccurrence {SourceRef actual_repair_result;StableId instance_id;};
using ItemConditionOccurrence=std::variant<ItemUseOccurrence,ItemRepairOccurrence>;
class ItemConditionOriginalRead {
public:virtual ~ItemConditionOriginalRead()=default;
 virtual Outcome<ActualItemUse> read_battle_wear(const BattleWear&,const StableId& instance_id)const=0;
 virtual Outcome<ActualItemUse> read_gather_tool_wear(const GatherToolWear&,const StableId& instance_id)const=0;
 virtual Outcome<ActualItemUse> read_infusion_failure_wear(const InfusionFailureWear&,const StableId& instance_id)const=0;
 virtual Outcome<ActualItemUse> read_facility_attack_tool_wear(const FacilityAttackToolWear&,const StableId& instance_id)const=0;
 virtual Outcome<ActualItemRepairCompletion> read_repair_completed(const SourceRef&,const StableId& instance_id)const=0;
 // Enumerate the original applied tool/wear/repair rows, including zero damage.
 // This reverse coverage rejects deletion of an otherwise invisible NoChange.
 virtual Outcome<std::vector<ItemConditionOccurrence>> enumerate_item_condition_occurrences()const=0;
};
struct ItemConditionReadContext {
 const CommonStore& common;const ItemInstancesState& state;const ItemOutputSourceRead& outputs;
 const ItemConditionOriginalRead& original;const CurrentMomentRead& clock;
 Integer selected_version=1;
};
// selected_version 2 or 3 is checked by the composition root. Every failure requires
// discarding the outer candidate; IDs, original results and all other effects too.
Outcome<const ItemConditionResultView*> apply_item_use_damage_in_candidate(
 CommonStore&,ItemInstancesState&,const ItemConditionReadContext&,const ItemUseRequest&,
 ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<const ItemConditionResultView*> apply_item_repair_completed_in_candidate(
 CommonStore&,ItemInstancesState&,const ItemConditionReadContext&,const ItemRepairCompletionRequest&,
 ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<const ItemConditionResultView*> read_item_condition_result(const ItemConditionReadContext&,const StableId& result_id);
Outcome<WorldMoment> read_item_condition_result_moment(const ItemConditionReadContext&,const StableId& result_id);
SourceRef item_condition_result_source(const StableId& result_id);
Outcome<const ItemConditionResultView*> read_item_condition_source(const ItemConditionReadContext&,const SourceRef&,Integer source_schema);
// Direct original dependencies only, not a recursive collector. Item and
// termination IDs retain their typed ownership, not invented Common sources.
class ItemConditionSourcePinSink {
public:
 virtual ~ItemConditionSourcePinSink()=default;
 virtual Status source(std::string_view actual_slot,std::size_t ordinal,const SourceRef&,std::optional<Integer> actual_schema)=0;
 virtual Status item_instance(std::string_view actual_slot,const StableId& instance_id)=0;
 virtual Status item_termination(std::string_view actual_slot,const StableId& termination_id)=0;
 virtual Status condition_result(std::string_view actual_slot,std::size_t ordinal,const StableId& result_id)=0;
};
Status visit_item_condition_source_pins(const ItemConditionReadContext&,const SourceRef&,Integer source_schema,ItemConditionSourcePinSink&);
Status visit_item_instance_condition_pins(const ItemConditionReadContext&,const StableId& instance_id,ItemConditionSourcePinSink&);
Status visit_item_termination_source_pins(const ItemConditionReadContext&,const StableId& termination_id,ItemConditionSourcePinSink&);
Status validate_item_condition_history(const ItemConditionReadContext&);
Outcome<std::uint64_t> read_item_instance_domain_version(const ItemInstancesState&,const DomainReadKey&);
}
