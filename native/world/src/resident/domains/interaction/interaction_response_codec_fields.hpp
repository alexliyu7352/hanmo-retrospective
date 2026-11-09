#pragma once
#include "interaction_response_sources.hpp"
#include "s3_leaf_life_codec.hpp"
#include "dialogue_command_codec_fields.hpp"
namespace hanmo::resident::interaction_response_codec {
template<class A>Status source(A&a,SourceRef&r){a.field("domain",r.domain);a.field("kind",r.kind);return a.field("id",r.id);}
template<class A>Status optional_source(A&a,const char*key,std::optional<SourceRef>&r){return a.optional(key,r,[](auto&c,auto&v){return source(c,v);});}
template<class A>Status next(A&a,InteractionResponseNext&v){Integer tag=static_cast<Integer>(v.index());a.field("tag",tag);if(!a.status().ok)return a.status();if(tag<0||tag>1)return a.reject("InteractionResponseNextTagUnsupported");if(a.reading()){if(tag==0){if(!prepare_checkpoint_default_values<InteractionResponseNode>(a).ok)return a.status();v=InteractionResponseNode{};}else {if(!prepare_checkpoint_default_values<InteractionResponseTerminal>(a).ok)return a.status();v=InteractionResponseTerminal{};}}if(tag==0){auto&node=std::get<InteractionResponseNode>(v);a.field("node_id",node.node_id);if(node.node_id.empty())return a.reject("InteractionResponseNextNodeRequired");}return a.status();}
template<class A>Status boundary(A&a,InteractionSessionBoundary&v){a.field("session_id",v.session_id);a.field("generation",v.generation);optional_source(a,"current_response_fact_ref",v.current_response_fact_ref);optional_source(a,"last_consumed_response_fact_ref",v.last_consumed_response_fact_ref);if(!a.status().ok)return a.status();if(v.session_id.empty()||v.generation<1)return a.reject("InvalidInteractionSessionBoundary");return a.status();}
template<class A>Status response(A&a,InteractionResponseSource&v,Integer schema){a.field("response_fact_id",v.response_fact_id);a.field("session_id",v.session_id);a.field("session_generation",v.session_generation);a.field("selected_node_id",v.selected_node_id);a.field("selected_choice_id",v.selected_choice_id);a.object("committed_result_ref",[&](auto&c){return source(c,v.committed_result_ref);});a.object("next_node_or_terminal",[&](auto&c){return next(c,v.next_node_or_terminal);});optional_source(a,"tail_consumed_result_ref",v.tail_consumed_result_ref);if(!a.status().ok)return a.status();return validate_interaction_response_source_shape(v,schema);}
template<class A>Status tail(A&a,SelectedResponseTail&v){a.field("session_id",v.session_id);a.field("session_generation",v.session_generation);a.object("response_fact_ref",[&](auto&c){return source(c,v.response_fact_ref);});a.object("next_node_or_terminal",[&](auto&c){return next(c,v.next_node_or_terminal);});if(!a.status().ok)return a.status();if(v.session_id.empty()||v.session_generation<1||v.response_fact_ref.domain!="interaction"||v.response_fact_ref.kind!="response_selected"||v.response_fact_ref.id.empty())return a.reject("InvalidSelectedResponseTail");return a.status();}
template<class A>Status life(A&a,InteractionSessionLifeSource&v){a.object("death_ref",[&](auto&c){return s3_leaf_life_codec::death(c,v.death_ref);});a.field("session_id",v.session_id);dynamic_codec::enumeration(a,"role",v.role,1);a.object("effect",[&](auto&c){return s3_leaf_life_codec::fields(c,v.effect,0);});if(!a.status().ok)return a.status();if(v.session_id.empty()||v.death_ref.domain!="family_life"||v.death_ref.kind!="family_death"||v.death_ref.id.empty())return a.reject("InvalidInteractionSessionLifeSource");return a.status();}
template<class A>Status confirmation(A&a,InteractionActionConfirmationSource&v){
 a.field("committed_result_id",v.committed_result_id);a.field("session_id",v.session_id);a.field("session_generation",v.session_generation);
 a.field("dialogue_id",v.dialogue_id);a.field("action_node_id",v.action_node_id);
 a.object("confirmed_command",[&](auto&c){return dialogue_command_codec::fields(c,v.confirmed_command);});a.field("action_id",v.action_id);
 a.object("confirmation_at",[&](auto&c){return dynamic_codec::exact_moment(c,v.confirmation_at);});
 if(!a.status().ok)return a.status();return validate_interaction_action_confirmation_shape(v);
}
template<class A>Status roots(A&a,InteractionResponseSources&v,Integer selected){
 if(!interaction_response_version(selected)||v.physical_version!=selected)return a.reject("InteractionResponseSelectionMismatch");
 if(!interaction_action_confirmation_schema_version(selected)&&v.action_confirmation_sources.size()!=0)return a.reject("InteractionActionConfirmationExplicitVersionRequired");
 dynamic_codec::table(a,"session_boundaries",v.session_boundaries,[](auto&c,auto&r){return boundary(c,r);});
 dynamic_codec::table(a,"response_sources",v.response_sources,[&](auto&c,auto&r){return response(c,r,interaction_response_source_schema_version(selected));});
 dynamic_codec::table(a,"selected_response_tails",v.selected_response_tails,[](auto&c,auto&r){return tail(c,r);});
 dynamic_codec::table(a,"life_sources",v.life_sources,[](auto&c,auto&r){return life(c,r);});
 if(interaction_action_confirmation_schema_version(selected))return dynamic_codec::table(a,"action_confirmation_sources",v.action_confirmation_sources,[](auto&c,auto&r){return confirmation(c,r);});
 return a.status();
}
}
