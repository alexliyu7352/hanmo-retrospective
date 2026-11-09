#include "resident/domains/military/occupation_model.hpp"
// 转移前捕获真实责任清单；本值只在同一候选的军事apply前后短暂使用。
#pragma once
#include "politics_takeover_state.hpp"
namespace hanmo::resident {
struct TaskState; struct TaskCatalog; struct GovernanceTaskState; struct EnvoyTaskState;
struct FiscalSettlementState; struct FiscalSettlementCatalog; struct PublicReliefState;
struct MilitaryState;struct OccupationFiscalSettlementState;struct DynamicStepInput;
struct PoliticalPublicLotReservationSnapshot {StableId inventory_id,lot_id;ReservationRef reservation;};
class PoliticalTakeoverCapture;
Outcome<PoliticalTakeoverCapture> capture_political_takeover_responsibilities(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const TaskState&,const TaskCatalog&,const GovernanceTaskState*,const EnvoyTaskState*,
    const MilitaryState&,const StableId& occupation_fact_id);
// 新财政仍读原候选；phase只沿实际半日，不从占领tick构造未来时间。
Outcome<PoliticalTakeoverCapture> capture_political_takeover_responsibilities(
    const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
    const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
    const TaskState&,const TaskCatalog&,const GovernanceTaskState*,const EnvoyTaskState*,
    const MilitaryState&,const StableId&,const OccupationFiscalSettlementState*,const DynamicStepInput*);
// 字段不接受玩家或codec输入；默认空值无法通过后续真实军事来源匹配。
class PoliticalTakeoverCapture {
public:
    PoliticalTakeoverCapture()=default;
    const PoliticalTakeoverRegistration& original() const {return value_;}
    const OccupationAssetManifest& assets() const {return assets_;}
    const std::vector<std::pair<StableId,std::vector<Reservation>>>& wallet_reservations()const{return wallet_reservations_;}
    const std::vector<PoliticalPublicLotReservationSnapshot>& public_lot_reservations()const{return public_lot_reservations_;}
private:
    PoliticalTakeoverRegistration value_;
    OccupationAssetManifest assets_;
    // 仅本次同步apply前后比较，不写入checkpoint或生成平行预约账。
    std::vector<std::pair<StableId,std::vector<Reservation>>> wallet_reservations_;
    std::vector<PoliticalPublicLotReservationSnapshot> public_lot_reservations_;
    friend Outcome<PoliticalTakeoverCapture> capture_political_takeover_responsibilities(
        const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
        const FiscalSettlementState*,const FiscalSettlementCatalog*,const PublicReliefState*,
        const TaskState&,const TaskCatalog&,const GovernanceTaskState*,const EnvoyTaskState*,
        const MilitaryState&,const StableId&,const OccupationFiscalSettlementState*,const DynamicStepInput*);
};
}
