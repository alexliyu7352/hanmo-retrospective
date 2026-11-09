#include "voluntary_path_commands.hpp"
namespace hanmo::resident {namespace {
bool reference(const StableId& s){return !s.empty()&&s.size()<=256;}
const SuccessionConsentFact* plan(const FamilyState& f,const StableId& id){const auto* v=f.facts.find(id);return v?std::get_if<SuccessionConsentFact>(v):nullptr;}
bool proof_equal(const KnownPersonalDialogueProof& a,const KnownPersonalDialogueProof& b){return compare_boundary(encode_known_personal_dialogue_proof(a),encode_known_personal_dialogue_proof(b)).exact();}
bool quote_equal(const VoluntaryPathQuote& a,const VoluntaryPathQuote& b){
 if(a.plan_id!=b.plan_id||a.consent.has_value()!=b.consent.has_value()||a.personal.has_value()!=b.personal.has_value()||a.withdrawal.has_value()!=b.withdrawal.has_value())return false;
 if(a.consent&&(!succession_consent_source_equal(a.consent->agreement,b.consent->agreement)||!voluntary_path_source_equal(a.consent->path,b.consent->path)||!succession_personal_terms_equal(a.consent->personal.terms,b.consent->personal.terms)))return false;
 return (!a.personal||succession_personal_terms_equal(a.personal->terms,b.personal->terms))&&(!a.withdrawal||proof_equal(*a.withdrawal,*b.withdrawal));
}
}
Outcome<VoluntaryPathCommand> decode_voluntary_path_command(const StableId& kind,const BoundaryValue& args){try{
 auto unique=validate_succession_json_unique(args);if(!unique.ok)return {false,{},unique.error};using namespace boundary;VoluntaryPathCommand c;c.kind=kind;
 if(kind=="family_voluntary_path_accept"){
  fields(args,{"request_id","successor_actor_id"},{"relation_fact_id"},kind);c.request_id=get_string(args,"request_id",kind);c.successor_actor_id=get_string(args,"successor_actor_id",kind);if(const auto* v=args.find("relation_fact_id"))c.relation_fact_id=string(*v,kind);
  if(!reference(c.request_id)||!reference(c.successor_actor_id)||(args.find("relation_fact_id")&&!reference(c.relation_fact_id)))fail("InvalidVoluntaryPathCommand",kind);
 }else if(kind=="family_voluntary_path_continue"||kind=="family_voluntary_path_cancel"||kind=="family_voluntary_path_withdraw"){
  fields(args,{"plan_id"},{},kind);c.plan_id=get_string(args,"plan_id",kind);if(!reference(c.plan_id))fail("InvalidVoluntaryPathCommand",kind);
 }else fail("UnknownVoluntaryPathCommand",kind);
 return {true,std::move(c),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<VoluntaryPathQuote> preview_voluntary_path_command(const VoluntaryPathContext& x,const VoluntaryPathState& state,const StableId& actor,const VoluntaryPathCommand& c){
 const auto& common=x.people.common;if(actor!=common.state().metadata.player_actor_id)return {false,{},common_failure("PlayerCommandAuthorityRequired").error};
 auto valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};valid=validate_voluntary_path_result_bindings(common,x.people.family,state);if(!valid.ok)return {false,{},valid.error};
 const auto* person=common.actor(actor);if(!person||!person->alive||!person->available||person->hp<=0)return {false,{},common_failure("ActorUnavailable",actor).error};VoluntaryPathQuote out;
 if(c.kind=="family_voluntary_path_accept"){
  if(!reference(c.request_id)||!reference(c.successor_actor_id)||!c.plan_id.empty())return {false,{},common_failure("InvalidVoluntaryPathCommand").error};
  for(const auto& row:x.people.family.facts.rows())if(const auto* old=std::get_if<SuccessionConsentFact>(&row.second);old&&!old->conclusion&&old->agreement.personal.predecessor_actor_id==actor)return {false,{},common_failure("SuccessionPlanAlreadyPending",row.first).error};
  auto source=preview_voluntary_path_consent(x,actor,c.successor_actor_id,c.request_id,c.relation_fact_id);if(!source.ok)return {false,{},source.error};out.consent=std::move(source.value);return {true,std::move(out),{}};
 }
 if(!reference(c.plan_id)||!c.request_id.empty()||!c.successor_actor_id.empty()||!c.relation_fact_id.empty())return {false,{},common_failure("InvalidVoluntaryPathCommand").error};
 const auto* p=plan(x.people.family,c.plan_id);if(!p||p->conclusion||!state.consents.find(c.plan_id)||p->agreement.personal.predecessor_actor_id!=actor)return {false,{},common_failure("VoluntaryPathPendingPlanRequired",c.plan_id).error};out.plan_id=c.plan_id;
 if(c.kind=="family_voluntary_path_continue"){auto assets=preview_accepted_voluntary_path_assets(x,state,c.plan_id);if(!assets.ok)return {false,{},assets.error};out.personal=std::move(assets.value);}
 else if(c.kind=="family_voluntary_path_withdraw"){auto source=preview_voluntary_path_withdrawal(x,state,c.plan_id);if(!source.ok)return {false,{},source.error};out.withdrawal=std::move(source.value);}
 else if(c.kind!="family_voluntary_path_cancel")return {false,{},common_failure("UnknownVoluntaryPathCommand").error};
 return {true,std::move(out),{}};
}
Outcome<FamilyEffect> apply_voluntary_path_command(CommonStore& common,FamilyState& family,VoluntaryPathState& state,const VoluntaryPathContext& x,const StableId& actor,const VoluntaryPathCommand& command,const VoluntaryPathQuote& quote){
 if(&common!=&x.people.common||&family!=&x.people.family)return {false,{},common_failure("VoluntaryPathCandidateRequired").error};
 auto fresh=preview_voluntary_path_command(x,state,actor,command);if(!fresh.ok)return {false,{},fresh.error};if(!quote_equal(quote,fresh.value))return {false,{},common_failure("StaleVoluntaryPathCommand").error};
 auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};const auto old_family=family;const auto old_state=state;
 auto abort=[&](const RuleError& error)->Outcome<FamilyEffect>{auto rolled=common.rollback_savepoint(point.value);family=old_family;state=old_state;return {false,{},rolled.ok?error:rolled.error};};
 Outcome<StableId> source;
 if(quote.consent)source=accept_voluntary_path_in_candidate(common,family,state,x,*quote.consent);
 else if(quote.personal)source=complete_voluntary_path_in_candidate(common,family,state,x,command.plan_id,*quote.personal);
 else source=cancel_voluntary_path_in_candidate(common,family,state,x,command.plan_id,quote.withdrawal);
 if(!source.ok)return abort(source.error);auto committed=common.commit_savepoint(point.value);if(!committed.ok)return abort(committed.error);
 FamilyEffect effect;effect.success=true;effect.actor_id=actor;effect.player_actor_id=common.state().metadata.player_actor_id;effect.fact_id=source.value;
 if(quote.consent)effect.heir_actor_id=quote.consent->agreement.personal.successor_actor_id;else if(quote.personal)effect.heir_actor_id=quote.personal->terms.successor_actor_id;
 return {true,std::move(effect),{}};
}
}
