#include "director_actor_read.hpp"
// 私人物资准备沿真实工作、交易和行旅行动推进；计划只借本人资产与已知机会。
#include "director_livelihood.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/kernel/command_types.hpp"
#include <limits>
#include <map>
#include <tuple>
namespace hanmo::resident {
namespace {
// 只构造命令值，所有费用和占用仍交中央正式采用。
template<class T> std::shared_ptr<const CommandPayload> command(const StableId& kind,T value){return std::make_shared<const CommandPayload>(CommandPayload{kind,std::move(value)});}
// 已知路线只估计原道路总费；不读取目的地真实人物或余额。
Outcome<std::pair<Integer,Integer>> route_cost(const TravelCatalog& travel,const StableId& from,const StableId& to,Integer tick){
    const auto* destination=travel.places.find(to);if(!destination)return {false,{},common_failure("UnknownPlace",to).error};
    if(destination->settlement_id==from)return {true,{0,0},{}};
    if(!travel.world_map_id.empty())return {false,{},common_failure("LivelihoodRouteUnavailable",to).error};
    auto route=plan_road_route(travel,from,TravelDestination{destination->settlement_id},tick);if(!route.ok)return {false,{},route.error};return {true,{route.value.cost,route.value.duration},{}};
}
// 同城只读BFS寻找下一相邻步；失败不会返回可绕过通道的直接移动。
StableId local_step(const TravelCatalog& travel,const StableId& from,const StableId& target,const StableId& settlement){
    std::vector<StableId> queue{from};std::map<StableId,StableId> previous{{from,{}}};
    for(std::size_t i=0;i<queue.size();++i){const auto* here=travel.places.find(queue[i]);if(!here)continue;for(const auto& next:here->passages){const auto* place=travel.places.find(next);if(!place||place->settlement_id!=settlement||previous.count(next))continue;previous.emplace(next,queue[i]);queue.push_back(next);if(next==target){auto step=target;while(previous.at(step)!=from)step=previous.at(step);return step;}}}return {};
}
// 算术失败保持显式拒绝，不能截断预算后出行。
bool add(Integer a,Integer b,Integer& result){return !__builtin_add_overflow(a,b,&result);}
// 选中报价只作用于同地有效provider；远地从不访问其当前任职状态。
std::optional<InteractionContext> local_context(const CommonStore& c,const KnownSubjectView& view,const StableId& role){
    const auto* assignment=c.state().roles.find(role);if(!assignment||!assignment->enabled||std::find(view.locally_occupied_actor_ids.begin(),view.locally_occupied_actor_ids.end(),assignment->actor_id)!=view.locally_occupied_actor_ids.end())return {};
    const auto* provider=c.actor(assignment->actor_id);auto location=c.resolved_location(assignment->actor_id);const auto* local=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
    if(!provider||!local||local->place_id!=view.place_id)return {};return InteractionContext{view.place_id,assignment->actor_id,role,{},{},{},{}};
}
// 复评提案沿同一序号和节流合同；仅资产读取版本进入短期采用证明。
Outcome<DecisionProposal> proposal_base(const CommonStore& c,const DirectorState& state,const DirectorCatalog& catalog,const KnownSubjectView& view,Integer tick){
    const auto* schedule=director_actor_schedule(state,view.actor_id);if(!schedule||view.tick!=tick||c.tick()!=tick||view.revision!=c.revision()||view.subject.kind!=SubjectKind::actor||view.subject.id!=view.actor_id)return {false,{},common_failure("InvalidDirectorInput").error};
    const Integer delay=std::max(catalog.review_interval_ticks,catalog.retry_delay_ticks);if(delay<1||tick>std::numeric_limits<Integer>::max()-delay||schedule->decision_seq==std::numeric_limits<Integer>::max())return {false,{},common_failure("DirectorSequenceOverflow").error};
    DecisionProposal p;p.subject_id=view.actor_id;auto transition=std::make_shared<DirectorTransition>();transition->expected_revision=c.revision();if(const auto registration=director_actor_registration(state,view.actor_id))transition->registration_epoch=registration->epoch;transition->decision_seq=schedule->decision_seq+1;transition->next_review_tick=tick+delay;transition->template_id="private_livelihood";transition->phase=PlanPhase::selecting;
    auto current_plan=read_director_actor_plan(state,view.actor_id);if(!current_plan.ok)return {false,{},current_plan.error};if(const auto*old=current_plan.value){p.plan_id=old->id;transition->livelihood=old->livelihood;transition->pending_life_action_id=old->pending_life_action_id;transition->last_life_fact_id=old->last_life_fact_id;transition->next_step_index=old->step_index;transition->commitment_ids=old->commitment_ids;}else p.plan_id="plan:"+view.actor_id+":"+std::to_string(transition->decision_seq);
    p.transition=transition;p.expected_versions.push_back(c.read_version({ResourceKey::Kind::actor,view.actor_id}));return {true,std::move(p),{}};
}
// 等待不创建虚假工作或完成事实；只保存下一次合法复评原因。
Outcome<std::vector<DecisionProposal>> wait(DecisionProposal p,const StableId& reason){auto& t=const_cast<DirectorTransition&>(*p.transition);t.phase=PlanPhase::waiting;t.wait_reason=reason;p.command.reset();return {true,{std::move(p)},{}};}
// 有界市场候选只从本人已知供货观察连接作者商店定义。
struct MarketOption {const KnownSupply* source=nullptr;const GrowthShop* shop=nullptr;Integer quantity=0,cost=0,outward=0,returning=0,duration=0;};
}

std::shared_ptr<const CommandPayload> director_move_toward(const TravelCatalog& travel,const KnownSubjectView& view,const StableId& target){
    const auto* destination=travel.places.find(target);if(!destination||view.place_id.empty()||view.place_id==target)return {};
    StableId local_target=target;
    if(destination->settlement_id!=view.settlement_id){const auto* origin=travel.settlements.find(view.settlement_id);if(!origin)return {};if(view.place_id==origin->entry_place)return command("travel",TravelCommandPayload{TravelCommand{TravelDestination{destination->settlement_id},{},true,false,{}}});local_target=origin->entry_place;}
    auto step=local_step(travel,view.place_id,local_target,view.settlement_id);return step.empty()?nullptr:command("move_local",TravelCommandPayload{MoveLocalCommand{step}});
}

KnownSubjectView with_livelihood_services(const CommonStore& c,const CommonCatalog& common,const GrowthCatalog& growth,const GrowthAccess& access,const TravelCatalog& travel,const DirectorPolicy& policy,const KnownSubjectView& view){
    auto result=view;result.services.clear();if(!policy.private_livelihood)return result;
    for(const auto& id:policy.private_livelihood->known_service_role_ids){const auto* definition=common.roles.find(id);const auto* role=growth.roles.find(id);const auto* place=definition?travel.places.find(definition->place_id):nullptr;if(!definition||!role||!place)continue;
        for(const auto& activity:{StableId{"work"},StableId{"rest"}}){const auto* terms=role->activities.find(activity);if(!terms)continue;KnownServiceOption option;option.source={id,definition->place_id,place->settlement_id,activity,"authored:"+view.actor_id+":"+id,ServiceKnowledgeKind::authored,0};option.duration=terms->duration;option.cost=terms->cost;option.wage=terms->wage;option.hp_gain=terms->hp_gain;option.fatigue_delta=terms->fatigue_delta;
            if(definition->place_id==view.place_id){option.source.knowledge_kind=ServiceKnowledgeKind::local_observation;option.source.observed_tick=view.tick;auto context=local_context(c,view,id);if(!context)option.refusal_reason="ServiceProviderUnavailable";else{option.provider_actor_id=context->provider_actor_id;GrowthCommand request;request.kind="service";request.activity=activity;request.context=*context;auto quote=growth::preview_service_trade(c,growth,common,access,view.actor_id,request,{});if(!quote.ok){option.refusal_reason=quote.error.original.code;option.refusal_kind=quote.error.kind;}}}
            result.services.push_back(std::move(option));
        }
    }return result;
}

Outcome<std::vector<DecisionProposal>> propose_private_livelihood(const CommonStore& c,const DirectorState& state,const DirectorCatalog& catalog,const KnownSubjectView& view,const GrowthCatalog& growth,const GrowthAccess& access,const TravelCatalog& travel,const GrowthLifeRead& life,Integer tick){
    if(!director_subject_active(state,view.actor_id))return {true,{}, {}};
    if(const auto registration=director_actor_registration(state,view.actor_id);registration&&registration->obligations_only)return {true,{}, {}};
    const auto* policy=director_policy(state,catalog,view.actor_id);if(!catalog.regional_life||!policy||!policy->private_livelihood)return {true,{}, {}};
    const auto* schedule=director_actor_schedule(state,view.actor_id);if(schedule&&(schedule->next_tick>tick||schedule->last_decision_tick>=tick))return {true,{}, {}};
    auto proposed=proposal_base(c,state,catalog,view,tick);if(!proposed.ok)return {false,{},proposed.error};auto p=std::move(proposed.value);auto& transition=const_cast<DirectorTransition&>(*p.transition);const auto& policy_terms=*policy->private_livelihood;
    const auto* person=c.state().actors.find(view.actor_id);if(!person)return {false,{},common_failure("UnknownActor",view.actor_id).error};
    if(!transition.livelihood)transition.livelihood=LivelihoodIntent{"private:"+view.actor_id+":"+policy_terms.item_id,policy_terms.item_id,policy_terms.target_quantity,{},{},{},tick,policy_terms.return_place_id};auto& intent=*transition.livelihood;transition.goal_ref=intent.goal_id;
    if(!view.alive||!view.available)return wait(std::move(p),"ActorUnavailable");if(!view.current_action_id.empty()){transition.current_action_id=view.current_action_id;return wait(std::move(p),"ActorBusy");}
    if(view.interaction_busy)return wait(std::move(p),"ExistingInteraction");
    if(!view.commitments.empty())return wait(std::move(p),"ExistingCommitment");
    const auto* members=c.members(view.actor_id);if(!members||members->size()!=1||members->front()!=view.actor_id||!std::holds_alternative<SettlementLocation>(person->location))return wait(std::move(p),"LivelihoodPartyCommitment");
    if(access.battle_occupied&&access.battle_occupied(view.actor_id))return wait(std::move(p),"BattlePending");
    // 每一工作/交易终态先消费真实来源；动作消失本身绝不成为收入证明。
    if(transition.pending_life_action_id){auto result=life.life_result(*transition.pending_life_action_id);if(!result||result->actor_id!=view.actor_id||result->action_id!=*transition.pending_life_action_id||result->fact_id.empty()||!c.state().facts.find(result->fact_id))return {false,{},common_failure("MissingLifeActionResult",view.actor_id,ErrorKind::internal).error};
        if(result->phase==LifeActivityPhase::active)return wait(std::move(p),"LifeActionPending");
        if(result->completed_tick<result->start_tick||result->completed_tick>tick)return {false,{},common_failure("InvalidLifeActionResult",view.actor_id,ErrorKind::internal).error};
        transition.last_life_fact_id=result->fact_id;transition.pending_life_action_id.reset();p.reason_fact_id=result->fact_id;
        if(result->phase==LifeActivityPhase::cancelled||!result->success)return wait(std::move(p),result->code.empty()?"LifeActionCancelled":result->code);
        if(result->settlement_tick<result->start_tick||result->settlement_tick>result->completed_tick)return {false,{},common_failure("UnsettledLifeActionResult",view.actor_id,ErrorKind::internal).error};
    }
    Owner owner{Owner::Kind::actor,view.actor_id,{},{}};const auto* wallet=c.wallet(person->wallet_id);const auto* bag=c.inventory(person->inventory_id);const auto* item=c.item_definition(intent.item_id);
    if(!wallet||!bag||!item||!owner_equal(wallet->owner,owner))return {false,{},common_failure("InvalidLivelihoodAssets",view.actor_id,ErrorKind::internal).error};
    auto funds=c.spendable(wallet->id);auto owned=c.count(bag->id,intent.item_id,{owner});auto weight=c.inventory_weight(bag->id);if(!funds.ok)return {false,{},funds.error};if(!owned.ok)return {false,{},owned.error};if(!weight.ok)return {false,{},weight.error};
    p.expected_versions.push_back(c.read_version({ResourceKey::Kind::wallet,wallet->id}));p.expected_versions.push_back(c.read_version({ResourceKey::Kind::inventory,bag->id}));
    Integer gap=std::max<Integer>(0,intent.target_quantity-owned.value);Integer desired_funds=0;StableId blocked="NoKnownSupply";
    if(gap==0){
        if(view.place_id==intent.return_place_id){transition.phase=PlanPhase::completed;transition.next_step_index=5;transition.wait_reason.clear();return {true,{std::move(p)},{}};}
        auto returning=route_cost(travel,view.settlement_id,intent.return_place_id,tick);if(!returning.ok)return wait(std::move(p),returning.error.original.code);
        if(!add(returning.value.first,policy_terms.cash_reserve,desired_funds))return {false,{},common_failure("LivelihoodBudgetOverflow").error};
        if(funds.value>=desired_funds){p.command=director_move_toward(travel,view,intent.return_place_id);if(!p.command)return wait(std::move(p),"NoLocalPassage");transition.phase=PlanPhase::delivering;transition.next_step_index=4;return {true,{std::move(p)},{}};}
        blocked="InsufficientTravelFunds";
    }else{
        if(item->base_price<=0||std::find(growth.tradeable_items.begin(),growth.tradeable_items.end(),intent.item_id)==growth.tradeable_items.end())return wait(std::move(p),"NotTradeable");
        Integer room=std::max<Integer>(0,bag->capacity-weight.value);Integer capacity=item->weight>0?room/item->weight:500;if(capacity<1)return wait(std::move(p),"InsufficientCapacity");
        std::vector<MarketOption> markets;Integer considered=0;
        for(const auto& source:view.supplies){if(source.item_id!=intent.item_id||source.observation_tick>tick||source.learned_tick>tick)continue;if(++considered>catalog.candidates_per_subject)break;
            if(source.quantity<1){blocked="InsufficientStock";continue;}if(source.valid_until_tick&&tick>*source.valid_until_tick){blocked="StaleSupplyObservation";continue;}
            for(const auto& row:growth.shops.rows()){const auto& shop=row.second;if(shop.inventory_id!=source.inventory_id)continue;const auto* place=travel.places.find(shop.place_id);if(!place)continue;
                if(shop.place_id==view.place_id){bool admitted=false;for(const auto& role:shop.representative_roles){auto context=local_context(c,view,role);if(!context){blocked="ServiceProviderUnavailable";continue;}auto quote=access.contact?access.contact(view.actor_id,*context,"trade",{}):Outcome<InteractionContext>{false,{},common_failure("InvalidInteractionContext").error};if(quote.ok){admitted=true;break;}if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};blocked=quote.error.original.code;}if(!admitted)continue;}
                auto outward=route_cost(travel,view.settlement_id,shop.place_id,tick);auto returning=route_cost(travel,place->settlement_id,intent.return_place_id,tick);if(!outward.ok||!returning.ok||(shop.place_id!=view.place_id&&!director_move_toward(travel,view,shop.place_id))){blocked="NoRoute";continue;}
                MarketOption option;option.source=&source;option.shop=&shop;option.quantity=std::min<Integer>({gap,source.quantity,capacity,500});option.cost=growth::trade_price(item->base_price,source.quantity,option.quantity,true);option.outward=outward.value.first;option.returning=returning.value.first;option.duration=outward.value.second;markets.push_back(option);
            }
        }
        // 先选择当前资金可执行的购买，不能因先遇到昂贵货源而错误继续做工。
        auto affordable=[&](const MarketOption& option){Integer amount=0,needed=0;const auto price=option.shop->place_id==view.place_id?growth::trade_price(item->base_price,option.source->quantity,1,true):option.cost;return add(option.outward,option.returning,amount)&&add(amount,policy_terms.cash_reserve,needed)&&add(needed,price,amount)&&amount<=funds.value;};
        std::stable_sort(markets.begin(),markets.end(),[&](const MarketOption& a,const MarketOption& b){const bool keep_a=a.shop->id==intent.selected_shop_id,keep_b=b.shop->id==intent.selected_shop_id;return std::make_tuple(!affordable(a),a.outward,a.duration,a.cost,!keep_a,a.shop->id)<std::make_tuple(!affordable(b),b.outward,b.duration,b.cost,!keep_b,b.shop->id);});
        if(markets.empty())return wait(std::move(p),blocked);
        const auto& market=markets.front();Integer reserve=0,total=0;if(!add(policy_terms.cash_reserve,market.returning,reserve)||!add(reserve,market.outward,total)||!add(total,market.cost,desired_funds))return {false,{},common_failure("LivelihoodBudgetOverflow").error};
        intent.selected_shop_id=market.shop->id;intent.selected_source_id=market.source->source_key;intent.selected_tick=tick;transition.known_report_ids={market.source->report_id};if(p.reason_fact_id.empty())p.reason_fact_id=market.source->fact_id;
        if(view.place_id!=market.shop->place_id){if(funds.value>=desired_funds){p.command=director_move_toward(travel,view,market.shop->place_id);if(!p.command)return wait(std::move(p),"NoLocalPassage");transition.phase=PlanPhase::sourcing;transition.next_step_index=2;return {true,{std::move(p)},{}};}}
        else{
            Integer quantity=market.quantity;while(quantity>0&&growth::trade_price(item->base_price,market.source->quantity,quantity,true)>funds.value-reserve)--quantity;
            if(quantity>0){std::optional<InteractionContext> context;StableId refusal="ServiceProviderUnavailable";
                for(const auto& role:market.shop->representative_roles){auto candidate=local_context(c,view,role);if(!candidate)continue;auto allowed=access.contact?access.contact(view.actor_id,*candidate,"trade",{}):Outcome<InteractionContext>{false,{},common_failure("InvalidInteractionContext").error};if(allowed.ok){context=allowed.value;break;}refusal=allowed.error.original.code;}
                if(!context)return wait(std::move(p),refusal);GrowthCommand request;request.kind="trade";request.shop_id=market.shop->id;request.item_id=intent.item_id;request.side="buy";request.quantity=quantity;request.quoted_total=growth::trade_price(item->base_price,market.source->quantity,quantity,true);request.market_revision=market.source->source_revision;request.payer_wallet_id=person->wallet_id;request.context=*context;p.command=command("trade",request);transition.phase=PlanPhase::purchasing;transition.next_step_index=3;return {true,{std::move(p)},{}};
            }
        }
        blocked="InsufficientFunds";
    }
    // 只有已知可改善当前资金缺口的工作才入选；远方发布条款不等于现场已获准。
    bool has_work=false,can_work=false;for(const auto& service:view.services)if(service.source.activity=="work"){has_work=true;if(person->fatigue+service.fatigue_delta<=policy_terms.work_fatigue_limit)can_work=true;}const bool tired=person->fatigue>=policy_terms.work_fatigue_limit||(has_work&&!can_work);struct ServiceCandidate{const KnownServiceOption* option;Integer net,duration,cost;};std::vector<ServiceCandidate> candidates;Integer considered=0;
    for(const auto& service:view.services){if(++considered>catalog.candidates_per_subject*2)break;if(service.source.observed_tick>tick||std::find(policy_terms.known_service_role_ids.begin(),policy_terms.known_service_role_ids.end(),service.source.role_id)==policy_terms.known_service_role_ids.end())continue;
        const bool rest=service.source.activity=="rest";if((tired&&!rest)||(!tired&&rest))continue;if(rest&&service.fatigue_delta>=0&&service.hp_gain<=0)continue;
        if(!rest&&(service.wage<=service.cost||person->fatigue+service.fatigue_delta>policy_terms.work_fatigue_limit)){blocked="WorkNeedsRecovery";continue;}
        if(service.source.place_id==view.place_id&&!service.refusal_reason.empty()){if(service.refusal_kind!=ErrorKind::rule)return {false,{},common_failure(service.refusal_reason,service.source.role_id,service.refusal_kind).error};blocked=service.refusal_reason;continue;}
        auto route=route_cost(travel,view.settlement_id,service.source.place_id,tick);if(!route.ok||(service.source.place_id!=view.place_id&&!director_move_toward(travel,view,service.source.place_id)))continue;Integer cost=0;if(!add(route.value.first,service.cost,cost))return {false,{},common_failure("LivelihoodBudgetOverflow").error};if(cost>funds.value){blocked="InsufficientTravelFunds";continue;}
        Integer net=service.wage-cost;if(!rest&&net<=0)continue;candidates.push_back({&service,rest?-service.fatigue_delta:net,service.duration+route.value.second,cost});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[&](const ServiceCandidate& a,const ServiceCandidate& b){const auto left=std::make_tuple(-a.net,a.duration,a.cost,a.option->source.role_id!=intent.selected_service_role_id,a.option->source.role_id);const auto right=std::make_tuple(-b.net,b.duration,b.cost,b.option->source.role_id!=intent.selected_service_role_id,b.option->source.role_id);return left<right;});
    if(candidates.empty())return wait(std::move(p),blocked);const auto& service=*candidates.front().option;intent.selected_service_role_id=service.source.role_id;intent.selected_tick=tick;
    if(service.source.place_id!=view.place_id){p.command=director_move_toward(travel,view,service.source.place_id);if(!p.command)return wait(std::move(p),"NoLocalPassage");}
    else{if(service.provider_actor_id.empty())return wait(std::move(p),"ServiceProviderUnavailable");GrowthCommand request;request.kind="service";request.activity=service.source.activity;request.context=InteractionContext{view.place_id,service.provider_actor_id,service.source.role_id,{},{},{},{}};p.command=command("service",request);}
    transition.phase=PlanPhase::sourcing;transition.next_step_index=1;return {true,{std::move(p)},{}};
}
} // 无工作、交易或行旅旁路；本文件不会写钱包、库存、动作或报告。
