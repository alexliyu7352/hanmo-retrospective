// 原住宿、雇工、交易规则的常驻实现；预付/工资预留/货币与lot转移只用共同资源recipe。
#include "resident/storage/money_transfer_recipe.hpp"
#include "growth_internal.hpp"
#include "resident/domains/social/social_relation_execution.hpp"
#include "growth_market_read.hpp"
#include "resident/model/procurement_timing.hpp"
#include "growth_life_results.hpp"
#include "growth_market_capture.hpp"
#include "growth_scalar_fifo.hpp"
#include "study_service_sources.hpp"
#include "growth_recovery_services.hpp"
#include "growth_terminal_recovery_write_v1.hpp"
#include <algorithm>
namespace hanmo::resident::growth {
    namespace {
        template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};
        }
        template<class T> Outcome<T> error(const RuleError& e){return {false,{},e};
        }
        Integer floor_div(Integer n,Integer d){auto q=n/d;
            return n<0&&n%d?q-1:q;
        }
        Integer ceil_div(Integer n,Integer d){return (n+d-1)/d;
        }
        // 原公式在这两端已经恒等上下限；先短路避免合法大库存乘法溢出。
        Integer marginal(Integer base,Integer stock){if(stock>=200)return ceil_div(base,2);if(stock<=-100)return 2*base;
            return std::clamp(base+floor_div(base*5000*(100-stock),1000000),ceil_div(base,2),2*base);
        }
        Integer price_impl(Integer base,Integer stock,Integer quantity,bool buy,Integer discount){Integer total=0;
            for(Integer i=0;
            i<quantity;
            ++i)total+=buy?ceil_div(marginal(base,stock-i-1)*(11500-discount),10000):floor_div(marginal(base,stock+i)*(8500+discount),10000);
            return total;
        }
        bool same_terms(const ServiceTerms& a,const ServiceTerms& b){return a.duration==b.duration&&a.cost==b.cost&&a.wage==b.wage&&a.hp_gain==b.hp_gain&&a.fatigue_delta==b.fatigue_delta&&a.wallet_id==b.wallet_id;
        }
    }
    Integer trade_price(Integer base,Integer stock,Integer quantity,bool buy,Integer discount){return price_impl(base,stock,quantity,buy,discount);
    }
