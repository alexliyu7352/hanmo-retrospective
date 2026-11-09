// 新命令严格拒额外/重复字段，报价仅呈现当前Known会面中的原听取返报条款。
#include "task_personal_report_boundary.hpp"
#include <set>
namespace hanmo::resident {
Outcome<TaskPersonalReport> decode_task_personal_report(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;if(kind!=task_personal_report_command)unsupported("UnsupportedTaskPersonalReportCommand",kind);std::set<StableId> keys;for(const auto& field:object(params,"params"))if(!keys.insert(field.first).second)fail("DuplicateTaskPersonalReportField",field.first);fields(params,{"task_id"},{},"params");auto id=get_string(params,"task_id","params");if(id.empty()||id.size()>256)fail("InvalidTaskPersonalReportTaskId","task_id");return {true,{id},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_task_personal_report_quote(const TaskPersonalReportQuote& q){using V=BoundaryValue;return boundary::obj({{"task_id",V{q.task_id}},{"policy_id",V{q.policy_id}},{"opening_fact_id",V{q.sender_declaration.opening_fact_id}},{"sender_actor_id",V{q.sender_declaration.declaration.meeting.provider_actor_id}},{"place_id",V{q.sender_declaration.declaration.meeting.context.place_id}},{"handoff_fact_id",V{q.handoff_fact_id}},{"duration",V{q.duration}},{"reward",V{q.reward}}});}
}
