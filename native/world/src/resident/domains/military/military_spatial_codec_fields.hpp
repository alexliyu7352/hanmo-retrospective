// S2 3.5: chart, original author definition and actual writer handoff fields.
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_spatial_handoff.hpp"
#include "military_spatial_point_codec_fields.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
namespace hanmo::resident::military_spatial_fields {
template<class C>Status value(C&,MilitarySpatialMatrix&);
template<class C>Status value(C&,MilitarySpatialTranslation&);
template<class C>Status value(C&,MilitarySpatialSegment&);
template<class C>Status value(C&,MilitarySpatialPortal&);
template<class C>Status value(C&,MilitarySpatialChart2&);
template<class C>Status value(C&,MilitaryRouteCorrespondence2&);
template<class C>Status value(C&,MilitarySpatialHandoff2&);
template<class C>Status value(C&,MilitaryOrientationHalfspace&);
template<class C>Status value(C&,MilitaryEncounterOrientation&);
template<class C>Status value(C&,LockedArenaChartDefinition&);
template<class C>Status value(C&,EncounterChartDefinition&);
template<class C>Status value(C&,MilitarySpatialGeometryDefinition2&);
template<class C>Status value(C&,MilitaryEntryConnectorDefinition&);
template<class C>Status value(C& c,StableId& v){return c.field("value",v);}
template<class C>Status value(C& c,Integer& v){return c.field("value",v);}
template<class C>Status value(C& c,WorldMoment& v){return exact_fields::moment(c,v);}
template<class C>Status value(C& c,SourceRef& v){return exact_fields::source(c,v);}
template<class C>Status value(C& c,CanonicalFraction& v){return exact_fields::fraction(c,v);}
template<class C>Status value(C& c,SignedSpatialRational& v){return military_spatial_point_fields::value(c,v);}
template<class C>Status value(C& c,MilitarySpatialPoint& v){return military_spatial_point_fields::value(c,v);}
template<class C>Status value(C& c,SpatialCarrierRef& v){return spatial_carrier_ref_codec_fields::fields(c,v);}
template<class C>Status value(C& c,MovementExecutionRef& v){return movement_execution_ref_codec_fields::fields(c,v);}
template<class C>Status value(C& c,SpatialWriterBinding& v){return movement_record_fields::value(c,v,MovementRecordCodecSelection{});}
template<class C,class T>Status field(C& c,const char* name,T& v){return c.object(name,[&](auto& x){return value(x,v);});}
template<class C>Status field(C& c,const char* name,StableId& v){return c.field(name,v);}
template<class C>Status field(C& c,const char* name,Integer& v){return c.field(name,v);}
template<class C,class T,class A>Status field(C& c,const char* name,std::vector<T,A>& v){return c.sequence(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C,class T>Status field(C& c,const char* name,std::optional<T>& v){return c.optional(name,v,[](auto& x,auto& row){return value(x,row);});}
template<class C>Status value(C& c,MilitarySpatialMatrix& v){field(c,"a11",v.a11);field(c,"a12",v.a12);field(c,"a21",v.a21);field(c,"a22",v.a22);return c.status();}
template<class C>Status value(C& c,MilitarySpatialTranslation& v){field(c,"b1",v.b1);return field(c,"b2",v.b2);}
template<class C>Status value(C& c,MilitarySpatialSegment& v){field(c,"from",v.from);return field(c,"to",v.to);}
template<class C>Status value(C& c,MilitarySpatialPortal& v){
 field(c,"portal_id",v.portal_id);field(c,"world_entry_exit_source_kind",v.world_entry_exit_source_kind);
 field(c,"world_boundary_ref",v.world_boundary_ref);field(c,"local_boundary_segment",v.local_boundary_segment);
 field(c,"allowed_carrier_kinds",v.allowed_carrier_kinds);field(c,"orientation_mapping",v.orientation_mapping);
 field(c,"connector_path_ref",v.connector_path_ref);return c.status();
}
template<class C>Status value(C& c,MilitarySpatialChart2& v){
 field(c,"id",v.id);field(c,"session_ref",v.session_ref);field(c,"geometry_definition_ref",v.geometry_definition_ref);
 field(c,"world_geometry_ref",v.world_geometry_ref);field(c,"world_length_unit_ref",v.world_length_unit_ref);
 field(c,"local_axis_unit_ref",v.local_axis_unit_ref);field(c,"matrix",v.matrix);field(c,"translation",v.translation);
 field(c,"domain_polygon",v.domain_polygon);field(c,"portal_rows",v.portal_rows);
 field(c,"creation_source_ref",v.creation_source_ref);field(c,"rule_version",v.rule_version);
 if(c.status().ok&&v.rule_version!=2)return c.reject("UnsupportedMilitarySpatialChartRuleVersion",v.id);return c.status();
}
template<class C>Status value(C& c,MilitaryRouteCorrespondence2& v){
 field(c,"old_segment_ordinal",v.old_segment_ordinal);field(c,"new_segment_ordinal",v.new_segment_ordinal);
 field(c,"old_parameter_from",v.old_parameter_from);field(c,"old_parameter_to",v.old_parameter_to);
 field(c,"new_parameter_from",v.new_parameter_from);field(c,"new_parameter_to",v.new_parameter_to);
 field(c,"old_from",v.old_from);field(c,"old_to",v.old_to);field(c,"new_from",v.new_from);field(c,"new_to",v.new_to);
 field(c,"chart_ref",v.chart_ref);field(c,"original_parameter_source",v.original_parameter_source);return c.status();
}
template<class C>Status value(C& c,MilitarySpatialHandoff2& v){
 field(c,"id",v.id);field(c,"carrier_ref",v.carrier_ref);field(c,"from_binding",v.from_binding);field(c,"to_binding",v.to_binding);
 field(c,"old_execution_ref",v.old_execution_ref);field(c,"new_execution_ref",v.new_execution_ref);
 field(c,"at",v.at);field(c,"entry_or_exit_source_ref",v.entry_or_exit_source_ref);field(c,"chart_ref",v.chart_ref);
 field(c,"portal_id",v.portal_id);field(c,"old_basis_ref",v.old_basis_ref);field(c,"old_cursor",v.old_cursor);
 field(c,"new_anchor",v.new_anchor);field(c,"remaining_route_correspondence",v.remaining_route_correspondence);
 field(c,"cost_and_work_disposition_refs",v.cost_and_work_disposition_refs);field(c,"closed_writer_source_ref",v.closed_writer_source_ref);return c.status();
}
template<class C>Status value(C& c,MilitaryOrientationHalfspace& v){field(c,"normal",v.normal);return field(c,"maximum",v.maximum);}
template<class C>Status value(C& c,MilitaryEncounterOrientation& v){
 field(c,"id",v.id);field(c,"tangent_region",v.tangent_region);field(c,"matrix",v.matrix);field(c,"local_entry",v.local_entry);return c.status();
}
template<class C>Status value(C& c,LockedArenaChartDefinition& v){field(c,"matrix",v.matrix);return field(c,"translation",v.translation);}
template<class C>Status value(C& c,EncounterChartDefinition& v){return field(c,"orientation_choices",v.orientation_choices);}
template<class C>Status value(C& c,MilitarySpatialGeometryDefinition2& v){
 field(c,"id",v.id);field(c,"world_geometry_ref",v.world_geometry_ref);
 field(c,"world_length_unit_ref",v.world_length_unit_ref);field(c,"local_axis_unit_ref",v.local_axis_unit_ref);
 field(c,"domain_polygon",v.domain_polygon);field(c,"portal_rows",v.portal_rows);
 c.object("author_mode",[&](auto& x){Integer tag=Integer(v.author_mode.index());x.field("tag",tag);
  if(x.status().ok&&(tag<0||tag>1))return x.reject("UnknownMilitaryChartAuthorMode");
  if(x.reading()&&x.status().ok){
   if(tag==0){auto ready=prepare_checkpoint_default_values<std::variant_alternative_t<0,decltype(v.author_mode)>>(x);if(!ready.ok)return ready;v.author_mode.template emplace<0>();}
   else{auto ready=prepare_checkpoint_default_values<std::variant_alternative_t<1,decltype(v.author_mode)>>(x);if(!ready.ok)return ready;v.author_mode.template emplace<1>();}
  }
  if(x.status().ok)x.object("value",[&](auto& body){return std::visit([&](auto& row){return value(body,row);},v.author_mode);});return x.status();
 });
 field(c,"actual_definition_source",v.actual_definition_source);return c.status();
}
template<class C>Status value(C& c,MilitaryEntryConnectorDefinition& v){
 field(c,"id",v.id);field(c,"chart_ref",v.chart_ref);field(c,"portal_id",v.portal_id);
 field(c,"path",v.path);field(c,"entered_cell_costs",v.entered_cell_costs);
 field(c,"original_path_source",v.original_path_source);return c.status();
}
}
