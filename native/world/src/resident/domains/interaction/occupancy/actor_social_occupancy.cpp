#include "actor_social_occupancy.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "party_acceptance_history.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {namespace {
constexpr const char* changed="actor_social_occupancy_changed";
constexpr const char* initial_kind="actor_social_occupancy_initial";
struct Failure {RuleError error;};
[[noreturn]] void reject(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
template<class T,class F>Outcome<T>guard(F f){try{return {true,f(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same(const ActorSocialOccupancy&a,const ActorSocialOccupancy&b){return a.interaction_id==b.interaction_id&&a.party_id==b.party_id;}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
SourceRef initial_source(const StableId&id){return {"interaction",initial_kind,id};}
SourceRef transition_source(const StableId&id){return {"interaction",changed,id};}
const ActorSocialOccupancySources& selected(const InteractionState&i){if(!i.social_occupancy||!actor_social_occupancy_version(i.social_occupancy->version))reject("ActorSocialOccupancySourcesRequired");return *i.social_occupancy;}
std::size_t position(const CommonStore&c,const SourceRef&ref){const auto*h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)reject("SocialOccupancySourceHeaderMissing",ref.id);return take(read_actual_common_fact_position(c,*h));}
Integer original_serial(const CommonStore&c,const StableId&id){
 // Only this owner's actual next_id("social_occupancy") result is accepted.
 constexpr std::string_view prefix="social_occupancy:";const std::string_view full=id;if(full.size()<=prefix.size()||full.substr(0,prefix.size())!=prefix)reject("SocialOccupancyAllocationSourceMismatch",id);const auto tail=full.substr(prefix.size());if(tail.front()=='0')reject("SocialOccupancyAllocationSourceMismatch",id);Integer value=0;auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),value);if(parsed.ec!=std::errc{}||parsed.ptr!=tail.data()+tail.size()||value<1||value>=c.state().next_instance_id)reject("SocialOccupancyAllocationSourceMismatch",id);return value;
}
void shape(const CommonStore&c,const ActorSocialOccupancyTransition&t){
 original_serial(c,t.id);if(t.id.empty()||t.actor_id.empty()||!c.actor(t.actor_id)||!c.actor(t.initiator_actor_id)||t.original_context_id.empty()||same(t.before,t.after))reject("InvalidSocialOccupancyTransition",t.id);
 for(const auto*id:{&t.before.interaction_id,&t.before.party_id,&t.after.interaction_id,&t.after.party_id})if(*id&&(*id)->empty())reject("InvalidSocialOccupancyReference",t.id);
 const bool party_same=t.before.party_id==t.after.party_id,session_same=t.before.interaction_id==t.after.interaction_id;
 switch(t.cause){
 case ActorSocialOccupancyCause::dialogue_open:case ActorSocialOccupancyCause::born_dialogue_open:case ActorSocialOccupancyCause::known_dialogue_open:
  if(!party_same||t.before.interaction_id||t.after.interaction_id!=std::optional<StableId>{t.original_context_id}||t.initiator_actor_id!=t.actor_id)reject("SocialOccupancyOpeningMismatch",t.id);break;
 case ActorSocialOccupancyCause::dialogue_replaced:
  if(!party_same||!t.before.interaction_id||t.after.interaction_id!=std::optional<StableId>{t.original_context_id}||t.initiator_actor_id!=t.actor_id)reject("SocialOccupancyReplacementMismatch",t.id);break;
 case ActorSocialOccupancyCause::end_interaction:case ActorSocialOccupancyCause::encounter_leave:case ActorSocialOccupancyCause::dialogue_end:case ActorSocialOccupancyCause::completed_dialogue_unavailable:case ActorSocialOccupancyCause::local_move:case ActorSocialOccupancyCause::personal_battle_started:case ActorSocialOccupancyCause::session_participant_died:
  if(!party_same||t.before.interaction_id!=std::optional<StableId>{t.original_context_id}||t.after.interaction_id)reject("SocialOccupancyClosingMismatch",t.id);break;
 case ActorSocialOccupancyCause::party_invitation_accepted:case ActorSocialOccupancyCause::party_recruited:
  if(!session_same||t.before.party_id||t.after.party_id!=std::optional<StableId>{t.original_context_id})reject("SocialOccupancyPartyJoinMismatch",t.id);break;
 case ActorSocialOccupancyCause::party_left:case ActorSocialOccupancyCause::party_dismissed:
  if(!session_same||t.before.party_id!=std::optional<StableId>{t.original_context_id}||t.after.party_id)reject("SocialOccupancyPartyLeaveMismatch",t.id);break;
 default:reject("UnsupportedSocialOccupancyCause",t.id);
 }
 if(t.session_opening){const auto&o=*t.session_opening;
  if(!actor_social_occupancy_opening_cause(t.cause)||o.session_id!=t.original_context_id||o.actor_id!=t.actor_id||t.after.interaction_id!=std::optional<StableId>{o.session_id}||o.dialogue_id.empty()||o.context.input_error||o.context.provider_actor_id.empty()||o.context.provider_actor_id==o.actor_id||!c.actor(o.context.provider_actor_id))reject("InvalidInteractionSessionOpening",t.id);
 }
 if(t.party_acceptance){const auto&a=*t.party_acceptance;const bool invited=t.cause==ActorSocialOccupancyCause::party_invitation_accepted;
  if((!invited&&t.cause!=ActorSocialOccupancyCause::party_recruited)||!t.after.party_id||a.leader_actor_id.empty()||a.leader_actor_id==t.actor_id||!c.actor(a.leader_actor_id)||a.role_id.empty()||a.cost_per_tick<0||a.context.input_error||a.context.kind||a.context.encounter_id||a.context.role_id!=a.role_id||a.context.place_id.empty()||a.context.provider_actor_id!=(invited?a.leader_actor_id:t.actor_id)||t.initiator_actor_id!=(invited?t.actor_id:a.leader_actor_id)||static_cast<Integer>(a.kind)<0||static_cast<Integer>(a.kind)>2)reject("InvalidOriginalPartyAcceptance",t.id);
  if(invited!=(a.kind==PartyAcceptanceKind::authored_invitation))reject("PartyAcceptanceKindMismatch",t.id);
  const bool authored=a.kind!=PartyAcceptanceKind::legacy_recruitment;if(authored!=a.authored_node.has_value()||authored!=a.authored_declaration_id.has_value())reject("PartyAcceptanceAuthoredSourceMissing",t.id);
  if(a.authored_node){const auto&n=*a.authored_node;if(n.session_actor_id!=t.initiator_actor_id||n.session_id.empty()||n.dialogue_id.empty()||n.node_id.empty()||a.authored_declaration_id->empty()||(a.kind==PartyAcceptanceKind::declared_recruitment?n.node_kind!="say":(n.node_kind!="say"&&n.node_kind!="choice"&&n.node_kind!="action")))reject("PartyAcceptanceAuthoredNodeMismatch",t.id);}
 }
 if(t.cause==ActorSocialOccupancyCause::session_participant_died&&(!t.actual_cause_source||t.actual_cause_source->domain!="family_life"||t.actual_cause_source->kind!="family_death"))reject("InteractionSessionOriginalDeathRequired",t.id);
 if(t.actual_cause_source){if(position(c,*t.actual_cause_source)>=position(c,transition_source(t.id)))reject("SocialOccupancyCauseAfterEffect",t.id);}
}
struct Folded {ActorSocialOccupancy value;SourceRef head;WorldMoment at;};
// All original history is checked. The selected head captures the real same-M
// insertion boundary; later same-M changes do not retroactively change it.
Folded fold(const CommonStore&c,const InteractionState&i,const ActorSocialOccupancyInitialRead&original,const StableId&actor,const WorldMoment&at,const WorldMoment&now,const SourceRef*requested){
 const auto&s=selected(i);for(const auto&row:s.transitions.rows()){if(s.version<3&&row.second.cause==ActorSocialOccupancyCause::session_participant_died)reject("UnselectedInteractionSessionLifeCause",row.first);if(s.version<3&&row.second.session_opening)reject("UnselectedInteractionSessionOpening",row.first);if(s.version==3&&actor_social_occupancy_opening_cause(row.second.cause)!=bool(row.second.session_opening))reject("InteractionSessionOpeningEvidenceRequired",row.first);if(s.version==1&&row.second.party_acceptance)reject("UnselectedPartyAcceptanceHistory",row.first);if(s.version>=2){const bool member=(row.second.cause==ActorSocialOccupancyCause::party_recruited&&row.second.actor_id!=row.second.initiator_actor_id)||(row.second.cause==ActorSocialOccupancyCause::party_invitation_accepted&&row.second.actor_id==row.second.initiator_actor_id);if(member!=bool(row.second.party_acceptance))reject("OriginalPartyAcceptanceEvidenceRequired",row.first);}if(row.first!=row.second.id||!c.actor(row.second.actor_id))reject("SocialOccupancyTransitionKeyMismatch",row.first);position(c,transition_source(row.first));}if(!original.bound_to(c))reject("SocialOccupancyInitialCandidateMismatch",actor);if(!c.actor(actor)||compare(at,now)>0)reject("SocialOccupancyObservationInvalid",actor);
 auto base=take(original.read_actor_initial_social_occupancy(actor));if(base.actual_origin.source_schema<1||base.actual_origin.source_ref.domain.empty()||base.actual_origin.source_ref.kind.empty()||base.actual_origin.source_ref.id.empty()||compare(base.created_at,at)>0)reject("SocialOccupancyInitialSourceRequired",actor);
 Folded current{std::move(base.value),initial_source(actor),base.created_at};Folded observed;Integer previous_serial=0;bool found=requested&&same(*requested,current.head);if(found)observed=current;
 for(const auto&id:c.state().facts.order()){
  const auto*h=c.state().facts.find(id);if(!h)reject("SocialOccupancyCommonOrderInvalid",id);
  if(h->domain!="interaction"||h->kind!=changed)continue;
  const auto*t=s.transitions.find(id);if(!t||t->id!=id)reject("SocialOccupancyTransitionMissing",id);const auto serial=original_serial(c,id);if(serial<=previous_serial)reject("SocialOccupancyOriginalOrderMismatch",id);previous_serial=serial;if(t->actor_id!=actor)continue;
  shape(c,*t);if(!same(t->previous_source,current.head)||!same(t->before,current.value)||compare(t->occurred_at,current.at)<0||compare(t->occurred_at,now)>0)reject("SocialOccupancyHistoryMismatch",id);
  if(requested&&found&&compare(t->occurred_at,at)<0)reject("SocialOccupancyHeadMissesEarlierChange",id);
  current={t->after,transition_source(id),t->occurred_at};
  if(requested&&same(*requested,current.head)){if(found||compare(t->occurred_at,at)>0)reject("SocialOccupancyHeadInvalid",id);found=true;observed=current;}
 }
 if(requested){if(!found)reject("SocialOccupancyHeadMissing",requested->id);auto actual=take(read_current_actor_social_occupancy(c,i,actor));if(!same(current.value,actual))reject("SocialOccupancyCurrentCoverageMismatch",actor);return observed;}return current;
}
}
Outcome<ActorSocialOccupancy>read_current_actor_social_occupancy(const CommonStore&c,const InteractionState&i,const StableId&actor){return guard<ActorSocialOccupancy>([&]{
 if(!c.actor(actor))reject("UnknownActor",actor);ActorSocialOccupancy out;
 for(const auto&s:i.interactions)if(s.actor_id==actor){if(out.interaction_id)reject("DuplicateActorInteraction",actor);if(s.id.empty())reject("InvalidActorInteraction",actor);out.interaction_id=s.id;}
 for(const auto&row:c.state().parties.rows()){const auto&p=row.second;const bool member=std::find(p.members.begin(),p.members.end(),actor)!=p.members.end();if(!member&&p.leader_id!=actor)continue;if(!member||out.party_id||row.first!=p.id)reject("InvalidActorPartyMembership",actor);out.party_id=p.id;}
 return out;
});}
Outcome<ActorSocialOccupancyView>NativeActorSocialOccupancyRead::read_actual_actor_social_occupancy(const StableId&actor,const WorldMoment&at)const{return guard<ActorSocialOccupancyView>([&]{
 const auto now=take(clock_.read_current_moment());if(compare(at,now))reject("SocialOccupancyActualMomentRequired",actor);auto history=fold(common_,interaction_,initial_,actor,at,now,nullptr);auto actual=take(read_current_actor_social_occupancy(common_,interaction_,actor));if(!same(history.value,actual))reject("SocialOccupancyCurrentCoverageMismatch",actor);return ActorSocialOccupancyView{std::move(actual),{std::move(history.head)}};
});}
Outcome<ActorSocialOccupancyView>NativeActorSocialOccupancyRead::read_actor_social_occupancy_at_source_head(const StableId&actor,const WorldMoment&at,const std::vector<SourceRef>&sources,const WorldMoment&now)const{return guard<ActorSocialOccupancyView>([&]{
 if(compare(now,take(clock_.read_current_moment())))reject("SocialOccupancyActualMomentRequired",actor);const SourceRef*head=nullptr;for(const auto&source:sources)if(source.domain=="interaction"&&(source.kind==initial_kind||source.kind==changed)){if(head)reject("AmbiguousSocialOccupancyHead",actor);head=&source;}if(!head)reject("SocialOccupancyHeadRequired",actor);auto history=fold(common_,interaction_,initial_,actor,at,now,head);return ActorSocialOccupancyView{std::move(history.value),{std::move(history.head)}};
});}
Outcome<ActorSocialOccupancyView>NativeActorSocialOccupancyRead::read_actor_social_occupancy_before_source(const StableId&actor,const SourceRef&boundary,const WorldMoment&at)const{return guard<ActorSocialOccupancyView>([&]{
 const auto now=take(clock_.read_current_moment());const auto cut=position(common_,boundary);SourceRef head=initial_source(actor);const auto&state=selected(interaction_);const auto&order=common_.state().facts.order();for(std::size_t n=0;n<cut;++n){const auto*h=common_.state().facts.find(order[n]);if(!h||h->domain!="interaction"||h->kind!=changed)continue;const auto*t=state.transitions.find(h->id);if(!t)reject("SocialOccupancyTransitionMissing",h->id);if(t->actor_id==actor){if(compare(t->occurred_at,at)>0)reject("SocialOccupancyBoundaryMomentMismatch",t->id);head=transition_source(t->id);}}auto history=fold(common_,interaction_,initial_,actor,at,now,&head);return ActorSocialOccupancyView{std::move(history.value),{std::move(head)}};
});}
Outcome<WorldMoment>NativeActorSocialOccupancyRead::read_source_moment(const SourceRef&ref,Integer schema)const{return guard<WorldMoment>([&]{take(actor_social_occupancy_source_schema(ref));if(schema!=1)reject("UnsupportedSocialOccupancySchema",ref.id);const auto now=take(clock_.read_current_moment());if(ref.kind==initial_kind){auto base=take(initial_.read_actor_initial_social_occupancy(ref.id));return fold(common_,interaction_,initial_,ref.id,base.created_at,now,&ref).at;}const auto*t=selected(interaction_).transitions.find(ref.id);if(!t)reject("SocialOccupancyTransitionMissing",ref.id);return fold(common_,interaction_,initial_,t->actor_id,t->occurred_at,now,&ref).at;});}
Status NativeActorSocialOccupancyRead::validate_all()const{return guard<Done>([&]{const auto&s=selected(interaction_);const auto now=take(clock_.read_current_moment());for(const auto&row:s.transitions.rows()){if(row.first!=row.second.id)reject("SocialOccupancyTransitionKeyMismatch",row.first);position(common_,transition_source(row.first));}for(const auto&row:common_.state().actors.rows())take(read_actual_actor_social_occupancy(row.first,now));if(s.version>=2){if(!catalog_)reject("OriginalPartyAcceptanceCatalogRequired");take(validate_party_membership_acceptance_sources(common_,interaction_,*catalog_));}if(s.version==3){if(!catalog_)reject("InteractionSessionOpeningCatalogRequired");take(validate_interaction_session_opening_sources(common_,interaction_,*catalog_));}return Done{};});}
Outcome<std::optional<SourceRef>>record_actor_social_occupancy_change_in_candidate(CommonStore&c,InteractionState&i,const ActorSocialOccupancyInitialRead&initial,const CurrentMomentRead&clock,const StableId&actor,const ActorSocialOccupancy&before,ActorSocialOccupancyCause cause,const StableId&context,const StableId&initiator,const std::optional<SourceRef>&actual_cause,const PartyMembershipAcceptance*party_acceptance){return guard<std::optional<SourceRef>>([&]{
 selected(i);const auto now=take(clock.read_current_moment());auto prior=fold(c,i,initial,actor,now,now,nullptr);if(!same(before,prior.value))reject("SocialOccupancyOriginalBeforeMismatch",actor);auto after=take(read_current_actor_social_occupancy(c,i,actor));if(same(before,after))return std::optional<SourceRef>{};if(i.social_occupancy->transitions.size()>=1048576)reject("SocialOccupancySourceCapacity",actor);auto id=take(c.next_id("social_occupancy"));ActorSocialOccupancyTransition row{id,actor,std::move(prior.head),before,std::move(after),now,cause,context,initiator,actual_cause};if(i.social_occupancy->version>=2){const bool member=(cause==ActorSocialOccupancyCause::party_recruited&&actor!=initiator)||(cause==ActorSocialOccupancyCause::party_invitation_accepted&&actor==initiator);if(member!=bool(party_acceptance))reject("OriginalPartyAcceptanceEvidenceRequired",actor);if(party_acceptance)row.party_acceptance=*party_acceptance;}if(i.social_occupancy->version==3&&actor_social_occupancy_opening_cause(cause)){
 const InteractionSession*opened=nullptr;for(const auto&v:i.interactions)if(v.id==context){if(opened)reject("AmbiguousInteractionSessionOpening",context);opened=&v;}
 if(!opened||opened->actor_id!=actor||opened->phase!="stepping"||opened->node_id.empty()||opened->pending_command||!opened->pending_action_id.empty())reject("InteractionActualOpeningPointRequired",context);
 row.session_opening=InteractionSessionOpening{opened->id,opened->actor_id,opened->dialogue_id,opened->context};
 }take(c.append_fact({id,changed,"interaction"}));shape(c,row);if(!i.social_occupancy->transitions.insert(id,std::move(row)))reject("DuplicateSocialOccupancyTransition",id);return std::optional<SourceRef>{transition_source(id)};
});}
Status NativeActorSocialOccupancyWrite::record(const StableId&a,const ActorSocialOccupancy&b,ActorSocialOccupancyCause c,const StableId&context,const StableId&initiator,const std::optional<SourceRef>&source){auto r=record_actor_social_occupancy_change_in_candidate(common_,interaction_,initial_,clock_,a,b,c,context,initiator,source);return r.ok?common_success():Status{false,{},r.error};}
Status NativeActorSocialOccupancyWrite::record_party_member(const StableId&a,const ActorSocialOccupancy&b,ActorSocialOccupancyCause cause,const StableId&party,const StableId&initiator,const PartyMembershipAcceptance&evidence){auto result=record_actor_social_occupancy_change_in_candidate(common_,interaction_,initial_,clock_,a,b,cause,party,initiator,{},&evidence);return result.ok?common_success():Status{false,{},result.error};}
Status record_captured_party_member_acceptance(const InteractionAccess&a,const StableId&actor,const std::optional<ActorSocialOccupancy>&before,ActorSocialOccupancyCause cause,const StableId&party,const StableId&initiator,const PartyMembershipAcceptance&evidence){
 if(bool(before)!=a.social_occupancy_selected)return common_failure("SocialOccupancyCaptureMismatch",actor);if(!before)return common_success();if(!a.social_occupancy)return common_failure("OriginalPartyAcceptanceWriterRequired",actor);return a.social_occupancy->record_party_member(actor,*before,cause,party,initiator,evidence);
}
Outcome<Integer>actor_social_occupancy_source_schema(const SourceRef&ref){if(ref.domain=="interaction"&&(ref.kind==initial_kind||ref.kind==changed)&&!ref.id.empty())return {true,1,{}};return {false,0,common_failure("UnsupportedSocialOccupancySource",ref.id).error};}
Outcome<ActorSocialOccupancy>NativeActorSocialOccupancyWrite::capture(const CommonStore&c,const StableId&actor)const{
 if(&c!=&common_)return {false,{},common_failure("SocialOccupancyWriterCandidateMismatch",actor).error};auto now=clock_.read_current_moment();if(!now.ok)return {false,{},now.error};NativeActorSocialOccupancyRead reader(common_,interaction_,initial_,clock_);auto current=reader.read_actual_actor_social_occupancy(actor,now.value);return current.ok?Outcome<ActorSocialOccupancy>{true,std::move(current.value.value),{}}:Outcome<ActorSocialOccupancy>{false,{},current.error};
}
Outcome<std::optional<ActorSocialOccupancy>>capture_actor_social_occupancy(const CommonStore&c,const InteractionAccess&a,const StableId&actor){
 if(!a.social_occupancy_selected){if(a.social_occupancy)return {false,{},common_failure("UnselectedSocialOccupancyWriter",actor).error};return {true,std::nullopt,{}};}
 if(!a.social_occupancy)return {false,{},common_failure("SocialOccupancyWriterRequired",actor).error};auto value=a.social_occupancy->capture(c,actor);if(!value.ok)return {false,{},value.error};return {true,std::move(value.value),{}};
}
Outcome<std::optional<ActorSocialOccupancy>>capture_actor_social_occupancy(const CommonRead&c,const InteractionState&i,const InteractionAccess&a,const StableId&actor){
 if(bool(i.social_occupancy)!=a.social_occupancy_selected)return {false,{},common_failure("SocialOccupancyProfileMismatch",actor).error};if(!i.social_occupancy)return {true,std::nullopt,{}};auto store=dynamic_cast<const CommonStore*>(&c);if(!store||!a.social_occupancy||!a.social_occupancy->bound_to(*store,i))return {false,{},common_failure("SocialOccupancyWriterCandidateMismatch",actor).error};return capture_actor_social_occupancy(*store,a,actor);
}
Status record_captured_actor_social_occupancy(const InteractionAccess&a,const StableId&actor,const std::optional<ActorSocialOccupancy>&before,ActorSocialOccupancyCause cause,const StableId&context,const StableId&initiator,const std::optional<SourceRef>&source){
 if(bool(before)!=a.social_occupancy_selected)return common_failure("SocialOccupancyCaptureMismatch",actor);if(!before)return common_success();if(!a.social_occupancy)return common_failure("SocialOccupancyWriterRequired",actor);return a.social_occupancy->record(actor,*before,cause,context,initiator,source);
}

Status erase_actor_interaction_with_source_in_candidate(const CommonRead&c,InteractionState&i,const InteractionAccess&a,const StableId&actor,ActorSocialOccupancyCause cause,const StableId&initiator,const std::optional<SourceRef>&source){
 switch(cause){case ActorSocialOccupancyCause::end_interaction:case ActorSocialOccupancyCause::encounter_leave:case ActorSocialOccupancyCause::dialogue_end:case ActorSocialOccupancyCause::completed_dialogue_unavailable:case ActorSocialOccupancyCause::local_move:case ActorSocialOccupancyCause::personal_battle_started:case ActorSocialOccupancyCause::session_participant_died:break;default:return common_failure("SocialOccupancyClosingCauseRequired",actor);}
 auto before=capture_actor_social_occupancy(c,i,a,actor);if(!before.ok)return {false,{},before.error};StableId original;for(const auto&session:i.interactions)if(session.actor_id==actor){if(!original.empty())return common_failure("DuplicateActorInteraction",actor);original=session.id;}if(original.empty())return common_success();i.interactions.erase(std::remove_if(i.interactions.begin(),i.interactions.end(),[&](const auto&session){return session.actor_id==actor;}),i.interactions.end());return record_captured_actor_social_occupancy(a,actor,before.value,cause,original,initiator,source);
}

Status NativeActorSocialOccupancyRead::visit_actor_social_occupancy_changes(const StableId&actor,const WorldMoment&from,const SourceRef&from_head,const WorldMoment&to,const SourceRef&to_head,const WorldMoment&now,ActorSocialOccupancyChangeVisitor&visitor)const{return guard<Done>([&]{
 if(compare(now,take(clock_.read_current_moment()))||compare(from,to)>0)reject("SocialOccupancySpanInvalid",actor);fold(common_,interaction_,initial_,actor,from,now,&from_head);fold(common_,interaction_,initial_,actor,to,now,&to_head);
 auto ordinal=[&](const SourceRef&r)->std::size_t{return same(r,initial_source(actor))?0:position(common_,r)+1;};const auto first=ordinal(from_head),last=ordinal(to_head);if(first>last)reject("SocialOccupancyHeadOrderInvalid",actor);const auto&s=selected(interaction_);const auto&order=common_.state().facts.order();
 for(std::size_t n=first;n<last;++n){const auto*h=common_.state().facts.find(order[n]);if(!h||h->domain!="interaction"||h->kind!=changed)continue;const auto*row=s.transitions.find(h->id);if(!row)reject("SocialOccupancyTransitionMissing",h->id);if(row->actor_id!=actor)continue;if(compare(row->occurred_at,from)<0||compare(row->occurred_at,to)>0)reject("SocialOccupancyChangeOutsideSpan",row->id);take(visitor.changed(*row));}return Done{};
});}

Outcome<Integer> NativeActorSocialOccupancyRead::read_actor_social_occupancy_source_order(const SourceRef&ref,Integer schema)const{return guard<Integer>([&]{if(ref.domain!="interaction"||ref.kind!=changed||schema!=1)reject("SocialOccupancyActualTransitionRequired",ref.id);take(read_source_moment(ref,schema));return original_serial(common_,ref.id);});}

bool actor_social_occupancy_version(Integer v)noexcept{return v==1||v==2||v==3;}
bool actor_social_occupancy_opening_cause(ActorSocialOccupancyCause c)noexcept{return c==ActorSocialOccupancyCause::dialogue_open||c==ActorSocialOccupancyCause::dialogue_replaced||c==ActorSocialOccupancyCause::born_dialogue_open||c==ActorSocialOccupancyCause::known_dialogue_open;}
Outcome<const ActorSocialOccupancyTransition*>read_original_interaction_session_opening(const CommonStore&c,const InteractionState&i,const InteractionCatalog&catalog,const StableId&id){return guard<const ActorSocialOccupancyTransition*>([&]{
 const auto&s=selected(i);if(s.version!=3)throw Failure{common_failure("InteractionSessionOpeningPhysical3Required",id,ErrorKind::unsupported_capability).error};const ActorSocialOccupancyTransition*out=nullptr;
 for(const auto&entry:s.transitions.rows())if(entry.second.session_opening&&entry.second.session_opening->session_id==id){if(out)reject("DuplicateInteractionSessionOpening",id);if(entry.first!=entry.second.id)reject("InteractionSessionOpeningKeyMismatch",id);out=&entry.second;}
 if(!out)throw Failure{common_failure("InteractionImmutableOpeningSourceRequired",id,ErrorKind::unsupported_capability).error};position(c,transition_source(out->id));shape(c,*out);const auto&o=*out->session_opening;
 constexpr std::string_view session_prefix="interaction:";std::string_view serial_text=o.session_id;if(serial_text.size()<=session_prefix.size()||serial_text.substr(0,session_prefix.size())!=session_prefix)reject("InteractionOpeningAllocationMismatch",id);serial_text.remove_prefix(session_prefix.size());Integer session_serial=0;auto parsed=std::from_chars(serial_text.data(),serial_text.data()+serial_text.size(),session_serial);if(serial_text.empty()||serial_text.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=serial_text.data()+serial_text.size()||session_serial<1||session_serial>=original_serial(c,out->id))reject("InteractionOpeningAllocationMismatch",id);
 const DialogueDefinition*dialogue=nullptr;for(const auto&d:catalog.dialogues)if(d.id==o.dialogue_id){if(dialogue)reject("AmbiguousInteractionOpeningDialogue",id);dialogue=&d;}
 if(!dialogue||dialogue->context_role!=o.context.role_id||dialogue->entry.empty())reject("InteractionOpeningDialogueMismatch",id);
 Integer entry_count=0;for(const auto&n:dialogue->nodes)if(n.id==dialogue->entry)++entry_count;if(entry_count!=1)reject("InteractionOpeningEntryMismatch",id);
 const auto&context=o.context;
 if(!context.kind){if(context.role_id.empty()||context.place_id.empty()||context.encounter_id||!catalog.common)reject("InteractionOpeningRoleContextRequired",id);const auto*role=catalog.common->roles.find(context.role_id);if(!role||role->place_id!=context.place_id)reject("InteractionOpeningRoleDefinitionMismatch",id);}
 else if(*context.kind=="narrative"){if(!context.role_id.empty()||context.place_id.empty()||context.encounter_id)reject("InteractionOpeningNarrativeMismatch",id);}
 else if(*context.kind=="encounter"){if(!context.place_id.empty()||!context.encounter_id||context.encounter_id->empty())reject("InteractionOpeningEncounterRequired",id);const EncounterRecord*encounter=nullptr;for(const auto&e:i.encounters)if(e.id==*context.encounter_id){if(encounter)reject("AmbiguousInteractionOpeningEncounter",id);encounter=&e;}if(!encounter||std::find(encounter->participants.begin(),encounter->participants.end(),o.actor_id)==encounter->participants.end()||std::find(encounter->participants.begin(),encounter->participants.end(),context.provider_actor_id)==encounter->participants.end())reject("InteractionOpeningEncounterSourceMissing",id);}
 else reject("UnsupportedInteractionOpeningContext",id);
 if(out->cause==ActorSocialOccupancyCause::born_dialogue_open||out->cause==ActorSocialOccupancyCause::known_dialogue_open){
 if(!out->actual_cause_source||!context.kind||*context.kind!="narrative")reject("InteractionPersonalOpeningSourceRequired",id);const auto&cause=*out->actual_cause_source;
 if(out->cause==ActorSocialOccupancyCause::born_dialogue_open){const auto*r=i.born_personal_dialogues?i.born_personal_dialogues->openings.find(cause.id):nullptr;if(cause.domain!=born_personal_dialogue_module||cause.kind!=born_personal_dialogue_opened_kind||!r||r->session_id!=id||r->listener_actor_id!=o.actor_id||r->provider_actor_id!=context.provider_actor_id||r->dialogue_id!=o.dialogue_id||r->place_id!=context.place_id)reject("InteractionBornOpeningSourceMismatch",id);}
 else {const auto*r=i.known_personal_dialogues?i.known_personal_dialogues->openings.find(cause.id):nullptr;if(cause.domain!=known_personal_dialogue_module||cause.kind!=known_personal_dialogue_opened_kind||!r||r->session_id!=id||r->listener_origin.actor_id!=o.actor_id||r->provider_origin.actor_id!=context.provider_actor_id||r->dialogue_id!=o.dialogue_id||r->place_id!=context.place_id)reject("InteractionKnownOpeningSourceMismatch",id);}
 }
 return out;
});}
Status validate_interaction_session_opening_sources(const CommonStore&c,const InteractionState&i,const InteractionCatalog&catalog){return guard<Done>([&]{const auto&s=selected(i);if(s.version!=3)reject("InteractionSessionOpeningPhysical3Required");for(const auto&entry:s.transitions.rows()){const auto&t=entry.second;if(actor_social_occupancy_opening_cause(t.cause)!=bool(t.session_opening))reject("InteractionSessionOpeningCoverageMismatch",entry.first);if(t.session_opening)take(read_original_interaction_session_opening(c,i,catalog,t.session_opening->session_id));}for(const auto&session:i.interactions){const auto*t=take(read_original_interaction_session_opening(c,i,catalog,session.id));const auto&o=*t->session_opening;if(o.actor_id!=session.actor_id||o.dialogue_id!=session.dialogue_id||o.context.place_id!=session.context.place_id||o.context.provider_actor_id!=session.context.provider_actor_id||o.context.role_id!=session.context.role_id||o.context.kind!=session.context.kind||o.context.encounter_id!=session.context.encounter_id||session.context.input_error)reject("InteractionLiveOpeningBindingMismatch",session.id);}return Done{};});}

}
