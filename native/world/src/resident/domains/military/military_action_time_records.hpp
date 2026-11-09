// Fixed S2 §6.2 exact source rows. Original terms/results keep their own types.
#pragma once
#include "military_model.hpp"
#include "military_action_resource_model.hpp"
#include "military_return_detachment_model.hpp"
#include "military_formation_reservation_model.hpp"
#include "military_action_condition_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct DynamicMilitaryActionFact2 {
 StableId id,command,actor_id;WorldMoment started_at;std::optional<WorldMoment> completed_at;
 StableId phase="started";bool success=false;MilitaryTerms original_terms;
 std::optional<InteractionContext> original_context;ActionElapsedEntryRef elapsed_source_ref;
 std::optional<MilitaryActionResult> result;std::optional<SourceRef> terminal_source_ref;
 Integer rule_version=2;
 std::optional<MilitaryActionResourceEffect1> actual_resource_effect;
 std::optional<MilitaryActionAcceptanceConditions1> acceptance_conditions;
 std::optional<MilitaryFormationReservations1> acceptance_resource_reservations;
 std::optional<MilitaryActionEndpointObservation2> acceptance_observation,completion_observation;
};
// Fresh original result/action input views. They are not persisted sources and
// do not own or recursively charge their borrowed payloads. The actual producer
// reserves the sole raw plan before constructing a transient row; OrderedTable
// separately owns its final persistent copy.
struct MilitaryActionResultBuildView {
 const std::optional<bool>& ok;
 const StableId& office_id;const StableId& army_id;const StableId& order_id;
 const std::vector<StableId>& field_order;
};
struct DynamicMilitaryActionFactBuildView2 {
 const StableId& id;const StableId& command;const StableId& actor_id;
 const WorldMoment& started_at;const std::optional<WorldMoment>& completed_at;
 std::string_view phase;bool success;
 const MilitaryTerms& original_terms;const std::optional<InteractionContext>& original_context;
 const ActionElapsedEntryRef& elapsed_source_ref;
 const std::optional<MilitaryActionResultBuildView>& result;
 const std::optional<SourceRefBuildView>& terminal_source_ref;
 Integer rule_version;
 const std::optional<MilitaryActionResourceEffect1>& actual_resource_effect;
 const std::optional<MilitaryActionAcceptanceConditions1>& acceptance_conditions;
 const std::optional<MilitaryFormationReservations1>& acceptance_resource_reservations;
 // Null only for callers outside the explicitly selected /2 endpoint route.
 const std::optional<MilitaryActionEndpointObservation2>* acceptance_observation=nullptr;
 const std::optional<MilitaryActionEndpointObservation2>* completion_observation=nullptr;
};


struct DynamicMilitaryLogisticsFact2 {
 StableId id,command,actor_id;WorldMoment started_at;std::optional<WorldMoment> completed_at;
 StableId phase="started";bool success=false;MilitaryLogisticsTerms original_terms;
 // The original logistics JSON context is always empty. The common exact slot
 // is absent and cannot authorize a new dialogue or a different acceptance.
 std::optional<InteractionContext> original_context;ActionElapsedEntryRef elapsed_source_ref;
 std::optional<MilitaryLogisticsResult> result;std::optional<SourceRef> terminal_source_ref;
 Integer rule_version=2;
 std::optional<MilitaryActionAcceptanceConditions1> acceptance_conditions;
};
struct DynamicMilitaryReturnFact2 {
 StableId id,army_id,order_id,garrison_id,source_garrison_id,ownership_id,actor_id;
 MilitaryReturnAuthority authority;WorldMoment completed_at;
 std::vector<std::pair<StableId,Integer>> goods;Integer money=0;
 std::vector<StableId> unit_ids,returned_retainers,lost_retainers,prisoner_ids;
 std::vector<MilitaryActorChange> actor_changes;
 StableId logistics_action_ref;SourceRef completion_source_ref;
 std::optional<MilitaryActionResourceEffect1> actual_resource_effect;
 std::optional<MilitaryReturnDetachmentReleases1> actual_detachment_releases;
};
// Borrowed exact C07 completion inputs. The nullable actor_changes member is
// only the original optional vector's finite build view, not generic pointers.
struct MilitaryLogisticsResultBuildView {
 std::optional<bool> ok;
 const StableId& army_id;const StableId& order_id;const StableId& receipt_id;
 const std::vector<MilitaryActorChange>* actor_changes;
 const FieldOrder& field_order;
};
struct DynamicMilitaryLogisticsFactBuildView2 {
 const StableId& id;const StableId& command;const StableId& actor_id;
 const WorldMoment& started_at;const std::optional<WorldMoment>& completed_at;
 std::string_view phase;bool success;
 const MilitaryLogisticsTerms& original_terms;const std::optional<InteractionContext>& original_context;
 const ActionElapsedEntryRef& elapsed_source_ref;
 const std::optional<MilitaryLogisticsResultBuildView>& result;
 const std::optional<SourceRefBuildView>& terminal_source_ref;
 Integer rule_version;
 const std::optional<MilitaryActionAcceptanceConditions1>& acceptance_conditions;
};
struct DynamicMilitaryReturnFactBuildView2 {
 const StableId& id;const StableId& army_id;const StableId& order_id;
 const StableId& garrison_id;const StableId& source_garrison_id;const StableId& ownership_id;const StableId& actor_id;
 const MilitaryReturnAuthority& authority;const WorldMoment& completed_at;
 const std::vector<std::pair<StableId,Integer>>& goods;Integer money;
 const std::vector<StableId>& unit_ids;const std::vector<StableId>& returned_retainers;
 const std::vector<StableId>& lost_retainers;const std::vector<StableId>& prisoner_ids;
 const std::vector<MilitaryActorChange>& actor_changes;
 const StableId& logistics_action_ref;SourceRefBuildView completion_source_ref;
 const std::optional<MilitaryActionResourceEffect1>& actual_resource_effect;
 const std::optional<MilitaryReturnDetachmentReleases1>& actual_detachment_releases;
};


}