// 接触与财产归属沿原首错顺序，付款方能是被任务owner明确授权的公款钱包。
    Outcome<GrowthQuote> preview_service_trade(const CommonStore& common,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& id,const GrowthCommand& command,const StableId& own){
        GrowthQuote q;
        q.command=command;
        const auto* actor=common.state().actors.find(id);
        if(!actor)return fail<GrowthQuote>("UnknownActor");
        if(command.kind=="service"){
            if(command.activity!="rest"&&command.activity!="work")return fail<GrowthQuote>("UnknownService");
            if(!command.context)return fail<GrowthQuote>("InvalidInteractionContext");
            auto contact=access.contact(id,*command.context,command.activity,own);
            if(!contact.ok)return error<GrowthQuote>(contact.error);
            auto* role=catalog.roles.find(contact.value.role_id);
            auto* terms=role?role->activities.find(command.activity):nullptr;
            if(!terms)return fail<GrowthQuote>("ServiceNotOffered");
            auto* definition=shared.roles.find(contact.value.role_id);
            auto* payer=common.wallet(actor->wallet_id);
            auto* provider=common.wallet(terms->wallet_id);
            if(!payer||!provider)return fail<GrowthQuote>("UnknownWallet");
            if(payer->id==provider->id)return fail<GrowthQuote>("SelfService");
            Owner owner=definition&&definition->organization_id?Owner{Owner::Kind::organization,*definition->organization_id,{}, {}}:Owner{Owner::Kind::actor,contact.value.provider_actor_id,{}, {}};
            if(!owner_equal(provider->owner,owner))return fail<GrowthQuote>("ServiceAuthority");
            auto available=common.spendable(payer->id);
            if(!available.ok)return error<GrowthQuote>(available.error);
            if(available.value<terms->cost)return fail<GrowthQuote>("InsufficientMoney");
            available=common.spendable(provider->id);
            if(!available.ok)return error<GrowthQuote>(available.error);
            if(available.value<terms->wage)return fail<GrowthQuote>("EmployerCannotPay");
            if(provider->balance>2147483647-terms->cost||payer->balance>2147483647-terms->wage)return fail<GrowthQuote>("MoneyOverflow");
            q.duration=terms->duration;
            q.cost=terms->cost;
            q.wage=terms->wage;
            q.context=contact.value;
            q.data=*terms;
            if(access.personal_recovery){auto qualified=access.personal_recovery->preview(common,id,q);if(!qualified.ok)return error<GrowthQuote>(qualified.error);}
            return {true,std::move(q),{}};
        }
        if(command.bargain_quote_id)return {false,{},common_failure("UnsupportedNativeCapability","bargain_quote_id",ErrorKind::unsupported_capability).error};
        auto* shop=catalog.shops.find(command.shop_id);
        auto* item=shared.items.find(command.item_id);
        if(!shop||!item||(command.side!="buy"&&command.side!="sell")||command.quantity<1||command.quantity>500)return fail<GrowthQuote>("InvalidTrade");
        if(!command.context)return fail<GrowthQuote>("InvalidInteractionContext");
        auto contact=access.contact(id,*command.context,"trade",own);
        if(!contact.ok)return error<GrowthQuote>(contact.error);
        if(contact.value.place_id!=shop->place_id||std::find(shop->representative_roles.begin(),shop->representative_roles.end(),contact.value.role_id)==shop->representative_roles.end())return fail<GrowthQuote>("WrongShopRepresentative");
        if(item->base_price<=0||std::find(catalog.tradeable_items.begin(),catalog.tradeable_items.end(),command.item_id)==catalog.tradeable_items.end())return fail<GrowthQuote>("NotTradeable");
        auto* payer=common.wallet(command.payer_wallet_id);
        if(!payer)return fail<GrowthQuote>("UnknownWallet");
        Owner owner{Owner::Kind::actor,id,{}, {}};
        if(!owner_equal(payer->owner,owner)){if(payer->owner.kind!=Owner::Kind::task||command.side!="buy"||!access.procurement_authorized||!access.procurement_authorized(id,payer->owner.id,item->id))return fail<GrowthQuote>("PublicGoodsRestricted");
            owner=payer->owner;
        }
        auto* stock=common.inventory(shop->inventory_id);
        auto* bag=common.inventory(actor->inventory_id);
        auto* merchant=common.wallet(shop->wallet_id);
        if(!stock||!bag||!merchant)return fail<GrowthQuote>("MissingTradeAssets");
        auto count=common.count(stock->id,item->id,{stock->owner});
        if(!count.ok)return error<GrowthQuote>(count.error);
        const auto total=trade_price(item->base_price,count.value,command.quantity,command.side=="buy");
        if(command.quoted_total&&(*command.quoted_total!=total||!command.market_revision||*command.market_revision!=stock->revision)){
            auto stale=fail<GrowthQuote>("StaleQuote");
            stale.error.new_total=total;
            return stale;
        }
        if(command.side=="buy"){if(count.value<command.quantity)return fail<GrowthQuote>("InsufficientStock");
            auto funds=common.spendable(payer->id);
            if(!funds.ok)return error<GrowthQuote>(funds.error);
            if(funds.value<total)return fail<GrowthQuote>("InsufficientFunds");
            auto weight=common.inventory_weight(bag->id);
            if(!weight.ok)return error<GrowthQuote>(weight.error);
            if(weight.value+command.quantity*item->weight>bag->capacity)return fail<GrowthQuote>("InsufficientCapacity");
        }
        else{count=common.count(bag->id,item->id,{owner});
            if(!count.ok)return error<GrowthQuote>(count.error);
            if(count.value<command.quantity)return fail<GrowthQuote>("InsufficientGoods");
            auto funds=common.spendable(merchant->id);
            if(!funds.ok)return error<GrowthQuote>(funds.error);
            if(funds.value<total)return fail<GrowthQuote>("ShopInsufficientFunds");
            auto weight=common.inventory_weight(stock->id);
            if(!weight.ok)return error<GrowthQuote>(weight.error);
            if(weight.value+command.quantity*item->weight>stock->capacity)return fail<GrowthQuote>("ShopCapacity");
        }
        TradeTerms terms;
        terms.shop_id=shop->id;
        terms.item_id=item->id;
        terms.quantity=command.quantity;
        terms.side=command.side;
        terms.payer_wallet_id=payer->id;
        terms.owner=owner;
        terms.total=total;
        terms.market_revision=stock->revision;
        terms.valid_tick=common.tick();
        terms.context=contact.value;
        q.duration=trade_action_ticks;
        q.context=contact.value;
        q.data=std::move(terms);
        return {true,std::move(q),{}};
    }
