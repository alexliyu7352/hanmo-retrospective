#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "resident/domains/tasks/task_source_projection.hpp"
// 入口拒绝未知/重复字段；Action冷参数只导出原永久开工源，载入不得自行构造另一份接触。
#include "task_message_return_boundary.hpp"
#include <set>
namespace hanmo::resident {
Outcome<TaskMessageReturnCollect> decode_task_message_return_collect(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;if(kind!=task_message_return_collect_command)unsupported("UnsupportedTaskMessageReturnCommand",kind);std::set<StableId> keys;for(const auto& pair:object(params,"params"))if(!keys.insert(pair.first).second)fail("DuplicateTaskMessageReturnField",pair.first);fields(params,{"task_id"},{},"params");auto id=get_string(params,"task_id","params");if(id.empty()||id.size()>256)fail("InvalidTaskMessageReturnTaskId","task_id");return {true,{id},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_task_message_return_quote(const TaskMessageReturnQuote& q){using V=BoundaryValue;return boundary::obj({{"task_id",V{q.task_id}},{"policy_id",V{q.policy_id}},{"handoff_fact_id",V{q.handoff_fact_id}},{"reply_item_id",V{q.reply_item_id}},{"duration",V{q.duration}}});}
Status emit_task_message_return_action_fields(ProjectionWriter&w,const TaskMessageReturnState&state,const StableId&action){const TaskMessageReturnStart*source=nullptr;for(const auto&row:state.starts.rows())if(row.second.action_id==action){if(source)return common_failure("DuplicateTaskMessageReturnAction",action);source=&row.second;}if(!source)return common_failure("TaskMessageReturnActionSourceMissing",action);return emit_task_message_return_fact_fields(w,state,source->fact_id);}
Outcome<BoundaryValue> encode_task_message_return_action(const TaskMessageReturnState& state,const StableId& action){BoundaryValue value;BoundaryProjectionWriter writer(value);auto checked=writer.object("",[&](auto& out){return emit_task_message_return_action_fields(out,state,action);});if(checked.ok)checked=writer.finish();return checked.ok?Outcome<BoundaryValue>{true,std::move(value),{}}:Outcome<BoundaryValue>{false,{},checked.error};}
}
