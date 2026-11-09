#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_local_motion.hpp"
namespace hanmo::resident::military_local_body_codec {
template<class C>Status value(C& c,MilitaryLocalUnitRef& v){return c.field("unit_id",v.unit_id);}
template<class C>Status value(C& c,MilitaryLocalVesselRef& v){return c.field("vessel_id",v.vessel_id);}
template<class C>Status value(C& c,MilitaryLocalBodyRef& v){Integer tag=Integer(v.index());c.field("tag",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("UnknownMilitaryLocalBodyVariant");if(c.reading()&&c.status().ok){if(tag==0){auto ready=prepare_checkpoint_default_values<MilitaryLocalUnitRef>(c);if(!ready.ok)return ready;v.template emplace<0>();}else{auto ready=prepare_checkpoint_default_values<MilitaryLocalVesselRef>(c);if(!ready.ok)return ready;v.template emplace<1>();}}if(c.status().ok)c.object("value",[&](auto& x){return std::visit([&](auto& item){return value(x,item);},v);});return c.status();}
}
