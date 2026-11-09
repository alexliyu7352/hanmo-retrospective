#include "resident/kernel/catalog_section_observation.hpp"
// 双方生前限店委托沿真实Common事实重放；原店主体和容器只是身份来源，不从当前余额制造清算权。
#include "private_shop_liquidation_detail.hpp"
#include "resident/domains/family/actor_age_read.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident::shop_liquidation_detail {
Outcome<Integer> fact(const CommonStore& c,const StableId& id,const StableId& prefix,const StableId& kind,const StableId& domain){
    const auto n=organization_instance_sequence(id,prefix);const auto* h=c.state().facts.find(id);
    if(!n||*n>=c.state().next_instance_id||!h||h->id!=id||h->kind!=kind||h->domain!=domain)return {false,0,common_failure("PrivateShopExecutorFactMismatch",id).error};
    return {true,*n,{}};
}
bool before(const CommonStore& c,const StableId& a,const StableId& b){const auto& v=c.state().facts.order();const auto x=std::find(v.begin(),v.end(),a),y=std::find(v.begin(),v.end(),b);return x!=v.end()&&y!=v.end()&&x<y;}
const DialogueNode* declaration(const InteractionCatalog& c,const StableId& dialogue,const StableId& node){for(const auto& d:c.dialogues)if(d.id==dialogue)for(const auto& n:d.nodes)if(n.id==node)return &n;return nullptr;}
const PrivateShopExecutorRevocation* revocation(const PrivateShopLiquidationState& s,const StableId& mandate){for(const auto& r:s.revocations.rows())if(r.second.mandate_fact_id==mandate)return &r.second;return nullptr;}
Status alive_before(const PrivateShopLiquidationContext& x,const StableId& actor,Integer tick,const StableId& boundary){
    const auto& c=x.common;const auto* person=c.actor(actor);if(!person||tick<0||tick>c.tick()||!x.death_origin)return common_failure("PrivateShopExecutorLifeHistoryUnavailable",actor);
    if(x.phase&&(c.tick()==INT64_MAX||x.phase->from_tick!=c.tick()||x.phase->to_tick!=c.tick()+1))return common_failure("PrivateShopExecutorPhaseMismatch",actor);
    if(x.phase&&boundary.empty())return common_failure("PrivateShopExecutorRequiresCommittedClock",actor);
    Integer cutoff=c.state().next_instance_id;
    if(boundary.empty()){if(tick!=c.tick())return common_failure("PrivateShopExecutorLifeBoundaryMismatch",actor);}
    else {const auto* h=c.state().facts.find(boundary);if(!h)return common_failure("PrivateShopExecutorLifeBoundaryMismatch",actor);const auto colon=boundary.rfind(':');if(colon==StableId::npos)return common_failure("PrivateShopExecutorLifeBoundaryMismatch",actor);auto n=fact(c,boundary,boundary.substr(0,colon),h->kind,h->domain);if(!n.ok)return {false,{},n.error};cutoff=n.value;}
    auto source=x.death_origin(actor);if(!source.ok)return {false,{},source.error};
    if(!source.value)return person->alive?common_success():common_failure("PrivateShopExecutorLifeHistoryUnavailable",actor);
    const auto& d=*source.value;auto n=fact(c,d.casualty_fact_id,"military","military_casualties","military");
    if(!n.ok)return {false,{},n.error};if(d.actor_id!=actor||d.battle_id.empty()||d.tick<0||d.tick>(x.phase?x.phase->to_tick:c.tick())||person->alive)return common_failure("PrivateShopExecutorLifeSourceMismatch",actor);
    const bool prior=n.value<cutoff;
    if(!boundary.empty()&&(prior!=before(c,d.casualty_fact_id,boundary)||(prior&&d.tick>tick)||(!prior&&d.tick<tick)))return common_failure("PrivateShopExecutorLifeOrderMismatch",actor);
    return boundary.empty()||prior?common_failure("PrivateShopExecutorPartyAlreadyDead",actor):common_success();
}
}
namespace hanmo::resident {
Status validate_private_shop_liquidation_catalog(const PrivateShopLiquidationContext& x){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 15, "PrivateShopLiquidationContext", "validate_private_shop_liquidation_catalog", &x);

