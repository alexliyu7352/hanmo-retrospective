// 正式货单议价保留既有来源摘要、一次实例与一次成交；钱货只由原trade开工统一转移。
#include "growth_internal.hpp"
#include "growth_equal.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident::growth {
    namespace {
        template<class T> Outcome<T> failed(const StableId& code){return {false,{},common_failure(code).error};
        }
        Outcome<GrowthChallengeDefinition> project(const GrowthCatalog& catalog,const CommonCatalog& common,const GrowthShop& shop,const GrowthCommand& request,const BargainMarketBasis& market){if(!shop.bargaining)return failed<GrowthChallengeDefinition>("BargainingNotOffered");
            auto* original=catalog.bargain.find(shop.bargaining->challenge_id);
            if(!original)return failed<GrowthChallengeDefinition>("MissingBargainDefinition");
            auto d=*original;
            std::set<StableId> seen;
            for(const auto& source:shop.bargaining->condition_sources){auto condition=std::find_if(d.conditions.begin(),d.conditions.end(),[&](const auto& c){return c.id==source.condition_id;
                });
                if(condition==d.conditions.end()||!seen.insert(source.condition_id).second)return failed<GrowthChallengeDefinition>("InvalidBargainConditionSource");
                if(source.source=="stock_covers_quantity"){if(condition->kind!="supply"||source.value)return failed<GrowthChallengeDefinition>("InvalidBargainConditionSource");
                    condition->matched=market.available_quantity>=request.quantity;
                }else if(source.source=="cash_payment"){if(condition->kind!="terms"||source.value)return failed<GrowthChallengeDefinition>("InvalidBargainConditionSource");
                    condition->matched=true;
                }else if(source.source=="quantity_at_most"){if(!source.value||*source.value<1||*source.value>500)return failed<GrowthChallengeDefinition>("InvalidBargainConditionSource");
                    condition->matched=request.quantity<=*source.value;
                }else return failed<GrowthChallengeDefinition>("InvalidBargainConditionSource");
            }if(seen.size()!=d.conditions.size())return failed<GrowthChallengeDefinition>("MissingBargainConditionSource");
            d.source_sha256=challenge_digest(d,common);
            return {true,std::move(d),{}};
        }
        StableId basis_digest(const StableId& actor,const BargainTerms& t){return canonical_digest(boundary::obj({{"actor_id",BoundaryValue{actor}},{"base_quote",encode_trade_terms(t.base_quote)},{"market_basis",encode_bargain_market(t.market_basis)},{"source_sha256",BoundaryValue{t.source_sha256}}}));
        }
        Outcome<BargainPrepared> basis(const CommonStore& common,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& actor,const GrowthCommand& request,const StableId& own){if(request.bargain_quote_id||request.quoted_total||request.market_revision||!request.context||request.context->kind)return failed<BargainPrepared>("InvalidBargainTrade");
            auto quoted=preview_service_trade(common,catalog,shared,access,actor,request,own);
            if(!quoted.ok)return {false,{},quoted.error};
            auto* shop=catalog.shops.find(request.shop_id);
            if(!shop||!shop->bargaining)return failed<BargainPrepared>("BargainingNotOffered");
            auto* inventory=common.inventory(shop->inventory_id);
            auto* person=common.actor(actor);
            auto* item=shared.items.find(request.item_id);
            if(!inventory||!person||!item)return failed<BargainPrepared>("InvalidBargainReferences");
            auto stock=common.count(inventory->id,item->id,{inventory->owner});
            if(!stock.ok)return {false,{},stock.error};
            BargainPrepared p;
            p.terms.request=request;
            p.terms.base_quote=std::get<TradeTerms>(quoted.value.data);
            auto& m=p.terms.market_basis;
            m.base_price=item->base_price;
            m.stock=stock.value;
            m.shop_wallet_id=shop->wallet_id;
            m.shop_inventory_id=shop->inventory_id;
            m.actor_inventory_id=person->inventory_id;
            if(request.side=="buy")m.available_quantity=stock.value;
            else{auto count=common.count(person->inventory_id,item->id,{p.terms.base_quote.owner});
                if(!count.ok)return {false,{},count.error};
                m.available_quantity=count.value;
            }p.terms.source_sha256=shop->bargaining->source_sha256;
            p.terms.challenge_id=shop->bargaining->challenge_id;
            auto projected=project(catalog,shared,*shop,request,m);
            if(!projected.ok)return {false,{},projected.error};
            p.definition=std::move(projected.value);
            p.basis_sha256=basis_digest(actor,p.terms);
            return {true,std::move(p),{}};
        }
        GrowthCatalog projected_catalog(const GrowthCatalog&,const GrowthChallengeDefinition& definition){GrowthCatalog projected;
            projected.bargain.insert(definition.id,definition);
            return projected;
        }
        Outcome<const BargainFact*> owned(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& actor,const StableId& id){auto* value=state.facts.find(id);
            auto* fact=value?std::get_if<BargainFact>(value):nullptr;
            if(!fact||fact->actor_id!=actor)return failed<const BargainFact*>("UnknownBargainQuote");
            auto r=validate_bargain(common,state,catalog,shared,id,*fact);
            if(!r.ok)return {false,nullptr,r.error};
            return {true,fact,{}};
        }
        Outcome<BargainPrepared> live(const CommonStore& common,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& actor,const BargainFact& fact,const StableId& own){if(!fact.consumed_action_id.empty())return failed<BargainPrepared>("BargainQuoteConsumed");
            auto* inventory=common.inventory(fact.terms.market_basis.shop_inventory_id);
            if(common.tick()!=fact.terms.base_quote.valid_tick||!inventory||inventory->revision!=fact.terms.base_quote.market_revision)return failed<BargainPrepared>("StaleBargainQuote");
            auto current=basis(common,catalog,shared,access,actor,fact.terms.request,own);
            if(!current.ok)return current;
            if(current.value.basis_sha256!=fact.basis_sha256)return failed<BargainPrepared>("BargainQuoteChanged");
            return current;
        }
        bool trade_equal(const TradeTerms& a,const TradeTerms& b){GrowthQuote x,y;
            x.data=a;
            y.data=b;
            return quote_equal(x,y)&&a.bargain_quote_id==b.bargain_quote_id&&a.discount_bp==b.discount_bp&&a.bargain_basis_sha256==b.bargain_basis_sha256;
        }
    }
    Outcome<GrowthQuote> preview_bargain(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& actor,const GrowthCommand& command,const StableId& own){GrowthQuote q;
        q.command=command;
        if(command.kind=="trade_bargain_open"){if(command.challenge_mode!="manual"&&command.challenge_mode!="automatic")return failed<GrowthQuote>("InvalidBargainOpen");
            auto request=command;
            request.kind="trade";
            request.field_order=command.trade_field_order;
            request.trade_field_order.clear();
            auto p=basis(common,catalog,shared,access,actor,request,own);
            if(!p.ok)return {false,{},p.error};
            p.value.mode=command.challenge_mode;
            q.data=std::move(p.value);
            return {true,std::move(q),{}};
        }
        const auto id=command.bargain_quote_id.value_or("");
        auto found=owned(common,state,catalog,shared,actor,id);
        if(!found.ok)return {false,{},found.error};
        const auto& fact=*found.value;
        auto current=live(common,catalog,shared,access,actor,fact,own);
        if(!current.ok)return {false,{},current.error};
        auto local=projected_catalog(catalog,current.value.definition);
        if(command.kind=="trade"){if(fact.instance.phase!="completed"||!fact.instance.result)return failed<GrowthQuote>("BargainNotCompleted");
            const auto discount=fact.instance.result->discount_bp;
            auto terms=current.value.terms.base_quote;
            terms.total=trade_price(current.value.terms.market_basis.base_price,current.value.terms.market_basis.stock,terms.quantity,terms.side=="buy",discount);
            auto* shop=catalog.shops.find(terms.shop_id);
            auto funds=common.spendable(terms.side=="buy"?terms.payer_wallet_id:shop->wallet_id);
            if(!funds.ok)return {false,{},funds.error};
            if(funds.value<terms.total)return failed<GrowthQuote>(terms.side=="buy"?"InsufficientFunds":"ShopInsufficientFunds");
            if(command.quoted_total&&(*command.quoted_total!=terms.total||!command.market_revision||*command.market_revision!=terms.market_revision))return failed<GrowthQuote>("StaleBargainConfirmation");
            terms.bargain_quote_id=id;
            terms.discount_bp=discount;
            terms.bargain_basis_sha256=fact.basis_sha256;
            q.duration=1;
            q.context=terms.context;
            q.data=std::move(terms);
            return {true,std::move(q),{}};
        }
        GrowthCommand transition;
        transition.kind=command.kind=="trade_bargain_choose"?"study_challenge_choose":"study_challenge_exit";
        transition.step_id=command.step_id;
        transition.option_id=command.option_id;
        auto next=challenge_transition(local,fact.instance,transition);
        if(!next.ok)return {false,{},next.error};
        BargainPrepared prepared;
        prepared.quote_id=id;
        prepared.terms=fact.terms;
        prepared.definition=std::move(current.value.definition);
        prepared.before=fact.instance;
        prepared.next=std::move(next.value);
        q.data=std::move(prepared);
        return {true,std::move(q),{}};
    }
    Outcome<GrowthEffect> apply_bargain(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& actor,const GrowthQuote& quote){auto current=preview_bargain(common,state,catalog,shared,access,actor,quote.command,{});
        if(!current.ok)return {false,{},current.error};
        const auto& p=std::get<BargainPrepared>(current.value.data);
        GrowthEffect effect;
        effect.kind=quote.command.kind;
        if(p.next){auto* value=state.facts.find(p.quote_id);
            auto* fact=value?std::get_if<BargainFact>(value):nullptr;
            if(!fact)return failed<GrowthEffect>("UnknownBargainQuote");
            fact->instance=*p.next;
            effect.quote_id=p.quote_id;
            return {true,std::move(effect),{}};
        }
        StableId existing;
        for(const auto& row:state.facts.rows())if(auto* fact=std::get_if<BargainFact>(&row.second))if(fact->basis_sha256==p.basis_sha256){if(!existing.empty())return failed<GrowthEffect>("DuplicateBargainQuote");
            existing=row.first;
        }if(!existing.empty()){auto checked=owned(common,state,catalog,shared,actor,existing);
            if(!checked.ok)return {false,{},checked.error};
            effect.quote_id=existing;
            effect.reopened=true;
            return {true,std::move(effect),{}};
        }
        auto* person=common.state().actors.find(actor);
        if(!person||person->skills.commerce<0||person->skills.commerce>1200||person->attributes.administration<1||person->attributes.administration>110)return failed<GrowthEffect>("InvalidBargainActor");
        const auto expected="quote:"+std::to_string(common.state().next_instance_id);
        BargainFact fact;
        fact.actor_id=actor;
        fact.basis_sha256=p.basis_sha256;
        fact.terms=p.terms;
        fact.terms.skill_level=growth_skill_level(person->skills.commerce);
        fact.terms.auxiliary_attribute=person->attributes.administration;
        fact.terms.quote_context={fact.terms.base_quote.total,fact.terms.request.quantity,0,{},BargainQuoteContext{expected,p.basis_sha256,fact.terms.base_quote.market_revision,fact.terms.base_quote.valid_tick,{}}};
        ChallengeFrozen frozen;
        frozen.field="challenge";
        frozen.kind="bargain";
        frozen.challenge_id=fact.terms.challenge_id;
        frozen.mode=p.mode;
        frozen.source_sha256=p.definition.source_sha256;
        frozen.skill_level=fact.terms.skill_level;
        frozen.auxiliary_attribute=fact.terms.auxiliary_attribute;
        frozen.context=fact.terms.quote_context;
        auto local=projected_catalog(catalog,p.definition);
        auto instance=challenge_start(common,local,frozen,actor,expected);
        if(!instance.ok)return {false,{},instance.error};
        fact.instance=std::move(instance.value);
        auto allocated=common.next_id("quote");
        if(!allocated.ok)return {false,{},allocated.error};
        if(allocated.value!=expected)return failed<GrowthEffect>("BargainInstanceCollision");
        state.facts.insert(expected,std::move(fact));
        auto recorded=common.append_fact({expected,"trade_bargain","economy"});
        if(!recorded.ok)return {false,{},recorded.error};
        effect.quote_id=expected;
        return {true,std::move(effect),{}};
    }
    Status validate_bargain(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const StableId& id,const BargainFact& fact){if(id.rfind("quote:",0)!=0||!common.actor(fact.actor_id))return common_failure("InvalidBargainFact",id);
        const auto& t=fact.terms;
        const auto& request=t.request;
        const auto& base=t.base_quote;
        const auto& market=t.market_basis;
        auto* shop=catalog.shops.find(request.shop_id);
        auto* item=shared.items.find(request.item_id);
        auto* role=request.context?shared.roles.find(request.context->role_id):nullptr;
        if(!shop||!item||!role||!common.actor(request.context->provider_actor_id)||request.bargain_quote_id||request.quoted_total||request.market_revision||request.context->kind)return common_failure("InvalidBargainReferences",id);
        if(base.valid_tick<0||base.valid_tick>common.tick()||base.market_revision<0||base.market_revision>2147483647||base.quantity<1||base.quantity>500||(base.side!="buy"&&base.side!="sell")||base.bargain_quote_id)return common_failure("InvalidBargainBaseQuote",id);
        auto* payer=common.wallet(base.payer_wallet_id);
        if(!payer||!owner_equal(payer->owner,base.owner)||(base.owner.kind!=Owner::Kind::actor&&base.owner.kind!=Owner::Kind::task)||(base.owner.kind==Owner::Kind::actor&&base.owner.id!=fact.actor_id))return common_failure("InvalidBargainPayer",id);
        if(std::tie(base.shop_id,base.item_id,base.quantity,base.side,base.payer_wallet_id)!=std::tie(request.shop_id,request.item_id,request.quantity,request.side,request.payer_wallet_id)||!context_equal(base.context,*request.context))return common_failure("BargainRequestMismatch",id);
        if(market.stock<0||market.stock>2147483647||market.available_quantity<request.quantity||market.available_quantity>2147483647||market.elasticity_bp!=5000||market.target_stock!=100||market.event_bp!=0)return common_failure("InvalidBargainMarketBasis",id);
        if(item->base_price<=0||std::find(catalog.tradeable_items.begin(),catalog.tradeable_items.end(),item->id)==catalog.tradeable_items.end()||(request.side=="buy"&&market.available_quantity!=market.stock)||market.base_price!=item->base_price||market.shop_wallet_id!=shop->wallet_id||market.shop_inventory_id!=shop->inventory_id||market.actor_inventory_id!=common.actor(fact.actor_id)->inventory_id||base.total!=trade_price(market.base_price,market.stock,request.quantity,request.side=="buy")||request.context->place_id!=shop->place_id||std::find(shop->representative_roles.begin(),shop->representative_roles.end(),request.context->role_id)==shop->representative_roles.end())return common_failure("BargainMarketBasisMismatch",id);
        if(!shop->bargaining||shop->bargaining->challenge_id!=t.challenge_id||shop->bargaining->source_sha256!=t.source_sha256)return common_failure("BargainSourceChanged",id);
        auto definition=project(catalog,shared,*shop,request,market);
        if(!definition.ok)return {false,{},definition.error};
        if(basis_digest(fact.actor_id,t)!=fact.basis_sha256)return common_failure("BargainBasisMismatch",id);
        const auto& ctx=t.quote_context;
        if(fact.instance.frozen.kind!="bargain"||fact.instance.frozen.challenge_id!=t.challenge_id||!challenge_context_equal(fact.instance.frozen.context,std::optional<StudyChallengeContext>{ctx})||ctx.budget!=base.total||ctx.quantity!=request.quantity||ctx.relation_discount_bp!=0||!ctx.quote||ctx.quote->quote_id!=id||ctx.quote->quote_sha256!=fact.basis_sha256||ctx.quote->market_revision!=base.market_revision||ctx.quote->valid_tick!=base.valid_tick||fact.instance.actor_id!=fact.actor_id||fact.instance.source_id!=id||fact.instance.frozen.skill_level!=t.skill_level||fact.instance.frozen.auxiliary_attribute!=t.auxiliary_attribute||!fact.instance.frozen.context||!fact.instance.frozen.context->quote||fact.instance.frozen.context->quote->quote_id!=id||fact.instance.frozen.context->quote->quote_sha256!=fact.basis_sha256)return common_failure("BargainInstanceBindingMismatch",id);
        auto local=projected_catalog(catalog,definition.value);
        auto valid=challenge_validate(local,fact.instance);
        if(!valid.ok)return valid;
        if(fact.consumed_action_id.empty()){if(fact.receipt)return common_failure("UnexpectedBargainReceipt",id);
        }else{if(fact.consumed_action_id.rfind("action:",0)!=0||fact.instance.phase!="completed"||!fact.instance.result||!fact.receipt)return common_failure("InvalidBargainConsumption",id);
            auto expected=base;
            expected.total=trade_price(market.base_price,market.stock,request.quantity,request.side=="buy",fact.instance.result->discount_bp);
            expected.bargain_quote_id=id;
            expected.discount_bp=fact.instance.result->discount_bp;
            expected.bargain_basis_sha256=fact.basis_sha256;
            if(!trade_equal(expected,*fact.receipt))return common_failure("BargainReceiptMismatch",id);
            if(auto* action=common.action(fact.consumed_action_id)){auto* payload=state.actions.find(action->id);
                auto* receipt=payload?std::get_if<TradeTerms>(&payload->quote.data):nullptr;
                if(action->kind!="trade"||action->actor_id!=fact.actor_id||!receipt||!trade_equal(*receipt,*fact.receipt))return common_failure("BargainReceiptActionMismatch",id);
        }}return common_success();
    }
    Outcome<BoundaryValue> bargain_view(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthAccess& access,const StableId& actor,const StableId& quote_id){using namespace boundary;
        auto found=owned(common,state,catalog,shared,actor,quote_id);
        if(!found.ok)return {false,{},found.error};
        const auto& fact=*found.value;
        if(!fact.consumed_action_id.empty())return {true,obj({{"ok",BoundaryValue{true}},{"quote_id",BoundaryValue{quote_id}},{"consumed",BoundaryValue{true}},{"action_id",BoundaryValue{fact.consumed_action_id}},{"receipt",encode_trade_terms(*fact.receipt)}}),{}};
        auto current=live(common,catalog,shared,access,actor,fact,{});
        if(!current.ok)return {false,{},current.error};
        auto local=projected_catalog(catalog,current.value.definition);
        auto value=challenge_view(local,shared,fact.instance);
        auto& rows=std::get<BoundaryValue::Dict>(value.data);
        rows.emplace_back("quote_id",BoundaryValue{quote_id});
        rows.emplace_back("consumed",BoundaryValue{false});
        rows.emplace_back("price_applied",BoundaryValue{false});
        auto* shop=catalog.shops.find(fact.terms.request.shop_id);
        BoundaryValue::Array sources;
        for(const auto& source:shop->bargaining->condition_sources){auto item=obj({{"condition_id",BoundaryValue{source.condition_id}},{"source",BoundaryValue{source.source}}});
            if(source.value)std::get<BoundaryValue::Dict>(item.data).emplace_back("value",BoundaryValue{*source.value});
            sources.push_back(ordered(std::move(item),source.field_order));
        }rows.emplace_back("condition_sources",BoundaryValue{std::move(sources)});
        rows.emplace_back("available_quantity",BoundaryValue{fact.terms.market_basis.available_quantity});
        rows.emplace_back("quantity",BoundaryValue{fact.terms.request.quantity});
        return {true,std::move(value),{}};
    }
} // 过期报价继续可存读但不能转移钱货，关闭窗口、换模式和重开都不会清除消费证据。