// 开工支付住宿并预留工资；交易在原开工点真实转移，之后半日绝不再次收费。
    Status start_service_trade(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const ActionCoreView& action,const GrowthQuote& q,MarketBusinessCaptureServices* market,std::optional<Integer> actual_started_tick,const GrowthExactStartContext* actual_start){ResourceRecipe recipe;
        CheckpointMemoryCharge actual_leg_fields;
        std::vector<MarketQuantityLeg> actual_legs;
        std::unique_ptr<MarketBusinessCaptureScope> capture;
        std::optional<GrowthScalarFifoRecipe> selected_recipe;
        auto guarded=guard_life_activity_start(common,state,catalog,action);
        if(!guarded.ok)return guarded;
        auto* actor=common.actor(action.actor_id);
        if(!actor)return common_failure("UnknownActor");
        if(action.kind=="service"){const auto& t=std::get<ServiceTerms>(q.data);
            auto payment=resource_recipe::append_money_transfer(recipe,actor->wallet_id,t.wallet_id,t.cost,resource_recipe::MoneyTransferPolicy::spendable);if(!payment.ok)return payment;
            recipe.operations.push_back(SetReservation{t.wallet_id,action.id,t.wage,false});
        }
        else {const auto& t=std::get<TradeTerms>(q.data);
            auto* shop=catalog.shops.find(t.shop_id);
            if(!shop)return common_failure("UnknownShop");
            auto* inventory=common.inventory(shop->inventory_id);
            if(!inventory)return common_failure("UnknownInventory");
            if(actual_start&&(!actual_started_tick||*actual_started_tick!=actual_start->actual_moment.half_day_tick))return common_failure("GrowthMarketActualStartProjectionMismatch",action.id);
            if(market){auto opened=market->open_growth_trade(common,catalog,action,q,actual_start);if(!opened.ok)return {false,{},opened.error};capture=std::move(opened.value);}
            // 真History2先选中原FIFO；旧普通recipe尚未构造，更没有再次转货收费。
            if(capture&&capture->original_scalar_fifo_selected()){
                if(!actual_start)return common_failure("GrowthMarketScalarActualStartRequired",action.id,ErrorKind::unsupported_capability);
                auto selected=prepare_growth_scalar_fifo_recipe(common,catalog,action,q);
                if(!selected.ok)return {false,{},selected.error};
                selected_recipe.emplace(std::move(selected.value));


                actual_leg_fields=CheckpointMemoryCharge(0);
            }else if(t.side=="buy"){
                recipe.operations.push_back(TransferLots{shop->inventory_id,actor->inventory_id,t.item_id,t.quantity,{inventory->owner},{},t.owner});
                auto payment=resource_recipe::append_money_transfer(recipe,t.payer_wallet_id,shop->wallet_id,t.total,resource_recipe::MoneyTransferPolicy::spendable);if(!payment.ok)return payment;
            }else {
                recipe.operations.push_back(TransferLots{actor->inventory_id,shop->inventory_id,t.item_id,t.quantity,{t.owner},{},inventory->owner});
                auto payment=resource_recipe::append_money_transfer(recipe,shop->wallet_id,t.payer_wallet_id,t.total,resource_recipe::MoneyTransferPolicy::spendable);if(!payment.ok)return payment;
            }
            actual_legs.reserve(2);
            if(t.side=="buy"){
                actual_legs.push_back({shop->inventory_id,t.item_id,inventory->owner,-t.quantity});
                actual_legs.push_back({actor->inventory_id,t.item_id,t.owner,t.quantity});
            }else{
                actual_legs.push_back({actor->inventory_id,t.item_id,t.owner,-t.quantity});
                actual_legs.push_back({shop->inventory_id,t.item_id,inventory->owner,t.quantity});
            }
        }
        const auto* actual_recipe=selected_recipe?selected_recipe->recipe():&recipe;
        if(!actual_recipe)return common_failure("GrowthMarketScalarRecipeMissing",action.id);
        auto r=capture?capture->apply(MarketCaptureStage::trade,*actual_recipe,actual_legs):common.apply_resources(*actual_recipe);
        if(!r.ok)return r;
        auto request=q.command;
        request.context=q.context;
        if(request.context&&!request.field_order.empty()&&std::find(request.field_order.begin(),request.field_order.end(),"context")==request.field_order.end())request.field_order.push_back("context");
        state.actions.insert(action.id,GrowthAction{request,q,{}});
        if(action.kind=="trade"){const auto& t=std::get<TradeTerms>(q.data);
            if(t.bargain_quote_id){auto* value=state.facts.find(*t.bargain_quote_id);
                auto* fact=value?std::get_if<BargainFact>(value):nullptr;
                if(!fact||!fact->consumed_action_id.empty())return common_failure("BargainQuoteConsumed");
                fact->consumed_action_id=action.id;
                fact->receipt=t;
        }}auto recorded=record_life_activity_start(common,state,catalog,action,q,actual_started_tick);if(!recorded.ok)return recorded;
        return capture?capture->commit_trade(state,action.id):recorded;
    }
