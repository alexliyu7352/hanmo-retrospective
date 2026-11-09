// 受领的业务事实与钱货只能来自原受领账；重复/额外参数均拒绝。
#include "task_personal_acceptance_boundary.hpp"
#include <set>
namespace hanmo::resident {
Outcome<TaskPersonalAcceptance> decode_task_personal_acceptance(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;if(kind!=task_personal_acceptance_command)unsupported("UnsupportedTaskPersonalAcceptanceCommand",kind);std::set<StableId> seen;for(const auto& row:object(params,"params"))if(!seen.insert(row.first).second)fail("DuplicateTaskPersonalAcceptanceField",row.first);fields(params,{"offer_id"},{},"params");const auto id=get_string(params,"offer_id","params");if(id.empty()||id.size()>256)fail("InvalidTaskPersonalAcceptanceOfferId","offer_id");return {true,{id},{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_task_personal_acceptance_quote(const TaskPersonalAcceptanceQuote& q){using V=BoundaryValue;return boundary::obj({{"offer_id",V{q.offer_id}},{"policy_id",V{q.policy_id}},{"opening_fact_id",V{q.sender_declaration.opening_fact_id}},{"sender_actor_id",V{q.sender_declaration.declaration.meeting.provider_actor_id}},{"place_id",V{q.sender_declaration.declaration.meeting.context.place_id}},{"duration",V{q.duration}},{"budget",V{q.budget}}});}
}
