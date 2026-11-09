#include "resident/storage/common_capture_read_set.hpp"
// 真实短工/交易的有限证据；金额只在原资源操作成功的调用点登记，查询没有副作用。
#include "growth_life_results.hpp"
#include "growth_trade_terminal_codec.hpp"
#include "growth_trade_terminal_read.hpp"
#include "growth_action_time_read.hpp"
#include "resident/model/common_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 相同上下文必须绑定原人、原岗位和原场所，不能用现任者替换历史参与者。
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id;}
// 比较被冻结的作者服务条款，不把当前余额当成历史发薪证明。
bool same_service(const ServiceTerms& a,const ServiceTerms& b){return a.duration==b.duration&&a.cost==b.cost&&a.wage==b.wage&&a.hp_gain==b.hp_gain&&a.fatigue_delta==b.fatigue_delta&&a.wallet_id==b.wallet_id;}
// 交易来源包括货权、双方接触和原价格，不依赖当前市场状态。
bool same_trade(const TradeTerms& a,const TradeTerms& b){return a.shop_id==b.shop_id&&a.item_id==b.item_id&&a.side==b.side&&a.payer_wallet_id==b.payer_wallet_id&&a.quantity==b.quantity&&a.total==b.total&&a.market_revision==b.market_revision&&a.valid_tick==b.valid_tick&&owner_equal(a.owner,b.owner)&&same_context(a.context,b.context)&&a.bargain_quote_id==b.bargain_quote_id&&a.bargain_basis_sha256==b.bargain_basis_sha256&&a.discount_bp==b.discount_bp;}
// 历史报告只来自终态work/trade；休息不扩充本批叙事来源。
bool reportable(const LifeActivityView& v){return v.phase!=LifeActivityPhase::active&&(v.kind==LifeActivityKind::trade||v.activity=="work");}
}
// 精确返回原行动记录；未登记来源保持缺失，不补造终态。
std::optional<LifeActivityView> growth_life_result(const GrowthState& state,const StableId& id){const auto* record=state.life_activities.find(id);return record?std::optional<LifeActivityView>(record->view):std::nullopt;}
// 仅枚举时间窗口内可传播的终态来源，活动记录仍由直接查询读取。
std::vector<StableId> growth_life_fact_ids(const GrowthState& state,Integer after,Integer through){std::vector<StableId> result;for(const auto& row:state.life_activities.rows()){const auto& v=row.second.view;if(reportable(v)&&v.completed_tick>after&&v.completed_tick<=through)result.push_back(row.first);}return result;}
// 私密历史仅投影原actor/provider与终态，不复制完整钱包或当前库存。
std::optional<DynamicFactView> growth_life_dynamic_fact(const GrowthState& state,const StableId& id){const auto* record=state.life_activities.find(id);if(!record||!reportable(record->view))return {};const auto& v=record->view;DynamicFactView fact;fact.id=v.fact_id;fact.kind="life_activity";fact.rule_version="regional_life_v1";fact.tick=v.completed_tick;fact.subject_id=v.actor_id;fact.outcome=(v.kind==LifeActivityKind::trade?StableId{"trade"}:StableId{"work"})+(v.phase==LifeActivityPhase::completed?"_completed":"_cancelled");for(const auto& actor:v.participant_actor_ids)fact.participants.push_back({{SubjectKind::actor,actor},actor==v.actor_id?"actor":"provider"});return fact;}
// 结算前拒绝重复action身份，防止重入导致重复支付或转货。
Status guard_life_activity_start(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const ActionCoreView& action){if(!catalog.regional_life)return common_success();if(action.kind!="service"&&action.kind!="trade")return common_failure("InvalidLifeActivityKind",action.id);if(state.life_activities.find(action.id)||common.state().facts.find(action.id))return common_failure("LifeActivityAlreadyStarted",action.id);return common_success();}
// 原开工recipe成功后冻结精确条款，work预留不冒充已发薪。
Status record_life_activity_start(CommonStore& common,GrowthState& state,const GrowthCatalog& catalog,const ActionCoreView& action,const GrowthQuote& quote,std::optional<Integer> actual_started_tick){
    if(actual_started_tick&&(*actual_started_tick<common.tick()||action.start_tick!=*actual_started_tick))return common_failure("LifeActivityActualStartClockMismatch",action.id);
    if(!catalog.regional_life)return common_success();auto guarded=guard_life_activity_start(common,state,catalog,action);if(!guarded.ok)return guarded;
    const auto* actor=common.actor(action.actor_id);if(!actor||!quote.context||!common.actor(quote.context->provider_actor_id))return common_failure("InvalidLifeActivityContext",action.id);
    LifeActivityRecord record;auto& v=record.view;v.fact_id=action.id;v.action_id=action.id;v.actor_id=action.actor_id;v.provider_actor_id=quote.context->provider_actor_id;v.start_tick=action.start_tick;v.participant_actor_ids={action.actor_id,v.provider_actor_id};record.context=*quote.context;
    if(action.kind=="service"){
        const auto* terms=std::get_if<ServiceTerms>(&quote.data);if(!terms)return common_failure("InvalidLifeServiceTerms",action.id);record.terms=*terms;record.cost_paid=terms->cost;v.activity=quote.command.activity;v.kind=LifeActivityKind::service;
        if(v.activity=="work"){v.payer_wallet_id=terms->wallet_id;v.payee_wallet_id=actor->wallet_id;}
        else if(v.activity=="rest"){v.payer_wallet_id=actor->wallet_id;v.payee_wallet_id=terms->wallet_id;v.amount=terms->cost;v.settlement_tick=action.start_tick;}
        else return common_failure("InvalidLifeServiceActivity",action.id);
    }else{
        const auto* terms=std::get_if<TradeTerms>(&quote.data);const auto* shop=terms?catalog.shops.find(terms->shop_id):nullptr;if(!terms||!shop)return common_failure("InvalidLifeTradeTerms",action.id);record.terms=*terms;v.kind=LifeActivityKind::trade;v.activity=terms->side;v.item_id=terms->item_id;v.quantity=terms->quantity;v.amount=terms->total;v.settlement_tick=action.start_tick;
        const bool buy=terms->side=="buy";v.payer_wallet_id=buy?terms->payer_wallet_id:shop->wallet_id;v.payee_wallet_id=buy?shop->wallet_id:terms->payer_wallet_id;v.from_inventory_id=buy?shop->inventory_id:actor->inventory_id;v.to_inventory_id=buy?actor->inventory_id:shop->inventory_id;
    }
    auto appended=common.append_fact({action.id,"life_activity","growth"});if(!appended.ok)return appended;if(!state.life_activities.insert(action.id,std::move(record)))return common_failure("DuplicateLifeActivity",action.id);return common_success();
}
// 原完成recipe成功后记实付工资和终态；重复完成明确拒绝。
Status record_life_activity_completion(const CommonStore&,GrowthState& state,const GrowthCatalog& catalog,const ActionCoreView& action,Integer tick){
    if(!catalog.regional_life)return common_success();auto* record=state.life_activities.find(action.id);if(!record)return common_failure("MissingLifeActivity",action.id);auto& v=record->view;if(v.phase!=LifeActivityPhase::active)return common_failure("LifeActivityAlreadyCompleted",action.id);if(tick<action.start_tick||v.action_id!=action.id||v.actor_id!=action.actor_id)return common_failure("InvalidLifeCompletion",action.id);
    if(v.kind==LifeActivityKind::service){const auto* terms=std::get_if<ServiceTerms>(&record->terms);if(!terms)return common_failure("InvalidLifeServiceTerms",action.id);record->wage_paid=terms->wage;if(v.activity=="work"){v.amount=terms->wage;v.settlement_tick=tick;}}
    v.phase=LifeActivityPhase::completed;v.completed_tick=tick;v.success=true;v.code.clear();return common_success();
}
// 取消只记录失败和日期，不撤销已支付的预付款或交易。
Status cancel_life_activity(const CommonStore& common,GrowthState& state,const StableId& id,Integer tick,const StableId& code){auto* record=state.life_activities.find(id);if(!record)return common_success();auto& v=record->view;if(v.phase!=LifeActivityPhase::active)return common_success();if(code.empty()||tick<v.start_tick||tick>common.tick()+1)return common_failure("InvalidLifeCancellation",id);v.phase=LifeActivityPhase::cancelled;v.completed_tick=tick;v.success=false;v.code=code;return common_success();}
// 冷恢复逐项核原人、原资产、条款、阶段和真实活动引用，缺证据即拒绝。
static Status validate_life_activities_impl(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthActionTimeRead* exact,const std::vector<ActionTimeRegistration>* registrations,const WorldMoment* actual_now){
    if(catalog.complete_study_sources!=bool(exact)||bool(exact)!=bool(registrations)||bool(exact)!=bool(actual_now))return common_failure("CompleteLifeTimeSourcesRequired");
    if(!catalog.regional_life)return state.life_activities.rows().empty()?common_success():common_failure("RegionalLifeUnavailable");
    for(const auto& row:state.life_activities.rows()){
        const auto& record=row.second;const auto& v=record.view;const auto* actor=common.actor(v.actor_id);const auto* provider=common.actor(v.provider_actor_id);const auto* header=common.state().facts.find(row.first);const auto* role=shared.roles.find(record.context.role_id);const auto* payload=state.actions.find(row.first);const auto* action=common.action(row.first);
        if(v.fact_id!=row.first||v.action_id!=row.first||!actor||!provider||v.actor_id==v.provider_actor_id||!header||header->kind!="life_activity"||header->domain!="growth"||!role||record.context.provider_actor_id!=v.provider_actor_id||record.context.place_id!=role->place_id||record.context.kind||record.context.encounter_id||record.context.input_error||v.participant_actor_ids!=std::vector<StableId>{v.actor_id,v.provider_actor_id}||v.start_tick<0||(!exact&&v.start_tick>common.tick())||v.quantity<0||v.amount<0||record.cost_paid<0||record.wage_paid<0||!common.wallet(v.payer_wallet_id)||!common.wallet(v.payee_wallet_id)||v.payer_wallet_id==v.payee_wallet_id)return common_failure("InvalidLifeActivity",row.first);
        if(v.kind!=LifeActivityKind::service&&v.kind!=LifeActivityKind::trade)return common_failure("InvalidLifeActivityKind",row.first);
        if(v.phase!=LifeActivityPhase::active&&v.phase!=LifeActivityPhase::completed&&v.phase!=LifeActivityPhase::cancelled)return common_failure("InvalidLifeActivityPhase",row.first);
        const bool active=v.phase==LifeActivityPhase::active,completed=v.phase==LifeActivityPhase::completed;
        auto terminal_shape=growth_trade_terminal_codec::validate_activity(record,exact!=nullptr);if(!terminal_shape.ok)return terminal_shape;
        if(exact&&!active&&v.kind==LifeActivityKind::trade){auto terminal=read_growth_trade_terminal_v2(common,state,catalog,shared,{"growth","life_activity",row.first},2,*actual_now);if(!terminal.ok)return {false,{},terminal.error};}

        if(exact){
            const StableId kind=v.kind==LifeActivityKind::service?"service":"trade";const ActionTimeRegistration* reg=nullptr;
            for(const auto& row:*registrations)if(row.owner_domain=="growth"&&row.action_kind==kind){if(reg)return common_failure("DuplicateCompleteGrowthRegistration",kind);reg=&row;}
            if(!reg)return common_failure("CompleteGrowthRegistrationRequired",kind);
            auto proof=exact->read_action_time_owner(*reg,v.action_id);if(!proof.ok)return {false,{},proof.error};
            auto actual=read_growth_original_time(*exact,*reg,v.action_id);if(!actual.ok)return {false,{},actual.error};
            if(actual.value.actor_id!=v.actor_id||actual.value.started_at.half_day_tick!=v.start_tick||actual.value.terminal==active||proof.value.participant_actor_ids!=v.participant_actor_ids)return common_failure("LifeExactOriginalTimeMismatch",row.first);
            if(active){auto remaining=exact->read_original_action_remaining(*reg,proof.value,*actual_now);if(!remaining.ok)return {false,{},remaining.error};}
            else {if(!actual.value.ended_at||actual.value.ended_at->half_day_tick!=v.completed_tick)return common_failure("LifeExactOriginalTerminalMismatch",row.first);auto adopted=exact->validate_adopted_terminal_source(*reg,v.action_id);if(!adopted.ok)return adopted;}
        }
        if(active){if(!action||!payload||action->actor_id!=v.actor_id||action->start_tick!=v.start_tick||action->kind!=(v.kind==LifeActivityKind::trade?"trade":"service")||v.completed_tick!=-1||v.success||!v.code.empty()||!payload->quote.context||!same_context(record.context,*payload->quote.context))return common_failure("InvalidActiveLifeActivity",row.first);}
        else if(action||payload||v.completed_tick<v.start_tick||(!exact&&v.completed_tick>common.tick())||v.success!=completed||(completed?!v.code.empty():v.code.empty()))return common_failure("InvalidLifeActivityCompletion",row.first);
        if(v.kind==LifeActivityKind::service){
            const auto* terms=std::get_if<ServiceTerms>(&record.terms);const auto* definition=catalog.roles.find(record.context.role_id);const auto* authored=definition?definition->activities.find(v.activity):nullptr;
            if(!terms||!authored||!same_service(*terms,*authored)||(v.activity!="work"&&v.activity!="rest")||terms->duration<1||terms->cost<0||terms->wage<0||record.cost_paid!=terms->cost||record.wage_paid!=(completed?terms->wage:0)||!v.item_id.empty()||v.quantity||!v.from_inventory_id.empty()||!v.to_inventory_id.empty())return common_failure("InvalidLifeServiceEvidence",row.first);
            if(v.activity=="work"){if(v.payer_wallet_id!=terms->wallet_id||v.payee_wallet_id!=actor->wallet_id||v.amount!=record.wage_paid||v.settlement_tick!=(completed?v.completed_tick:-1))return common_failure("InvalidLifeWageEvidence",row.first);}
            else if(v.payee_wallet_id!=terms->wallet_id||v.payer_wallet_id!=actor->wallet_id||v.amount!=record.cost_paid||v.settlement_tick!=v.start_tick)return common_failure("InvalidLifeRestEvidence",row.first);
            if(!exact&&completed&&v.completed_tick-v.start_tick!=terms->duration)return common_failure("InvalidLifeServiceDuration",row.first);
            if(active){const auto* frozen=std::get_if<ServiceTerms>(&payload->quote.data);if(!frozen||!same_service(*terms,*frozen)||payload->request.activity!=v.activity)return common_failure("LifeServiceActionMismatch",row.first);}
        }else{
            const auto* terms=std::get_if<TradeTerms>(&record.terms);const auto* shop=terms?catalog.shops.find(terms->shop_id):nullptr;const bool buy=v.activity=="buy";
            if(!terms||!shop||(v.activity!="buy"&&v.activity!="sell")||v.activity!=terms->side||v.item_id!=terms->item_id||!shared.items.find(v.item_id)||v.quantity!=terms->quantity||v.quantity<1||v.quantity>500||v.amount!=terms->total||v.settlement_tick!=v.start_tick||terms->valid_tick!=v.start_tick||!same_context(record.context,terms->context)||record.cost_paid||record.wage_paid||!common.inventory(v.from_inventory_id)||!common.inventory(v.to_inventory_id)||v.from_inventory_id==v.to_inventory_id||v.payer_wallet_id!=(buy?terms->payer_wallet_id:shop->wallet_id)||v.payee_wallet_id!=(buy?shop->wallet_id:terms->payer_wallet_id)||(buy?v.from_inventory_id:v.to_inventory_id)!=shop->inventory_id||(buy?v.to_inventory_id:v.from_inventory_id)!=actor->inventory_id)return common_failure("InvalidLifeTradeEvidence",row.first);
            if(!exact&&completed&&v.completed_tick-v.start_tick!=1)return common_failure("InvalidLifeTradeDuration",row.first);
            if(active){const auto* frozen=std::get_if<TradeTerms>(&payload->quote.data);if(!frozen||!same_trade(*terms,*frozen))return common_failure("LifeTradeActionMismatch",row.first);}
        }
    }
    for(const auto& row:state.actions.rows())if((row.second.request.kind=="service"||row.second.request.kind=="trade")&&!state.life_activities.find(row.first))return common_failure("MissingLifeActivity",row.first);
    return common_success();
}
Status validate_life_activities(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared){return validate_life_activities_impl(common,state,catalog,shared,nullptr,nullptr,nullptr);}
Status validate_life_activities_at_moment(const CommonStore& common,const GrowthState& state,const GrowthCatalog& catalog,const CommonCatalog& shared,const GrowthActionTimeRead& reader,const std::vector<ActionTimeRegistration>& registrations,const WorldMoment& now){return validate_life_activities_impl(common,state,catalog,shared,&reader,&registrations,&now);}
// 当前候选内回收超额无引用终态，保留活动与中央传入的因果pin。
Status prune_life_evidence(CommonStore& common,GrowthState& state,const std::vector<StableId>& pins){
    const std::set<StableId> retained(pins.begin(),pins.end());std::map<StableId,std::vector<const LifeActivityView*>> by_actor;
    for(const auto& row:state.life_activities.rows())if(row.second.view.phase!=LifeActivityPhase::active&&!retained.count(row.first))by_actor[row.second.view.actor_id].push_back(&row.second.view);
    std::vector<StableId> remove;
    for(auto& row:by_actor){auto& values=row.second;std::stable_sort(values.begin(),values.end(),[](const auto* a,const auto* b){return a->completed_tick!=b->completed_tick?a->completed_tick>b->completed_tick:a->fact_id>b->fact_id;});for(std::size_t n=16;n<values.size();++n)remove.push_back(values[n]->fact_id);}
    for(const auto& id:remove){const auto* header=common.state().facts.find(id);if(!header||header->kind!="life_activity"||header->domain!="growth"||common.action(id)||state.actions.find(id))return common_failure("LifeEvidenceStillActive",id);}
    for(const auto& id:remove){state.life_activities.erase(id);invalidate_common_capture_source(common,id);common.state().facts.erase(id);}return common_success();
}
// growth4逐字段保存有限读视图；未知枚举拒绝，不保存任意运行载荷。
Status visit_life_activity_view(CodecVisitor& c,LifeActivityView& v){
    c.field("fact_id",v.fact_id);c.field("action_id",v.action_id);c.field("actor_id",v.actor_id);c.field("provider_actor_id",v.provider_actor_id);c.field("activity",v.activity);Integer kind=static_cast<Integer>(v.kind),phase=static_cast<Integer>(v.phase);c.field("kind",kind);c.field("phase",phase);if(kind<0||kind>1||phase<0||phase>2)return c.reject("InvalidLifeActivityTag");if(c.reading()){v.kind=static_cast<LifeActivityKind>(kind);v.phase=static_cast<LifeActivityPhase>(phase);}c.field("start_tick",v.start_tick);c.field("settlement_tick",v.settlement_tick);c.field("completed_tick",v.completed_tick);c.field("success",v.success);c.field("code",v.code);c.field("item_id",v.item_id);c.field("quantity",v.quantity);c.field("amount",v.amount);c.field("payer_wallet_id",v.payer_wallet_id);c.field("payee_wallet_id",v.payee_wallet_id);c.field("from_inventory_id",v.from_inventory_id);c.field("to_inventory_id",v.to_inventory_id);return common_checkpoint::strings(c,"participant_actor_ids",v.participant_actor_ids);
}
}
