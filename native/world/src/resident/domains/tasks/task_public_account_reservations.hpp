// 占领前只认领真实Task公钱预约；不释放预约、不转移任何任务或私人货权。
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
class PoliticalTakeoverRead;
// 先由组合层验证同一World；每项仍逐原容器、预约kind/id及冻结Task动作核对。
Outcome<bool> read_task_public_wallet_reservation(const CommonStore&,const TaskState&,const TaskCatalog&,
    const Owner& original_owner,const StableId& wallet_id,const Reservation&,
    const PoliticalTakeoverRead* authority=nullptr,const DynamicStepInput* phase=nullptr);
// 当前任务只预约本人背包中的Task/actor货，不能把它认领为可占领的组织公开货。
Outcome<bool> read_task_public_lot_reservation(const CommonStore&,const TaskState&,const TaskCatalog&,
    const Owner& original_owner,const StableId& inventory_id,const StableId& lot_id,const ReservationRef&,
    const PoliticalTakeoverRead* authority=nullptr,const DynamicStepInput* phase=nullptr);
}
