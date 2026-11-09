// 身后执行只调用原关店唯一资产算法；剩余钱货回死者原私容器，不授予执行人继受或新营业权。
#include "private_shop_liquidation_detail.hpp"
#include "../private_shop_closing_assets.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include <set>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
// 两借口均来自同一Shop/Common，避免给甲世界的授权处分乙世界资产。
Status same_candidate(const PrivateShopContext& shops,const PrivateShopLiquidationContext& estate){return &shops.common==&estate.common&&&shops.common_catalog==&estate.common_catalog&&&shops.state==&estate.shops&&&shops.catalog==&estate.shop_catalog&&!shops.phase?common_success():common_failure("PrivateShopEstateCandidateMismatch");}
bool same_authority(const PrivateShopEstateCancellationSource& a,const PrivateShopEstateCancellationSource& b){return shop_liquidation_detail::same_identity(a.source_identity,b.source_identity)&&a.scenario_id==b.scenario_id&&a.request.shop_id==b.request.shop_id&&a.request.order_id==b.request.order_id&&a.request.original_proprietor_actor_id==b.request.original_proprietor_actor_id&&a.request.executor_actor_id==b.request.executor_actor_id&&a.request.mandate_fact_id==b.request.mandate_fact_id&&a.request.death_notice_binding_fact_id==b.request.death_notice_binding_fact_id&&a.mandate_fact_id==b.mandate_fact_id&&a.acceptance_fact_id==b.acceptance_fact_id&&a.death_fact_id==b.death_fact_id&&a.death_cause_fact_id==b.death_cause_fact_id&&a.death_notice_binding_fact_id==b.death_notice_binding_fact_id&&a.death_notice_handoff_fact_id==b.death_notice_handoff_fact_id&&a.shop_wallet_id==b.shop_wallet_id&&a.shop_inventory_id==b.shop_inventory_id&&a.return_wallet_id==b.return_wallet_id&&a.return_inventory_id==b.return_inventory_id&&a.appointed_tick==b.appointed_tick&&a.accepted_tick==b.accepted_tick&&a.death_tick==b.death_tick&&a.death_recorded_tick==b.death_recorded_tick&&a.observed_tick==b.observed_tick&&a.before_settlement_fact_id==b.before_settlement_fact_id;}
}
Outcome<PrivateShopEstateClosingQuote> preview_private_shop_estate_closing(const PrivateShopContext& shops,const PrivateShopLiquidationContext& estate,const StableId& actor,const PrivateShopEstateClose& command){
    auto valid=same_candidate(shops,estate);if(!valid.ok)return {false,{},valid.error};valid=validate_private_shops(shops);if(!valid.ok)return {false,{},valid.error};valid=validate_private_shop_estate_closings(shops,estate);if(!valid.ok)return {false,{},valid.error};
    auto authority=read_private_shop_estate_closing_authority(estate,actor,command);if(!authority.ok)return {false,{},authority.error};
    const auto* person=shops.common.actor(actor);const auto* shop=shops.state.shops.find(command.shop_id);auto place=shops.common.resolved_location(actor);if(!place.ok)return {false,{},place.error};const auto* at=std::get_if<SettlementLocation>(&place.value);
    if(!person||!shop||!person->alive||!person->available||!person->action_id.empty()||shops.activity.in_open_battle(actor)||shops.activity.in_interaction(actor)||!at||at->place_id!=shop->terms.place_id)return fail<PrivateShopEstateClosingQuote>("PrivateShopEstateExecutorPresenceRequired");
    auto assets=private_shop_closing_assets::preview(shops,command.shop_id);if(!assets.ok)return {false,{},assets.error};
    if(assets.value.actor_id!=authority.value.request.original_proprietor_actor_id||assets.value.to_wallet_id!=authority.value.return_wallet_id||assets.value.to_inventory_id!=authority.value.return_inventory_id)return fail<PrivateShopEstateClosingQuote>("PrivateShopEstateReturnScopeMismatch");
    return {true,{shops.common.state().identity,actor,command,std::move(assets.value),std::move(authority.value)},{}};
}
Outcome<StableId> close_private_shop_estate_in_candidate(CommonStore& common,PrivateShopState& shops,PrivateShopLiquidationState& state,const PrivateShopContext& shop_context,const PrivateShopLiquidationContext& estate,const PrivateShopEstateClosingQuote& quote){
    if(&common!=&estate.common||&shops!=&estate.shops||&state!=&estate.state)return fail<StableId>("PrivateShopEstateCandidateMismatch");
    if(!shop_liquidation_detail::same_identity(common.state().identity,quote.source_identity))return fail<StableId>("StalePrivateShopEstateClosing");
    auto fresh=preview_private_shop_estate_closing(shop_context,estate,quote.actor_id,quote.command);if(!fresh.ok)return {false,{},fresh.error};if(quote.assets.shop_id!=fresh.value.assets.shop_id||quote.assets.actor_id!=fresh.value.assets.actor_id||quote.assets.to_wallet_id!=fresh.value.assets.to_wallet_id||quote.assets.to_inventory_id!=fresh.value.assets.to_inventory_id||!same_authority(quote.authority,fresh.value.authority))return fail<StableId>("StalePrivateShopEstateClosing");
    local_economy_detail::CandidateStateGuard<PrivateShopState> shop_guard(shops);local_economy_detail::CandidateStateGuard<PrivateShopLiquidationState> authority_guard(state);
    auto closed=private_shop_closing_assets::commit_in_candidate(common,shops,shop_context,quote.assets);if(!closed.ok)return closed;
    const auto& a=fresh.value.authority;auto id=common.next_id(private_shop_liquidation_receipt_prefix);if(!id.ok)return id;auto written=common.append_fact({id.value,private_shop_liquidation_receipt_kind,private_shop_liquidation_module});if(!written.ok)return {false,{},written.error};
    PrivateShopEstateClosingRecord receipt{id.value,closed.value,quote.command.shop_id,a.request.original_proprietor_actor_id,quote.actor_id,a.mandate_fact_id,a.acceptance_fact_id,a.death_fact_id,a.death_cause_fact_id,a.death_notice_binding_fact_id,a.death_notice_handoff_fact_id,common.tick()};
    if(!state.closings.insert(id.value,std::move(receipt)))return fail<StableId>("PrivateShopEstateDuplicateClosing");
    written=validate_private_shops(shop_context);if(!written.ok)return {false,{},written.error};written=validate_private_shop_estate_closings(shop_context,estate);if(!written.ok)return {false,{},written.error};shop_guard.commit();authority_guard.commit();return id;
}
Status validate_private_shop_estate_closings(const PrivateShopContext& shops,const PrivateShopLiquidationContext& estate){
    using namespace shop_liquidation_detail;auto valid=same_candidate(shops,estate);if(!valid.ok)return valid;valid=validate_private_shop_liquidation_authorizations(estate);if(!valid.ok)return valid;std::set<StableId> bound;
    for(const auto& row:estate.state.closings.rows()){
        const auto& r=row.second;if(!bound.insert(r.closing_fact_id).second)return common_failure("PrivateShopEstateDuplicateClosing",r.id);
        auto source=read_private_shop_estate_closing_authority_before_fact(estate,r.executor_actor_id,{r.shop_id,r.mandate_fact_id,r.death_notice_binding_fact_id},r.closing_fact_id,r.tick);if(!source.ok)return {false,{},source.error};const auto& a=source.value;
        if(a.request.original_proprietor_actor_id!=r.proprietor_actor_id||a.acceptance_fact_id!=r.acceptance_fact_id||a.death_fact_id!=r.death_fact_id||a.death_cause_fact_id!=r.death_cause_fact_id||a.death_notice_handoff_fact_id!=r.death_notice_handoff_fact_id)return common_failure("PrivateShopEstateClosingAuthorityMismatch",r.id);
    }
    // 反向覆盖所有死后关闭，删去清算副记录也不能只靠旧钱货余额自洽通过。
    for(const auto& row:shops.state.closings.rows()){
        const auto& r=row.second;auto alive=alive_before(estate,r.actor_id,r.tick,r.id);
        if(alive.ok){if(bound.count(r.id))return common_failure("PrivateShopEstateClosedWhileProprietorAlive",r.id);}
        else {if(alive.error.original.code!="PrivateShopExecutorPartyAlreadyDead")return alive;if(!bound.count(r.id))return common_failure("PrivateShopEstateClosingAuthorizationRequired",r.id);}
    }
    return common_success();
}
}
