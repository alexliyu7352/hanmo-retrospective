// 原Talk/pump、Task受领/实交/报告与实际Common资源配对；不替World/跨城Travel验收。
#define HANMO_MESSAGE_HANDOFF_TEST_LIBRARY
#include "task_message_handoff_test.cpp"
#include "resident/domains/tasks/message_returns/task_message_return_services.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_receipts.hpp"
#include "resident/kernel/command_types.hpp"
namespace {
struct ReturnFixture : Fixture {
    TaskMessageReturnCatalog returns;
    Integer request=0;
    ReturnFixture(){
        common.state().identity.execution_epoch=1;
        shared->actor_ids={"player","sender","recipient","stranger"};
        shared->items.insert("reply",{"reply",1});
        tasks.message_handoffs->version=2;tasks.static_assets.emplace();tasks.message_returns.emplace();
        for(const auto* person:{"sender","recipient"}){
            auto* role=shared->roles.find(person);role->dialogue_id=StableId{person}+"_dialogue";
            DialogueNode say;say.id="say";say.kind="say";say.speaker="$provider";say.text_key="explicit_return_terms";
            DialogueDefinition dialogue;dialogue.id=*role->dialogue_id;dialogue.context_role=role->id;dialogue.entry="say";dialogue.nodes.push_back(say);ic.dialogues.push_back(dialogue);
        }
        returns.policies.push_back({"return_policy","letter","reply","recipient","recipient_dialogue","say"});
        cold_returns();
    }
    TaskMessageReturnState& state(){return *tasks.message_returns;}
    TaskMessageReturnSources source(const DynamicStepInput* phase=nullptr){
        TaskMessageReturnSources x{common,*shared,tasks,catalog,*tasks.message_handoffs,*tasks.static_assets,ic,returns,state(),phase,{},TaskMessageReturnSubmitterMode::legacy_actor,{}};
        x.presence_before=[this,phase](const StableId& actor,Integer tick,const StableId& fact){return read_actor_presence_before_fact(common,*shared,{},actor,tick,fact,phase);};
        return x;
    }
    TaskMessageReturnServices services(const DynamicStepInput* phase=nullptr){return {source(phase),interaction,access};}
    void cold_returns(){auto checked=validate_task_message_returns(source());require(checked.ok,"whole return source: "+checked.error.original.code);}
    void interaction_command(const InteractionCommand& command){
        auto q=preview_interaction(common,interaction,ic,access,"player",command);require(q.ok,"actual interaction preview: "+q.error.original.code);
        auto done=apply_interaction(common,interaction,ic,access,"player",command,q.value);require(done.ok,"actual interaction commit: "+done.error.original.code);
    }
    void meet(const StableId& person){
        if(!interaction.interactions.empty())interaction_command(EndInteraction{});
        interaction_command(Talk{person});
        require(interaction.interactions.size()==1&&interaction.interactions.front().phase=="say"&&interaction.interactions.front().context.provider_actor_id==person,"original Talk/pump reaches actual provider say");
    }
    void result(const TaskMessageReturnState& before,const StableId& kind,ReceiptData payload,Integer from){
        ReceiptDetails details{std::move(payload),{}};
        auto delta=task_message_return_new_result_sources(before,state(),"player",kind,details,common.tick(),from);
        require(delta.ok,"new source delta: "+delta.error.original.code);
        auto id=common.next_id("result");require(id.ok,"actual result id");
        auto committed=std::make_shared<CommitResult>();committed->result_id=id.value;committed->kind=kind;committed->tick=common.tick();committed->revision=++common.state().identity.revision;committed->details=std::make_shared<ReceiptDetails>(std::move(details));
        ++request;if(auto* seq=common.state().request_sequences.find("player"))*seq=request;else common.state().request_sequences.insert("player",request);
        common.state().history.push_back({common.tick(),kind,"player",id.value,{}});common.state().results.insert("player:"+std::to_string(request),committed);
        auto x=source();auto bound=bind_task_message_return_results(common,state(),x,delta.value,"player",request,*committed,from);require(bound.ok,"actual result binding: "+bound.error.original.code);cold_returns();
    }
    ActionCoreView start_original(const StableId& kind){
        const StableId provider=kind=="deliver_message"?"recipient":"sender";meet(provider);
        TaskCommand command{kind,"letter",{"market",provider,provider}};auto service=services();
        auto quote=preview_task(common,tasks,catalog,*shared,interaction,ic,access,"player",command,nullptr,nullptr,nullptr,nullptr,&service);require(quote.ok,"original preview: "+quote.error.original.code);
        const auto before=state();auto id=common.next_id("action");require(id.ok,"original action id");
        ActionCoreView action{id.value,kind,"player",{"player",provider},common.tick(),quote.value.duration};ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;require(common.apply_action(insert).ok,"original action insert");for(const auto& actor:action.participants)common.state().actors.find(actor)->action_id=action.id;
        auto begun=start_task_action(common,tasks,catalog,action,command,quote.value,nullptr,nullptr,&service);require(begun.ok,"original task begin: "+begun.error.original.code);task_id=tasks.actions.find(action.id)->task_id;result(before,kind,ActionStarted{action.id},common.tick());return action;
    }
    ActionCoreView collect(){
        meet("recipient");auto x=source();auto quote=preview_task_message_return_collection(x,interaction,access,"player",{task_id});require(quote.ok,"actual return quote: "+quote.error.original.code);const auto before=state();
        auto action=begin_task_message_return_collection_in_candidate(common,state(),x,interaction,access,quote.value);require(action.ok,"actual return begin: "+action.error.original.code);const auto core=*common.action(action.value);result(before,task_message_return_collect_command,ActionStarted{action.value},common.tick());return core;
    }
    Outcome<TaskEffect> finish_business(const ActionCoreView& action,const DynamicStepInput& phase){
        ActionMutation patch;patch.action.id=action.id;patch.remaining_ticks=0;auto reduced=common.apply_action(patch);if(!reduced.ok)return {false,{},reduced.error};const auto actual=*common.action(action.id);
        if(action.kind==task_message_return_collect_command){auto x=source(&phase);return finish_task_message_return_collection_in_candidate(common,state(),x,interaction,access,action.id,phase);}
        auto service=services(&phase);return finish_task_action(common,tasks,social,catalog,interaction,ic,access,actual,phase.to_tick,nullptr,nullptr,nullptr,&service);
    }
    TaskEffect finish(const ActionCoreView& action){
        const auto before=state();const DynamicStepInput phase{common.tick(),common.tick()+1};auto done=finish_business(action,phase);require(done.ok,"actual business finish: "+done.error.original.code);
        tasks.actions.erase(action.id);ActionMutation erase;erase.kind=ActionMutation::Kind::erase;erase.action.id=action.id;require(common.apply_action(erase).ok,"original unified action release");for(const auto& actor:action.participants)common.state().actors.find(actor)->action_id.clear();common.state().identity.tick=phase.to_tick;
        ActionCompletion completed;completed.task=done.value;AdvanceDetails details;details.completed_actions.insert(action.id,completed);result(before,"advance",details,phase.from_tick);return done.value;
    }
    void outbound(){require(finish(start_original("accept_offer")).success,"real acceptance");require(finish(start_original("deliver_message")).success,"real outbound letter");}
    Outcome<TaskQuote> report_preview(){meet("sender");auto service=services();return preview_task(common,tasks,catalog,*shared,interaction,ic,access,"player",TaskCommand{"report_offer","letter",{"market","sender","sender"}},nullptr,nullptr,nullptr,nullptr,&service);}
    void cancel(){auto cancelled=cancel_task(common,tasks,catalog,*shared,task_id);require(cancelled.ok,"original cancel: "+cancelled.error.original.code);cold_returns();}
};
}
int main(){try{
    ReturnFixture f;f.outbound();require(!f.report_preview().ok,"mere outbound delivery cannot complete round trip");
    const auto collecting=f.collect();require(f.common.count("bag:player","reply",{{Owner::Kind::task,f.task_id,{}}}).value==0,"recipient say and beginning do not create reply lot");require(f.finish(collecting).success,"real recipient halfday produces reply");
    const auto collected=f.state().collections.rows().front().second;require(f.common.count("bag:player","reply",{{Owner::Kind::task,f.task_id,{}}}).value==1,"one Task-owned reply after actual halfday");require(f.common.wallet("cash:player")->balance==100,"collection pays no second reward");
    auto reporting=f.start_original("report_offer");require(f.finish(reporting).success,"original report completes return delivery");require(f.tasks.tasks.find(f.task_id)->status=="success"&&f.common.wallet("cash:player")->balance==108&&f.common.wallet("cash:sender")->balance==92,"original eight-coin payment once");require(f.common.count("bag:player","reply",{{Owner::Kind::task,f.task_id,{}}}).value==0&&f.common.count("bag:sender","reply",{{Owner::Kind::actor,"sender",{}}}).value==1,"exact actual reply ownership transferred to original sender");
    require(f.state().deliveries.rows().size()==1&&f.state().deliveries.rows().front().second.original_lot_id==collected.reply_lot_id,"permanent delivery preserves exact collected lot");f.common.state().results={};f.common.state().history.clear();f.cold_returns();
    auto saved=f.state();f.state().deliveries.find(saved.deliveries.order().front())->actual_result.reset();require(!validate_task_message_returns(f.source()).ok,"full pruning cannot remove delivery result");f.state()=saved;
    ReturnFixture stopped;stopped.outbound();auto attempt=stopped.collect();stopped.common.state().actors.find("recipient")->location=SettlementLocation{"town","elsewhere"};auto failed=stopped.finish(attempt);require(!failed.success&&stopped.state().collections.rows().back().second.reply_lot_id.empty(),"recipient leaves after start: permanent interruption and no false reply");stopped.common.state().actors.find("recipient")->location=SettlementLocation{"town","market"};require(stopped.finish(stopped.collect()).success,"new actual meeting can retry failed collection");
    stopped.cancel();require(stopped.tasks.tasks.find(stopped.task_id)->status=="cancelled"&&stopped.common.wallet("cash:player")->balance==100&&stopped.common.wallet("cash:sender")->balance==100,"original cancellation refunds all reward without paying carrier");require(stopped.common.count("bag:sender","reply",{{Owner::Kind::actor,"sender",{}}}).value==1,"cancel uses original static-assets actual return");
    ReturnFixture report_stop;report_stop.outbound();require(report_stop.finish(report_stop.collect()).success,"report-interruption case actual collection");auto report_attempt=report_stop.start_original("report_offer");report_stop.common.state().actors.find("sender")->location=SettlementLocation{"town","elsewhere"};auto interrupted=report_stop.finish(report_attempt);require(!interrupted.success&&!interrupted.task_id&&interrupted.on_unavailable==std::optional<StableId>{"wait_or_leave"},"original failed report optional shape is retained");require(report_stop.state().report_interruptions.size()==1&&report_stop.common.wallet("cash:player")->balance==100,"failed sender meeting creates no payment or delivery");report_stop.common.state().actors.find("sender")->location=SettlementLocation{"town","market"};const auto failed_at=report_stop.common.tick();report_stop.cancel();require(report_stop.common.tick()==failed_at&&report_stop.state().report_interruptions.rows().front().second.tick==failed_at,"same-tick actual interruption then original cancellation is legal");const auto facts=report_stop.common.state().facts;const auto failure_id=report_stop.state().report_interruptions.order().front();const auto actual_failure=*report_stop.common.state().facts.find(failure_id);report_stop.common.state().facts.erase(failure_id);report_stop.common.state().facts.insert(failure_id,actual_failure);require(!validate_task_message_returns(report_stop.source()).ok,"same-tick cancellation cannot be reordered before the actual report interruption");report_stop.common.state().facts=facts;report_stop.cold_returns();
    ReturnFixture report_retry;report_retry.outbound();require(report_retry.finish(report_retry.collect()).success,"report retry uses one actual collected reply");auto retry_action=report_retry.start_original("report_offer");report_retry.common.state().actors.find("sender")->location=SettlementLocation{"town","elsewhere"};require(!report_retry.finish(retry_action).success,"original report interrupted once");report_retry.common.state().actors.find("sender")->location=SettlementLocation{"town","market"};require(report_retry.finish(report_retry.start_original("report_offer")).success&&report_retry.state().report_starts.size()==2&&report_retry.state().collections.size()==1&&report_retry.common.wallet("cash:player")->balance==108,"fresh actual report can retry without another reply lot or double pay");
    ReturnFixture missing;missing.outbound();require(missing.finish(missing.collect()).success,"missing-lot case actual collection");require(missing.common.apply_resources({{ConsumeLots{"bag:player","reply",1,{{Owner::Kind::task,missing.task_id,{}}},{}}},{}}).ok,"explicit later physical loss fixture");require(!missing.report_preview().ok,"lost reply cannot be reported or paid");missing.cancel();require(missing.common.wallet("cash:player")->balance==100,"lost reply cancellation pays nothing");
    ReturnFixture deadline;deadline.outbound();deadline.common.state().identity.tick=deadline.tasks.tasks.find(deadline.task_id)->deadline_tick+3;deadline.meet("recipient");require(!preview_task_message_return_collection(deadline.source(),deadline.interaction,deadline.access,"player",{deadline.task_id}).ok,"past original deadline and grace cannot collect");deadline.cancel();require(deadline.tasks.tasks.find(deadline.task_id)->status=="failed","original overdue cancellation owns final status");
    ReturnFixture late;late.outbound();require(late.finish(late.collect()).success,"late-failure case actual reply");auto original=late.start_original("report_offer");late.common.state().next_instance_id=std::numeric_limits<Integer>::max()-3;const auto before_common=encode_common_state(late.common.state());const auto before_task=late.tasks;const auto before_social=late.social;auto save=late.common.begin_savepoint();require(save.ok,"original savepoint");DynamicStepInput phase{late.common.tick(),late.common.tick()+1};auto failure=late.finish_business(original,phase);require(!failure.ok&&failure.error.original.code=="InstanceIdOverflow","late source ID failure rejects candidate");require(late.common.wallet("cash:player")->balance==108&&late.common.count("bag:sender","reply",{{Owner::Kind::actor,"sender",{}}}).value==1,"failure is after original real cash and reply transfer");require(late.common.rollback_savepoint(save.value).ok,"original Common rollback");late.tasks=before_task;late.social=before_social;require(compare_boundary(before_common,encode_common_state(late.common.state())).exact()&&late.tasks.tasks.find(late.task_id)->status=="ready"&&late.state().deliveries.empty(),"original whole write set restores late failure without false success");late.cold_returns();
    std::cout<<"Real Task/Talk round-trip source cases; cross-city Travel, full Family and World remain separate\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
