#pragma once
#include "treaty_resource_termination.hpp"
#include "treaty_termination_codec_fields.hpp"
namespace hanmo::resident::treaty_resource_termination_fields {
template<class C>Status fields(C& c,TreatyTerminationSource& r){
 Integer activation_source_schema=2;c.field("activation_source_schema",activation_source_schema);
 if(!c.status().ok)return c.status();if(activation_source_schema!=2)return c.reject("TreatyResourceTerminationActivationSchemaRequired");
 return treaty_termination_codec_fields::fields(c,r);
}
}
