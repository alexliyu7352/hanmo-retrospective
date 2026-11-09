// 只复用原真实认领夹具；占领授权读由独立S5组件/World证明，本门核Affairs消费边界。
#define main existing_dynamic_affairs_main
#include "dynamic_affairs_test.cpp"
#undef main
#ifdef HANMO_SUPPLY_SUSPENSION_ADAPTER
#include "resident/domains/affairs/affairs_supply_suspension.hpp"
#endif
int main(){try{
    Fixture f;auto quote=require(preview_need_claim(f.common,f.affairs,f.tasks,f.known,f.need_id(),2));
    f.affairs.affairs.rows().front().second.phase=AffairPhase::suspended;
    const auto before=f.common.state().next_instance_id;
    auto refused=prepare_need_claim(f.common,f.affairs,f.tasks,quote.need_id,quote.amount,quote.assignee,quote.executor_actor_id,quote.offer_id,quote.need_revision);
    check(!refused.ok&&refused.error.original.code=="NeedSourceSuspended","existing published offer cannot bypass suspended original source");
    auto stale=claim_need(f.common,f.affairs,f.tasks,quote);check(!stale.ok&&f.common.state().next_instance_id==before,"pre-suspension quote cannot create task, claim, escrow or IDs");
    check(query_opportunities(f.affairs,f.tasks,f.known).empty(),"known suspended source is not shown as an available opportunity");
    require(publish_need_offers(f.common,f.affairs,f.affairs_catalog,f.tasks,f.task_catalog,f.common.tick()));
    check(f.tasks.offer_instances.find(f.offer_id())->status==OfferStatus::withdrawn,"republish keeps old offer withdrawn");
#ifdef HANMO_SUPPLY_SUSPENSION_ADAPTER
    Fixture g;const auto task=g.accept();const auto contract_id=*g.tasks.tasks.find(task)->contract_id;g.buy(task,2);auto report=g.begin(g.command("report_offer",1));g.finish(report,2);const auto contributions=g.affairs.contributions.size();
    const auto claims=g.affairs.claims.size(),contracts=g.tasks.contracts.size();const auto escrow=g.common.wallet(g.tasks.tasks.find(task)->budget_wallet_id)->balance;
    require(g.common.append_fact({"registered","political_public_responsibilities_pending","politics"}));
    const auto& policy=g.affairs_catalog.policies.rows().front().second;
    std::vector<EconomicSupplySuspension> suspended{{policy.source_key,policy.organization_id,policy.site_id,policy.inventory_id,policy.item_id,"registered","PoliticalDecisionPending"}};
    require(reconcile_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,{},g,g.common.tick(),
#ifndef HANMO_SUPPLY_ONLY_COMPONENT
        nullptr,
#endif
        &suspended));
    require(publish_need_offers(g.common,g.affairs,g.affairs_catalog,g.tasks,g.task_catalog,g.common.tick()));
    check(g.affairs.affairs.rows().front().second.phase==AffairPhase::suspended,"exact prior supply identity suspends original affair");
    check(g.affairs.claims.size()==claims&&g.tasks.contracts.size()==contracts&&g.common.wallet(g.tasks.tasks.find(task)->budget_wallet_id)->balance==escrow&&g.tasks.contracts.find(contract_id)->issuer.id=="org.city"&&contributions==1&&g.affairs.contributions.size()==contributions,"accepted contract, issuer, claim, contribution and cash survive source suspension");
    require(validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&suspended));
    g.common.state().inventories.find("store")->owner={Owner::Kind::organization,"captor",{}};
    require(validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&suspended));
    check(!validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks).ok,"foreign inventory without exact source proof remains invalid");
    auto malformed=suspended;malformed[0].original_organization_id="captor";check(!validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&malformed).ok,"new owner cannot replace original beneficiary in suspension proof");
    malformed=suspended;malformed[0].item_id="other";check(!read_affairs_suspended_policies(g.common,g.affairs_catalog,&malformed).ok,"same source with different item is rejected");
    malformed=suspended;malformed.push_back(malformed.front());check(!read_affairs_suspended_policies(g.common,g.affairs_catalog,&malformed).ok,"duplicate suspension source is rejected");
    malformed=suspended;malformed[0].registration_fact_id="missing";check(!read_affairs_suspended_policies(g.common,g.affairs_catalog,&malformed).ok,"missing real registration header is rejected");
    g.tasks.offer_instances.find(g.offer_id())->status=OfferStatus::published;check(!validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&suspended).ok,"cold forged published suspended offer is rejected");
    g.tasks.offer_instances.find(g.offer_id())->status=OfferStatus::withdrawn;g.affairs.affairs.rows().front().second.phase=AffairPhase::active;check(!validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&suspended).ok,"cold removed suspension is rejected");
    Fixture h;auto prior_quote=require(preview_need_claim(h.common,h.affairs,h.tasks,h.known,h.need_id(),1));require(h.common.append_fact({"registered","political_public_responsibilities_pending","politics"}));const auto assets_before=encode_common_state(h.common.state());
    require(project_affairs_supply_suspensions_in_candidate(h.common,h.affairs,h.affairs_catalog,h.tasks,suspended));
    check(compare_boundary(assets_before,encode_common_state(h.common.state())).exact()&&h.tasks.contracts.empty()&&h.affairs.claims.empty(),"zero-time suspension projection cannot write money, goods, facts, IDs or contracts");
    auto projected_claim=claim_need(h.common,h.affairs,h.tasks,prior_quote);check(!projected_claim.ok&&projected_claim.error.original.code=="NeedSourceSuspended"&&compare_boundary(assets_before,encode_common_state(h.common.state())).exact(),"reclassified source rejects a previously prepared actual claim without writes");
    require(validate_affairs(h.common,h.affairs,h.affairs_catalog,h.tasks,nullptr,nullptr,nullptr,&suspended));
    Fixture bad;require(bad.common.append_fact({"registered","political_public_responsibilities_pending","politics"}));bad.tasks.offer_instances.find(bad.offer_id())->issuer.id="captor";const auto affairs_before=encode_affairs_state(bad.affairs);const auto tasks_before=encode_dynamic_task_state(bad.tasks);auto corrupt=project_affairs_supply_suspensions_in_candidate(bad.common,bad.affairs,bad.affairs_catalog,bad.tasks,suspended);check(!corrupt.ok&&compare_boundary(affairs_before,encode_affairs_state(bad.affairs)).exact()&&compare_boundary(tasks_before,encode_dynamic_task_state(bad.tasks)).exact(),"bad bound offer is rejected before changing any existing projection");
    // 零耗时原主体续办批准后，旧暂停可保守等到原半日边界，不能阻塞政治提交。
    g.common.state().inventories.find("store")->owner=organization();g.affairs.affairs.rows().front().second.phase=AffairPhase::suspended;
    std::vector<EconomicSupplySuspension> resumed;
    require(validate_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,nullptr,nullptr,nullptr,&resumed));
    auto still_paused=preview_need_claim(g.common,g.affairs,g.tasks,g.known,g.need_id(),1);check(!still_paused.ok&&still_paused.error.original.code=="NeedSourceSuspended","new approval permits conservative cold pause until original boundary");
    require(reconcile_affairs(g.common,g.affairs,g.affairs_catalog,g.tasks,{},g,g.common.tick(),
#ifndef HANMO_SUPPLY_ONLY_COMPONENT
        nullptr,
#endif
        &resumed));
    require(publish_need_offers(g.common,g.affairs,g.affairs_catalog,g.tasks,g.task_catalog,g.common.tick()));
    check(g.affairs.affairs.rows().front().second.phase==AffairPhase::active&&g.tasks.offer_instances.find(g.offer_id())->status==OfferStatus::published&&g.affairs.contributions.size()==contributions,"original boundary resumes published source while retaining accepted history");
#endif
}catch(const std::exception& error){++failures;std::cerr<<"EXCEPTION "<<error.what()<<'\n';}std::cout<<"AFFAIRS_SUPPLY_SUSPENSION "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;}
