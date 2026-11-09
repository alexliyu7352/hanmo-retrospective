// 地方对白只绑定已存在来源和原任务；只读选择不会创建资源、任务或曝光。
#include "regional_life_dialogue.hpp"
#include "interaction_codec.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 来源字段逐项相等，不能以同名话题替代旧Offer或新任办理人。
bool same_outcome(const std::optional<RegionalTaskOutcomeRef>& a,const std::optional<RegionalTaskOutcomeRef>& b){
    if(bool(a)!=bool(b))return false;if(!a)return true;
    return std::tie(a->task_id,a->contract_id,a->task_template_id,a->delivery_fact_id,a->transfer_fact_id,a->settlement_id,a->receipt_id,a->action_id,a->state_digest)==std::tie(b->task_id,b->contract_id,b->task_template_id,b->delivery_fact_id,b->transfer_fact_id,b->settlement_id,b->receipt_id,b->action_id,b->state_digest);
}
bool same_source(const RegionalSourceRef& a,const RegionalSourceRef& b){
    return same_outcome(a.task_outcome,b.task_outcome)&&a.kind==b.kind&&a.fact_id==b.fact_id&&a.report_id==b.report_id&&a.need_id==b.need_id&&a.need_revision==b.need_revision&&bool(a.offer_ref)==bool(b.offer_ref)&&(!a.offer_ref||(a.offer_ref->kind==b.offer_ref->kind&&a.offer_ref->id==b.offer_ref->id));
}
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id;}
// typed任务不允许在同一会话内更换数量、经办上下文或来源修订。
bool same_task(const std::shared_ptr<const TaskCommand>& a,const std::shared_ptr<const TaskCommand>& b){
    if(bool(a)!=bool(b))return false;if(!a)return true;
    return a->kind==b->kind&&a->offer_id==b->offer_id&&a->need_revision==b->need_revision&&a->quantity==b->quantity&&a->source_kind==b->source_kind&&same_context(a->context,b->context)&&bool(a->offer_ref)==bool(b->offer_ref)&&(!a->offer_ref||(a->offer_ref->kind==b->offer_ref->kind&&a->offer_ref->id==b->offer_ref->id));
}
// 只从明确作者绑定读冷却；不存在的主题不会获得零冷却通行证。
std::optional<Integer> cooldown(const InteractionCatalog& cat,const StableId& id){
    for(const auto& t:cat.regional_actor_topics)if(t.id==id)return t.cooldown_ticks;
    for(const auto& t:cat.regional_need_topics)if(t.id==id)return t.cooldown_ticks;
    for(const auto& t:cat.regional_outcome_topics)if(t.id==id)return t.cooldown_ticks;return {};
}
const DialogueDefinition* dialogue(const InteractionCatalog& cat,const StableId& id){for(const auto& d:cat.dialogues)if(d.id==id)return &d;return nullptr;}
// 非空且有界的作者ID不允许兼任拼接曝光键的结构分隔符。
bool bounded_id(const StableId& id){return !id.empty()&&id.size()<=4096;}
Status source_shape(const RegionalSourceRef& s){
    if(s.kind==RegionalSourceKind::life_result){if(s.task_outcome||!bounded_id(s.fact_id)||!bounded_id(s.report_id)||s.offer_ref||!s.need_id.empty()||s.need_revision!=0)return common_failure("InvalidRegionalSource");}
    else if(s.kind==RegionalSourceKind::runtime_offer){if(s.task_outcome||!s.fact_id.empty()||!s.report_id.empty()||!s.offer_ref||s.offer_ref->kind!=OfferRef::Kind::runtime||!bounded_id(s.offer_ref->id)||!bounded_id(s.need_id)||s.need_revision<1)return common_failure("InvalidRegionalSource");}
    else if(s.kind==RegionalSourceKind::task_outcome){
        if(!s.task_outcome||!s.fact_id.empty()||!s.report_id.empty()||!s.need_id.empty()||s.need_revision!=0||!s.offer_ref||!bounded_id(s.offer_ref->id))return common_failure("InvalidRegionalSource");
        const auto& r=*s.task_outcome;if(!bounded_id(r.task_id)||!bounded_id(r.contract_id)||!bounded_id(r.task_template_id)||r.state_digest.size()!=64||r.delivery_fact_id.empty()!=r.transfer_fact_id.empty()||r.receipt_id.empty()!=r.action_id.empty())return common_failure("InvalidRegionalSource");
        for(const auto c:r.state_digest)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return common_failure("InvalidRegionalSource");
        for(const auto* id:{&r.delivery_fact_id,&r.transfer_fact_id,&r.settlement_id,&r.receipt_id,&r.action_id})if(id->size()>4096)return common_failure("InvalidRegionalSource");
    }
    else return common_failure("InvalidRegionalSource");return common_success();
}
}
// ID长度前缀避免作者词条中的标点令不同来源碰撞；不分配世界编号。
StableId regional_source_episode(const RegionalSourceRef& s){
    auto field=[](const StableId& value){return std::to_string(value.size())+":"+value;};
    if(s.kind==RegionalSourceKind::task_outcome)return s.task_outcome?"outcome:"+field(s.task_outcome->contract_id)+field(s.task_outcome->state_digest):"invalid_outcome";
    return s.kind==RegionalSourceKind::life_result?"life:"+field(s.fact_id)+field(s.report_id):"offer:"+field(s.offer_ref?s.offer_ref->id:StableId{})+field(s.need_id)+":"+std::to_string(s.need_revision);
}
bool same_regional_binding(const RegionalOpportunityBinding& a,const RegionalOpportunityBinding& b){
    return a.binding_id==b.binding_id&&a.topic_id==b.topic_id&&a.dialogue_id==b.dialogue_id&&a.observer_actor_id==b.observer_actor_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.place_id==b.place_id&&same_source(a.source,b.source)&&same_task(a.task_command,b.task_command);
}
// 目录配置与普通dialogue共用节点，只放行封闭叙事/原采购命令。
Status configure_regional_life(InteractionCatalog& cat,const BoundaryValue& config){
    try{
        using namespace boundary;fields(config,{"actor_topics","need_topics"},{"outcome_topics"},"regional_life");
        if(cat.regional_enabled)return common_failure("DuplicateRegionalLifeCatalog");
        std::vector<RegionalActorTopic> actors;std::vector<RegionalNeedTopic> needs;std::set<StableId> ids,dialogue_ids;
        for(const auto& row:array(field(config,"actor_topics","regional_life"),"actor_topics")){
            fields(row,{"id","actor_id","dialogue_id","source_kind","activity","cooldown_ticks"},{},"actor_topics");
            RegionalActorTopic t{get_string(row,"id","topic"),get_string(row,"actor_id","topic"),get_string(row,"dialogue_id","topic"),get_string(row,"activity","topic"),get_int(row,"cooldown_ticks","topic")};
            const auto* d=dialogue(cat,t.dialogue_id);
            if(!bounded_id(t.id)||!ids.insert(t.id).second||!dialogue_ids.insert(t.dialogue_id).second||!cat.common||!cat.common->actor_names.find(t.actor_id)||!d||!d->context_role.empty()||get_string(row,"source_kind","topic")!="life_result"||(t.activity!="work"&&t.activity!="trade")||t.cooldown_ticks<0||t.cooldown_ticks>720)return common_failure("InvalidRegionalActorTopic");
            for(const auto& n:d->nodes){if(n.kind!="say"&&n.kind!="choice"&&n.kind!="end")return common_failure("NarrativeServiceDenied");if(n.command)return common_failure("NarrativeServiceDenied");if(n.kind=="say"&&n.speaker!="$provider"&&n.speaker!="$player")return common_failure("InvalidRegionalSpeaker");}
            actors.push_back(std::move(t));
        }
        for(const auto& row:array(field(config,"need_topics","regional_life"),"need_topics")){
            fields(row,{"id","role_id","dialogue_id","item_id","cooldown_ticks"},{},"need_topics");
            RegionalNeedTopic t{get_string(row,"id","topic"),get_string(row,"role_id","topic"),get_string(row,"dialogue_id","topic"),get_string(row,"item_id","topic"),get_int(row,"cooldown_ticks","topic")};
            const auto* d=dialogue(cat,t.dialogue_id);
            if(!bounded_id(t.id)||!ids.insert(t.id).second||!dialogue_ids.insert(t.dialogue_id).second||!cat.common||!cat.common->roles.find(t.role_id)||!cat.common->items.find(t.item_id)||!d||d->context_role!=t.role_id||t.cooldown_ticks<0||t.cooldown_ticks>720)return common_failure("InvalidRegionalNeedTopic");
            for(const auto& n:d->nodes){if(n.kind!="say"&&n.kind!="choice"&&n.kind!="end"&&n.kind!="action")return common_failure("InvalidRegionalDialogueNode");if(n.command&&(n.command->kind!="accept_offer"||n.command->field_order!=std::vector<StableId>{"kind"}))return common_failure("InvalidRegionalTaskCommand");if(n.kind=="say"&&n.speaker!="$provider"&&n.speaker!="$player")return common_failure("InvalidRegionalSpeaker");}
            needs.push_back(std::move(t));
        }
        if(actors.size()+needs.size()>64)return common_failure("RegionalTopicLimit");
        cat.regional_actor_topics=std::move(actors);cat.regional_need_topics=std::move(needs);cat.regional_enabled=true;return common_success();
    }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status initialize_regional_life(InteractionState& state,bool outcomes){if(state.regional)return common_failure("DuplicateRegionalLifeState");state.regional=RegionalInteractionState{};state.regional->version=outcomes?2:1;return common_success();}
// 会话来源复验只接受原候选；新任办理人或新need revision不是原候选的修复。
Status validate_regional_binding(const CommonRead& common,const InteractionCatalog& cat,const InteractionAccess& access,const RegionalOpportunityBinding& binding){
    if(!cat.regional_enabled||!access.regional_candidates)return common_failure("RegionalLifeUnavailable");
    auto shape=source_shape(binding.source);if(!shape.ok)return shape;
    auto values=access.regional_candidates(binding.observer_actor_id,binding.place_id);if(!values.ok)return {false,{},values.error};
    for(const auto& value:values.value)if(same_regional_binding(value,binding))return common_success();
    (void)common;return common_failure("RegionalSourceChanged");
}
// 正常岗位主题仍使用真实role上下文，普通人只有narrative上下文且禁止业务权限。
Outcome<InteractionQuote> preview_regional_talk(const CommonRead& common,const InteractionState& state,const InteractionCatalog& cat,const InteractionAccess& access,const StableId& observer,const StableId& topic,const StableId& provider,const StableId& role){
    if(!cat.regional_enabled||!state.regional||!access.regional_candidates)return {false,{},common_failure("RegionalLifeUnavailable").error};
    if(interaction_occupied(state,observer))return {false,{},common_failure("AlreadyTalking").error};
    const auto* actor=common.actor(observer);if(!actor)return {false,{},common_failure("UnknownActor").error};
    auto location=access.resolve_location?access.resolve_location(observer):actor->location;const auto* place=std::get_if<SettlementLocation>(&location);if(!place)return {false,{},common_failure("NotAtPlace").error};
    auto values=access.regional_candidates(observer,place->place_id);if(!values.ok)return {false,{},values.error};
    auto interval=cooldown(cat,topic);if(!interval)return {false,{},common_failure("UnknownRegionalTopic").error};
    for(const auto& b:values.value){
        if(b.topic_id!=topic||(!provider.empty()&&b.provider_actor_id!=provider)||b.role_id!=role)continue;
        InteractionContext context{b.place_id,b.provider_actor_id,b.role_id,{},{},{},{}};if(role.empty())context.kind="narrative";
        auto valid=validate_interaction(common,state,cat,access,observer,context);if(!valid.ok)return {false,{},valid.error};
        const auto episode=regional_source_episode(b.source);for(const auto& exposure:state.regional->exposures)if(exposure.observer_actor_id==observer&&exposure.binding_id==b.binding_id&&exposure.episode==episode&&common.tick()-exposure.last_tick<*interval)return {false,{},common_failure("RegionalTopicCoolingDown").error};
        InteractionQuote quote;quote.context=context;quote.dialogue_id=b.dialogue_id;quote.regional_binding=b;return {true,std::move(quote),{}};
    }
    return {false,{},common_failure("RegionalSourceUnavailable").error};
}
// 分享和曝光均在成功打开同一候选内进行；失败由外层原子事务回滚。
Status expose_regional_binding(const CommonRead& common,InteractionState& state,const InteractionCatalog& cat,const InteractionAccess& access,const RegionalOpportunityBinding& binding){
    if(!state.regional)return common_failure("RegionalLifeUnavailable");
    auto valid=validate_regional_binding(common,cat,access,binding);if(!valid.ok)return valid;
    if(binding.source.kind==RegionalSourceKind::life_result){if(!access.regional_share)return common_failure("RegionalSharingUnavailable");auto shared=access.regional_share(binding);if(!shared.ok)return shared;}
    const auto episode=regional_source_episode(binding.source);
    for(auto& value:state.regional->exposures)if(value.observer_actor_id==binding.observer_actor_id&&value.binding_id==binding.binding_id&&value.episode==episode){value.last_tick=common.tick();return common_success();}
    state.regional->exposures.push_back({binding.observer_actor_id,binding.binding_id,episode,common.tick(),binding.source});return common_success();
}
// 确认只提供当前action节点绑定的原任务；没有绑定时明确返回空供原对白路径处理。
Outcome<std::shared_ptr<const TaskCommand>> pending_regional_task(const CommonRead& common,const InteractionState& state,const InteractionCatalog& cat,const InteractionAccess& access,const StableId& actor){
    for(const auto& session:state.interactions)if(session.actor_id==actor){
        if(!session.regional_binding)return {true,{},{}};
        if(session.phase!="action"||session.regional_action_committed||!session.pending_command||session.pending_command->kind!="accept_offer"||!session.regional_binding->task_command)return {false,{},common_failure("NoPendingRegionalAction").error};
        auto context=validate_interaction(common,state,cat,access,actor,session.context);if(!context.ok)return {false,{},context.error};
        auto valid=validate_regional_binding(common,cat,access,*session.regional_binding);if(!valid.ok)return {false,{},valid.error};
        return {true,session.regional_binding->task_command,{}};
    }return {true,{},{}};
}
// 冷校验允许来源已经过时的会话保留供用户退出，但不允许伪造作者、人物或任务绑定。
Status validate_regional_interaction(const CommonRead& common,const InteractionState& state,const InteractionCatalog& cat,const InteractionAccess* access){
    if(bool(state.regional)!=cat.regional_enabled)return common_failure("RegionalLifeStateMismatch");
    std::set<std::tuple<StableId,StableId,StableId>> exposed;
    if(state.regional){if(state.regional->version!=(cat.regional_outcome_topics.empty()?1:2))return common_failure("UnsupportedRegionalLifeState");for(const auto& e:state.regional->exposures){if(!common.actor(e.observer_actor_id)||!cooldown(cat,e.binding_id)||e.last_tick<0||e.last_tick>common.tick()||e.episode!=regional_source_episode(e.source)||!exposed.emplace(e.observer_actor_id,e.binding_id,e.episode).second)return common_failure("InvalidRegionalExposure");auto valid=source_shape(e.source);if(!valid.ok)return valid;}}
    for(const auto& s:state.interactions){
        if(!s.regional_binding){bool born=false;if(access&&access->born_personal_session){auto bound=access->born_personal_session(s);if(!bound.ok)return {false,{},bound.error};born=bound.value;}if(!born&&access&&access->known_personal_session){auto bound=access->known_personal_session(s);if(!bound.ok)return {false,{},bound.error};born=bound.value;}if(s.regional_action_committed||((s.context.kind&&*s.context.kind=="narrative")&&!born))return common_failure("MissingRegionalBinding");continue;}
        const auto& b=*s.regional_binding;if(!state.regional||b.binding_id!=b.topic_id||b.observer_actor_id!=s.actor_id||b.provider_actor_id!=s.context.provider_actor_id||b.place_id!=s.context.place_id||b.role_id!=s.context.role_id||b.dialogue_id!=s.dialogue_id||!cooldown(cat,b.topic_id))return common_failure("InvalidRegionalBinding");
        auto valid=source_shape(b.source);if(!valid.ok)return valid;
        bool authored=false;
        if(b.source.kind==RegionalSourceKind::life_result){for(const auto& t:cat.regional_actor_topics)if(t.id==b.topic_id&&t.actor_id==b.provider_actor_id&&t.dialogue_id==b.dialogue_id)authored=true;if(b.task_command||!b.role_id.empty()||s.context.kind!=std::optional<StableId>("narrative")||s.regional_action_committed)return common_failure("NarrativeServiceDenied");}
        else if(b.source.kind==RegionalSourceKind::task_outcome){for(const auto& t:cat.regional_outcome_topics)if(t.id==b.topic_id&&t.role_id==b.role_id&&t.dialogue_id==b.dialogue_id&&b.source.task_outcome&&t.task_template_id==b.source.task_outcome->task_template_id)authored=true;if(b.task_command||b.role_id.empty()||s.regional_action_committed)return common_failure("NarrativeServiceDenied");}
        else{for(const auto& t:cat.regional_need_topics)if(t.id==b.topic_id&&t.role_id==b.role_id&&t.dialogue_id==b.dialogue_id)authored=true;const auto& task=b.task_command;if(!task||task->kind!="accept_offer"||!task->offer_ref||task->offer_ref->kind!=OfferRef::Kind::runtime||task->offer_ref->id!=b.source.offer_ref->id||task->need_revision!=std::optional<Integer>(b.source.need_revision)||!task->quantity||*task->quantity<1||!same_context(task->context,s.context))return common_failure("InvalidRegionalTaskCommand");}
        if(!authored)return common_failure("UnknownRegionalTopic");
        if(!exposed.count({s.actor_id,b.binding_id,regional_source_episode(b.source)}))return common_failure("MissingRegionalExposure");
    }return common_success();
}
// 清理仅删已经无来源的曝光，不触碰会话、资源或实际结果。
void prune_regional_exposures(InteractionState& state,const std::vector<StableId>& retained){
    if(!state.regional)return;std::set<StableId> facts(retained.begin(),retained.end());
    state.regional->exposures.erase(std::remove_if(state.regional->exposures.begin(),state.regional->exposures.end(),[&](const auto& e){return e.source.kind==RegionalSourceKind::life_result&&!facts.count(e.source.fact_id);}),state.regional->exposures.end());
}
}
