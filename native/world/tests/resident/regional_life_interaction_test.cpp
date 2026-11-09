// 有限安全消费者覆盖真实会话入口、来源冻结和新旧冷边界，不运行完整Main或重写资产规则。
#include "resident/domains/interaction/regional_life_dialogue.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/interaction_actions.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
// 小夹具只检验交互权限边界；可分享来源由明确typed候选替身提供。
struct Fixture {
    std::shared_ptr<CommonCatalog> common_catalog=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},common_catalog};InteractionCatalog catalog;InteractionState state;InteractionAccess access;
    RegionalOpportunityBinding candidate;int shares=0;
    Fixture(){
        common_catalog->places.insert("square",{"square","town",{}});common_catalog->items.insert("grain",{"grain",1,10});
        common_catalog->roles.insert("clerk",{"clerk","npc","square",{}, {},{"accept_offer"},0});
        for(const auto& id:{"player","npc","replacement"}){ActorRecord actor;actor.id=id;actor.alive=true;actor.available=true;actor.location=SettlementLocation{"town","square"};common.state().actors.insert(id,actor);common_catalog->actor_names.insert(id,{id,{},{}});state.knowledge.push_back({id,{}});}
        common.state().roles.insert("clerk",{"npc",true,{}});catalog.common=common_catalog;catalog.place_ids={"square"};
        DialogueDefinition d;d.id="ordinary";d.entry="say";DialogueNode say;say.id="say";say.kind="say";say.speaker="$provider";say.text_key="hello";say.next="choice";
        DialogueNode choice;choice.id="choice";choice.kind="choice";choice.text_key="choose";choice.options.push_back({"later","end","later",{}});DialogueNode end;end.id="end";end.kind="end";d.nodes={say,choice,end};catalog.dialogues.push_back(d);
        d.id="need";d.context_role="clerk";d.nodes[1].options.push_back({"accept","accept","accept",{}});DialogueNode accept;accept.id="accept";accept.kind="action";DialogueCommand command;command.kind="accept_offer";command.field_order={"kind"};accept.command=command;accept.on_success="say";accept.on_failure="say";d.nodes.push_back(accept);catalog.dialogues.push_back(d);
        auto config=parse_boundary_json(R"({"actor_topics":[{"id":"work","actor_id":"npc","dialogue_id":"ordinary","source_kind":"life_result","activity":"work","cooldown_ticks":2}],"need_topics":[{"id":"grain","role_id":"clerk","dialogue_id":"need","item_id":"grain","cooldown_ticks":2}]})");assert(config.ok&&configure_regional_life(catalog,config.value).ok&&initialize_regional_life(state).ok);
        candidate.binding_id="work";candidate.topic_id="work";candidate.dialogue_id="ordinary";candidate.observer_actor_id="player";candidate.provider_actor_id="npc";candidate.place_id="square";candidate.source.fact_id="service:1";candidate.source.report_id="report:service:1";
        access.resolve_location=[this](const StableId& actor){return common.actor(actor)->location;};access.battle_occupied=[](const StableId&){return false;};
        access.role=[this](const StableId& role)->std::optional<RoleAssignmentView>{const auto* r=common.state().roles.find(role);if(!r)return {};return RoleAssignmentView{r->actor_id,r->enabled};};
        access.regional_candidates=[this](const StableId&,const StableId&){return Outcome<std::vector<RegionalOpportunityBinding>>{true,{candidate},{}};};
        access.regional_share=[this](const RegionalOpportunityBinding&){++shares;return common_success();};
    }
};
// 完整新域逐字节继续；旧域入口仍不附加regional字段。
CodecVisitor::Buffer bytes(InteractionState state,bool regional){CodecVisitor::Buffer value;CodecVisitor writer(value);assert(visit_interaction_checkpoint(writer,state,regional).ok&&writer.finish().ok);return value;}
void actor_binding(){
    Fixture f;const auto before=f.common.state().next_instance_id;const auto old=bytes(f.state,true);
    auto quote=preview_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"});assert(quote.ok&&f.shares==0&&bytes(f.state,true)==old&&f.common.state().next_instance_id==before);
    auto begun=apply_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"},quote.value);assert(begun.ok&&f.shares==1&&f.state.interactions.size()==1&&f.state.regional->exposures.size()==1);assert(validate_interaction_state(f.common,f.state,f.catalog,f.access).ok);
    auto unavailable=preview_interaction(f.common,f.state,f.catalog,f.access,"player",AskNews{});assert(!unavailable.ok&&unavailable.error.original.code=="SocialUnavailable");
    f.catalog.npc_society=true;int heard=0;StableId selected_news="report:frozen";
    f.access.news_preview=[&](const StableId& listener,const StableId& speaker){assert(listener=="player"&&speaker=="npc");auto selected=std::make_shared<SocialNewsQuote>();selected->kind=SocialNewsQuote::Kind::life_report;selected->source_id=selected_news;return Outcome<std::shared_ptr<const SocialNewsQuote>>{true,selected,{}};};
    f.access.news_apply=[&](const StableId&,const StableId&,const SocialNewsQuote& source){++heard;assert(source.source_id=="report:frozen");return Outcome<std::vector<StableId>>{true,{source.source_id},{}};};
    auto news=preview_interaction(f.common,f.state,f.catalog,f.access,"player",AskNews{});assert(news.ok&&heard==0);selected_news="report:replacement";auto effect=apply_interaction(f.common,f.state,f.catalog,f.access,"player",AskNews{},news.value);assert(effect.ok&&heard==1&&effect.value.news_ids&&effect.value.news_ids->at(0)=="report:frozen");assert(encode_interaction_extra_effect(effect.value).find("news_ids"));
    const auto context=f.state.interactions[0].context;assert(context.kind==std::optional<StableId>("narrative")&&context.role_id.empty());
    auto unauthorized=validate_interaction(f.common,f.state,f.catalog,f.access,"player",context,"work");assert(!unauthorized.ok&&unauthorized.error.original.code=="NarrativeServiceDenied");
    auto recorded=bytes(f.state,true);InteractionState restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(recorded));assert(visit_interaction_checkpoint(reader,restored,true).ok&&reader.finish().ok&&bytes(restored,true)==recorded);
    auto exported=export_interaction_state(f.state);assert(exported.ok&&exported.value.find("regional_life"));auto decoded=decode_interaction_seed(exported.value);assert(decoded.ok);auto reexported=export_interaction_state(decoded.value);assert(reexported.ok&&compare_boundary(exported.value,reexported.value).exact());
    auto next=preview_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueNext{});assert(next.ok);assert(apply_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueNext{},next.value).ok);
    f.candidate.source.fact_id="service:2";f.candidate.source.report_id="report:service:2";auto stale=preview_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueChoose{"later"});assert(!stale.ok&&stale.error.original.code=="RegionalSourceChanged");
    auto end=preview_interaction(f.common,f.state,f.catalog,f.access,"player",EndInteraction{});assert(end.ok&&apply_interaction(f.common,f.state,f.catalog,f.access,"player",EndInteraction{},end.value).ok&&f.state.interactions.empty());
    f.candidate.source.fact_id="service:1";f.candidate.source.report_id="report:service:1";auto cooling=preview_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"});assert(!cooling.ok&&cooling.error.original.code=="RegionalTopicCoolingDown"&&f.shares==1);
    f.common.state().identity.tick=2;assert(preview_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"}).ok);
    f.common.state().actors.find("npc")->action_id="busy";auto busy=preview_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"});assert(!busy.ok&&busy.error.original.code=="ActorBusy");f.common.state().actors.find("npc")->action_id.clear();
    f.common.state().actors.find("npc")->location=SettlementLocation{"town","away"};auto absent=preview_interaction(f.common,f.state,f.catalog,f.access,"player",TalkActor{"npc","work"});assert(!absent.ok&&absent.error.original.code=="PersonAbsent"&&f.shares==1);
}
void need_binding(){
    Fixture f;auto& b=f.candidate;b.binding_id="grain";b.topic_id="grain";b.dialogue_id="need";b.role_id="clerk";b.source={};b.source.kind=RegionalSourceKind::runtime_offer;b.source.offer_ref=OfferRef{OfferRef::Kind::runtime,"offer:1"};b.source.need_id="need:1";b.source.need_revision=3;
    TaskCommand task;task.kind="accept_offer";task.offer_id="offer:1";task.offer_ref=b.source.offer_ref;task.need_revision=3;task.quantity=2;task.context={"square","npc","clerk",{},{},{},{}};b.task_command=std::make_shared<const TaskCommand>(task);
    auto quote=preview_interaction(f.common,f.state,f.catalog,f.access,"player",Talk{"clerk","grain"});assert(quote.ok&&apply_interaction(f.common,f.state,f.catalog,f.access,"player",Talk{"clerk","grain"},quote.value).ok&&f.shares==0);
    auto next=preview_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueNext{});assert(next.ok&&apply_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueNext{},next.value).ok);
    auto choice=preview_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueChoose{"accept"});assert(choice.ok&&apply_interaction(f.common,f.state,f.catalog,f.access,"player",DialogueChoose{"accept"},choice.value).ok);
    auto frozen=pending_regional_task(f.common,f.state,f.catalog,f.access,"player");assert(frozen.ok&&frozen.value&&frozen.value->offer_ref->id=="offer:1"&&*frozen.value->quantity==2);
    assert(validate_interaction_state(f.common,f.state,f.catalog,f.access).ok);const auto recorded=bytes(f.state,true);auto exported=export_interaction_state(f.state);assert(exported.ok);auto decoded=decode_interaction_seed(exported.value);assert(decoded.ok);auto reexported=export_interaction_state(decoded.value);assert(reexported.ok&&compare_boundary(exported.value,reexported.value).exact());
    b.source.need_revision=4;auto stale=pending_regional_task(f.common,f.state,f.catalog,f.access,"player");assert(!stale.ok&&stale.error.original.code=="RegionalSourceChanged");b.source.need_revision=3;
    f.common.state().roles.find("clerk")->actor_id="replacement";auto replaced=pending_regional_task(f.common,f.state,f.catalog,f.access,"player");assert(!replaced.ok&&replaced.error.original.code=="RoleNotAuthorized");
    auto malformed=exported.value;auto* bindings=malformed.find("regional_life")->find("bindings")->as<BoundaryValue::Array>();bindings->front().find("binding")->find("task_command")->as<BoundaryValue::Dict>()->emplace_back("forged",BoundaryValue{true});assert(!decode_interaction_seed(malformed).ok);
}
}
int main(){actor_binding();need_binding();InteractionState legacy;const auto old=bytes(legacy,false);auto exported=export_interaction_state(legacy);assert(exported.ok&&!exported.value.find("regional_life"));auto decoded=decode_interaction_seed(exported.value);assert(decoded.ok&&bytes(decoded.value,false)==old);std::cout<<"regional_life_interaction: PASS (narrative authority; preview nonconsumption; successful exposure/cooldown; busy/absence; frozen need/provider; exact new cold roundtrip; strict task fields; legacy bytes)\n";}
