// 有货才呈现受托携物、真同伴才呈现护送；接了采购单不生成货车、远地位置或第二库存。
#include "task_travel_manifest.hpp"
#include "resident/views/world_observation.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 明确拒绝失效来源，不将损坏索引降级为看似可信的空页。
Outcome<TaskTravelManifestPage> rejected(const StableId& code){return {false,{},common_failure(code).error};}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool active(const TaskRecord& task){return task.status=="active"||task.status=="ready"||task.status=="settling";}
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
// 精确同地沿原物理位置，不新增观察半径或将同一城内不同场所视为在一起。
bool same_location(const Location& a,const Location& b){
    if(a.index()!=b.index())return false;
    if(const auto* x=std::get_if<SettlementLocation>(&a)){const auto& y=std::get<SettlementLocation>(b);return x->settlement_id==y.settlement_id&&x->place_id==y.place_id;}
    if(const auto* x=std::get_if<OnRouteLocation>(&a)){const auto& y=std::get<OnRouteLocation>(b);return x->edge_id==y.edge_id&&x->origin==y.origin&&x->progress==y.progress;}
    if(const auto* x=std::get_if<WildernessLocation>(&a)){const auto& y=std::get<WildernessLocation>(b);return x->map_id==y.map_id&&x->origin==y.origin&&x->point.x==y.point.x&&x->point.y==y.point.y;}
    return false;
}
// 先核本人当前成员，再沿真实Following引用查party；军团同籍不冒称个人护送。
bool same_party(const CommonStore& common,const StableId& observer,const StableId& target){
    const auto* members=common.members(observer);if(!members||!contains(*members,target)||target==observer)return false;
    const auto* actor=common.actor(observer);const auto* person=common.actor(target);if(!actor||!person||!person->alive)return false;
    const auto* own=std::get_if<FollowingLocation>(&actor->location);const auto* peer=std::get_if<FollowingLocation>(&person->location);
    const auto* party=own?common.state().parties.find(own->party_id):(peer?common.state().parties.find(peer->party_id):nullptr);
    return party&&contains(party->members,observer)&&contains(party->members,target)&&(own||party->leader_id==observer);
}
// 缺少作者名称时保稳定物品引用，不读取他人的库存或商业报价补字。
StableId item_name(const TaskCatalog& catalog,const StableId& id){const auto* name=catalog.presentation_names.find("items/"+id);return name?*name:id;}
bool text_bounded(const StableId& value){return value.size()<=4096;}
}
Outcome<TaskTravelManifestPage> query_task_travel_manifest(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const WorldObservationIndex& index,const PresentationRead& names,const StableId& observer,const std::optional<TaskTravelManifestCursor>& cursor){
    if(observer.empty()||observer!=common.state().metadata.player_actor_id)return rejected("TaskTravelObserverRequired");
    const auto* actor=common.actor(observer);if(!actor||!actor->alive)return rejected("TaskTravelObserverUnavailable");
    const auto* bag=common.inventory(actor->inventory_id);if(!bag||bag->owner.kind!=Owner::Kind::actor||bag->owner.id!=observer)return rejected("InvalidTaskTravelInventory");
    if(index.revision!=common.revision()||index.tick!=common.tick())return rejected("StaleTaskTravelIndex");
    const auto identity=common.state().identity;Integer offset=0;if(cursor){if(!same_identity(cursor->identity,identity)||cursor->observer_actor_id!=observer)return rejected("StaleTaskTravelCursor");offset=cursor->offset;}
    const std::vector<StableId> empty;const auto found=index.tasks_by_actor.find(observer);const auto& own=found==index.tasks_by_actor.end()?empty:found->second;
    if(bag->lots.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max())||own.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max())-bag->lots.size())return rejected("TaskTravelLimit");
    const auto lots=static_cast<Integer>(bag->lots.size()),total=lots+static_cast<Integer>(own.size());if(offset<0||offset>total)return rejected("InvalidTaskTravelCursor");
    TaskTravelManifestPage page;page.identity=identity;page.observer_actor_id=observer;page.cursor=offset;
    while(offset<total&&page.scanned<task_travel_scan_limit&&page.rows.size()<static_cast<std::size_t>(task_travel_page_limit)){
        const auto at=offset++;++page.scanned;
        if(at<lots){
            const auto& lot=bag->lots[static_cast<std::size_t>(at)];if(lot.quantity<=0||lot.owner.kind!=Owner::Kind::task||!contains(own,lot.owner.id))continue;
            // 只有本人实际货批并已在本人任务索引内，才允许按owner.task_id反查。
            const auto* task=tasks.tasks.find(lot.owner.id);if(!task||task->assignee!=observer||!active(*task))return rejected("InvalidTaskTravelIndex");const auto* definition=resolve_task_definition(tasks,catalog,*task);if(!definition)return rejected("UnknownOffer");
            const auto label=item_name(catalog,lot.item_id);if(!text_bounded(task->id)||!text_bounded(definition->display_name)||!text_bounded(lot.id)||!text_bounded(lot.item_id)||!text_bounded(label))return rejected("TaskTravelTextLimit");
            page.rows.emplace_back(TaskTravelCargo{task->id,definition->display_name,lot.id,lot.item_id,label,definition->template_id=="message"&&definition->goal.message_id==lot.item_id?"message":"goods",task->status,lot.quantity,common.tick(),!lot.reserved_by.empty()});continue;
        }
        const auto* task=tasks.tasks.find(own[static_cast<std::size_t>(at-lots)]);if(!task||task->assignee!=observer||!active(*task))return rejected("InvalidTaskTravelIndex");
        // 此段只投影真实护送进度；治理等非护送任务无需借旧offer定义，实物仍在上方严格核验。
        if(!task->escort)continue;const auto* definition=resolve_task_definition(tasks,catalog,*task);if(!definition)return rejected("UnknownOffer");
        if(definition->template_id!="escort"||!same_party(common,observer,definition->goal.actor_id))continue;
        const auto here=common.resolved_location(observer),peer=common.resolved_location(definition->goal.actor_id);if(!here.ok)return {false,{},here.error};if(!peer.ok)return {false,{},peer.error};if(!same_location(here.value,peer.value))continue;
        const auto display=names.display_name(definition->goal.actor_id);if(!display.ok)return {false,{},display.error};if(!text_bounded(task->id)||!text_bounded(definition->display_name)||!text_bounded(definition->goal.actor_id)||!text_bounded(display.value))return rejected("TaskTravelTextLimit");
        if(!task->escort)return rejected("InvalidEscortState");
        const auto objective=!task->escort->failure.empty()?"failed":task->escort->arrival_tick>=0?"arrived":task->escort->departed?"travelling":"not_departed";
        page.rows.emplace_back(TaskTravelEscort{task->id,definition->display_name,definition->goal.actor_id,display.value,task->status,objective,common.tick()});
    }
    if(offset<total)page.next_cursor=TaskTravelManifestCursor{identity,observer,offset};return {true,std::move(page),{}};
}
}
