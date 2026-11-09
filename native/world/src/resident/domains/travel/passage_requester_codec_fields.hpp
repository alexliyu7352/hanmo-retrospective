// Original PassageRequester union. Shared fields do not translate its tags.
#pragma once
#include "passage_model.hpp"
#include "spatial_carrier_ref_codec_fields.hpp"
namespace hanmo::resident::passage_requester_codec_fields {
using spatial_carrier_ref_codec_fields::scalar;
template<class C>Status branch_fields(C& c,ActorPassageRequester& v){return scalar(c,"acting_actor_id",v.acting_actor_id);}
template<class C>Status branch_fields(C& c,HouseholdPolicyPassageRequester& v){scalar(c,"household_id",v.household_id);scalar(c,"accepted_household_policy_ref",v.accepted_household_policy_ref);scalar(c,"policy_revision",v.policy_revision);scalar(c,"purchase_plan_ref",v.purchase_plan_ref);c.object("purchase_ref",[&](auto& a){return a.optional("optional",v.purchase_ref,[](auto& x,auto& id){return x.field("value",id);});});return scalar(c,"phase",v.phase);}
template<class C>Status fields(C& c,PassageRequester& v){Integer tag=Integer(v.index());c.field("variant",tag);if(c.status().ok&&(tag<0||tag>1))return c.reject("InvalidMovementVariant");if(c.reading()&&c.status().ok){if(tag==0)v=ActorPassageRequester{};else v=HouseholdPolicyPassageRequester{};}if(c.status().ok)return std::visit([&](auto& b){return branch_fields(c,b);},v);return c.status();}
}
