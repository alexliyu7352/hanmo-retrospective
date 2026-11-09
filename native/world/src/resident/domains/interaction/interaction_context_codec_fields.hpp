#pragma once
#include "interaction_context.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident::interaction_context_codec {
template<class A>Status fields(A&a,InteractionContext&v){
 if(v.input_error)return a.reject("InvalidInteractionContext");
 a.field("place_id",v.place_id);a.field("provider_actor_id",v.provider_actor_id);a.field("role_id",v.role_id);
 a.optional("kind",v.kind,[](auto&c,auto&x){return c.field("value",x);});
 a.optional("encounter_id",v.encounter_id,[](auto&c,auto&x){return c.field("value",x);});
 return a.sequence("field_order",v.field_order,[](auto&c,auto&x){return c.field("value",x);});
}
}
