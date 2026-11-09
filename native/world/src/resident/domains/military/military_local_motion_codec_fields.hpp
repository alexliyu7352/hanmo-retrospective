// S2 3.1: original four geometry alternatives and their complete source fields.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_local_motion.hpp"
#include "military_local_body_codec_fields.hpp"
#include "military_spatial_point_codec_fields.hpp"
#include "resident/domains/travel/movement_execution_ref_codec_fields.hpp"
namespace hanmo::resident::military_local_motion_fields {
template<class C>Status value(C&,LocalMotionSegmentRef&);
template<class C>Status value(C&,LocalHexNeighbor&);
template<class C>Status value(C&,ReverseLocalMotionAtPrefix&);
template<class C>Status value(C&,RepriceLocalMotionSuffix&);
template<class C>Status value(C&,LocalPartialEdgeContinuation&);
template<class C>Status value(C&,LocalEntryConnector&);
template<class C>Status value(C&,LocalExitConnector&);
template<class C>Status value(C&,LocalMotionSegment&);
template<class C>Status value(C&,LocalHexMotionBasis2&);
template<class C>Status value(C&,LocalMotionCursor2&);
template<class C>Status value(C&,LocalMotionPrefixClose2&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,CanonicalFraction& v){return exact_fields::fraction(c,v);}
template<class C>Status value(C& c,WorkUnits& v){return exact_fields::work_units(c,v);}
template<class C>Status value(C& c,MilitarySpatialPoint& v){return military_spatial_point_fields::value(c,v);}
template<class C>Status value(C& c,MilitaryLocalBodyRef& v){return military_local_body_codec::value(c,v);}
template<class C>Status value(C& c,SpatialCarrierRef& v){return spatial_carrier_ref_codec_fields::fields(c,v);}
template<class C>Status value(C& c,MovementExecutionRef& v){return movement_execution_ref_codec_fields::fields(c,v);}
template<class C>Status value(C& c,HexPoint& v){c.field("q",v.q);return c.field("r",v.r);}
template<class C>Status value(C& c,LocalMotionPhase& v){
 Integer tag=Integer(v);c.field("tag",tag);
 if(c.status().ok&&(tag<0||tag>3))return c.reject("UnknownLocalMotionPhase");
 if(c.reading()&&c.status().ok)v=LocalMotionPhase(tag);return c.status();
}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<std::size_t I=0,class C,class V>bool construct(C& c,V& v,Integer tag){
 if constexpr(I<std::variant_size_v<V>){if(tag==Integer(I)){if(!prepare_checkpoint_default_values<std::variant_alternative_t<I,V>>(c).ok)return true;v.template emplace<I>();return true;}return construct<I+1>(c,v,tag);}
 return false;
}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){
 // Only the two original local-motion unions use this table. Carrier, body and
 // execution above delegate their already selected unique owner protocols.
 using V=std::variant<T...>;
 static_assert(std::is_same_v<V,LocalMotionGeometry>||std::is_same_v<V,LocalMotionChange>);
 Integer tag=Integer(v.index());c.field("tag",tag);
 if(c.status().ok&&(tag<0||std::size_t(tag)>=sizeof...(T)))return c.reject("UnknownLocalMotionVariant");
 if(c.reading()&&c.status().ok&&!construct(c,v,tag))return c.reject("UnknownLocalMotionVariant");
 if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& row){return value(x,row);},v);});
 return c.status();
}
template<class C>Status value(C& c,LocalMotionSegmentRef& v){field(c,"basis_ref",v.basis_ref);field(c,"segment_ordinal",v.segment_ordinal);return c.status();}
template<class C>Status value(C& c,LocalHexNeighbor& v){
 field(c,"root_edge_ref",v.root_edge_ref);field(c,"from_vertex",v.from_vertex);
 field(c,"to_vertex",v.to_vertex);field(c,"directional_cost",v.directional_cost);return c.status();
}
template<class C>Status value(C& c,ReverseLocalMotionAtPrefix& v){return field(c,"accepted_intent_source_ref",v.accepted_intent_source_ref);}
template<class C>Status value(C& c,RepriceLocalMotionSuffix& v){field(c,"cost_change_source_ref",v.cost_change_source_ref);return field(c,"continuing_intent_source_ref",v.continuing_intent_source_ref);}
template<class C>Status value(C& c,LocalPartialEdgeContinuation& v){
 field(c,"root_edge_ref",v.root_edge_ref);field(c,"parent_segment_ref",v.parent_segment_ref);
 field(c,"parent_prefix_close_ref",v.parent_prefix_close_ref);field(c,"change",v.change);
 field(c,"root_u_start",v.root_u_start);field(c,"root_u_end",v.root_u_end);
 field(c,"directional_cost",v.directional_cost);field(c,"topology_anchor_source_ref",v.topology_anchor_source_ref);return c.status();
}
template<class C,class T>Status connector_tail(C& c,T& v){
 field(c,"chart_ref",v.chart_ref);field(c,"portal_id",v.portal_id);
 field(c,"connector_path_ref",v.connector_path_ref);field(c,"piece_ordinal",v.piece_ordinal);
 field(c,"from_qr",v.from_qr);field(c,"to_qr",v.to_qr);
 field(c,"topology_anchor_source_ref",v.topology_anchor_source_ref);return c.status();
}
template<class C>Status value(C& c,LocalEntryConnector& v){field(c,"admission_source_ref",v.admission_source_ref);return connector_tail(c,v);}
template<class C>Status value(C& c,LocalExitConnector& v){field(c,"exit_source_ref",v.exit_source_ref);return connector_tail(c,v);}
template<class C>Status value(C& c,LocalMotionSegment& v){
 field(c,"ordinal",v.ordinal);field(c,"geometry",v.geometry);field(c,"required_edge_work",v.required_edge_work);
 field(c,"terrain_cost_basis_ref",v.terrain_cost_basis_ref);field(c,"original_cost_unit_ref",v.original_cost_unit_ref);return c.status();
}
template<class C>Status value(C& c,LocalHexMotionBasis2& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"carrier_ref",v.carrier_ref);
 field(c,"local_body_ref",v.local_body_ref);field(c,"execution_ref",v.execution_ref);field(c,"intent_source_ref",v.intent_source_ref);
 field(c,"geometry_definition_ref",v.geometry_definition_ref);field(c,"map_version",v.map_version);
 field(c,"terrain_rule_ref",v.terrain_rule_ref);field(c,"unit_or_vessel_capability_source_ref",v.unit_or_vessel_capability_source_ref);
 field(c,"chart_ref",v.chart_ref);field(c,"started_at",v.started_at);field(c,"segments",v.segments);
 field(c,"route_terminal",v.route_terminal);field(c,"original_unit_cost_source_ref",v.original_unit_cost_source_ref);return c.status();
}
template<class C>Status value(C& c,LocalMotionCursor2& v){
 field(c,"execution_ref",v.execution_ref);field(c,"local_body_ref",v.local_body_ref);
 field(c,"basis_ref",v.basis_ref);field(c,"segment_ordinal",v.segment_ordinal);
 field(c,"credited_edge_work",v.credited_edge_work);field(c,"last_settled_at",v.last_settled_at);
 field(c,"rate_source_ref",v.rate_source_ref);field(c,"rate_work_per_half_day",v.rate_work_per_half_day);
 field(c,"phase",v.phase);field(c,"phase_source_ref",v.phase_source_ref);
 field(c,"reserved_topology_refs",v.reserved_topology_refs);field(c,"closed_prefix_sources",v.closed_prefix_sources);return c.status();
}
template<class C>Status value(C& c,LocalMotionPrefixClose2& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"execution_ref",v.execution_ref);
 field(c,"local_body_ref",v.local_body_ref);field(c,"root_edge_ref",v.root_edge_ref);
 field(c,"parent_basis_ref",v.parent_basis_ref);field(c,"parent_segment_ordinal",v.parent_segment_ordinal);
 field(c,"parent_u_start",v.parent_u_start);field(c,"parent_u_end",v.parent_u_end);
 field(c,"parent_required_work",v.parent_required_work);field(c,"credited_work_at_close",v.credited_work_at_close);
 field(c,"closed_root_u",v.closed_root_u);field(c,"closed_at",v.closed_at);
 field(c,"topology_anchor_source_ref",v.topology_anchor_source_ref);field(c,"reservation_source_refs",v.reservation_source_refs);
 field(c,"original_cost_unit_ref",v.original_cost_unit_ref);field(c,"actual_work_cost_source_refs",v.actual_work_cost_source_refs);
 field(c,"change_source_ref",v.change_source_ref);return c.status();
}
}
