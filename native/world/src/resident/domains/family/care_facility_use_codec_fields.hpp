// Exactly FH:102's ten fields; owner tag 0 is fixed by FH:360.
#pragma once
#include "care_facility_use.hpp"
#include "care_requirement_codec_fields.hpp"
#include "resident/facility_claims/facility_claims_codec_fields.hpp"
namespace hanmo::resident::care_facility_codec {
template<class C>Status arrangement(C& c,CareArrangementRef& v){Integer tag=0;c.field("tag",tag);if(c.status().ok&&tag!=0)return c.reject("CareFacilityOwnerTagRequired");return care_requirement_codec::text(c,"accepted_arrangement_id",v.accepted_arrangement_id);}
template<class C>Status use_mode(C& c,CareFacilityUseMode& v){Integer tag=static_cast<Integer>(v.index());c.field("tag",tag);if(!c.status().ok)return c.status();if(tag<0||tag>1)return c.reject("InvalidCareFacilityUseMode");if(c.reading())v=tag==0?CareFacilityUseMode{CareFacilityOwnClaim{}}:CareFacilityUseMode{CareFacilityBorrowedPhysicalHolder{}};
 if(tag==0){auto& x=std::get<CareFacilityOwnClaim>(v);care_requirement_codec::text(c,"original_claim_id",x.original_claim_id);return c.object("holder",[&](auto& a){return facility_fields::holder(a,x.holder);});}
 auto& x=std::get<CareFacilityBorrowedPhysicalHolder>(v);c.object("physical_holder_source_ref",[&](auto& a){return care_requirement_codec::source(a,x.physical_holder_source_ref);});c.field("physical_holder_source_schema",x.physical_holder_source_schema);care_requirement_codec::text(c,"placement_claim_id",x.placement_claim_id);c.object("authorized_use_source_ref",[&](auto& a){return care_requirement_codec::source(a,x.authorized_use_source_ref);});c.field("authorized_use_source_schema",x.authorized_use_source_schema);return c.status().ok&&(x.physical_holder_source_schema<1||x.authorized_use_source_schema<1)?c.reject("InvalidCareFacilityBorrowedSchema"):c.status();}
template<class C>Status binding(C& c,CareFacilityUseBinding& v,Integer version){
 if(version!=5&&version!=6)return c.reject("CareFacilityHistoryVersion5Required");
 care_requirement_codec::text(c,"binding_id",v.binding_id);
 c.object("owner_business_ref",[&](auto& x){return arrangement(x,v.owner_business_ref);});
 care_requirement_codec::text(c,"accepted_terms_ref",v.accepted_terms_ref);
 c.object("usage_rule_ref",[&](auto& x){return care_requirement_codec::rule(x,v.usage_rule_ref);});
 c.object("slot_ref",[&](auto& x){return facility_fields::slot(x,v.slot_ref);});
 c.object("mode",[&](auto& x){return use_mode(x,v.mode);});
 c.object("created_source_ref",[&](auto& x){return care_requirement_codec::source(x,v.created_source_ref);});
 c.field("created_source_schema",v.created_source_schema);
 c.object("created_at",[&](auto& x){return family_life4_codec::moment(x,v.created_at);});
 c.sequence("field_order",v.field_order,[](auto& x,auto& name){return care_requirement_codec::text(x,"value",name);});
 if(!c.status().ok)return c.status();auto valid=validate_care_facility_use_shape(v,version);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
