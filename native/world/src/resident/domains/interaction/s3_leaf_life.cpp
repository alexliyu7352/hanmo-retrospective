#include "s3_leaf_life.hpp"
namespace hanmo::resident {namespace {
bool valid(const SourceRef&r){return !r.domain.empty()&&!r.kind.empty()&&!r.id.empty();}
bool valid(const std::optional<SourceRef>&r){return !r||valid(*r);}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool refs(const std::vector<SourceRef>&r){for(std::size_t i=0;i<r.size();++i){if(!valid(r[i]))return false;for(std::size_t j=0;j<i;++j)if(same(r[i],r[j]))return false;}return true;}
}
Status validate_s3_leaf_life_effect_shape(const S3LeafLifeEffect&e,std::size_t expected,bool legacy){
 if(e.result_id.empty()||expected>4||e.effect.index()!=expected||!valid(e.original_terms_ref)||!refs(e.preserved_result_refs))return common_failure("S3LeafLifeEffectIdentityMismatch",e.result_id,ErrorKind::internal);
 if(e.effective_game_time.kind==ActorFoundationAt::Kind::legacy_tick){if(!legacy||e.effective_game_time.legacy_tick<0)return common_failure("S3LifeActualExactTimeRequired",e.result_id,ErrorKind::unsupported_capability);}
 else {if(e.effective_game_time.kind!=ActorFoundationAt::Kind::exact_moment||e.effective_game_time.legacy_tick!=0)return common_failure("S3LifeInvalidTimePrecision",e.result_id,ErrorKind::internal);auto at=compare_world_moments(e.effective_game_time.exact_moment,e.effective_game_time.exact_moment);if(!at.ok)return {false,{},at.error};}
 bool shape=std::visit([](const auto&v){using T=std::decay_t<decltype(v)>;
 if constexpr(std::is_same_v<T,S3SessionStopped>)return valid(v.original_action_terminal_ref)&&valid(v.selected_response_tail_ref);
 else if constexpr(std::is_same_v<T,S3InquiryInterrupted>)return valid(v.original_action_terminal_ref);
 else if constexpr(std::is_same_v<T,S3AppointmentParticipationStopped>)return valid(v.start_fact_ref)&&valid(v.original_action_terminal_ref)&&valid(v.actual_meeting_end_ref)&&(!v.original_action_terminal_ref||v.start_fact_ref);
 else if constexpr(std::is_same_v<T,S3MessageResponsibilityContinued>)return static_cast<Integer>(v.disposition)>=0&&static_cast<Integer>(v.disposition)<=2&&valid(v.actual_channel_result_ref)&&valid(v.successor_authority_ref);
 else return static_cast<Integer>(v.disposition)>=0&&static_cast<Integer>(v.disposition)<=2&&valid(v.original_action_terminal_ref)&&valid(v.original_task_terminal_ref)&&valid(v.actual_authority_ref)&&refs(v.remaining_responsibility_refs)&&(v.disposition!=S3TaskLifeDisposition::continued_under_original_terms||v.actual_authority_ref.has_value());},e.effect);
 return shape?common_success():common_failure("S3LeafLifeOriginalResultMissing",e.result_id,ErrorKind::internal);
}
}
