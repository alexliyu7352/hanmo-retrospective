#pragma once
#include "military_model.hpp"
#include "military_lifecycle.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <optional>
#include <string_view>
#include <vector>
namespace hanmo::resident {
// The original return roster rule shared by current acceptance and exact source
// replay. Inputs and output IDs are borrowed; no second participant snapshot.
template<class Alive,class Append>Status visit_original_return_participants(
 const CheckpointVector<const StableId*>& members,const StableId& actor,Alive alive,Append append){
 bool actor_included=false;
 for(const auto* id:members){if(!id)return common_failure("MilitaryReturnParticipantRequired");auto actual=alive(*id);if(!actual.ok)return {false,{},actual.error};if(actual.value){auto emitted=append(*id);if(!emitted.ok)return emitted;actor_included=actor_included||*id==actor;}}
 if(!actor_included)return append(actor);return common_success();
}
class MilitaryActionResourceCapture;
class OriginalEstateDetachmentReleaseSink;
// Transient original business result. The caller adopts exactly one legacy or
// exact permanent return row; no tick or Common source is assigned by this leaf.
struct OriginalArmyReturnActorChangeBuildView {
 std::string_view actor_id,status,captor_army_id;
 std::optional<std::string_view> garrison_id;
 std::string_view army_id;
 const FieldOrder& field_order;
};
struct OriginalArmyReturnFactBuildView {
 std::string_view id,army_id,order_id,garrison_id,source_garrison_id,ownership_id,actor_id;
 const MilitaryReturnAuthority& authority;
 Integer tick;
 const std::vector<std::pair<StableId,Integer>>& goods;
 Integer money;
 const std::vector<StableId>& unit_ids;
 const CheckpointVector<std::string_view>& returned_retainers;
 const CheckpointVector<std::string_view>& lost_retainers;
 const std::vector<StableId>& prisoner_ids;
 const CheckpointVector<OriginalArmyReturnActorChangeBuildView>& actor_changes;
 const FieldOrder& field_order;
};
struct OriginalArmyReturnCompletionBuildView {
 bool success;
 const OriginalArmyReturnFactBuildView* actual_return;
};

struct OriginalArmyReturnCompletion;


// Owns only this genuine business output. Later legacy receipt mutation and
// persistent/result copies require their own original ownership boundaries.
struct OriginalArmyReturnCompletion {
 CheckpointMemoryCharge copied_fields;
 bool success=false;
 std::optional<MilitaryReturnFact> actual_return;
 OriginalArmyReturnCompletion()=default;
 explicit OriginalArmyReturnCompletion(const OriginalArmyReturnCompletionBuildView&);
 OriginalArmyReturnCompletion(const OriginalArmyReturnCompletion&);
 OriginalArmyReturnCompletion(OriginalArmyReturnCompletion&&);
 OriginalArmyReturnCompletion& operator=(OriginalArmyReturnCompletion);
 void swap(OriginalArmyReturnCompletion&)noexcept;
};
Outcome<OriginalArmyReturnCompletion> complete_original_army_return_in_candidate(
 CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const ActionCoreView&,const MilitaryLogisticsTerms&,MilitaryActionResourceCapture* resource_capture=nullptr,OriginalEstateDetachmentReleaseSink* detachment_capture=nullptr);
}
