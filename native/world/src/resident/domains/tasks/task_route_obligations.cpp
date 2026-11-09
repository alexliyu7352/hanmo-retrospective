#include "shop_orders/task_shop_order_sources.hpp"
// 受领条款是本人知情来源；读口只将原场所映射到目的城，不查询收件人或经办人的当前位置。
#include "task_route_obligations.hpp"
#include "task_travel_manifest.hpp"
#include "governance_task_rules.hpp"
#include "envoy_task_view.hpp"
#include "resident/model/procurement_timing.hpp"
#include "resident/views/world_observation.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 来源过时或损坏时明确拒绝，不能把未知合同伪装为空的路线提醒。
Outcome<TaskRouteObligationsPage> rejected(const StableId& code){return {false,{},common_failure(code).error};}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool bounded(const StableId& value){return value.size()<=4096;}
V identity_value(const WorldIdentity& identity){StableId hex;const char* digits="0123456789abcdef";for(const auto byte:identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}});}
V cursor_value(const TaskRouteObligationsCursor& cursor){return boundary::obj({{"identity",identity_value(cursor.identity)},{"observer_actor_id",V{cursor.observer_actor_id}},{"destination_settlement_id",V{cursor.destination_settlement_id}},{"offset",V{cursor.offset}}});}
}
static Outcome<TaskRouteObligationsPage> route_obligations(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const CommonCatalog& shared,const WorldObservationIndex& index,const StableId& observer,const StableId& destination,const std::optional<TaskRouteObligationsCursor>& cursor,const GovernanceTaskState* governance,const EnvoyTaskState* envoy,const DiplomaticMissionRead* missions,const DiplomacyPresentationRead* presentations,const DiplomaticEnvoyReportRead* reports=nullptr){
    if(observer.empty()||observer!=common.state().metadata.player_actor_id)return rejected("TaskRouteObserverRequired");
    const auto* actor=common.actor(observer);if(!actor||!actor->alive)return rejected("TaskRouteObserverUnavailable");
    if(destination.empty()||!bounded(destination))return rejected("InvalidTaskRouteDestination");
    if(index.revision!=common.revision()||index.tick!=common.tick())return rejected("StaleTaskRouteIndex");
    if(governance&&governance->version!=1)return rejected("UnsupportedGovernanceTaskVersion");
    if(envoy&&(envoy->version!=1&&envoy->version!=2))return rejected("UnsupportedEnvoyTaskVersion");
    if(tasks.shop_orders){auto shop_source=validate_task_shop_order_ledger(common,tasks,*tasks.shop_orders);if(!shop_source.ok)return {false,{},shop_source.error};}
    const auto identity=common.state().identity;Integer offset=0;if(cursor){if(!same_identity(cursor->identity,identity)||cursor->observer_actor_id!=observer||cursor->destination_settlement_id!=destination)return rejected("StaleTaskRouteCursor");offset=cursor->offset;}
    const std::vector<StableId> empty;const auto found=index.tasks_by_actor.find(observer);const auto& own=found==index.tasks_by_actor.end()?empty:found->second;
    if(own.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()/2))return rejected("TaskRouteLimit");const auto total=static_cast<Integer>(own.size())*2;
    if(offset<0||offset>total)return rejected("InvalidTaskRouteCursor");
    TaskRouteObligationsPage page;page.identity=identity;page.observer_actor_id=observer;page.destination_settlement_id=destination;page.cursor=offset;
    // 每份任务固定两个扫描槽：原回报场所、原护送目的地。空槽也推进预算，避免无限筛选。
    while(offset<total&&page.scanned<task_route_scan_limit&&page.rows.size()<static_cast<std::size_t>(task_route_page_limit)){
        const auto at=offset++;++page.scanned;const auto& id=own[static_cast<std::size_t>(at/2)];const auto* task=tasks.tasks.find(id);
        if(!task||task->id!=id||task->assignee!=observer||task->accepted_tick>common.tick())return rejected("InvalidTaskRouteIndex");
        // 店单不借普通Offer推远方客户地址；原客户当面办理仍在本人店单页。
        if(tasks.shop_orders&&tasks.shop_orders->by_task.count(id))continue;
        if(governance&&governance->governance_contracts.find(id)){auto source=GovernanceTaskRead(common,tasks,*governance).governance_task_source(id);if(!source.ok)return {false,{},source.error};continue;}
        if(envoy&&envoy->envoy_contracts.find(id)){
            if(!missions||!presentations)return rejected("MissingEnvoyTaskSources");auto source=query_envoy_task_progress(common,tasks,*envoy,*missions,*presentations,id,reports);if(!source.ok)return {false,{},source.error};const auto& value=source.value;
            // 第二槽留给原回报；使者目前没有独立返报截止，不能借递交截止冒充。
            if(at%2!=0||value.delivery_fact_id||value.final_fact_id)continue;auto original=missions->diplomatic_mission_source(value.mission_authority_fact_id);if(!original.ok)return {false,{},original.error};const auto* place=shared.places.find(value.delivery_site_id);if(!place)return rejected("UnknownPlace");if(place->settlement_id!=destination)continue;
            const auto* label=catalog.presentation_names.find("places/"+value.delivery_site_id);const auto name=label?*label:value.delivery_site_id;if(!bounded(id)||!bounded(value.display_name)||!bounded(value.delivery_site_id)||!bounded(name)||original.value.delivery_work_ticks<1)return rejected("InvalidEnvoyTaskRoute");
            page.rows.push_back({id,value.display_name,"envoy",value.status,value.delivery_site_id,name,"envoy_delivery","delivery_completion",value.delivery_expires_tick,original.value.delivery_work_ticks,false,false});continue;
        }
        if(task->status=="settling")continue;if(task->status!="active"&&task->status!="ready")return rejected("InvalidTaskRouteIndex");
        const auto* definition=resolve_task_definition(tasks,catalog,*task);if(!definition)return rejected("UnknownOffer");
        if(definition->template_id!="procurement"&&definition->template_id!="message"&&definition->template_id!="escort")return rejected("UnsupportedTaskRouteKind");
        if(task->deadline_tick<task->accepted_tick||task->accepted_tick<0)return rejected("InvalidTaskRouteDeadline");
        if(definition->template_id=="escort"&&!task->escort)return rejected("InvalidEscortState");
        if(task->contract_id){const auto* contract=tasks.contracts.find(*task->contract_id);if(!contract||contract->current_task_id!=std::optional<StableId>(id)||contract->executor_actor_id!=std::optional<StableId>(observer)||contract->frozen_terms.deadline_tick!=task->deadline_tick)return rejected("InvalidTaskContract");if(contract->execution_stopped)continue;}
        StableId place_id,target="report";
        if(at%2==0){const auto* role=shared.roles.find(definition->report_role);if(!role)return rejected("UnknownRole");place_id=role->place_id;}
        else{if(definition->template_id!="escort"||task->status!="active"||!task->escort->failure.empty()||task->escort->arrival_tick>=0)continue;place_id=definition->goal.destination_place_id;target="escort_arrival";}
        const auto* place=shared.places.find(place_id);if(!place)return rejected("UnknownPlace");if(place->settlement_id!=destination)continue;
        const auto* label=catalog.presentation_names.find("places/"+place_id);const auto place_name=label?*label:place_id;
        if(!bounded(id)||!bounded(definition->display_name)||!bounded(place_id)||!bounded(place_name))return rejected("TaskRouteTextLimit");
        const auto scope=definition->template_id=="procurement"?"delivery_completion":"report_completion";
        const bool prior=task->status=="active"&&(definition->template_id=="message"||definition->template_id=="escort");
        page.rows.push_back({id,definition->display_name,definition->template_id,task->status,place_id,place_name,target,scope,task->deadline_tick,task_handover_ticks,true,prior});
    }
    if(offset<total)page.next_cursor=TaskRouteObligationsCursor{identity,observer,destination,offset};return {true,std::move(page),{}};
}
// 旧签名保持原行为，未提供使者来源时未知任务仍明确拒绝。
Outcome<TaskRouteObligationsPage> query_task_route_obligations(const CommonStore& c,const TaskState& t,const TaskCatalog& catalog,const CommonCatalog& shared,const WorldObservationIndex& index,const StableId& actor,const StableId& destination,const std::optional<TaskRouteObligationsCursor>& cursor,const GovernanceTaskState* governance){return route_obligations(c,t,catalog,shared,index,actor,destination,cursor,governance,nullptr,nullptr,nullptr);}
// 中央仅在新模块已登记时提供这份真实sidecar与政治只读源。
Outcome<TaskRouteObligationsPage> query_task_route_obligations_with_envoy(const CommonStore& c,const TaskState& t,const TaskCatalog& catalog,const CommonCatalog& shared,const WorldObservationIndex& index,const StableId& actor,const StableId& destination,const std::optional<TaskRouteObligationsCursor>& cursor,const GovernanceTaskState* governance,const EnvoyTaskState& envoy,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DiplomaticEnvoyReportRead* reports){return route_obligations(c,t,catalog,shared,index,actor,destination,cursor,governance,&envoy,&missions,&presentations,reports);}
// 明示deadline_scope；表现层不能把护送到达或到城时刻当成最终回报完成。
BoundaryValue encode_task_route_obligations(const TaskRouteObligationsPage& page){
    V::Array rows;for(const auto& row:page.rows)rows.push_back(boundary::obj({{"task_id",V{row.task_id}},{"display_name",V{row.display_name}},{"template_id",V{row.template_id}},{"task_status",V{row.task_status}},{"place_id",V{row.place_id}},{"place_name",V{row.place_name}},{"target_kind",V{row.target_kind}},{"deadline_scope",V{row.deadline_scope}},{"deadline_tick",V{row.deadline_tick}},{"deadline_inclusive",V{row.deadline_inclusive}},{"minimum_handover_ticks",V{row.minimum_handover_ticks}},{"requires_prior_objective",V{row.requires_prior_objective}}}));
    V::Dict result{{"ok",V{true}},{"version",V{Integer{1}}},{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"destination_settlement_id",V{page.destination_settlement_id}},{"cursor",V{page.cursor}},{"scanned",V{page.scanned}},{"rows",V{std::move(rows)}}};if(page.next_cursor)result.emplace_back("next_cursor",cursor_value(*page.next_cursor));return V{std::move(result)};
}
// 身份位格式沿现有携行游标；目的城单独严格绑定，不接受客户端加入人物或场所真值。
Outcome<TaskRouteObligationsCursor> decode_task_route_obligations_cursor(const BoundaryValue& input){try{
    using namespace boundary;fields(input,{"identity","observer_actor_id","destination_settlement_id","offset"},{},"cursor");
    auto destination=get_string(input,"destination_settlement_id","cursor");if(destination.empty()||!bounded(destination))fail("InvalidTaskRouteCursor","cursor");
    auto shared=decode_task_travel_manifest_cursor(obj({{"identity",field(input,"identity","cursor")},{"observer_actor_id",field(input,"observer_actor_id","cursor")},{"offset",field(input,"offset","cursor")}}));if(!shared.ok)return {false,{},shared.error};return {true,{shared.value.identity,shared.value.observer_actor_id,destination,shared.value.offset},{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
}
