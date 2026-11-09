// V10.5: the original two travel_dependents/2 tables. Birth is a finite
// binding-change result, including an actual settlement placement with no link.
#pragma once
#include "birth_placement.hpp"
namespace hanmo::resident {
struct BirthDependentBindingChange {
 StableId id;
 MomentOwnerSource actual_birth_source;WorldMoment occurred_at;
 StableId child_actor_id,delivery_actor_id,birth_arrangement_id;
 BirthPlacement actual_placement;
 std::vector<StableId> before_binding_refs,after_binding_refs;
};
// The real completed handover keeps the child at the proven common place.
// It ends an old transport dependency; it does not create a move or a new
// spatial head merely because the caregiver changed.
struct CareDependentBindingChange {
 StableId id;MomentOwnerSource actual_handover_source;WorldMoment occurred_at;
 StableId child_actor_id,carer_actor_id;
 Location actual_location;MovementAnchor resolved_anchor;
 std::vector<StableId> before_binding_refs,after_binding_refs;
};
using TravelDependentBindingChangeResult=std::variant<BirthDependentBindingChange,CareDependentBindingChange>;
struct TravelDependentState {
 OrderedTable<TravelDependentBinding> bindings;
 OrderedTable<TravelDependentBindingChangeResult> binding_change_results;
};
// Reads only the original immutable before/after relation chain. Full Birth
// and Care source validation is separately required by their original owners.
// before_source is a true retained Common cut; null reads the current chain.
Outcome<const TravelDependentBinding*> read_active_travel_dependent_binding(
 const CommonStore&,const TravelDependentState&,const StableId& dependent_actor_id,
 const std::optional<SourceRef>& before_source={},ExactArithmeticBudget={});
Status validate_travel_dependent_binding_chain(const CommonStore&,const TravelDependentState&,ExactArithmeticBudget={});
// Same physical version and explicit selected capability; no inference from a
// nonempty table or another Birth selector. New-world initial roots are empty.
Status validate_initial_travel_dependents(const CommonStore&,const TravelDependentState&,Integer selected_version);
class ProjectionWriter;class CodecVisitor;
Status visit_travel_dependents_checkpoint(CodecVisitor&,TravelDependentState&,Integer selected_version,bool carrier_location_enabled);
Status emit_travel_dependents_projection(ProjectionWriter&,std::string_view,TravelDependentState&,Integer selected_version,bool carrier_location_enabled);
Outcome<BoundaryValue> encode_travel_dependents(TravelDependentState&,Integer selected_version,bool carrier_location_enabled);
Outcome<TravelDependentState> decode_travel_dependents(const BoundaryValue&,Integer selected_version,bool carrier_location_enabled);
}