// 工资兑现后按原条款恢复生命疲劳和伤势；工作关系由社会owner写入真实有向边。
    Outcome<GrowthEffect> finish_service_trade(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,SocialState& social,InteractionState& interaction,const ActionCoreView& action,const GrowthPersonalRecoveryServices* recovery,std::optional<Integer> actual_completed_tick,OriginalGrowthServiceRelationWrite* relation_writer,const PreparedTerminalRecoveryV1* terminal_recovery){
        std::optional<GrowthRecoveryCredit> terminal_credit;
        if(terminal_recovery){
            if(recovery||action.kind!="service")return fail<GrowthEffect>("TerminalRecoveryExclusiveOwnerRequired");
            auto credit=read_terminal_recovery_service_write_v1(*terminal_recovery,common,state,action);
            if(!credit.ok)return error<GrowthEffect>(credit.error);
            terminal_credit=credit.value;
        }
        auto* payload=state.actions.find(action.id);
        if(!payload)return fail<GrowthEffect>("MissingGrowthAction");
        if(social.original_relation_version==2&&action.kind=="service"&&payload->request.activity=="work"&&(!actual_completed_tick||!catalog.complete_exact_period_sources||!relation_writer||!relation_writer->bound_to(common,social,state)))return {false,{},common_failure("OriginalGrowthServiceRelationWriterRequired",action.id,ErrorKind::unsupported_capability).error};
        if(catalog.regional_life){const auto* evidence=state.life_activities.find(action.id);
            if(!evidence)return fail<GrowthEffect>("MissingLifeActivity");
            if(evidence->view.phase!=LifeActivityPhase::active)return fail<GrowthEffect>("LifeActivityAlreadyCompleted");
        }
        auto* actor=common.state().actors.find(action.actor_id);
        if(!actor)return fail<GrowthEffect>("UnknownActor");
        GrowthEffect effect;
        effect.kind=action.kind;
        if(action.kind=="trade"){auto skills=actor->skills;
            // 新完整格式只记录本次原写者的真实前后像；不补差、不第二次授予。
            const bool capture_grant=catalog.complete_capability_application_chain;
            const Integer xp_before=skills.commerce;
            if(capture_grant){
                const auto* life=state.life_activities.find(action.id);
                if(!catalog.complete_study_sources||!actual_completed_tick||
                   !life||life->view.kind!=LifeActivityKind::trade||
                   life->view.phase!=LifeActivityPhase::active||
                   life->view.actor_id!=actor->id||life->trade_terminal_time||
                   !std::holds_alternative<TradeTerms>(life->terms)||
                   xp_before<0||xp_before>1200)
                    return fail<GrowthEffect>("GrowthTradeGrantActualWriterRequired");
                if(state.trade_grants.find(action.id))
                    return fail<GrowthEffect>("DuplicateGrowthTradeGrant");
            }
            skills.commerce=std::min<Integer>(1200,skills.commerce+10);
            ActorMutation mutation;
            mutation.actor_id=actor->id;
            mutation.skills=skills;
            auto r=common.apply_actor(mutation);
            if(!r.ok)return error<GrowthEffect>(r.error);
            if(capture_grant){
                const auto* after=common.state().actors.find(action.actor_id);
                if(!after||after->skills.commerce!=skills.commerce)
                    return fail<GrowthEffect>("GrowthTradeGrantActualWriteMismatch");
                GrowthTradeGrant grant;
                grant.action_id=action.id;
                grant.actor_id=after->id;
                grant.skill_id="commerce";
                grant.original_trade_result_ref={"growth","life_activity",action.id};
                grant.original_trade_source_schema=2;
                grant.xp_before=xp_before;
                grant.actual_xp_gain=after->skills.commerce-xp_before;
                grant.xp_after=after->skills.commerce;
                if(!state.trade_grants.insert(action.id,std::move(grant)))
                    return fail<GrowthEffect>("DuplicateGrowthTradeGrant");
            }
            for(auto& session:interaction.interactions)if(session.actor_id==actor->id&&session.phase=="trade")session.trade_completed=true;
            r=record_life_activity_completion(common,state,catalog,action,(actual_completed_tick?*actual_completed_tick:common.tick()+1));
            if(!r.ok)return error<GrowthEffect>(r.error);
            return {true,std::move(effect),{}};
        }
        const auto& t=std::get<ServiceTerms>(payload->quote.data);
        auto* provider=common.wallet(t.wallet_id);
        if(!provider||reserved(provider,action.id)!=t.wage||provider->balance<t.wage)return fail<GrowthEffect>("ServiceEscrowMismatch");
        ResourceRecipe recipe;
        auto r=resource_recipe::append_money_transfer(recipe,t.wallet_id,actor->wallet_id,t.wage,resource_recipe::MoneyTransferPolicy::original_debit_spendable_credit);
        if(!r.ok)return error<GrowthEffect>(r.error);
        r=common.apply_resources(recipe);
        if(!r.ok)return error<GrowthEffect>(r.error);
        ActorMutation mutation;
        mutation.actor_id=actor->id;
        mutation.hp=std::clamp<Integer>(actor->hp+t.hp_gain,0,terminal_credit?growth_injury_hp_limit(terminal_credit->after.injury):recovery?growth_injury_hp_limit(actor->injury):100);
        mutation.fatigue=std::clamp<Integer>(actor->fatigue+t.fatigue_delta,0,100);
        if(terminal_credit)mutation.injury=terminal_credit->after.injury;
        else if(!recovery&&payload->request.activity=="rest"&&t.hp_gain>0)mutation.injury=std::max<Integer>(0,actor->injury-1);
        r=common.apply_actor(mutation);
        if(!r.ok)return error<GrowthEffect>(r.error);
        if(payload->request.activity=="work"&&social.original_relation_version!=2){if(!payload->quote.context)return fail<GrowthEffect>("InvalidServiceContext");
            const auto relation_count=social.relations.rows().size();
            r=change_original_social_relation_legacy(common,social,payload->quote.context->provider_actor_id,action.actor_id,OriginalSocialRelationRule::completed_work_service);
            if(!r.ok)return error<GrowthEffect>(r.error);
            // 雇工新建边的原字段序不同于社会援助；只标记本次追加边，保留已有关系的存档序。
            if(social.relations.rows().size()>relation_count)social.relations.rows().back().second.field_order={"id","from_actor","to_actor","affinity","trust"};
        }r=record_life_activity_completion(common,state,catalog,action,(actual_completed_tick?*actual_completed_tick:common.tick()+1));
        if(!r.ok)return error<GrowthEffect>(r.error);
        return {true,std::move(effect),{}};
    }
