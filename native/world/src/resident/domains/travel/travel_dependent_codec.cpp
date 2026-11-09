#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "travel_dependent_state.hpp"
#include "movement_exact_detail.hpp"
#include "transport_physical_table.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::transport_decoded_transfer {
template<>struct OriginalRow<TravelDependentBinding>:std::true_type {};
template<>struct OriginalRow<TravelDependentBindingChangeResult>:std::true_type {};
}
namespace hanmo::resident {namespace {
using namespace movement_record_fields;
template<class C>Status placement(C& c,BirthPlacement& p,const MovementRecordCodecSelection& selected){
 field(c,"location",p.location,selected);field(c,"resolved_anchor",p.resolved_anchor,selected);
 Integer kind=static_cast<Integer>(p.placement_kind);c.field("placement_kind",kind);if(kind<0||kind>4)return c.reject("UnknownBirthPlacementKind");if(c.reading())p.placement_kind=static_cast<BirthPlacementKind>(kind);
 field(c,"carrier_ref",p.carrier_ref,selected);field(c,"safe_boundary_ref",p.safe_boundary_ref,selected);return field(c,"placement_basis_ref",p.placement_basis_ref,selected);
}
template<class C>Status binding(C& c,TravelDependentBinding& v,const MovementRecordCodecSelection& selected){
 c.field("binding_id",v.binding_id);c.field("dependent_actor_id",v.dependent_actor_id);c.field("carrier_actor_id",v.carrier_actor_id);field(c,"transport_ref",v.transport_ref,selected);field(c,"source_birth_ref",v.source_birth_ref,selected);field(c,"attached_at",v.attached_at,selected);
 Integer policy=static_cast<Integer>(v.policy);c.field("policy",policy);if(policy!=0)return c.reject("UnknownTravelDependentPolicy");if(c.reading())v.policy=TravelDependentPolicy::follow_carrier_until_explicit_handover;return c.field("revision",v.revision);
}
template<class C>Status result(C& c,TravelDependentBindingChangeResult& value,const MovementRecordCodecSelection& selected){
 Integer tag=static_cast<Integer>(value.index());c.field("variant",tag);if(tag<0||tag>1)return c.reject("UnsupportedTravelDependentChangeKind");
 if(tag==0){if(c.reading())value.emplace<BirthDependentBindingChange>();auto& v=std::get<BirthDependentBindingChange>(value);
  c.field("id",v.id);field(c,"actual_birth_source",v.actual_birth_source,selected);field(c,"occurred_at",v.occurred_at,selected);c.field("child_actor_id",v.child_actor_id);c.field("delivery_actor_id",v.delivery_actor_id);c.field("birth_arrangement_id",v.birth_arrangement_id);
  c.object("actual_placement",[&](auto& body){return placement(body,v.actual_placement,selected);});field(c,"before_binding_refs",v.before_binding_refs,selected);return field(c,"after_binding_refs",v.after_binding_refs,selected);
 }
 if(c.reading())value.emplace<CareDependentBindingChange>();auto& v=std::get<CareDependentBindingChange>(value);
 c.field("id",v.id);field(c,"actual_handover_source",v.actual_handover_source,selected);field(c,"occurred_at",v.occurred_at,selected);c.field("child_actor_id",v.child_actor_id);c.field("carer_actor_id",v.carer_actor_id);
 field(c,"actual_location",v.actual_location,selected);field(c,"resolved_anchor",v.resolved_anchor,selected);field(c,"before_binding_refs",v.before_binding_refs,selected);return field(c,"after_binding_refs",v.after_binding_refs,selected);
}
template<class C>Status roots(C& c,TravelDependentState& state,Integer version,bool carried){
 if(version!=2)return c.reject("UnsupportedTravelDependentsVersion");MovementRecordCodecSelection selected;selected.carrier_location_enabled=carried;
 transport_physical_table::DecodedRows<TravelDependentBinding> bindings;transport_physical_table::DecodedRows<TravelDependentBindingChangeResult> changes;
 auto status=transport_physical_table::all(c,"bindings",state.bindings,bindings,[&](auto& row,const StableId& key,TravelDependentBinding& v){auto s=binding(row,v,selected);if(!s.ok)return s;if(v.binding_id!=key)return row.reject("TravelDependentBindingKeyMismatch",key);return row.status();});if(!status.ok)return status;
 status=transport_physical_table::all(c,"binding_change_results",state.binding_change_results,changes,[&](auto& row,const StableId& key,TravelDependentBindingChangeResult& v){auto s=result(row,v,selected);if(!s.ok)return s;if(std::visit([&](const auto& actual){return actual.id!=key;},v))return row.reject("TravelDependentResultKeyMismatch",key);return row.status();});if(!status.ok)return status;
 if(c.reading()){status=transport_physical_table::adopt(state.bindings,bindings,"bindings");if(!status.ok)return status;status=transport_physical_table::adopt(state.binding_change_results,changes,"binding_change_results");if(!status.ok)return status;}return c.status();
}
}
Status visit_travel_dependents_checkpoint(CodecVisitor& c,TravelDependentState& state,Integer version,bool carried){if(c.carrier_location_enabled!=carried)return c.reject("TravelDependentLocationSelectionMismatch");return roots(c,state,version,carried);}
Status emit_travel_dependents_projection(ProjectionWriter& w,std::string_view name,TravelDependentState& state,Integer version,bool carried){return w.object(name,[&](auto& sink){ActorFoundationProjectionArchive c(sink);return roots(c,state,version,carried);});}
Outcome<BoundaryValue> encode_travel_dependents(TravelDependentState& state,Integer version,bool carried){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(emit_travel_dependents_projection(w,"",state,version,carried));movement_detail::take(w.finish());return out;});}
Outcome<TravelDependentState> decode_travel_dependents(const BoundaryValue& input,Integer version,bool carried){return movement_detail::guard<TravelDependentState>([&]{TravelDependentState out;ActorFoundationJsonVisitor c(input);movement_detail::take(roots(c,out,version,carried));movement_detail::take(c.finish());return out;});}
}
