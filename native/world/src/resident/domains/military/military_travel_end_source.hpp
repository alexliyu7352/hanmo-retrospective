// 只转发实际writer已经分配的来源键；Naval此时可能尚未append，不能当永久许可。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
struct MilitaryTravelEndingSource {
    enum class Kind { field_casualties, naval_round, naval_capture, naval_continue };
    Kind kind;
    StableId fact_id;
};
// 新口被选中后失败即失败，不回退旧口掩盖来源；同栈快照须在原完整事务成功后精确绑定。
using MilitaryTravelEndWithSource=std::function<Status(const StableId& action_id,const MilitaryTravelEndingSource&)>;
}
