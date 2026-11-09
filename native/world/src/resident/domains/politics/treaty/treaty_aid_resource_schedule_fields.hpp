#pragma once
#include "treaty_aid_resource_schedule.hpp"
#include "treaty_aid_schedule_codec_fields.hpp"
namespace hanmo::resident::treaty_aid_resource_schedule_fields {
template<class C>Status fields(C& c,TreatyAidSchedule& r){
 Integer treaty_source_schema=2;c.field("treaty_source_schema",treaty_source_schema);
 if(!c.status().ok)return c.status();if(treaty_source_schema!=2)return c.reject("TreatyAidResourceScheduleTreatySchemaRequired");
 return treaty_aid_schedule_codec_fields::fields(c,r);
}
}
