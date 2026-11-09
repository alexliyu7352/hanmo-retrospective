// /1,/2 retain untagged SourceRef fields; /3 and /4 use the same ST unions.
// /4 alone removes the legacy advance slot, without inventing scope schema 4.
#pragma once
#include "service_actual_termination_result.hpp"
#include "service_actual_termination_codec_fields.hpp"
#include "resident/domains/actor_foundation/character_origin_codec_detail.hpp"
namespace hanmo::resident::service_termination_codec {
template<class A>Status fee_terms(A&a,ServiceAcceptedFeeTermsRef&v,Integer schema){
 if(schema<3){if(a.reading())v=SourceRef{};auto*x=std::get_if<SourceRef>(&v);return x?source(a,*x):a.reject("StudyFeeTermsNotSelected");}
 Integer tag=v.index();a.field("tag",tag);if(tag<0||tag>1)return a.reject("UnknownServiceFeeTermsTag");if(a.reading())v=tag==0?ServiceAcceptedFeeTermsRef{SourceRef{}}:ServiceAcceptedFeeTermsRef{StudyFactTermsRef{}};
 return tag==0?source(a,std::get<SourceRef>(v)):study_service_ref_codec_fields::terms(a,std::get<StudyFactTermsRef>(v));
}
template<class A>Status charge(A&a,ServiceEarnedChargeRef&v,Integer schema){
 if(schema<3){if(a.reading())v=SourceRef{};auto*x=std::get_if<SourceRef>(&v);return x?source(a,*x):a.reject("StudyChargeNotSelected");}
 Integer tag=v.index();a.field("tag",tag);if(tag<0||tag>1)return a.reject("UnknownServiceChargeTag");if(a.reading())v=tag==0?ServiceEarnedChargeRef{SourceRef{}}:ServiceEarnedChargeRef{StudyLessonCharge{}};
 if(tag==0)return source(a,std::get<SourceRef>(v));return study_service_ref_codec_fields::charge(a,std::get<StudyLessonCharge>(v));
}
template<class A>Status result(A&a,ServiceActualTerminationResult&v,Integer schema){
 auto selected=service_actual_termination_result_contract_schema(schema);if(!selected.ok)return {false,{},selected.error};const auto nested=selected.value;
 a.field("termination_id",v.termination_id);a.object("scope",[&](auto&c){return scope(c,v.scope,nested);});a.object("original_terminal_source",[&](auto&c){return terminal(c,v.original_terminal_source,nested);});a.object("accepted_fee_terms_ref",[&](auto&c){return fee_terms(c,v.accepted_fee_terms_ref,nested);});
 a.object("reason",[&](auto&c){Integer tag=v.reason.index();c.field("tag",tag);if(tag<0||tag>1)return c.reject("UnknownServiceTerminationReason");if(c.reading())v.reason=tag==0?ServiceActualTerminationReason{ServiceVerifiedDeath{}}:ServiceActualTerminationReason{ServiceOwnerConfirmedUnavailability{}};
  if(tag==0){auto&r=std::get<ServiceVerifiedDeath>(v.reason);c.object("death_ref",[&](auto&d){d.field("domain",r.death_ref.domain);d.field("kind",r.death_ref.kind);return d.field("id",r.death_ref.id);});c.field("actor_id",r.actor_id);return c.field("role",r.role);}auto&r=std::get<ServiceOwnerConfirmedUnavailability>(v.reason);return c.object("terminal_ref",[&](auto&d){return terminal(d,r.terminal_ref,nested);});
 });
 a.object("effective_game_time",[&](auto&c){return character_origin_codec::at(c,v.effective_game_time,true);});a.sequence("earned_charge_refs",v.earned_charge_refs,[&](auto&c,auto&r){return charge(c,r,nested);});
 if(schema<4){
  // Preserve the original empty sequence bytes only. Reject the first actual
  // historical entry before copying any of its strings; never discard it.
  std::vector<SourceRef> legacy_empty;
  auto old=a.sequence("advance_allocation_basis_refs",legacy_empty,[](auto&,auto&)->Status{return common_failure("LegacyServiceAdvanceSourcesRequireOriginalOwner",{},ErrorKind::unsupported_capability);});
  if(!old.ok)return old;
 }
 a.sequence("existing_payable_refs",v.existing_payable_refs,[](auto&c,auto&r){return source(c,r);});a.sequence("custody_or_continuity_refs",v.custody_or_continuity_refs,[](auto&c,auto&r){return source(c,r);});
 if(!a.status().ok)return a.status();auto valid=validate_service_actual_termination_result_shape(v,schema);return valid.ok?a.status():a.reject(valid.error.original.code);
}
}
