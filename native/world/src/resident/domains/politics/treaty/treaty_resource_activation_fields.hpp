#pragma once
#include "treaty_resource_activation.hpp"
#include "treaty_activation_codec_fields.hpp"
namespace hanmo::resident::treaty_resource_activation_fields {
template<class C>Status fields(C& c,TreatyActivationSource& r){
 Integer signature_source_schema=2;c.field("signature_source_schema",signature_source_schema);
 if(!c.status().ok)return c.status();if(signature_source_schema!=2)return c.reject("TreatyResourceActivationSignatureSchemaRequired");
 return treaty_activation_codec_fields::fields(c,r);
}
}
