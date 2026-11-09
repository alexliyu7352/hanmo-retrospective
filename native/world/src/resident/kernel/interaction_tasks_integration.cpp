#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
// 本层只桥接有限查询与具名TaskCommand；公共守卫、资金和交付由原网关负责。
#include "interaction_tasks_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_missions.hpp"
#include "resident/domains/tasks/task_lifecycle_view.hpp"
namespace hanmo::resident {
// 观察者固定当前控制人，预览保持世界只读；任何未来能力都必须在原领域登记。
Outcome<BoundaryValue> query_interaction_tasks(const WorldBundle& world,Integer cursor){
    const auto& actor=world.common.state().metadata.player_actor_id;auto access=world.services.interaction_access();
    TaskConversationPreview preview=[&](const TaskCommand& command)->Outcome<TaskQuote>{
        auto prepared=prepare_command(world,actor,{command.kind,command});if(!prepared.ok)return {false,{},prepared.error};
        const auto* quote=prepared.value&&prepared.value->quote?std::get_if<TaskQuote>(&prepared.value->quote->data):nullptr;
        if(!quote)return {false,{},common_failure("MissingTaskConversationQuote",{},ErrorKind::internal).error};
        return {true,*quote,{}};
    };
    if(world.envoy_tasks_enabled()){DiplomaticMissionAccess missions(world.common,world.organization,*world.catalog->organization,*world.catalog->diplomacy,world.diplomacy);auto envoy_access=world.services.interaction_access();DiplomaticPresentRead report_context{world.common,world.organization,*world.catalog->organization,*world.catalog->diplomacy,world.diplomacy,world.interaction,*world.catalog->interaction,envoy_access};std::optional<DiplomaticReportAccess> report_source;if(world.envoy_tasks.version==2){if(!world.diplomacy.v2)return {false,{},common_failure("MissingDiplomacyV2State").error};report_source.emplace(report_context,*world.diplomacy.v2);}return query_task_conversation_with_envoy(world.common,world.tasks,*world.catalog->tasks,world.interaction,*world.catalog->interaction,access,actor,cursor,preview,world.governance_tasks_enabled()?&world.governance_tasks:nullptr,world.envoy_tasks,true,missions,missions,world.tasks.offer_responses?&*world.tasks.offer_responses:nullptr,report_source?&*report_source:nullptr);}
    if(world.governance_tasks_enabled())return query_task_conversation_with_governance(world.common,world.tasks,*world.catalog->tasks,world.interaction,*world.catalog->interaction,access,actor,cursor,preview,world.governance_tasks,true,world.tasks.offer_responses?&*world.tasks.offer_responses:nullptr);
    return query_task_conversation(world.common,world.tasks,*world.catalog->tasks,world.interaction,*world.catalog->interaction,access,actor,cursor,preview,world.tasks.offer_responses?&*world.tasks.offer_responses:nullptr);
}
}
