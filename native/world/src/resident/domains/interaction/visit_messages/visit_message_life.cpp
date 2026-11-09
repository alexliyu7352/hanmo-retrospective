#include "visit_message_life.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "visit_report_custody.hpp"
#include <charconv>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void reject(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class T,class F>Outcome<T>guard(F fn){try{return {true,fn(),{}};}catch(Failure&failure){return {false,{},std::move(failure.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const DeathRef&a,const DeathRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}

SourceRef death_ref(const DeathRef&d){return {d.domain,d.kind,d.id};}
std::size_t position(const CommonStore&common,const SourceRef&ref){const auto*h=common.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)reject("VisitMessageLifeActualSourceMissing",ref.id);return take(read_actual_common_fact_position(common,*h));}
Integer serial(const CommonStore&common,const StableId&id){constexpr std::string_view prefix="visit_message_life:";if(id.compare(0,prefix.size(),prefix)!=0)reject("VisitMessageLifeResultIdentityMismatch",id);const auto*first=id.data()+prefix.size();const auto*last=id.data()+id.size();Integer n=0;auto parsed=std::from_chars(first,last,n);if(first==last||parsed.ec!=std::errc{}||parsed.ptr!=last||n<1||n>=common.state().next_instance_id||*first=='0')reject("VisitMessageLifeResultIdentityMismatch",id);return n;}
void selected(const VisitMessageLifeContext&x){if(x.state.version<2||x.state.version>3||x.policies.version!=x.state.version||!x.life_policy.enabled||x.life_policy.selected_version!=4)reject("VisitMessageLife4SourcesRequired",{},ErrorKind::unsupported_capability);take(validate_visit_messages(x.common,x.state,x.policies,x.interaction,&x.clock));}
struct OriginalMessage {const StableId*id=nullptr;const VisitMessagePolicy*rules=nullptr;Integer tick=0;const WorldMoment*at=nullptr;bool report=false;};
SourceRef original_ref(const OriginalMessage&p){return {"visit_messages",p.report?visit_report_deposited_kind:visit_message_posted_kind,*p.id};}
OriginalMessage original(const VisitMessageLifeContext&x,const StableId&id){
 OriginalMessage out;StableId const*policy_id=nullptr;if(const auto*p=x.state.posted.find(id)){out={&p->fact_id,nullptr,p->contact.tick,nullptr,false};policy_id=&p->policy_id;}
 if(const auto*p=x.state.report_deposits.find(id)){if(out.id)reject("AmbiguousVisitMessageLifeOriginal",id);take(read_original_visit_report_custody_moment(x.common,x.state,x.policies,x.interactions,x.interaction,{"visit_messages",visit_report_deposited_kind,id},3));out={&p->fact_id,nullptr,p->contact.tick,&p->deposited_at,true};policy_id=&p->policy_id;}
 if(!out.id)reject("VisitMessageLifeOriginalMissing",id);for(const auto&v:x.policies.policies)if(v.id==*policy_id){out.rules=&v;break;}if(!out.rules)reject("VisitMessageLifePolicyMissing",id);return out;
}
const StableId&actor(const OriginalMessage&p,VisitMessageLifeRole role){if(role==VisitMessageLifeRole::sender)return p.rules->sender_actor_id;if(role==VisitMessageLifeRole::recipient)return p.rules->recipient_actor_id;if(role==VisitMessageLifeRole::custodian)return p.rules->custodian_actor_id;reject("VisitMessageLifeRoleMismatch",*p.id);}
struct Terminal {const StableId*id=nullptr;bool received=false;};
Terminal terminal(const VisitMessageState&state,const OriginalMessage&p){Terminal out;auto accept=[&](const StableId&id,bool received){if(out.id)reject("DuplicateVisitMessageTerminal",*p.id);out={&id,received};};if(p.report){for(const auto&row:state.report_pickups.rows())if(row.second.deposit_ref==*p.id)accept(row.second.fact_id,true);for(const auto&row:state.report_withdrawals.rows())if(row.second.deposit_ref==*p.id)accept(row.second.fact_id,false);}else{for(const auto&row:state.received.rows())if(row.second.message_fact_id==*p.id)accept(row.second.fact_id,true);for(const auto&row:state.withdrawn.rows())if(row.second.message_fact_id==*p.id)accept(row.second.fact_id,false);}return out;}
SourceRef terminal_ref(const OriginalMessage&p,const Terminal&t){return {"visit_messages",p.report?(t.received?visit_report_picked_up_kind:visit_report_withdrawn_kind):(t.received?visit_message_received_kind:visit_message_withdrawn_kind),*t.id};}
const VisitMessageLifeSource*once(const VisitMessageState&state,const DeathRef&death,const StableId&message,VisitMessageLifeRole role){const VisitMessageLifeSource*out=nullptr;for(const auto&row:state.life_sources.rows())if(same(row.second.death_ref,death)&&row.second.message_fact_id==message&&row.second.role==role){if(out)reject("DuplicateVisitMessageLifeConsumption",message);out=&row.second;}return out;}
VerifiedDeathView death(const VisitMessageLifeContext&x,const DeathRef&ref){auto actual=take(read_verified_death4(x.common,x.deaths,x.causes,ref,take(x.clock.read_current_moment())));if(actual.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||actual.recorded_at.kind!=ActorFoundationAt::Kind::exact_moment)reject("VisitMessageExactDeathRequired",ref.id);position(x.common,death_ref(ref));return actual;}
// The original leaf has one actual custody result. No invented return is used
// for death, and no one inherits the private recipient's address.
bool affected(const VisitMessageLifeContext&x,const OriginalMessage&p,const VerifiedDeathView&d,VisitMessageLifeRole role){
 if(actor(p,role)!=d.actor_id)return false;
 const auto opening=position(x.common,original_ref(p)),cut=position(x.common,death_ref(d.death_ref));const auto t=terminal(x.state,p);
 if(t.id&&position(x.common,terminal_ref(p,t))<cut)return false; // already settled history is untouched
 if(opening>=cut)reject("VisitMessagePostDeathResponsibilitySourceRequired",*p.id,ErrorKind::unsupported_capability);
 if(p.at){if(take(compare_world_moments(*p.at,d.effective_at.exact_moment))>0)reject("VisitMessageAcceptedAfterEffectiveDeath",*p.id);}
 else if(p.tick>d.effective_at.exact_moment.half_day_tick||(p.tick==d.effective_at.exact_moment.half_day_tick&&take(compare_world_moments(d.recorded_at.exact_moment,d.effective_at.exact_moment))!=0))reject("VisitMessageOriginalAcceptanceTimeRequired",*p.id,ErrorKind::unsupported_capability);
 // A same-M immediate Death uses the actual Common append order above. A
 // delayed Death never manufactures the old acceptance's fractional time.
 if(t.id&&(role==VisitMessageLifeRole::custodian||(t.received&&role==VisitMessageLifeRole::recipient)||(!t.received&&role==VisitMessageLifeRole::sender)))reject("VisitMessageTerminalAfterResponsibleDeath",*t.id);
 return true;
}
const VisitMessageLifeSource&read(const VisitMessageLifeContext&x,const StableId&id){
 const auto*v=x.state.life_sources.find(id);if(!v||v->effect.result_id!=id)reject("VisitMessageLifeSourceMissing",id);serial(x.common,id);take(validate_s3_leaf_life_effect_shape(v->effect,3));
 const auto p=original(x,v->message_fact_id);auto d=death(x,v->death_ref);if(!affected(x,p,d,v->role)||once(x.state,v->death_ref,*p.id,v->role)!=v)reject("VisitMessageLifeNotActual",id);
 const auto ref=original_ref(p);const auto*effect=std::get_if<S3MessageResponsibilityContinued>(&v->effect.effect);
 if(!effect||!v->effect.original_terms_ref||!same(*v->effect.original_terms_ref,ref)||!same(effect->actual_channel_result_ref,ref)||effect->successor_authority_ref||effect->disposition!=(v->role==VisitMessageLifeRole::custodian?S3MessageLifeDisposition::awaiting_lawful_successor:S3MessageLifeDisposition::held)||v->effect.preserved_result_refs.size()!=1||!same(v->effect.preserved_result_refs.front(),ref)||take(compare_world_moments(v->effect.effective_game_time.exact_moment,d.effective_at.exact_moment))!=0)reject("VisitMessageLifeChannelResultMismatch",id);
 return *v;
}
}
Outcome<const VisitMessageLifeSource*>read_visit_message_life_source(const VisitMessageLifeContext&x,const StableId&id){return guard<const VisitMessageLifeSource*>([&]{selected(x);return &read(x,id);});}
Status validate_visit_message_life_sources(const VisitMessageLifeContext&x){return guard<Done>([&]{selected(x);if(x.state.life_sources.size()>393216)reject("VisitMessageLifeHistoryLimit");Integer previous=0;for(const auto&row:x.state.life_sources.rows()){const auto n=serial(x.common,row.first);if(n<=previous)reject("VisitMessageLifeOrderMismatch",row.first);previous=n;read(x,row.first);}
 for(const auto&row:x.deaths.deaths.rows()){auto d=death(x,{"family_life","family_death",row.first});auto check=[&](const StableId&id){const auto p=original(x,id);for(auto role:{VisitMessageLifeRole::sender,VisitMessageLifeRole::recipient,VisitMessageLifeRole::custodian})if(affected(x,p,d,role)&&!once(x.state,d.death_ref,id,role))reject("VisitMessageDeathConsumptionMissing",id);};for(const auto&message:x.state.posted.rows())check(message.first);for(const auto&message:x.state.report_deposits.rows())check(message.first);}
 return Done{};
});}
Status apply_visit_message_death_in_candidate(CommonStore&common,VisitMessageState&state,const VisitMessageLifeContext&x,const DeathRef&ref){return guard<Done>([&]{if(&common!=&x.common||&state!=&x.state)reject("VisitMessageLifeCandidateMismatch");selected(x);auto d=death(x,ref);for(const auto&row:state.life_sources.rows())read(x,row.first);
 auto consume=[&](const StableId&message){const auto p=original(x,message);for(auto role:{VisitMessageLifeRole::sender,VisitMessageLifeRole::recipient,VisitMessageLifeRole::custodian}){if(!affected(x,p,d,role))continue;if(const auto*old=once(state,ref,message,role)){read(x,old->effect.result_id);continue;}if(state.life_sources.size()>=393216)reject("VisitMessageLifeHistoryLimit");const auto id=take(common.next_id("visit_message_life"));const auto original_source=original_ref(p);VisitMessageLifeSource result;result.death_ref=ref;result.message_fact_id=message;result.role=role;result.effect.result_id=id;result.effect.original_terms_ref=original_source;result.effect.effect=S3MessageResponsibilityContinued{role==VisitMessageLifeRole::custodian?S3MessageLifeDisposition::awaiting_lawful_successor:S3MessageLifeDisposition::held,original_source,{}};result.effect.preserved_result_refs.push_back(original_source);result.effect.effective_game_time=d.effective_at;if(!state.life_sources.insert(id,std::move(result)))reject("DuplicateVisitMessageLifeResult",id);read(x,id);}};
 for(const auto&row:state.posted.rows())consume(row.first);for(const auto&row:state.report_deposits.rows())consume(row.first);
 return Done{};
});}
Status validate_visit_message_life_command(const VisitMessageLifeContext&x,const VisitMessageCommand&command){return guard<Done>([&]{take(validate_visit_message_life_sources(x));if(std::holds_alternative<LeaveVisitMessage>(command))return Done{};const bool receiving=std::holds_alternative<ReceiveVisitMessage>(command);const auto&id=receiving?std::get<ReceiveVisitMessage>(command).message_fact_id:std::get<WithdrawVisitMessage>(command).message_fact_id;for(const auto&row:x.state.life_sources.rows())if(row.second.message_fact_id==id&&(row.second.role==VisitMessageLifeRole::custodian||(receiving&&row.second.role==VisitMessageLifeRole::recipient)||(!receiving&&row.second.role==VisitMessageLifeRole::sender)))reject("VisitMessageNotKnown",{},ErrorKind::rule);return Done{};});}
}
