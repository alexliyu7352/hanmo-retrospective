#include "resident/domains/military/military_model.hpp"
// 已有批准逐项还原原请求；只替换明确供给项，不代办首次pending、不释放或改派旧责任。
#include "politics_supply_renewal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_detail.hpp"
namespace hanmo::resident {
Outcome<PoliticalSupplyRenewalProposal> propose_political_supply_renewal(
    const PoliticalTakeoverContext& x,const EconomicSupplyPolicyContext& economy,
    const StableId& actor,const PoliticalSupplyRenewalIntent& intent,const StableId& registration){
    using Result=Outcome<PoliticalSupplyRenewalProposal>;
    PoliticalSupplyRenewalProposal out;out.registration_fact_id=registration;
    const auto fail=[&](const StableId& code)->Result{return {false,{},common_failure(code,intent.id,ErrorKind::internal).error};};
    const auto wait=[&](const StableId& code)->Result{out.wait_code=code;return {true,std::move(out),{}};};
    if(intent.id.empty()||intent.takeover_rule_id.empty()||intent.author_policy_id.empty()
        ||&x.common!=&economy.ledger.common||x.phase||economy.ledger.phase||!x.supply_adoptions||!x.supply_policies)
        return fail("PoliticalSupplyRenewalContextMismatch");
    auto checked=validate_political_takeover_state(x);if(!checked.ok)return {false,{},checked.error};
    checked=validate_economic_supply_policies(economy);if(!checked.ok)return {false,{},checked.error};
    const auto* r=x.state.registrations.find(registration);const auto* rule=x.catalog.rules.find(intent.takeover_rule_id);
    const auto* policy=economy.ledger.catalog.policies.find(intent.author_policy_id);
    if(!r||!rule||r->rule_id!=rule->id||!policy||policy->organization_id!=r->successor_organization_id)
        return fail("PoliticalSupplyRenewalIntentMismatch");
    if(actor==x.common.state().metadata.player_actor_id)return wait("PoliticalSupplyRenewalRequiresNpc");
    if(!takeover_detail::known_to_actor(x,actor,*r))return wait("PoliticalTakeoverNotKnown");
    const auto* office=x.organizations.offices.find(rule->successor_office_id);
    const auto* person=x.common.actor(actor);auto location=x.common.resolved_location(actor);
    const auto* at=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!office||office->holder_actor_id!=std::optional<StableId>{actor}||!person||!person->alive
        ||!person->available||!person->action_id.empty()||!at||at->place_id!=rule->site_id)
        return wait("PoliticalSupplyRenewalActorUnavailable");
    const auto* previous=takeover_detail::latest(x.state,registration);
    if(!previous)return wait("PoliticalSupplyInitialResponsibilityDecisionRequired");
    out.previous_approval_fact_id=previous->id;
    const auto original=std::find_if(r->responsibilities.begin(),r->responsibilities.end(),[&](const auto& source){return source.responsibility.kind==PoliticalResponsibilityKind::supply_source&&source.responsibility.responsibility_id==policy->original_source_key;});
    if(original==r->responsibilities.end())return fail("PoliticalSupplyRenewalSourceMismatch");
    ActingAuthority authority{r->successor_organization_id,rule->successor_office_id,office->epoch,actor};
    auto draft=x.supply_policies->economic_supply_pending_policy(registration,policy->original_source_key);
    if(!draft.ok)return {false,{},draft.error};
    const auto issue=[&]()->Result{
        IssueEconomicSupplyPolicyCommand command{authority,registration,policy->id};
        auto quote=preview_economic_supply_policy(economy,actor,command);
        if(!quote.ok)return quote.error.kind==ErrorKind::rule?wait(quote.error.original.code):Result{false,{},quote.error};
        out.command=std::move(command);return {true,std::move(out),{}};
    };
    // 过期或其它作者方案不成为本意图的默认批准；另提真实签发，绝不原址重写旧草案。
    if(!draft.value||draft.value->author_policy_id!=policy->id||draft.value->through_tick<=x.common.tick()
        ||draft.value->issuing_authority.actor_id!=actor||draft.value->issuing_authority.authority_epoch!=office->epoch)
        return issue();
    out.policy_fact_id=draft.value->policy_fact_id;
    PoliticalTakeoverDecisionCommand command;command.authority=authority;command.registration_fact_id=registration;
    command.expected_identity=x.common.state().identity;command.expected_previous_approval_fact_id=previous->id;
    bool replaced=false;
    for(const auto& id:previous->decision_fact_ids){
        const auto* saved=x.state.decisions.find(id);if(!saved||saved->approval_fact_id!=previous->id)return fail("PoliticalSupplyRenewalApprovalMismatch");
        const auto& decision=saved->decision;PoliticalResponsibilityRequest request;
        request.responsibility=decision.responsibility;request.disposition=decision.disposition;
        request.payment_wallet_id=decision.payment_wallet_id;request.payment_inventory_id=decision.payment_inventory_id;
        request.production_asset_ids=decision.production_asset_ids;
        if(decision.responsibility.kind==PoliticalResponsibilityKind::supply_source&&decision.disposition==PoliticalResponsibilityDisposition::assume_by_successor){
            const auto* binding=x.supply_adoptions->bindings.find(id);if(!binding)return fail("MissingPoliticalSupplyAdoption");request.supply_policy_fact_id=binding->policy_issue_fact_id;
        }
        if(takeover_detail::key(decision.responsibility)==takeover_detail::key(original->responsibility)){
            if(replaced)return fail("DuplicatePoliticalResponsibility");replaced=true;
            if(request.disposition==PoliticalResponsibilityDisposition::assume_by_successor&&request.supply_policy_fact_id==out.policy_fact_id){
                auto allowed=PoliticalSupplyAdoptionAccess(x).political_supply_adoption(out.policy_fact_id);
                if(!allowed.ok)return {false,{},allowed.error};
                if(allowed.value&&allowed.value->unavailable_code=="PoliticalSupplyPolicyAuthorityChanged")return issue();
                return wait(allowed.value&&allowed.value->new_contracts_allowed?"PoliticalSupplyRenewalAlreadyAdopted":allowed.value?allowed.value->unavailable_code:"PoliticalSupplyRenewalApprovalMissing");
            }
            request.disposition=PoliticalResponsibilityDisposition::assume_by_successor;
            request.supply_policy_fact_id=out.policy_fact_id;
        }
        command.decisions.push_back(std::move(request));
    }
    if(!replaced)return fail("PoliticalSupplyRenewalSourceMismatch");
    auto quote=preview_political_responsibility_decisions(x,actor,command);
    if(!quote.ok)return quote.error.kind==ErrorKind::rule?wait(quote.error.original.code):Result{false,{},quote.error};
    out.command=std::move(command);return {true,std::move(out),{}};
}
}
