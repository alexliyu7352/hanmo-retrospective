#pragma once
#include "dialogue_command.hpp"
#include "interaction_context_codec_fields.hpp"
namespace hanmo::resident::dialogue_command_codec {
// The original visit_command field sequence, now shared by its original typed
// codec and the explicitly selected immutable confirmation appendix.
template<class A>Status fields(A&a,DialogueCommand&v){
 a.field("kind",v.kind);a.field("offer_id",v.offer_id);a.field("shop_id",v.shop_id);
 a.field("service_id",v.service_id);a.field("target_actor_id",v.target_actor_id);
 a.field("activity",v.activity);a.field("skill_id",v.skill_id);a.field("place_id",v.place_id);
 a.optional("context",v.context,[](auto&c,auto&context){return interaction_context_codec::fields(c,context);});
 return a.sequence("field_order",v.field_order,[](auto&c,auto&key){return c.field("key",key);});
}
}