// 活动只核冻结条款、在场和预留，不因岗位后来停用而重新拒绝已经付款的服务。
    Status validate_service_trade(const CommonStore& common,const GrowthCatalog& catalog,const CommonCatalog& shared,const ActionCoreView& action,const GrowthAction& payload){if(action.kind=="trade"){auto* t=std::get_if<TradeTerms>(&payload.quote.data);
            if(!t||!catalog.shops.find(t->shop_id)||!shared.items.find(t->item_id)||!common.wallet(t->payer_wallet_id)||t->quantity<1||t->quantity>500||t->total<0||(t->side!="buy"&&t->side!="sell")||action.remaining_ticks!=trade_action_ticks)return common_failure("InvalidTradeReceipt");
            return common_success();
        }
        auto* t=std::get_if<ServiceTerms>(&payload.quote.data);
        if(!t||!payload.quote.context)return common_failure("InvalidServiceAction");
        const auto& context=*payload.quote.context;
        auto* role=shared.roles.find(context.role_id);
        auto* assignment=common.state().roles.find(context.role_id);
        if(!role||!common.actor(context.provider_actor_id)||std::find(action.participants.begin(),action.participants.end(),context.provider_actor_id)==action.participants.end())return common_failure("InvalidServiceContext");
        auto provider=common.resolved_location(context.provider_actor_id),person=common.resolved_location(action.actor_id);
        auto* pp=provider.ok?std::get_if<SettlementLocation>(&provider.value):nullptr;
        auto* ap=person.ok?std::get_if<SettlementLocation>(&person.value):nullptr;
        if(context.place_id!=role->place_id||!assignment||assignment->actor_id!=context.provider_actor_id||!pp||!ap||pp->place_id!=context.place_id||ap->place_id!=context.place_id)return common_failure("InvalidServicePresence");
        auto* definition=catalog.roles.find(context.role_id);
        auto* terms=definition?definition->activities.find(payload.request.activity):nullptr;
        if(!terms||!same_terms(*terms,*t))return common_failure("ServiceTermsMismatch");
        if(reserved(common.wallet(t->wallet_id),action.id)!=t->wage||action.remaining_ticks>t->duration)return common_failure("ServiceEscrowMismatch");
        return common_success();
    }
} // 这里没有独立时钟、库存价格缓存或新经济公式。

