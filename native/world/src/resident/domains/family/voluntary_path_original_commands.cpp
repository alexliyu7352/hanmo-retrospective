#include "voluntary_path_original_commands.hpp"
namespace hanmo::resident {namespace {
const SuccessionConsentFact* plan(const FamilyState& f,const StableId& id){const auto* v=f.facts.find(id);return v?std::get_if<SuccessionConsentFact>(v):nullptr;}
}
Outcome<FamilyQuote> preview_original_voluntary_path_command(const VoluntaryPathContext& x,const VoluntaryPathState& state,const InteractionState& interactions,const InteractionAccess& access,const StableId& actor,const FamilyCommand& command){
 const auto& c=x.people.common;if(actor!=c.state().metadata.player_actor_id)return {false,{},common_failure("PlayerCommandAuthorityRequired").error};const auto* person=c.actor(actor);if(!person||!person->alive||!person->available||person->hp<=0)return {false,{},common_failure("ActorUnavailable",actor).error};
 auto valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};valid=validate_voluntary_path_result_bindings(c,x.people.family,state);if(!valid.ok)return {false,{},valid.error};FamilyQuote out;
 if(command.kind=="family_agree_succession"){
  if(!command.request_id||!command.heir_actor_id||command.plan_id||!x.original_requests.enabled||x.catalog.definitions.find(*command.request_id))return {false,{},common_failure("InvalidSuccessionConsentCommand").error};
  for(const auto& row:x.people.family.facts.rows())if(const auto* p=std::get_if<SuccessionConsentFact>(&row.second);p&&!p->conclusion&&p->agreement.personal.predecessor_actor_id==actor)return {false,{},common_failure("SuccessionPlanAlreadyPending",row.first).error};
  auto quote=preview_succession_consent(c,x.people.family,interactions,x.interactions,access,x.original_requests,actor,*command.heir_actor_id,*command.request_id);if(!quote.ok)return {false,{},quote.error};out.succession_consent=std::move(quote.value);return {true,std::move(out),{}};
 }
 if(command.kind!="family_cancel_succession"&&command.kind!="family_continue_as")return {false,{},common_failure("UnknownFamilyCommand").error};
 if(!command.plan_id)return {false,{},common_failure("SuccessionPlanRequired").error};const auto* p=plan(x.people.family,*command.plan_id);
 if(!p||p->conclusion||state.consents.find(*command.plan_id)||p->agreement.personal.predecessor_actor_id!=actor)return {false,{},common_failure("VoluntaryOriginalPendingPlanRequired").error};out.succession_plan_id=*command.plan_id;
 if(command.kind=="family_continue_as"){auto assets=preview_accepted_voluntary_path_assets(x,state,*command.plan_id);if(!assets.ok)return {false,{},assets.error};out.succession_personal=std::move(assets.value);}
 return {true,std::move(out),{}};
}
Outcome<FamilyEffect> apply_original_voluntary_path_command(CommonStore& c,FamilyState& family,VoluntaryPathState& state,const VoluntaryPathContext& x,const InteractionState& interactions,const InteractionAccess& access,const StableId& actor,const FamilyCommand& command,const FamilyQuote& quote){
 if(&c!=&x.people.common||&family!=&x.people.family)return {false,{},common_failure("VoluntaryPathCandidateRequired").error};auto fresh=preview_original_voluntary_path_command(x,state,interactions,access,actor,command);if(!fresh.ok)return {false,{},fresh.error};const auto& b=fresh.value;
 if(quote.duration!=0||quote.cost||quote.context||quote.training||quote.family||quote.adult_adoption||quote.succession_plan_id!=b.succession_plan_id||quote.succession_consent.has_value()!=b.succession_consent.has_value()||quote.succession_personal.has_value()!=b.succession_personal.has_value()||(quote.succession_consent&&!succession_consent_source_equal(quote.succession_consent->agreement,b.succession_consent->agreement))||(quote.succession_personal&&!succession_personal_terms_equal(quote.succession_personal->terms,b.succession_personal->terms)))return {false,{},common_failure("StaleSuccessionPlan").error};
 auto point=c.begin_savepoint();if(!point.ok)return {false,{},point.error};const auto old_family=family;const auto old_state=state;auto abort=[&](const RuleError& error)->Outcome<FamilyEffect>{auto rolled=c.rollback_savepoint(point.value);family=old_family;state=old_state;return {false,{},rolled.ok?error:rolled.error};};Outcome<StableId> source;
 if(quote.succession_consent)source=accept_original_voluntary_path_in_candidate(c,family,state,x,interactions,access,*quote.succession_consent);
 else if(quote.succession_personal)source=complete_voluntary_path_in_candidate(c,family,state,x,*quote.succession_plan_id,*quote.succession_personal);
 else source=cancel_voluntary_path_in_candidate(c,family,state,x,*quote.succession_plan_id);
 if(!source.ok)return abort(source.error);auto committed=c.commit_savepoint(point.value);if(!committed.ok)return abort(committed.error);
 FamilyEffect effect;effect.success=true;effect.player_actor_id=c.state().metadata.player_actor_id;effect.fact_id=command.kind=="family_cancel_succession"?*command.plan_id:source.value;
 if(quote.succession_consent)effect.heir_actor_id=quote.succession_consent->agreement.personal.successor_actor_id;else if(quote.succession_personal)effect.heir_actor_id=quote.succession_personal->terms.successor_actor_id;
 return {true,std::move(effect),{}};
}
}
