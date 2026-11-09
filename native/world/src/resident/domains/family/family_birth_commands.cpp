// 新鲜报价保存确切本人意愿或照护条款；提交不得调包成另一人物、另一孩子或一次即时消费。
#include "family_birth_commands.hpp"
#include "resident/domains/family/succession_personal_assets.hpp"
#include <tuple>
namespace hanmo::resident {namespace {
// 每条命令只允许自己的最少字段，未知前缀和多余权利字段都拒绝。
bool shape(const FamilyBirthCommand& c){auto id=[](const StableId& s){return !s.empty()&&s.size()<=256;};
    if(c.kind=="family_birth_receive")return id(c.arrangement_id)&&c.proposal_id.empty()&&c.decision.empty()&&c.accepted_id.empty()&&c.action_id.empty();
    if(c.kind=="family_birth_reply")return c.arrangement_id.empty()&&id(c.proposal_id)&&(c.decision=="accepted"||c.decision=="refused"||c.decision=="withdrawn")&&c.accepted_id.empty()&&c.action_id.empty();
    if(c.kind=="family_birth_withdraw"||c.kind=="family_child_care")return c.arrangement_id.empty()&&c.proposal_id.empty()&&c.decision.empty()&&id(c.accepted_id)&&c.action_id.empty();
    if(c.kind=="family_child_care_cancel")return c.arrangement_id.empty()&&c.proposal_id.empty()&&c.decision.empty()&&c.accepted_id.empty()&&id(c.action_id);
    return false;
}
bool same(const FamilyBirthCommand& a,const FamilyBirthCommand& b){return std::tie(a.kind,a.arrangement_id,a.proposal_id,a.decision,a.accepted_id,a.action_id)==std::tie(b.kind,b.arrangement_id,b.proposal_id,b.decision,b.accepted_id,b.action_id);}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.tick,a.revision)==std::tie(b.world_id,b.execution_epoch,b.tick,b.revision);}
bool same_care(const ChildCareQuote& a,const ChildCareQuote& b){return same_identity(a.identity,b.identity)&&std::tie(a.accepted_id,a.child_actor_id,a.caregiver_actor_id,a.place_id,a.duration,a.period_index,a.food_per_period)==std::tie(b.accepted_id,b.child_actor_id,b.caregiver_actor_id,b.place_id,b.duration,b.period_index,b.food_per_period);}
}
Outcome<FamilyBirthCommand> decode_family_birth_command(const StableId& kind,const BoundaryValue& value){try{using namespace boundary;auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};FamilyBirthCommand c;c.kind=kind;
    if(kind=="family_birth_receive"){fields(value,{"arrangement_id"},{},kind);c.arrangement_id=get_string(value,"arrangement_id","");}
    else if(kind=="family_birth_reply"){fields(value,{"proposal_id","decision"},{},kind);c.proposal_id=get_string(value,"proposal_id","");c.decision=get_string(value,"decision","");}
    else if(kind=="family_birth_withdraw"||kind=="family_child_care"){fields(value,{"accepted_id"},{},kind);c.accepted_id=get_string(value,"accepted_id","");}
    else if(kind=="family_child_care_cancel"){fields(value,{"action_id"},{},kind);c.action_id=get_string(value,"action_id","");}
    else fail("UnknownFamilyBirthCommand",kind);
    if(!shape(c))fail("InvalidFamilyBirthCommand",kind);return {true,std::move(c),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<FamilyBirthQuote> preview_family_birth_command(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& actor,const FamilyBirthCommand& command){
    auto bad=[](const StableId& code)->Outcome<FamilyBirthQuote>{return {false,{},common_failure(code).error};};
    if(!catalog.enabled)return bad("BirthProfileRequired");if(!shape(command))return bad("InvalidFamilyBirthCommand");
    const auto* who=c.actor(actor);if(!who||!who->alive||!who->available)return bad("BirthLivingDecisionMakerRequired");
    const bool care=command.kind=="family_child_care",cancel=command.kind=="family_child_care_cancel";
    if(!care&&actor!=c.state().metadata.player_actor_id)return bad("BirthPersonalDecisionRequired");
    if(!cancel&&!who->action_id.empty())return bad("BirthParticipantUnavailable");
    auto history=validate_birth_arrangements(c,interactions,family,catalog,state,access);if(!history.ok)return {false,{},history.error};FamilyBirthQuote quote{c.state().identity,command,{},{}};
    if(care){auto wanted=preview_child_care(c,interactions,family,catalog,state,access,actor,command.accepted_id);if(!wanted.ok)return {false,{},wanted.error};quote.care=std::move(wanted.value);return {true,std::move(quote),{}};}
    if(cancel){const auto* start=state.care_starts.find(command.action_id);const auto* original=c.action(command.action_id);if(!start||start->caregiver_actor_id!=actor||!original||original->kind!="family_child_care"||original->actor_id!=actor||who->action_id!=command.action_id)return bad("ChildCareOriginalActionRequired");return {true,std::move(quote),{}};}
    if(command.kind=="family_birth_withdraw"){const auto* accepted=state.replies.find(command.accepted_id);if(!accepted||!accepted->confirmation||history.value.closed_acceptances.count(command.accepted_id)||history.value.born_acceptances.count(command.accepted_id))return bad("BirthCareReleaseSourceRequired");const auto& source=*accepted->confirmation;if(actor!=source.speaker_actor_id&&actor!=source.listener_actor_id)return bad("BirthPersonalDecisionRequired");return {true,std::move(quote),{}};}
    StableId arrangement=command.arrangement_id;
    if(command.kind=="family_birth_reply"){const auto* proposal=state.proposals.find(command.proposal_id);if(!proposal||history.value.replied_proposals.count(command.proposal_id))return bad("BirthProposalAlreadyAnswered");if(actor!=(command.decision=="withdrawn"?proposal->source.speaker_actor_id:proposal->source.listener_actor_id))return bad("BirthPersonalDecisionRequired");if(command.decision!="accepted")return {true,std::move(quote),{}};arrangement=proposal->source.arrangement_id;}
    auto proof=preview_birth_arrangement_source(c,meeting,interactions,contact,family,catalog,actor,arrangement,access.people);if(!proof.ok)return {false,{},proof.error};quote.source=std::move(proof.value);
    if(command.kind=="family_birth_receive"){for(const auto& row:state.proposals.rows())if(!history.value.replied_proposals.count(row.first)&&row.second.source.arrangement_id==arrangement&&row.second.source.listener_actor_id==actor)return bad("BirthProposalAlreadyPending");}
    else{const auto& original=state.proposals.find(command.proposal_id)->source;if(!birth_care_terms_equal(original.terms,quote.source->source.terms))return bad("BirthAcceptanceBindingMismatch");for(const auto& row:state.replies.rows()){if(!row.second.confirmation||history.value.closed_acceptances.count(row.first))continue;const auto& s=*row.second.confirmation;if(s.speaker_actor_id==original.speaker_actor_id||s.speaker_actor_id==original.listener_actor_id||s.listener_actor_id==original.speaker_actor_id||s.listener_actor_id==original.listener_actor_id)return bad("BirthArrangementAlreadyActive");}auto supply=preview_birth_care_supply(c,quote.source->source.terms);if(!supply.ok)return {false,{},supply.error};}
    if(!access.eligible_pair_before)return bad("BirthPairHistoryRequired");auto eligible=access.eligible_pair_before(quote.source->source.speaker_actor_id,quote.source->source.listener_actor_id,c.tick(),{});if(!eligible.ok)return {false,{},eligible.error};
    return {true,std::move(quote),{}};
}
Status revalidate_family_birth_quote(const CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,const FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& actor,const FamilyBirthCommand& command,const FamilyBirthQuote& quote){
    if(!same(command,quote.command)||!same_identity(c.state().identity,quote.identity))return common_failure("StaleBirthQuote");
    auto fresh=preview_family_birth_command(c,meeting,interactions,contact,family,catalog,state,access,actor,command);if(!fresh.ok)return {false,{},fresh.error};
    if(bool(fresh.value.source)!=bool(quote.source)||bool(fresh.value.care)!=bool(quote.care)||(quote.source&&!birth_arrangement_source_equal(fresh.value.source->source,quote.source->source))||(quote.care&&!same_care(*fresh.value.care,*quote.care)))return common_failure("BirthQuoteSourceChanged");
    return common_success();
}
Outcome<FamilyBirthCommandResult> apply_family_birth_command_in_candidate(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interactions,const InteractionAccess& contact,const FamilyCatalog& family,const BirthArrangementCatalog& catalog,FamilyBirthState& state,const BirthArrangementAccess& access,const StableId& actor,const FamilyBirthCommand& command,const FamilyBirthQuote& quote){
    auto valid=revalidate_family_birth_quote(c,meeting,interactions,contact,family,catalog,state,access,actor,command,quote);if(!valid.ok)return {false,{},valid.error};
    if(command.kind=="family_child_care")return {false,{},common_failure("ChildCareTimedActionRequired").error};
    FamilyBirthCommandResult result;result.effect.actor_id=actor;result.effect.player_actor_id=c.state().metadata.player_actor_id;result.effect.success=true;
    if(command.kind=="family_child_care_cancel"){const auto action=*c.action(command.action_id);auto ended=cancel_child_care_action_in_candidate(c,interactions,family,catalog,state,access,actor,action);if(!ended.ok)return {false,{},ended.error};result.release_action_id=action.id;result.effect.fact_id=ended.value.fact_id;return {true,std::move(result),{}};}
    Outcome<StableId> recorded;
    if(command.kind=="family_birth_receive")recorded=receive_birth_proposal_in_candidate(c,meeting,interactions,contact,family,catalog,state,access,*quote.source);
    else if(command.kind=="family_birth_reply")recorded=reply_birth_proposal_in_candidate(c,meeting,interactions,contact,family,catalog,state,access,command.proposal_id,command.decision,quote.source);
    else recorded=withdraw_birth_arrangement_in_candidate(c,interactions,family,catalog,state,access,command.accepted_id);
    if(!recorded.ok)return {false,{},recorded.error};result.effect.fact_id=recorded.value;return {true,std::move(result),{}};
}
BoundaryValue encode_family_birth_quote(const FamilyBirthQuote& quote){using V=BoundaryValue;const BirthCareTerms* terms=quote.source?&quote.source->source.terms:nullptr;return boundary::obj({{"ok",V{true}},{"kind",V{quote.command.kind}},{"duration",V{quote.care?quote.care->duration:Integer{0}}},{"reserved_food",V{terms&&quote.command.decision=="accepted"?terms->food_per_period*terms->prepaid_care_periods:Integer{0}}},{"food_on_completion",V{quote.care?quote.care->food_per_period:Integer{0}}},{"scope",V{"explicit_birth_and_finite_personal_care"}}});}
Status validate_family_birth_command_receipt(const FamilyBirthState& state,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect& r){if(r.success!=std::optional<bool>{true}||r.actor_id!=std::optional<StableId>{actor}||r.player_actor_id!=r.actor_id||!r.fact_id||r.already_completed||r.code||r.heir_actor_id)return common_failure("InvalidBirthCommandReceipt");
    if(kind=="family_birth_receive"){const auto* p=state.proposals.find(*r.fact_id);return p&&p->source.listener_actor_id==actor&&p->source.tick==tick?common_success():common_failure("BirthReceiptSourceMismatch");}
    if(kind=="family_birth_reply"){const auto* p=state.replies.find(*r.fact_id);return p&&p->actor_id==actor&&p->tick==tick?common_success():common_failure("BirthReceiptSourceMismatch");}
    if(kind=="family_birth_withdraw"){const auto* p=state.closures.find(*r.fact_id);return p&&p->actor_id==actor&&p->tick==tick&&p->reason=="withdrawn"?common_success():common_failure("BirthReceiptSourceMismatch");}
    if(kind=="family_child_care_cancel"){const auto* p=state.care.find(*r.fact_id);return p&&p->caregiver_actor_id==actor&&p->tick==tick&&p->outcome=="cancelled"&&p->code=="CaregiverStopped"?common_success():common_failure("BirthReceiptSourceMismatch");}
    return common_failure("UnknownFamilyBirthCommand");
}
}