namespace hanmo::resident {
Outcome<Integer> growth_market_midpoint_from_stock(Integer base,Integer stock){
    if(base<=0||base>2147483647||stock<0)return {false,0,common_failure("GrowthMarketPriceInputInvalid").error};
    return {true,growth::marginal(base,std::min<Integer>(stock,200)),{}};
}
// 复用本文件原交易边际内核；高库存已经处于原半价下界，先夹到平台起点避免无意义乘法溢出。
Outcome<GrowthMarketMidpoint> read_growth_market_midpoint(const CommonStore& common,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& shop_id,const StableId& item_id){
    auto fail=[](const StableId& code)->Outcome<GrowthMarketMidpoint>{return {false,{},common_failure(code).error};};
    if(!catalog.economy)return fail("GrowthEconomyProfileRequired");
    const auto* shop=catalog.shops.find(shop_id);const auto* item=shared.items.find(item_id);
    if(!shop||shop->id!=shop_id||!item||item->id!=item_id)return fail("GrowthMarketSourceRequired");
    if(item->base_price<=0||item->base_price>2147483647||std::find(catalog.tradeable_items.begin(),catalog.tradeable_items.end(),item_id)==catalog.tradeable_items.end())return fail("NotTradeable");
    const auto* inventory=common.inventory(shop->inventory_id);const auto* wallet=common.wallet(shop->wallet_id);
    if(!inventory||!wallet||!shared.places.find(shop->place_id))return fail("GrowthMarketAssetsMismatch");
    auto stock=common.count(inventory->id,item_id,{inventory->owner});if(!stock.ok)return {false,{},stock.error};
    if(stock.value<0||inventory->revision<0)return fail("GrowthMarketStockInvalid");
    const auto midpoint=growth_market_midpoint_from_stock(item->base_price,stock.value);if(!midpoint.ok)return {false,{},midpoint.error};
    return {true,GrowthMarketMidpoint{common.state().identity,shop_id,item_id,inventory->id,shop->place_id,common.tick(),inventory->revision,stock.value,item->base_price,midpoint.value},{}};
}
}
