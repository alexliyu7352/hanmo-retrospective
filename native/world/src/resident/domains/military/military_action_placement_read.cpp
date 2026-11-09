#include "military_action_placement_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <set>
namespace hanmo::resident {using namespace movement_detail;
namespace {
CheckpointMemoryCharge reserve_placement(const MilitaryActionPlacementBuildView& input){return CheckpointMemoryCharge(0);}
Location copy_placement_location(const MilitaryActionPlacementLocationBuildView& input){
 return std::visit([](const auto& value)->Location{
  using T=std::decay_t<decltype(value)>;
  if constexpr(std::is_same_v<T,SettlementLocationBuildView>)return SettlementLocation{StableId(value.settlement_id),StableId(value.place_id)};
  else return InArmyLocation{StableId(value.army_id)};
 },input);
}
}
MilitaryActionPlacement::MilitaryActionPlacement(const MilitaryActionPlacementBuildView& input)
 :copied_fields(reserve_placement(input)),actor_id(checkpoint_fresh_string_copy(input.actor_id)),occurred_at(input.occurred_at),
 actual_application{{StableId(input.actual_application.source_ref.domain),StableId(input.actual_application.source_ref.kind),StableId(input.actual_application.source_ref.id)},input.actual_application.source_schema},
 original_location_result(copy_placement_location(input.original_location_result)),
 actual_anchor(OriginalMovementAnchor{SettlementLocation{StableId(input.actual_anchor.location.settlement_id),StableId(input.actual_anchor.location.place_id)},
  {StableId(input.actual_anchor.actual_source.domain),StableId(input.actual_anchor.actual_source.kind),StableId(input.actual_anchor.actual_source.id)}}){}

const MilitaryActionPlacementRead& NativeMilitaryActionPlacementRead::owner(const MomentOwnerSource& ref)const{
 if(ref.source_schema!=2||!is_military_action_placement_source_kind(ref.source_ref))fail("MilitaryActionPlacementSourceRequired",ref.source_ref.id);
 if(ref.source_ref.kind=="military_action_completion_applied")return formations_;return returns_;
}
Outcome<WorldMoment> NativeMilitaryActionPlacementRead::read_military_action_placement_source_moment(const MomentOwnerSource& ref)const{return guard<WorldMoment>([&]{return take(owner(ref).read_military_action_placement_source_moment(ref));});}
Outcome<std::optional<MilitaryActionPlacement>> NativeMilitaryActionPlacementRead::read_military_action_placement(const MomentOwnerSource& ref,const StableId& actor)const{return guard<std::optional<MilitaryActionPlacement>>([&]{return take(owner(ref).read_military_action_placement(ref,actor));});}
Outcome<CheckpointVector<MilitaryActionPlacement>> NativeMilitaryActionPlacementRead::read_military_action_placement_history(const StableId& actor)const{return guard<CheckpointVector<MilitaryActionPlacement>>([&]{
 auto out=take(formations_.read_military_action_placement_history(actor));auto returned=take(returns_.read_military_action_placement_history(actor));
 if(returned.size()>out.max_size()-out.size())fail("MilitaryActionPlacementHistoryTooLarge",actor);
 // Reserve before borrowing result strings; no later append invalidates keys.
 out.reserve(out.size()+returned.size());
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> ids;
 for(const auto& row:out)if(!ids.insert(row.actual_application.source_ref.id).second)fail("MilitaryActionPlacementSourceRepeated",row.actual_application.source_ref.id);
 for(auto& row:returned){if(!ids.insert(row.actual_application.source_ref.id).second)fail("MilitaryActionPlacementSourceRepeated",row.actual_application.source_ref.id);out.push_back(std::move(row));}return out;
});}
}
