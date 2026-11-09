// 读取同一世界当前本人已知ID，明确指令不能代替原回复权限或真实接触。
#include "diplomacy_response_policy.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool valid_choice(DiplomaticResponseChoice c){return c==DiplomaticResponseChoice::defer||c==DiplomaticResponseChoice::accept||c==DiplomaticResponseChoice::refuse;}
}
Status validate_diplomatic_response_policy(const OrganizationCatalog& org,const DiplomacyCatalog& catalog,const DiplomaticResponsePolicy& policy){
    if(!catalog.enabled||policy.id.empty()||policy.office_id.empty()||policy.directives.size()>256)return common_failure("InvalidDiplomaticResponsePolicy",policy.id);
    const auto* office=org.offices.find(policy.office_id);if(!office)return common_failure("UnknownDiplomaticResponseOffice",policy.office_id);
    std::set<std::pair<StableId,StableId>> seen;
    for(const auto& rule:policy.directives){const auto* mission=catalog.mission_policies.find(rule.mission_policy_id);const auto* terms=catalog.passage_terms.find(rule.terms_id);
        if(!valid_choice(rule.choice)||!mission||!terms||mission->target_office_id!=office->id||mission->target_organization_id!=office->organization_id||std::find(mission->passage_terms_ids.begin(),mission->passage_terms_ids.end(),rule.terms_id)==mission->passage_terms_ids.end()||!seen.emplace(rule.mission_policy_id,rule.terms_id).second)return common_failure("InvalidDiplomaticResponseDirective",rule.mission_policy_id);
    }
    return common_success();
}
Outcome<DiplomaticResponseCandidate> propose_diplomatic_response(const DiplomaticPresentRead& read,const DiplomacyViewIndex& index,const StableId& actor,const DiplomaticResponsePolicy& policy,Integer candidate_limit){
    const auto fail=[](const StableId& code)->Outcome<DiplomaticResponseCandidate>{return {false,{},common_failure(code).error};};
    if(candidate_limit<1||candidate_limit>256)return fail("InvalidDiplomaticCandidateBudget");
    auto valid=validate_diplomatic_response_policy(read.organization_catalog,read.catalog,policy);if(!valid.ok)return {false,{},valid.error};
    if(!index.initialized||!same_identity(index.identity,read.common.state().identity)||index.fact_count!=read.state.facts.size()||index.offer_count!=read.state.offers.size()||index.mission_count!=read.state.missions.size())return fail("StaleDiplomacyViewIndex");
    DiplomaticResponseCandidate result;
    if(actor==read.common.state().metadata.player_actor_id)return {true,std::move(result),{}};
    const auto* office=read.organizations.offices.find(policy.office_id);if(!office||office->holder_actor_id!=actor)return {true,std::move(result),{}};
    auto mine=index.by_observer.find(actor);if(mine==index.by_observer.end())return {true,std::move(result),{}};
    for(const auto& known:mine->second){if(result.inspected>=candidate_limit)break;++result.inspected;
        const auto* fact=read.state.facts.find(known.fact_id);const auto* offer=read.state.offers.find(known.offer_id);
        if(!fact||!offer||fact->offer_id!=offer->id||fact->tick>read.common.tick()||std::find(fact->witness_actor_ids.begin(),fact->witness_actor_ids.end(),actor)==fact->witness_actor_ids.end())return fail("InvalidDiplomaticDecisionSource");
        if(fact->kind!=diplomatic_presentation_fact_kind||offer->status!=DiplomaticOfferStatus::presented||offer->last_fact_id!=fact->id)continue;
        const auto* presentation=read.state.presentations.find(fact->id);if(!presentation||presentation->representative.actor_id!=actor||presentation->representative.office_id!=policy.office_id)continue;
        const auto directive=std::find_if(policy.directives.begin(),policy.directives.end(),[&](const auto& row){return row.mission_policy_id==offer->policy_id&&row.terms_id==fact->terms_id;});if(directive==policy.directives.end())continue;
        if(result.offer_id.empty()){result.offer_id=offer->id;result.source_fact_id=fact->id;}
        if(directive->choice==DiplomaticResponseChoice::defer){if(result.waiting_code.empty())result.waiting_code="DiplomaticResponseDeferred";continue;}
        DiplomaticReplyCommand command{offer->id,fact->id,fact->offer_revision,fact->terms_revision,directive->choice==DiplomaticResponseChoice::accept?DiplomaticReplyKind::accept:DiplomaticReplyKind::refuse};
        auto quote=preview_diplomatic_reply(read,actor,command);if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};if(result.waiting_code.empty())result.waiting_code=quote.error.original.code;continue;}
        result.offer_id=offer->id;result.source_fact_id=fact->id;result.waiting_code.clear();result.command=std::move(command);return {true,std::move(result),{}};
    }
    return {true,std::move(result),{}};
}
}
