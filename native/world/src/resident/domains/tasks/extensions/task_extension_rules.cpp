// 申请只消费原委托人的实际say许可；新的请求不能重复延期，失败由外层完整候选撤回。
#include "task_extension.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
const TaskExtensionPolicy* policy_for(const TaskExtensionCatalog& catalog,const StableId& id){for(const auto& row:catalog.policies)if(row.id==id)return &row;return nullptr;}
}
Outcome<TaskExtensionQuote> preview_task_extension(const CommonStore& common,const TaskState& tasks,const TaskCatalog& task_catalog,const InteractionState& interaction,const InteractionCatalog& definitions,const InteractionAccess& access,const TaskExtensionCatalog& catalog,const TaskExtensionState& state,const TaskExtensionHistorySources& history,const StableId& actor,const TaskExtensionCommand& request){
    auto valid=validate_task_extensions(common,tasks,task_catalog,definitions,catalog,state,history);if(!valid.ok)return {false,{},valid.error};const auto* task=tasks.tasks.find(request.task_id);if(!task||task->assignee!=actor)return fail<TaskExtensionQuote>("TaskNotKnown");const auto* policy=policy_for(catalog,request.policy_id);const auto* offer=policy?task_catalog.offers.find(policy->offer_id):nullptr;
    if(!policy||!offer||task->offer_id!=offer->id||task->contract_id||task->offer_ref||offer->issuer.kind!=Owner::Kind::actor||actor==offer->issuer.id)return fail<TaskExtensionQuote>("TaskExtensionPrivatePromiseRequired");
    if(state.by_task.count(task->id))return fail<TaskExtensionQuote>("TaskExtensionAlreadyGranted");
    if((task->status!="active"&&task->status!="ready")||!task->settlement_id.empty()||task->reported_tick)return fail<TaskExtensionQuote>("TaskExtensionAlreadySettled");
    if(common.tick()>task->deadline_tick)return fail<TaskExtensionQuote>("TaskExtensionOriginalDeadlinePassed");
    for(const auto& row:tasks.actions.rows())if(row.second.task_id==task->id)return fail<TaskExtensionQuote>("TaskExtensionTaskBusy");
    if(task->escort&&!task->escort->failure.empty())return fail<TaskExtensionQuote>("TaskExtensionObjectiveFailed");
    auto consent=preview_authored_provider_source(common,interaction,definitions,access,actor,offer->issuer.id);if(!consent.ok)return {false,{},consent.error};const auto& source=consent.value.meeting;
    if(source.dialogue_id!=policy->dialogue_id||source.node_id!=policy->node_id||source.context.role_id!=offer->report_role||source.context.kind||source.context.encounter_id||source.context.input_error)return fail<TaskExtensionQuote>("TaskExtensionProviderDeclarationRequired");
    Integer deadline=0;if(__builtin_add_overflow(task->deadline_tick,policy->additional_ticks,&deadline)||deadline>2147483647)return fail<TaskExtensionQuote>("TaskExtensionDeadlineOverflow");
    return {true,{std::move(consent.value),policy->id,task->id,offer->id,task->accepted_tick,task->deadline_tick,deadline},{}};
}
Outcome<StableId> commit_task_extension_in_candidate(CommonStore& common,TaskState& tasks,const TaskCatalog& task_catalog,const InteractionState& interaction,const InteractionCatalog& definitions,const InteractionAccess& access,const TaskExtensionCatalog& catalog,TaskExtensionState& state,const TaskExtensionHistorySources& history,const TaskExtensionQuote& quote){
    auto valid=revalidate_authored_provider_source(common,interaction,definitions,access,quote.consent);if(!valid.ok)return {false,{},valid.error};auto current=preview_task_extension(common,tasks,task_catalog,interaction,definitions,access,catalog,state,history,quote.consent.meeting.actor_id,{quote.task_id,quote.policy_id});if(!current.ok)return {false,{},current.error};
    if(current.value.offer_id!=quote.offer_id||current.value.accepted_tick!=quote.accepted_tick||current.value.original_deadline_tick!=quote.original_deadline_tick||current.value.deadline_tick!=quote.deadline_tick)return fail<StableId>("StaleTaskExtensionQuote");
    const auto& consent=current.value.consent;const auto& source=consent.meeting;auto id=common.next_id(task_extension_fact_prefix);if(!id.ok)return id;TaskExtensionFact fact{id.value,quote.policy_id,quote.task_id,quote.offer_id,source.actor_id,consent.speaker_actor_id,source.session_id,source.context.place_id,source.context.role_id,source.dialogue_id,source.node_id,quote.accepted_tick,common.tick(),quote.original_deadline_tick,quote.deadline_tick};
    valid=common.append_fact({id.value,task_extension_fact_kind,task_extension_module});if(!valid.ok)return {false,{},valid.error};if(!state.facts.insert(id.value,std::move(fact)))return fail<StableId>("DuplicateTaskExtensionFact");tasks.tasks.find(quote.task_id)->deadline_tick=quote.deadline_tick;rebuild_task_extension_index(state);
    valid=validate_task_extensions(common,tasks,task_catalog,definitions,catalog,state,history);if(!valid.ok)return {false,{},valid.error};return id;
}
}
