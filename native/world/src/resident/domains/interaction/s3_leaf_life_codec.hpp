#pragma once
#include "s3_leaf_life.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/actor_foundation/character_origin_codec_detail.hpp"
namespace hanmo::resident::s3_leaf_life_codec {
template<class A>Status source(A&a,SourceRef&r){a.field("domain",r.domain);a.field("kind",r.kind);return a.field("id",r.id);}
template<class A>Status death(A&a,DeathRef&r){a.field("domain",r.domain);a.field("kind",r.kind);return a.field("id",r.id);}
template<class A>Status refs(A&a,const char*name,std::vector<SourceRef>&r){return a.sequence(name,r,[](auto&c,auto&v){return source(c,v);});}
template<class A>Status optional(A&a,const char*name,std::optional<SourceRef>&r){return a.optional(name,r,[](auto&c,auto&v){return source(c,v);});}
template<class A>Status effect(A&a,S3LeafLifeEffectValue&v){Integer tag=static_cast<Integer>(v.index());a.field("tag",tag);if(!a.status().ok)return a.status();if(tag<0||tag>4)return a.reject("InvalidS3LifeEffectTag");if(a.reading()){switch(tag){case 0:{if(!prepare_checkpoint_default_values<S3SessionStopped>(a).ok)return a.status();v=S3SessionStopped{};}break;case 1:{if(!prepare_checkpoint_default_values<S3InquiryInterrupted>(a).ok)return a.status();v=S3InquiryInterrupted{};}break;case 2:{if(!prepare_checkpoint_default_values<S3AppointmentParticipationStopped>(a).ok)return a.status();v=S3AppointmentParticipationStopped{};}break;case 3:{if(!prepare_checkpoint_default_values<S3MessageResponsibilityContinued>(a).ok)return a.status();v=S3MessageResponsibilityContinued{};}break;default:{if(!prepare_checkpoint_default_values<S3TaskResponsibilityContinued>(a).ok)return a.status();v=S3TaskResponsibilityContinued{};}}}
 return std::visit([&](auto&r)->Status{using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,S3SessionStopped>){optional(a,"original_action_terminal_ref",r.original_action_terminal_ref);return optional(a,"selected_response_tail_ref",r.selected_response_tail_ref);}
 else if constexpr(std::is_same_v<T,S3InquiryInterrupted>)return a.object("original_action_terminal_ref",[&](auto&c){return source(c,r.original_action_terminal_ref);});
 else if constexpr(std::is_same_v<T,S3AppointmentParticipationStopped>){optional(a,"start_fact_ref",r.start_fact_ref);optional(a,"original_action_terminal_ref",r.original_action_terminal_ref);return optional(a,"actual_meeting_end_ref",r.actual_meeting_end_ref);}
 else if constexpr(std::is_same_v<T,S3MessageResponsibilityContinued>){dynamic_codec::enumeration(a,"disposition",r.disposition,2);a.object("actual_channel_result_ref",[&](auto&c){return source(c,r.actual_channel_result_ref);});return optional(a,"successor_authority_ref",r.successor_authority_ref);}
 else {dynamic_codec::enumeration(a,"disposition",r.disposition,2);optional(a,"original_action_terminal_ref",r.original_action_terminal_ref);optional(a,"original_task_terminal_ref",r.original_task_terminal_ref);optional(a,"actual_authority_ref",r.actual_authority_ref);return refs(a,"remaining_responsibility_refs",r.remaining_responsibility_refs);}},v);
}
template<class A>Status fields(A&a,S3LeafLifeEffect&v,std::size_t expected,bool legacy_allowed=false){a.field("result_id",v.result_id);optional(a,"original_terms_ref",v.original_terms_ref);a.object("effect",[&](auto&c){return effect(c,v.effect);});refs(a,"preserved_result_refs",v.preserved_result_refs);a.object("effective_game_time",[&](auto&c){return character_origin_codec::at(c,v.effective_game_time,legacy_allowed);});if(!a.status().ok)return a.status();return validate_s3_leaf_life_effect_shape(v,expected,legacy_allowed);}
}
