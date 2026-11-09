#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
// 人物近况与共享需求沿实际来源显现；作者文本只解释，不授予岗位或虚构结算。
#include "regional_life_view.hpp"
#include "resident/domains/interaction/regional_outcome_dialogue.hpp"
#include "common_presentation.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
template<class T> Outcome<T> failed(const StableId& code){return {false,{},common_failure(code).error};}
// 地点判断只接受当前解析位置；不能从计划或旧消息推测此人已经到场。
bool here(const CommonStore& common,const StableId& actor,const StableId& place){auto location=common.resolved_location(actor);if(!location.ok)return false;auto local=std::get_if<SettlementLocation>(&location.value);return local&&local->place_id==place;}
const KnownReport* known(const SocialState& social,const StableId& actor,const StableId& report){if(!social.dynamic)return nullptr;auto* rows=social.dynamic->known_reports.find("0:"+actor);return rows?rows->find(report):nullptr;}
bool participant(const LifeActivityView& fact,const StableId& actor){return std::find(fact.participant_actor_ids.begin(),fact.participant_actor_ids.end(),actor)!=fact.participant_actor_ids.end();}
// 玩家只看到实际在场者的动作种类，不读取未来计划目标、私钱或背包。
StableId visible_activity(const CommonStore& common,const StableId& id){const auto* actor=common.actor(id);if(!actor||actor->action_id.empty())return "idle";const auto* action=common.action(actor->action_id);return action?action->kind:StableId{"busy"};}
StableId activity_text(const StableId& kind){if(kind=="service")return "正在办理已经接下的活计。";if(kind=="trade")return "正在交割货物。";if(kind=="travel"||kind=="move_local")return "正在行路。";if(kind=="idle")return "此刻人在这里。";return "眼下正在忙。";}
// 只有已获知并与会话完全对应的报告才可形成历史补句；不借最新来源替换它。
Outcome<StableId> conversation_text(const CommonStore& common,const InteractionSession& session,const GrowthLifeRead& life,const SocialState& social,const InteractionAccess& access){
    if(!session.regional_binding)return {true,{}, {}};const auto& binding=*session.regional_binding;
    if(binding.source.kind==RegionalSourceKind::task_outcome){if(!access.regional_outcome_text)return failed<StableId>("RegionalOutcomeUnavailable");auto result=access.regional_outcome_text(binding);if(!result.ok&&result.error.kind==ErrorKind::rule)return {true,"这份公托结果已经变化，请结束交谈后重新询问原办理人。",{}};return result;}
    if(binding.source.kind==RegionalSourceKind::runtime_offer)return {true,"这是本处同一份公开补给需求；是否仍可承接，以确认时的实际份额和办理条件为准。",{}};
    if(!social.dynamic||!known(social,session.actor_id,binding.source.report_id))return failed<StableId>("RegionalReportNotKnown");
    const auto* report=social.dynamic->reports.find(binding.source.report_id);auto fact=life.life_result(binding.source.fact_id);
    if(!report||!fact||report->fact_id!=fact->fact_id||fact->fact_id!=binding.source.fact_id||!participant(*fact,binding.provider_actor_id))return failed<StableId>("RegionalSourceUnavailable");
    StableId text="半日"+std::to_string(report->observation_tick)+"的近况：";
    if(fact->activity=="work")text+=fact->settlement_tick>=0&&fact->success?"这次短工已经做完，"+std::to_string(fact->amount)+"钱工钱已经结清。":"这次短工未完成；预留的工钱不算已经到账。";
    else if(fact->kind==LifeActivityKind::trade){text+=fact->settlement_tick>=0?"这笔货物已经按当时约定交割，数量"+std::to_string(fact->quantity)+"，价款"+std::to_string(fact->amount)+"钱。":"这笔交易尚未结算。";if(fact->phase==LifeActivityPhase::cancelled)text+="后续行动已取消，已经发生的交割没有倒退。";}
    else return failed<StableId>("RegionalSourceUnavailable");
    if(report->valid_until_tick&&*report->valid_until_tick<common.tick())text+="这条消息已经过时，当前情况还需当面核实。";else text+="这是当时的结果，不代表此刻的钱货。";
    return {true,std::move(text),{}};
}
}
// 所有候选保持稳定顺序，生活来源新旧均保留供旧会话复验；开新话题优先最近真实来源。
Outcome<std::vector<RegionalOpportunityBinding>> resolve_regional_life_candidates(const WorldRead& world,const PreparedCatalog& catalog,const OrganizationRead& organization,const GrowthLifeRead& life,const RegionalSharePreview& share,const StableId& observer,const StableId& place,const EconomicSupplyPolicyRead* issued_supply,const CurrentMomentRead* actual_clock){
    auto* common=dynamic_cast<const CommonStore*>(&world.common);if(!common||!catalog.interaction||!catalog.interaction->regional_enabled||!world.interaction.regional||!world.social.dynamic||!catalog.common||!catalog.tasks||!world.affairs)return failed<std::vector<RegionalOpportunityBinding>>("RegionalLifeUnavailable");
    if(!common->actor(observer)||!here(*common,observer,place))return failed<std::vector<RegionalOpportunityBinding>>("PlaceNotObservable");
    std::vector<RegionalOpportunityBinding> out;
    for(const auto& topic:catalog.interaction->regional_actor_topics){
        const auto* speaker=common->actor(topic.actor_id);if(!speaker||!speaker->alive||!here(*common,topic.actor_id,place)||topic.actor_id==observer)continue;
        std::vector<const Report*> reports;for(const auto& row:world.social.dynamic->reports.rows())if(row.second.payload_kind=="life_activity"&&known(world.social,topic.actor_id,row.first))reports.push_back(&row.second);
        std::sort(reports.begin(),reports.end(),[](const auto* a,const auto* b){return a->observation_tick!=b->observation_tick?a->observation_tick>b->observation_tick:a->id<b->id;});
        for(const auto* report:reports){
            if(report->valid_until_tick&&*report->valid_until_tick<common->tick())continue;
            auto result=life.life_result(report->fact_id);if(!result||result->phase==LifeActivityPhase::active||result->actor_id!=topic.actor_id||!participant(*result,topic.actor_id)||(topic.activity=="work"?result->activity!="work":result->kind!=LifeActivityKind::trade))continue;
            if(!share)continue;auto allowed=share(report->id,topic.actor_id,observer,{topic.activity});if(!allowed.ok){if(allowed.error.kind!=ErrorKind::rule)return {false,{},allowed.error};continue;}
            RegionalOpportunityBinding b;b.binding_id=topic.id;b.topic_id=topic.id;b.dialogue_id=topic.dialogue_id;b.observer_actor_id=observer;b.provider_actor_id=topic.actor_id;b.place_id=place;b.source.kind=RegionalSourceKind::life_result;b.source.fact_id=result->fact_id;b.source.report_id=report->id;out.push_back(std::move(b));
        }
    }
    const auto observed=known_subject_view(*common,world.social,organization,observer,&world.interaction);
    auto opportunities=query_opportunities_with_issued_supply(*common,*world.affairs,world.tasks,observed,issued_supply);if(!opportunities.ok)return {false,{},opportunities.error};
    for(const auto& topic:catalog.interaction->regional_need_topics){
        const auto* role=catalog.common->roles.find(topic.role_id);const auto* assignment=common->state().roles.find(topic.role_id);if(!role||role->place_id!=place||!assignment||!assignment->enabled)continue;
        const auto* provider=common->actor(assignment->actor_id);if(!provider||!provider->alive||!here(*common,provider->id,place))continue;
        for(const auto& opportunity:opportunities.value){
            const auto* definition=resolve_offer_definition(world.tasks,*catalog.tasks,opportunity.offer);if(!definition||definition->accept_role!=topic.role_id||opportunity.item_id!=topic.item_id||opportunity.offer.kind!=OfferRef::Kind::runtime)continue;
            TaskCommand task;task.kind="accept_offer";task.offer_id=opportunity.offer.id;task.offer_ref=opportunity.offer;task.need_revision=opportunity.need_revision;task.quantity=std::min(opportunity.available_amount,definition->goal.quantity);task.context={place,provider->id,topic.role_id,{},{},{},{}};
            if(*task.quantity<1)continue;RegionalOpportunityBinding b;b.binding_id=topic.id;b.topic_id=topic.id;b.dialogue_id=topic.dialogue_id;b.observer_actor_id=observer;b.provider_actor_id=provider->id;b.role_id=topic.role_id;b.place_id=place;b.source.kind=RegionalSourceKind::runtime_offer;b.source.offer_ref=opportunity.offer;b.source.need_id=opportunity.need_id;b.source.need_revision=opportunity.need_revision;b.task_command=checkpoint_make_shared<const TaskCommand>(std::move(task));out.push_back(std::move(b));
        }
    }
    auto outcomes=resolve_regional_outcome_candidates(*common,world.tasks,*catalog.tasks,*catalog.interaction,observer,place,actual_clock);if(!outcomes.ok)return {false,{},outcomes.error};out.insert(out.end(),outcomes.value.begin(),outcomes.value.end());
    return {true,std::move(out),{}};
}
// 同地普通人直接入现有名单，缺席岗位仅留地方说明，绝不伪造人物肖像。
Outcome<BoundaryValue> query_regional_life_topics(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const GrowthLifeRead& life,const SocialState& social,const StableId& observer,const StableId& place,Integer cursor){
    if(!catalog.regional_enabled||!state.regional)return failed<V>("RegionalLifeUnavailable");if(cursor<0||cursor%64!=0)return failed<V>("InvalidRegionalCursor");
    if(!common.actor(observer)||!here(common,observer,place))return failed<V>("PlaceNotObservable");CommonPresentation names(common.state(),*catalog.common);A rows;std::set<StableId> actors;
    for(const auto& topic:catalog.regional_actor_topics){
        const auto* actor=common.actor(topic.actor_id);if(!actor||!actor->alive||actor->id==observer||!here(common,actor->id,place))continue;
        if(actors.insert(actor->id).second){auto name=names.display_name(actor->id);if(!name.ok)return {false,{},name.error};const auto activity=visible_activity(common,actor->id);rows.push_back(boundary::obj({{"kind",V{"actor"}},{"actor_id",V{actor->id}},{"display_name",V{name.value}},{"activity",V{activity}},{"text",V{activity_text(activity)}}}));}
        auto quote=preview_regional_talk(common,state,catalog,access,observer,topic.id,actor->id,{});
        if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};
        // 没有可以分享的结果不假造往事；忙碌已经由人物当前动作解释。
        if(!quote.ok&&(quote.error.original.code=="RegionalSourceUnavailable"||quote.error.original.code=="AlreadyTalking"))continue;
        D row{{"kind",V{"topic"}},{"actor_id",V{actor->id}},{"topic_id",V{topic.id}},{"text",V{topic.activity=="work"?"问问做工近况":"问问采买近况"}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}};
        if(quote.ok){const auto& binding=*quote.value.regional_binding;row.emplace_back("source_fact_id",V{binding.source.fact_id});row.emplace_back("source_report_id",V{binding.source.report_id});row.emplace_back("action",boundary::obj({{"command",V{"talk_actor"}},{"params",boundary::obj({{"actor_id",V{actor->id}},{"topic_id",V{topic.id}}})}}));}rows.push_back(V{std::move(row)});
    }
    for(const auto& topic:catalog.regional_need_topics){
        const auto* role=catalog.common->roles.find(topic.role_id);if(!role||role->place_id!=place)continue;
        const auto* assignment=common.state().roles.find(topic.role_id);const auto* provider=assignment?common.actor(assignment->actor_id):nullptr;
        auto quote=preview_regional_talk(common,state,catalog,access,observer,topic.id,{},topic.role_id);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};
        StableId code=quote.ok?StableId{}:quote.error.original.code;if(!assignment||!assignment->enabled)code="RoleNotAuthorized";else if(!provider||!provider->alive||!here(common,provider->id,place))code="PersonAbsent";else if(!provider->action_id.empty())code="ActorBusy";
        D row{{"kind",V{"need_topic"}},{"role_id",V{topic.role_id}},{"topic_id",V{topic.id}},{"text",V{"本处公开补给事务"}},{"available",V{quote.ok}},{"code",V{code}}};
        if(quote.ok){const auto& b=*quote.value.regional_binding;row.emplace_back("actor_id",V{b.provider_actor_id});row.emplace_back("need_id",V{b.source.need_id});row.emplace_back("need_revision",V{b.source.need_revision});row.emplace_back("offer_id",V{b.source.offer_ref->id});row.emplace_back("quantity",V{*b.task_command->quantity});row.emplace_back("action",boundary::obj({{"command",V{"talk"}},{"params",boundary::obj({{"role_id",V{topic.role_id}},{"topic_id",V{topic.id}}})}}));}rows.push_back(V{std::move(row)});
    }
    for(const auto& topic:catalog.regional_outcome_topics){
        const auto* role=catalog.common->roles.find(topic.role_id);if(!role||role->place_id!=place)continue;
        auto quote=preview_regional_talk(common,state,catalog,access,observer,topic.id,{},topic.role_id);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};
        D row{{"kind",V{"outcome_topic"}},{"role_id",V{topic.role_id}},{"topic_id",V{topic.id}},{"text",V{"回访本人已办理的公托"}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}};
        if(quote.ok){const auto& b=*quote.value.regional_binding;row.emplace_back("actor_id",V{b.provider_actor_id});row.emplace_back("task_id",V{b.source.task_outcome->task_id});row.emplace_back("contract_id",V{b.source.task_outcome->contract_id});row.emplace_back("action",boundary::obj({{"command",V{"talk"}},{"params",boundary::obj({{"role_id",V{topic.role_id}},{"topic_id",V{topic.id}}})}}));}rows.push_back(V{std::move(row)});
    }
    StableId conversation;for(const auto& session:state.interactions)if(session.actor_id==observer&&session.regional_binding){auto text=conversation_text(common,session,life,social,access);if(!text.ok)return {false,{},text.error};conversation=text.value;}
    if(static_cast<std::size_t>(cursor)>rows.size()||(cursor>0&&static_cast<std::size_t>(cursor)==rows.size()))return failed<V>("InvalidRegionalCursor");const auto end=std::min(rows.size(),static_cast<std::size_t>(cursor)+64);A page;for(std::size_t i=cursor;i<end;++i)page.push_back(std::move(rows[i]));
    return {true,boundary::obj({{"ok",V{true}},{"place_id",V{place}},{"actor_id",V{observer}},{"revision",V{common.revision()}},{"tick",V{common.tick()}},{"rows",V{std::move(page)}},{"next_cursor",V{end<rows.size()?static_cast<Integer>(end):Integer(-1)}},{"conversation_text",V{conversation}}}),{}};
}
// 只检索会话/曝光真正引用的来源，冷恢复不要求人物仍在原地，也不改写过期来源。
Status validate_regional_life_sources(const WorldRead& world,const PreparedCatalog& catalog,const GrowthLifeRead& life,const CurrentMomentRead* actual_clock){
    if(!catalog.interaction||!catalog.interaction->regional_enabled)return common_success();if(!world.interaction.regional||!world.social.dynamic)return common_failure("RegionalLifeStateMismatch");
    auto check=[&](const RegionalSourceRef& source,const StableId& topic_id,const StableId& observer)->Status{
        if(source.kind==RegionalSourceKind::life_result){const auto* report=world.social.dynamic->reports.find(source.report_id);auto result=life.life_result(source.fact_id);if(!report||!result||report->fact_id!=source.fact_id||report->payload_kind!="life_activity"||result->fact_id!=source.fact_id||result->phase==LifeActivityPhase::active)return common_failure("MissingRegionalLifeSource");bool authored=false;for(const auto& topic:catalog.interaction->regional_actor_topics)if(topic.id==topic_id&&topic.actor_id==result->actor_id&&(topic.activity=="work"?result->activity=="work":result->kind==LifeActivityKind::trade))authored=true;if(!authored)return common_failure("RegionalSourceActorMismatch");}
        else if(source.kind==RegionalSourceKind::task_outcome){const auto* common=dynamic_cast<const CommonStore*>(&world.common);if(!common||!catalog.tasks||!source.task_outcome)return common_failure("RegionalOutcomeUnavailable");bool authored=false;for(const auto& topic:catalog.interaction->regional_outcome_topics)if(topic.id==topic_id&&topic.task_template_id==source.task_outcome->task_template_id)authored=true;if(!authored)return common_failure("UnknownRegionalTopic");return validate_regional_outcome_source(*common,world.tasks,*catalog.tasks,observer,source,false,actual_clock);}
        else{const auto* offer=source.offer_ref?world.tasks.offer_instances.find(source.offer_ref->id):nullptr;if(!offer||offer->source_need_id!=source.need_id||source.need_revision<offer->need_revision_at_creation)return common_failure("MissingRegionalNeedSource");const auto* need=world.affairs?world.affairs->needs.find(source.need_id):nullptr;if(!need||source.need_revision>need->revision)return common_failure("MissingRegionalNeedSource");}return common_success();
    };
    for(const auto& e:world.interaction.regional->exposures){auto valid=check(e.source,e.binding_id,e.observer_actor_id);if(!valid.ok)return valid;}
    for(const auto& s:world.interaction.interactions)if(s.regional_binding){auto valid=check(s.regional_binding->source,s.regional_binding->topic_id,s.actor_id);if(!valid.ok)return valid;}
    return common_success();
}
}
