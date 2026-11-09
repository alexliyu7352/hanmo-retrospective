// Explicit /3 tagged successor; the /2 original row calls its unique old table.
#pragma once
#include "military_local_motion_codec_fields.hpp"
#include "dynamic_action_codec_fields.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
namespace hanmo::resident::military_local_boundary_fields {
template<class C>Status value(C&,BodyMotionBoundaryInput3&);
template<class C>Status value(C&,LocalMotionEdgeCompletion3&);
template<class C>Status value(C&,LocalMotionBlockedCell3&);
template<class C>Status value(C&,LocalMotionChartBoundary3&);
template<class C>Status value(C&,LocalMotionForbiddenRegion3&);
template<class C>Status value(C&,LocalMotionContactBody3&);
template<class C>Status value(C&,LocalMotionContactObstacle3&);
template<class C>Status value(C&,LocalMotionSimultaneousContact3&);
template<class C>Status value(C&,LocalMotionContact3&);
template<class C>Status value(C&,LocalMotionContactConsequence3&);
template<class C>Status value(C&,MilitaryContactParameterWindow&);
template<class C>Status value(C&,CommonActualFactPrefix&);
template<class C,class...T>Status value(C&,std::variant<T...>&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,MomentOwnerSource& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,MomentSpan& v){return dynamic_action_codec::value(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,CanonicalFraction& v){return exact_fields::fraction(c,v);}
template<class C>Status value(C& c,WorkUnits& v){return exact_fields::work_units(c,v);}
template<class C>Status value(C& c,MilitaryLocalBodyRef& v){return military_local_body_codec::value(c,v);}
template<class C>Status value(C& c,MovementExecutionRef& v){return movement_execution_ref_codec_fields::fields(c,v);}
template<class C>Status value(C& c,LocalMotionPhase& v){return military_local_motion_fields::value(c,v);}
template<class C>Status value(C& c,HexPoint& v){return military_local_motion_fields::value(c,v);}
template<class C>Status value(C& c,LocalMotionPrefixClose2& v){return military_local_motion_fields::value(c,v);}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class...T>Status value(C& c,std::variant<T...>& v){
 using V=std::variant<T...>;
 static_assert(std::is_same_v<V,LocalMotionBoundarySource>||std::is_same_v<V,LocalMotionObstacleInput3>||std::is_same_v<V,LocalMotionContactOther3>);
 Integer tag=Integer(v.index());c.field("tag",tag);
 if(c.status().ok&&(tag<0||std::size_t(tag)>=sizeof...(T)))return c.reject("UnknownMilitaryLocalBoundaryVariant");
 if(c.reading()&&c.status().ok&&!military_local_motion_fields::construct(c,v,tag))return c.reject("UnknownMilitaryLocalBoundaryVariant");
 if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& row){return value(x,row);},v);});
 return c.status();
}
template<class C>Status value(C& c,LocalMotionContactConsequence3& v){
 Integer tag=Integer(v);c.field("tag",tag);
 if(c.status().ok&&(tag<0||tag>2))return c.reject("UnknownMilitaryContactConsequence");
 if(c.reading()&&c.status().ok)v=LocalMotionContactConsequence3(tag);return c.status();
}
template<class C>Status value(C& c,CommonActualFactPrefix& v){
 return common_fact_prefix_fields::fields(c,v,
     [](auto& x,auto& source){return exact_fields::source(x,source);});
}
template<class C>Status value(C& c,BodyMotionBoundaryInput3& v){
 field(c,"local_body_ref",v.local_body_ref);field(c,"execution_ref",v.execution_ref);field(c,"basis_ref",v.basis_ref);field(c,"segment_ordinal",v.segment_ordinal);field(c,"prior_phase_source",v.prior_phase_source);field(c,"prior_motion_source",v.prior_motion_source);field(c,"credited_work_before",v.credited_work_before);field(c,"from_at",v.from_at);field(c,"rate_source_ref",v.rate_source_ref);field(c,"rate_work_per_half_day",v.rate_work_per_half_day);field(c,"phase_before",v.phase_before);field(c,"reserved_topology_refs",v.reserved_topology_refs);field(c,"heading",v.heading);field(c,"heading_source_ref",v.heading_source_ref);field(c,"shape_source",v.shape_source);field(c,"spatial_binding_ref",v.spatial_binding_ref);field(c,"qualification_source_refs",v.qualification_source_refs);return c.status();
}
template<class C>Status value(C& c,LocalMotionEdgeCompletion3& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"actual_before_common_prefix",v.actual_before_common_prefix);field(c,"input",v.input);field(c,"completed_at",v.completed_at);field(c,"credited_work_at_completion",v.credited_work_at_completion);field(c,"actual_work_cost_source_refs",v.actual_work_cost_source_refs);field(c,"reservation_release_sources",v.reservation_release_sources);field(c,"arrived_topology_source",v.arrived_topology_source);return c.status();
}
template<class C>Status value(C& c,LocalMotionBlockedCell3& v){
 field(c,"battlefield_ref",v.battlefield_ref);field(c,"map_version",v.map_version);field(c,"cell",v.cell);field(c,"terrain_cost_source",v.terrain_cost_source);field(c,"capability_source",v.capability_source);return c.status();
}
template<class C>Status value(C& c,LocalMotionChartBoundary3& v){
 field(c,"chart_ref",v.chart_ref);field(c,"geometry_definition_source",v.geometry_definition_source);return c.status();
}
template<class C>Status value(C& c,LocalMotionForbiddenRegion3& v){
 field(c,"definition_source",v.definition_source);field(c,"region_ordinal",v.region_ordinal);field(c,"capability_source",v.capability_source);return c.status();
}
template<class C>Status value(C& c,LocalMotionContactBody3& v){
 field(c,"body_ordinal",v.body_ordinal);return c.status();
}
template<class C>Status value(C& c,LocalMotionContactObstacle3& v){
 field(c,"obstacle_ordinal",v.obstacle_ordinal);return c.status();
}
template<class C>Status value(C& c,LocalMotionSimultaneousContact3& v){
 field(c,"first_body_ordinal",v.first_body_ordinal);field(c,"other",v.other);field(c,"closed_parameter_window",v.closed_parameter_window);field(c,"first_inward_parameter",v.first_inward_parameter);field(c,"original_relation_source",v.original_relation_source);field(c,"consequence_kind",v.consequence_kind);return c.status();
}
template<class C>Status value(C& c,LocalMotionContact3& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"generation",v.generation);field(c,"actual_before_common_prefix",v.actual_before_common_prefix);field(c,"prediction_window",v.prediction_window);field(c,"occurred_at",v.occurred_at);field(c,"body_inputs",v.body_inputs);field(c,"obstacle_inputs",v.obstacle_inputs);field(c,"simultaneous_contacts",v.simultaneous_contacts);field(c,"actual_work_cost_source_refs",v.actual_work_cost_source_refs);return c.status();
}
template<class C>Status value(C& c,MilitaryContactParameterWindow& v){
 field(c,"first",v.first);field(c,"last",v.last);return c.status();
}
template<class C>Status selected(C& c,LocalMotionBoundarySource& v,Integer schema){
 if(schema==2){
  if(c.reading()&&c.status().ok){auto ready=prepare_checkpoint_default_values<LocalMotionPrefixClose2>(c);if(!ready.ok)return ready;v.template emplace<LocalMotionPrefixClose2>();}
  auto* old=std::get_if<LocalMotionPrefixClose2>(&v);
  if(!old)return c.reject("MilitaryLocalBoundary3RequiresExplicitSelection");
  return military_local_motion_fields::value(c,*old);
 }
 if(schema!=3)return c.reject("MilitaryLocalMotionSchemaRequired");
 return value(c,v);
}
}
