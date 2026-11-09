// Fresh original Action/Logistics/Return rows, charged before their first copy.
#pragma once
#include "military_action_time_records.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace military_action_completion_detail {
inline SourceRef source(const SourceRefBuildView& view){return {StableId(view.domain),StableId(view.kind),StableId(view.id)};}
inline std::optional<SourceRef> terminal(const std::optional<SourceRefBuildView>& view){if(!view)return {};return source(*view);}
}
class PreparedMilitaryActionCompletion2 {
 static CheckpointMemoryCharge reserve(const DynamicMilitaryActionFactBuildView2& ){return CheckpointMemoryCharge(0);}
 static std::optional<MilitaryActionResult> result(const std::optional<MilitaryActionResultBuildView>& view){if(!view)return {};const auto& v=*view;return MilitaryActionResult{v.ok,v.office_id,v.army_id,v.order_id,v.field_order};}
public:
 CheckpointMemoryCharge copied_fields;
 DynamicMilitaryActionFact2 row;
 explicit PreparedMilitaryActionCompletion2(const DynamicMilitaryActionFactBuildView2& v)
  :copied_fields(reserve(v)),row{v.id,v.command,v.actor_id,v.started_at,v.completed_at,StableId(v.phase),v.success,v.original_terms,v.original_context,v.elapsed_source_ref,result(v.result),military_action_completion_detail::terminal(v.terminal_source_ref),v.rule_version,v.actual_resource_effect,v.acceptance_conditions,v.acceptance_resource_reservations,v.acceptance_observation?*v.acceptance_observation:std::optional<MilitaryActionEndpointObservation2>{},v.completion_observation?*v.completion_observation:std::optional<MilitaryActionEndpointObservation2>{}}{}
};
class PreparedMilitaryLogisticsCompletion2 {
 static CheckpointMemoryCharge reserve(const DynamicMilitaryLogisticsFactBuildView2& ){return CheckpointMemoryCharge(0);}
 static std::optional<MilitaryLogisticsResult> result(const std::optional<MilitaryLogisticsResultBuildView>& view){
  if(!view)return {};const auto& v=*view;
  // The one fresh optional vector is already part of the enclosing raw plan.
  std::optional<std::vector<MilitaryActorChange>> changes;if(v.actor_changes)changes.emplace(*v.actor_changes);
  return MilitaryLogisticsResult{v.ok,v.army_id,v.order_id,v.receipt_id,std::move(changes),v.field_order};
 }
public:
 CheckpointMemoryCharge copied_fields;
 DynamicMilitaryLogisticsFact2 row;
 explicit PreparedMilitaryLogisticsCompletion2(const DynamicMilitaryLogisticsFactBuildView2& v)
  :copied_fields(reserve(v)),row{v.id,v.command,v.actor_id,v.started_at,v.completed_at,StableId(v.phase),v.success,v.original_terms,v.original_context,v.elapsed_source_ref,result(v.result),military_action_completion_detail::terminal(v.terminal_source_ref),v.rule_version,v.acceptance_conditions}{}
};
class PreparedMilitaryReturnFact2 {
 static CheckpointMemoryCharge reserve(const DynamicMilitaryReturnFactBuildView2& ){return CheckpointMemoryCharge(0);}
public:
 CheckpointMemoryCharge copied_fields;
 DynamicMilitaryReturnFact2 row;
 explicit PreparedMilitaryReturnFact2(const DynamicMilitaryReturnFactBuildView2& v)
  :copied_fields(reserve(v)),row{v.id,v.army_id,v.order_id,v.garrison_id,v.source_garrison_id,v.ownership_id,v.actor_id,v.authority,v.completed_at,v.goods,v.money,v.unit_ids,v.returned_retainers,v.lost_retainers,v.prisoner_ids,v.actor_changes,v.logistics_action_ref,military_action_completion_detail::source(v.completion_source_ref),v.actual_resource_effect,v.actual_detachment_releases}{}
};
static_assert(std::is_nothrow_move_constructible_v<MilitaryLogisticsResult>,"the planned original result must transfer without a second payload copy");
static_assert(std::is_nothrow_move_constructible_v<SourceRef>,"the planned original source must transfer without a second payload copy");

}
