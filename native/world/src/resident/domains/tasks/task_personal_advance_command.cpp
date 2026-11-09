// 冷参数边界不接受调用者自报的付款人、原雇主、利息或偿还来源。
#include "task_personal_advance_command.hpp"
#include <set>
namespace hanmo::resident {
Outcome<TaskPersonalAdvanceCommand> decode_task_personal_advance_command(const BoundaryValue& value){try{
    using namespace boundary;std::set<StableId> names;for(const auto& row:object(value,"params"))if(!names.insert(row.first).second)fail("DuplicateTaskAdvanceField","params/"+row.first);
    fields(value,{"task_id","amount"},{},"params");TaskPersonalAdvanceCommand result{get_string(value,"task_id","params"),get_int(value,"amount","params")};
    if(result.task_id.empty())fail("TaskNotKnown","params/task_id");if(result.amount<1)fail("InvalidTaskAdvanceAmount","params/amount");return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
BoundaryValue encode_task_personal_advance_quote(const TaskPersonalAdvanceQuote& quote){
    using V=BoundaryValue;return boundary::obj({{"task_id",V{quote.task_id}},{"contract_id",V{quote.contract_id}},{"amount",V{quote.amount}},{"previous_total",V{quote.previous_total}},{"authorized_maximum",V{quote.maximum_cash}},{"repayment_policy",V{StableId{"authorized_advance_first_v1"}}}});
}
}
