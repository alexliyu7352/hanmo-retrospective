// 当地真实Office本人按具名作者方案提出原签署命令；已受领Task和原行动不被经营意图抢占。
#include "politics_supply_renewal_director.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_supply_renewal(
    const PoliticalTakeoverContext& x,const EconomicSupplyPolicyContext& economic,const KnownSubjectView& known,
    const DirectorPolicy& policy,const PoliticalSupplyRenewalIntent& intent,const DecisionProposal& base,Integer limit){
    using Result=Outcome<std::optional<DecisionProposal>>;
    if(!base.transition||base.subject_id!=known.actor_id||known.subject.kind!=SubjectKind::actor
        ||known.subject.id!=known.actor_id||known.tick!=x.common.tick()||known.revision!=x.common.revision()
        ||base.transition->expected_revision!=known.revision||limit<1||limit>64)
        return {false,{},common_failure("InvalidSupplyRenewalDirectorProposal",known.actor_id,ErrorKind::internal).error};
    if(base.command||base.transition->current_action_id||!base.transition->commitment_ids.empty()
        ||base.transition->template_id=="maintain_commitment"||known.actor_id==x.common.state().metadata.player_actor_id
        ||!known.alive||!known.available||!known.current_action_id.empty()||known.interaction_busy)
        return {true,std::nullopt,{}};
    const auto* rule=x.catalog.rules.find(intent.takeover_rule_id);const auto* defined=economic.ledger.catalog.policies.find(intent.author_policy_id);
    if(!rule||!defined||defined->organization_id!=policy.organization_id)
        return {false,{},common_failure("PoliticalSupplyRenewalIntentMismatch",intent.id).error};
    if(known.place_id!=rule->site_id)return {true,std::nullopt,{}};
    Integer scanned=0;const auto& order=x.state.registrations.order();
    for(auto i=order.rbegin();i!=order.rend()&&scanned<limit;++i,++scanned){
        const auto* r=x.state.registrations.find(*i);if(!r)return {false,{},common_failure("InvalidPoliticalTakeoverState",*i).error};
        if(r->rule_id!=intent.takeover_rule_id||r->successor_organization_id!=policy.organization_id)continue;
        auto choice=propose_political_supply_renewal(x,economic,known.actor_id,intent,r->id);
        if(!choice.ok)return {false,{},choice.error};
        if(choice.value.wait_code=="PoliticalSupplyRenewalAlreadyAdopted"||choice.value.wait_code=="PoliticalTakeoverNotKnown")return {true,std::nullopt,{}};
        auto out=base;auto transition=std::make_shared<DirectorTransition>(*base.transition);out.transition=transition;
        transition->template_id="political_supply_renewal";transition->goal_ref=r->id;transition->next_step_index=0;
        transition->current_action_id.reset();transition->wait_reason.clear();out.reason_fact_id=r->id;
        if(const auto* command=std::get_if<IssueEconomicSupplyPolicyCommand>(&choice.value.command)){
            out.command=std::make_shared<const CommandPayload>(CommandPayload{political_supply_policy_grant,*command});transition->phase=PlanPhase::completed;
        }else if(const auto* command=std::get_if<PoliticalTakeoverDecisionCommand>(&choice.value.command)){
            out.command=std::make_shared<const CommandPayload>(CommandPayload{political_takeover_command_kind,*command});transition->phase=PlanPhase::completed;
        }else {transition->phase=PlanPhase::waiting;transition->wait_reason=choice.value.wait_code;}
        return {true,std::move(out),{}};
    }
    return {true,std::nullopt,{}};
}
}
