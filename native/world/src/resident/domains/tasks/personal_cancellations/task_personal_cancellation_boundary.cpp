// 销约的业务事实与钱货只能来自原受领账；重复/额外参数均拒绝。
#include "task_personal_cancellation_boundary.hpp"
#include <set>
namespace hanmo::resident {
Outcome<TaskPersonalCancel> decode_task_personal_cancel(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;if(kind!=task_personal_cancellation_command)unsupported("UnsupportedTaskPersonalCancellationCommand",kind);std::set<StableId> seen;for(const auto& row:object(params,"params"))if(!seen.insert(row.first).second)fail("DuplicateTaskPersonalCancellationField",row.first);fields(params,{"task_id"},{},"params");const auto id=get_string(params,"task_id","params");if(id.empty()||id.size()>256)fail("InvalidTaskPersonalCancellationTaskId","task_id");return {true,{id},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
