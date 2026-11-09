// 双向当面办理均使用真实发起者，不交换会话身份；委任与接受在同一外层候选形成两份来源。
#include "private_shop_liquidation_detail.hpp"
#include "resident/domains/family/actor_age_read.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
// 这里只证当前具名双方可作决定，不能将成年或空闲当作对方接受。
Status available(const PrivateShopLiquidationContext& x,const ActivityRead& activity,const StableId& actor){
    const auto* a=x.common.actor(actor);if(!a||!a->alive||!a->available||!a->action_id.empty()||activity.in_open_battle(actor))return common_failure("PrivateShopExecutorPartyUnavailable",actor);
    auto life=shop_liquidation_detail::alive_before(x,actor,x.common.tick());if(!life.ok)return life;
    auto age=read_actor_age(x.common,x.common_catalog,actor);if(!age.ok)return {false,{},age.error};return age.value.adult?common_success():common_failure("PrivateShopExecutorAdultRequired",actor);
}
}
Outcome<PrivateShopExecutorQuote> preview_private_shop_executor(const PrivateShopLiquidationContext& x,const PrivateShopMeetingContext& meeting,const ActivityRead& activity,const StableId& actor,const PrivateShopExecutorCommand& command){
    using namespace shop_liquidation_detail;auto valid=validate_private_shop_liquidation_authorizations(x);if(!valid.ok)return {false,{},valid.error};
    PrivateShopExecutorQuote quote;quote.source_identity=x.common.state().identity;quote.actor_id=actor;quote.command=command;
    if(const auto* revoke=std::get_if<PrivateShopRevokeExecutor>(&command)){
        const auto* m=x.state.mandates.find(revoke->mandate_fact_id);if(!m||!((revoke->decision=="revoke"&&actor==m->proprietor_actor_id)||(revoke->decision=="withdraw"&&actor==m->executor_actor_id)))return fail<PrivateShopExecutorQuote>("PrivateShopExecutorDecisionDenied");
        if(revocation(x.state,m->id))return fail<PrivateShopExecutorQuote>("PrivateShopExecutorAlreadyRevoked");
        for(const auto& party:{m->proprietor_actor_id,m->executor_actor_id}){valid=alive_before(x,party,x.common.tick());if(!valid.ok)return {false,{},valid.error};}
        valid=available(x,activity,actor);if(!valid.ok)return {false,{},valid.error};
        quote.shop_id=m->shop_id;quote.policy_id=m->policy_id;quote.proprietor_actor_id=m->proprietor_actor_id;quote.executor_actor_id=m->executor_actor_id;return {true,std::move(quote),{}};
    }
    const bool executor=std::holds_alternative<PrivateShopAcceptExecutorship>(command);
    const auto shop_id=executor?std::get<PrivateShopAcceptExecutorship>(command).shop_id:std::get<PrivateShopAppointExecutor>(command).shop_id;
    const auto policy_id=executor?std::get<PrivateShopAcceptExecutorship>(command).policy_id:std::get<PrivateShopAppointExecutor>(command).policy_id;
    const auto* shop=x.shops.shops.find(shop_id);const auto* p=x.catalog.policies.find(policy_id);
    if(!shop||shop->status=="closed"||!p||p->premises_id!=shop->terms.premises_id)return fail<PrivateShopExecutorQuote>("PrivateShopExecutorPolicyMismatch");
    const auto& owner=shop->terms.proprietor_actor_id;const auto& other=p->executor_actor_id;
    if(owner!=p->proprietor_actor_id||owner==other||actor!=(executor?other:owner))return fail<PrivateShopExecutorQuote>("PrivateShopExecutorPartyRequired");
    for(const auto& prior:x.state.mandates.rows())if(prior.second.shop_id==shop_id&&!revocation(x.state,prior.first))return fail<PrivateShopExecutorQuote>("PrivateShopExecutorAlreadyAppointed");
    for(const auto& party:{owner,other}){valid=available(x,activity,party);if(!valid.ok)return {false,{},valid.error};}
    auto proof=preview_authored_provider_source(x.common,meeting.state,x.interaction_catalog,meeting.access,actor,executor?owner:other);if(!proof.ok)return {false,{},proof.error};
    const auto& source=proof.value.meeting;
    if(source.context.place_id!=shop->terms.place_id||source.context.role_id!=(executor?p->proprietor_role_id:p->executor_role_id)||source.dialogue_id!=(executor?p->proprietor_dialogue_id:p->executor_dialogue_id)||source.node_id!=(executor?p->proprietor_appointment_node_id:p->executor_acceptance_node_id))return fail<PrivateShopExecutorQuote>("PrivateShopExecutorDeclarationRequired");
    quote.shop_id=shop_id;quote.policy_id=policy_id;quote.proprietor_actor_id=owner;quote.executor_actor_id=other;quote.agreement=std::move(proof.value);return {true,std::move(quote),{}};
}
Outcome<PrivateShopExecutorReceipt> apply_private_shop_executor_in_candidate(CommonStore& c,PrivateShopLiquidationState& state,const PrivateShopLiquidationContext& x,const PrivateShopMeetingContext& meeting,const ActivityRead& activity,const PrivateShopExecutorQuote& quote){
    using namespace shop_liquidation_detail;
    if(&c!=&x.common||&state!=&x.state)return fail<PrivateShopExecutorReceipt>("PrivateShopExecutorCandidateMismatch");
    if(!same_identity(c.state().identity,quote.source_identity))return fail<PrivateShopExecutorReceipt>("StalePrivateShopExecutorQuote");
    auto fresh=preview_private_shop_executor(x,meeting,activity,quote.actor_id,quote.command);if(!fresh.ok)return {false,{},fresh.error};const auto& q=fresh.value;
    if(q.shop_id!=quote.shop_id||q.policy_id!=quote.policy_id||q.proprietor_actor_id!=quote.proprietor_actor_id||q.executor_actor_id!=quote.executor_actor_id||bool(q.agreement)!=bool(quote.agreement))return fail<PrivateShopExecutorReceipt>("StalePrivateShopExecutorQuote");
    if(quote.agreement){auto valid=revalidate_authored_provider_source(c,meeting.state,x.interaction_catalog,meeting.access,*quote.agreement);if(!valid.ok)return {false,{},valid.error};const auto& a=quote.agreement->meeting;const auto& b=q.agreement->meeting;if(a.actor_id!=b.actor_id||a.provider_actor_id!=b.provider_actor_id||a.session_id!=b.session_id||a.dialogue_id!=b.dialogue_id||a.node_id!=b.node_id||a.context.place_id!=b.context.place_id||a.context.role_id!=b.context.role_id)return fail<PrivateShopExecutorReceipt>("StalePrivateShopExecutorQuote");}
    local_economy_detail::CandidateStateGuard<PrivateShopLiquidationState> guard(state);
    if(const auto* revoke=std::get_if<PrivateShopRevokeExecutor>(&quote.command)){
        auto id=c.next_id(private_shop_liquidation_revocation_prefix);if(!id.ok)return {false,{},id.error};auto valid=c.append_fact({id.value,private_shop_liquidation_revocation_kind,private_shop_liquidation_module});if(!valid.ok)return {false,{},valid.error};
        if(!state.revocations.insert(id.value,{id.value,revoke->mandate_fact_id,quote.actor_id,revoke->decision,c.tick()}))return fail<PrivateShopExecutorReceipt>("PrivateShopExecutorDuplicateDecision");
        valid=validate_private_shop_liquidation_authorizations(x);if(!valid.ok)return {false,{},valid.error};guard.commit();return {true,{revoke->mandate_fact_id,{},id.value},{}};
    }
    const auto* shop=x.shops.shops.find(q.shop_id);const auto* owner=c.actor(q.proprietor_actor_id);
    auto id=c.next_id(private_shop_liquidation_mandate_prefix);if(!id.ok)return {false,{},id.error};auto valid=c.append_fact({id.value,private_shop_liquidation_mandate_kind,private_shop_liquidation_module});if(!valid.ok)return {false,{},valid.error};
    auto accepted=c.next_id(private_shop_liquidation_acceptance_prefix);if(!accepted.ok)return {false,{},accepted.error};valid=c.append_fact({accepted.value,private_shop_liquidation_acceptance_kind,private_shop_liquidation_module});if(!valid.ok)return {false,{},valid.error};
    PrivateShopExecutorMandate m;m.id=id.value;m.policy_id=q.policy_id;m.shop_id=q.shop_id;m.proprietor_actor_id=q.proprietor_actor_id;m.executor_actor_id=q.executor_actor_id;m.shop_wallet_id=shop->wallet_id;m.shop_inventory_id=shop->inventory_id;m.return_wallet_id=owner->wallet_id;m.return_inventory_id=owner->inventory_id;m.acceptance_fact_id=accepted.value;m.appointed_tick=c.tick();m.requested_by_executor=std::holds_alternative<PrivateShopAcceptExecutorship>(quote.command);
    const auto& p=q.agreement->meeting;m.agreement={p.actor_id,p.provider_actor_id,p.session_id,p.dialogue_id,p.node_id,p.context.place_id,p.context.role_id};
    if(!state.mandates.insert(m.id,m)||!state.acceptances.insert(accepted.value,{accepted.value,m.id,m.executor_actor_id,c.tick()}))return fail<PrivateShopExecutorReceipt>("PrivateShopExecutorDuplicateMandate");
    valid=validate_private_shop_liquidation_authorizations(x);if(!valid.ok)return {false,{},valid.error};guard.commit();return {true,{m.id,accepted.value,{}},{}};
}
}
