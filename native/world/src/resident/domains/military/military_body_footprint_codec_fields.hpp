#pragma once
#include "military_body_footprint.hpp"
#include "military_spatial_point_codec_fields.hpp"
namespace hanmo::resident::military_body_footprint_fields {
template<class C>Status value(C& c,MilitaryBodyDefinitionRef& ref){
 Integer tag=Integer(ref.index());c.field("tag",tag);
 if(!c.status().ok)return c.status();
 if(tag<0||tag>1)return c.reject("UnknownMilitaryFootprintBodyDefinition");
 if(c.reading()){
  if(tag==0)ref.template emplace<MilitaryTroopFootprintRef>();
  else ref.template emplace<MilitaryHullFootprintRef>();
 }
 return c.object("value",[&](auto& body)->Status{
  if(tag==0)return body.field("troop_type_id",std::get<MilitaryTroopFootprintRef>(ref).troop_type_id);
  return body.field("hull_id",std::get<MilitaryHullFootprintRef>(ref).hull_id);
 });
}
template<class C>Status value(C& c,MilitaryBodyFootprintDefinition2& v){
 c.field("id",v.id);c.field("version",v.version);c.field("geometry_ref",v.geometry_ref);
 c.object("original_body_definition",[&](auto& body){return value(body,v.original_body_definition);});
 c.sequence("offsets",v.offsets,[](auto& point,auto& item){
  return military_spatial_point_fields::value(point,item);
 });
 if(!c.status().ok)return c.status();
 if(v.id.empty()||v.version<1||v.geometry_ref.empty())
  return c.reject("MilitaryBodyFootprintDefinitionRequired");
 // Field decoding shares the canonical rational point table. Whole finite
 // polygon and true catalog references are checked by the original read.
 return c.status();
}
}