    using namespace shop_liquidation_detail;
    if(x.catalog.version!=1||x.state.version!=1)return common_failure("UnsupportedPrivateShopLiquidationVersion");
    for(const auto& row:x.catalog.policies.rows()){
        const auto& p=row.second;if(p.id!=row.first||p.id.empty()||!x.shop_catalog.premises.find(p.premises_id)||!x.common.actor(p.executor_actor_id)||!x.common.actor(p.proprietor_actor_id)||p.proprietor_actor_id==p.executor_actor_id)return common_failure("InvalidPrivateShopExecutorPolicy",row.first);
        const auto* place=x.shop_catalog.premises.find(p.premises_id);
        for(const auto& role_link:{std::make_tuple(p.proprietor_role_id,p.proprietor_actor_id,p.proprietor_dialogue_id),std::make_tuple(p.executor_role_id,p.executor_actor_id,p.executor_dialogue_id)}){const auto* role=x.common_catalog.roles.find(std::get<0>(role_link));if(!role||role->initial_actor!=std::get<1>(role_link)||role->place_id!=place->place_id||!role->dialogue_id||*role->dialogue_id!=std::get<2>(role_link))return common_failure("PrivateShopExecutorRoleDeclarationRequired",p.id);}
        for(const auto& link:{std::make_pair(p.proprietor_dialogue_id,p.proprietor_appointment_node_id),std::make_pair(p.executor_dialogue_id,p.executor_acceptance_node_id)}){const auto* n=declaration(x.interaction_catalog,link.first,link.second);if(!n||n->kind!="say"||n->speaker.empty()||n->speaker=="$player")return common_failure("PrivateShopExecutorDeclarationRequired",p.id);}
    }
    return common_success();
}
Status validate_private_shop_liquidation_authorizations(const PrivateShopLiquidationContext& x){
    using namespace shop_liquidation_detail;const auto& c=x.common;
    auto valid=validate_private_shop_liquidation_catalog(x);if(!valid.ok)return valid;
    std::set<StableId> seen,accepted,ended;std::set<Integer> allocated;
    for(const auto& row:x.state.mandates.rows()){
        const auto& m=row.second;const auto* p=x.catalog.policies.find(m.policy_id);const auto* shop=x.shops.shops.find(m.shop_id);const auto* owner=c.actor(m.proprietor_actor_id);const auto* executor=c.actor(m.executor_actor_id);
        auto n=fact(c,m.id,private_shop_liquidation_mandate_prefix,private_shop_liquidation_mandate_kind,private_shop_liquidation_module);if(!n.ok)return {false,{},n.error};
        if(!allocated.insert(n.value).second||row.first!=m.id||!p||!shop||!owner||!executor||m.executor_actor_id==m.proprietor_actor_id||shop->terms.proprietor_actor_id!=m.proprietor_actor_id||p->premises_id!=shop->terms.premises_id||p->executor_actor_id!=m.executor_actor_id||p->proprietor_actor_id!=m.proprietor_actor_id||m.shop_wallet_id!=shop->wallet_id||m.shop_inventory_id!=shop->inventory_id||m.return_wallet_id!=owner->wallet_id||m.return_inventory_id!=owner->inventory_id||m.appointed_tick<shop->opened_tick||m.appointed_tick>c.tick()||!before(c,shop->id,m.id))return common_failure("PrivateShopExecutorMandateMismatch",row.first);
        auto opening=fact(c,shop->id,private_shop_open_prefix,private_shop_open_kind,private_shop_module);if(!opening.ok)return {false,{},opening.error};if(opening.value>=n.value)return common_failure("PrivateShopExecutorMandateOrderMismatch",m.id);
        for(const auto& closed:x.shops.closings.rows())if(closed.second.shop_id==shop->id&&(closed.second.tick<m.appointed_tick||before(c,closed.first,m.id)))return common_failure("PrivateShopExecutorAppointedAfterClosing",m.id);
        const Owner private_owner{Owner::Kind::actor,m.proprietor_actor_id,{}};
        for(const auto& wallet:{m.shop_wallet_id,m.return_wallet_id}){const auto* a=c.wallet(wallet);if(!a||!owner_equal(a->owner,private_owner))return common_failure("PrivateShopExecutorPrivateAssetsRequired",m.id);}
        for(const auto& inventory:{m.shop_inventory_id,m.return_inventory_id}){const auto* a=c.inventory(inventory);if(!a||!owner_equal(a->owner,private_owner))return common_failure("PrivateShopExecutorPrivateAssetsRequired",m.id);}
        const auto* a=x.state.acceptances.find(m.acceptance_fact_id);if(!a||!accepted.insert(a->id).second)return common_failure("PrivateShopExecutorAcceptanceRequired",m.id);
        auto an=fact(c,a->id,private_shop_liquidation_acceptance_prefix,private_shop_liquidation_acceptance_kind,private_shop_liquidation_module);if(!an.ok)return {false,{},an.error};
        if(!allocated.insert(an.value).second||a->id!=m.acceptance_fact_id||a->mandate_fact_id!=m.id||a->executor_actor_id!=m.executor_actor_id||a->tick!=m.appointed_tick||an.value!=n.value+1||!before(c,m.id,a->id))return common_failure("PrivateShopExecutorAcceptanceMismatch",m.id);
        const auto& meeting=m.agreement;const auto session=organization_instance_sequence(meeting.session_id,"interaction");if(!session||*session>=n.value)return common_failure("PrivateShopExecutorSessionSourceMismatch",m.id);const auto requester=m.requested_by_executor?m.executor_actor_id:m.proprietor_actor_id;const auto provider=m.requested_by_executor?m.proprietor_actor_id:m.executor_actor_id;
        if(meeting.actor_id!=requester||meeting.provider_actor_id!=provider||meeting.session_id.empty()||meeting.place_id!=shop->terms.place_id||meeting.role_id!=(m.requested_by_executor?p->proprietor_role_id:p->executor_role_id)||meeting.dialogue_id!=(m.requested_by_executor?p->proprietor_dialogue_id:p->executor_dialogue_id)||meeting.node_id!=(m.requested_by_executor?p->proprietor_appointment_node_id:p->executor_acceptance_node_id))return common_failure("PrivateShopExecutorMeetingMismatch",m.id);
        const auto* role=x.common_catalog.roles.find(meeting.role_id);if(!role||role->place_id!=meeting.place_id||!role->dialogue_id||*role->dialogue_id!=meeting.dialogue_id)return common_failure("PrivateShopExecutorRoleMismatch",m.id);
        const auto* node=declaration(x.interaction_catalog,meeting.dialogue_id,meeting.node_id);if(!node||(node->speaker!="$provider"&&node->speaker!=provider))return common_failure("PrivateShopExecutorSpeakerMismatch",m.id);
        for(const auto& party:{m.proprietor_actor_id,m.executor_actor_id}){valid=alive_before(x,party,a->tick,a->id);if(!valid.ok)return valid;auto age=read_actor_age_before_fact(c,x.common_catalog,party,a->tick,a->id);if(!age.ok)return {false,{},age.error};if(!age.value.adult)return common_failure("PrivateShopExecutorAdultRequired",m.id);}
        const auto* terminal=revocation(x.state,m.id);
        if(terminal){auto rn=fact(c,terminal->id,private_shop_liquidation_revocation_prefix,private_shop_liquidation_revocation_kind,private_shop_liquidation_module);if(!rn.ok)return {false,{},rn.error};if(!allocated.insert(rn.value).second||!ended.insert(terminal->id).second||rn.value<=an.value||!before(c,a->id,terminal->id)||terminal->tick<a->tick||terminal->tick>c.tick()||!((terminal->decision=="revoke"&&terminal->actor_id==m.proprietor_actor_id)||(terminal->decision=="withdraw"&&terminal->actor_id==m.executor_actor_id)))return common_failure("PrivateShopExecutorRevocationMismatch",m.id);for(const auto& party:{m.proprietor_actor_id,m.executor_actor_id}){valid=alive_before(x,party,terminal->tick,terminal->id);if(!valid.ok)return valid;}}
        for(const auto& other:x.state.mandates.rows())if(other.first!=m.id&&other.second.shop_id==m.shop_id&&before(c,other.first,m.id)){const auto* revoked=revocation(x.state,other.first);if(!revoked||!before(c,revoked->id,m.id)||revoked->tick>m.appointed_tick)return common_failure("PrivateShopExecutorOverlappingMandates",m.id);}
        seen.insert(m.id);seen.insert(a->id);if(terminal)seen.insert(terminal->id);
    }
    if(accepted.size()!=x.state.acceptances.size()||ended.size()!=x.state.revocations.size())return common_failure("PrivateShopExecutorRegistryMismatch");
    for(const auto& row:x.state.acceptances.rows())if(row.first!=row.second.id)return common_failure("PrivateShopExecutorAcceptanceKeyMismatch",row.first);
    for(const auto& row:x.state.revocations.rows())if(row.first!=row.second.id)return common_failure("PrivateShopExecutorRevocationKeyMismatch",row.first);
    for(const auto& row:x.state.closings.rows()){
        const auto& r=row.second;const auto* m=x.state.mandates.find(r.mandate_fact_id);const auto* closed=x.shops.closings.find(r.closing_fact_id);auto n=fact(c,r.id,private_shop_liquidation_receipt_prefix,private_shop_liquidation_receipt_kind,private_shop_liquidation_module);if(!n.ok)return {false,{},n.error};auto original=fact(c,r.closing_fact_id,"private_shop_closed","private_shop_closed",private_shop_module);if(!original.ok)return {false,{},original.error};
        if(!allocated.insert(n.value).second||row.first!=r.id||!m||!closed||r.shop_id!=m->shop_id||r.shop_id!=closed->shop_id||r.proprietor_actor_id!=m->proprietor_actor_id||r.executor_actor_id!=m->executor_actor_id||r.acceptance_fact_id!=m->acceptance_fact_id||r.tick!=closed->tick||n.value!=original.value+1||!before(c,r.closing_fact_id,r.id)||!seen.insert(r.id).second)return common_failure("PrivateShopEstateClosingSourceMismatch",r.id);
    }
    for(const auto& row:c.state().facts.rows())if(row.second.domain==private_shop_liquidation_module&&!seen.count(row.first))return common_failure("PrivateShopExecutorOrphanFact",row.first);
    return common_success();
}
}
