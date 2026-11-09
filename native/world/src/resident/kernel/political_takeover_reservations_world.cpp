// 只借当前World候选，预约及其责任主体始终留在原Common/Task/赈济账。
#include "political_takeover_reservations_world.hpp"
#include "world_bundle.hpp"
#include "political_takeover_execution_world.hpp"
#include "resident/domains/politics/takeover/politics_takeover_phase.hpp"
#include "resident/domains/tasks/task_public_account_reservations.hpp"
#include "resident/domains/tasks/governance_task_rules.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include "resident/domains/politics/politics_governance.hpp"
namespace hanmo::resident {
OccupationReservationRead political_takeover_reservation_read(const WorldBundle& w,const DynamicStepInput* phase){
    return [&w,phase](const OccupationReservedAsset& asset)->Outcome<std::vector<OccupationReservationProof>>{
        const auto fail=[&](const StableId& code){return Outcome<std::vector<OccupationReservationProof>>{false,{},common_failure(code,asset.reservation.id).error};};
        auto evaluation=political_takeover_evaluation_tick(w.common,phase);if(!evaluation.ok)return {false,{},evaluation.error};
        if(!w.political_takeover_enabled()||asset.reservation.empty())return fail("PoliticalReservationProfileRequired");
        const MilitaryOccupationFact* occupation=nullptr;for(const auto& row:w.military.occupation_facts)if(row.id==asset.occupation_id){if(occupation)return fail("PoliticalReservationOccupationMismatch");occupation=&row;}
        const auto* header=w.common.state().facts.find(asset.occupation_id);
        if(!occupation||!header||header->domain!="military"||header->kind!="military_occupation"||occupation->assets_applied.value_or(false)||occupation->garrison_id!=asset.garrison_id||occupation->from_organization_id!=asset.from_organization_id)return fail("PoliticalReservationOccupationMismatch");
        auto execution=PoliticalTakeoverExecutionWorldAccess::create(w,phase);if(!execution.ok)return {false,{},execution.error};
        const Owner original{Owner::Kind::organization,asset.from_organization_id,{}};
        const bool wallet=asset.kind==OccupationReservedAsset::Kind::wallet;
        if(wallet){if(asset.amount<0||asset.quantity!=0||!asset.inventory_id.empty()||!asset.lot_id.empty()||!asset.item_id.empty()||std::find(occupation->public_wallet_ids.begin(),occupation->public_wallet_ids.end(),asset.wallet_id)==occupation->public_wallet_ids.end())return fail("PoliticalReservationAssetMismatch");}
        else if(asset.kind==OccupationReservedAsset::Kind::inventory_lot){
            if(asset.quantity<=0||asset.amount!=0||!asset.wallet_id.empty()||std::find(occupation->public_inventory_ids.begin(),occupation->public_inventory_ids.end(),asset.inventory_id)==occupation->public_inventory_ids.end())return fail("PoliticalReservationAssetMismatch");
            const auto* inventory=w.common.inventory(asset.inventory_id);const LotView* lot=nullptr;if(inventory)for(const auto& row:inventory->lots)if(row.id==asset.lot_id){if(lot)return fail("PoliticalReservationAssetMismatch");lot=&row;}
            if(!lot||lot->item_id!=asset.item_id||lot->quantity!=asset.quantity||lot->reserved_by!=asset.reservation)return fail("PoliticalReservationAssetMismatch");
        }else return fail("PoliticalReservationAssetMismatch");
        OccupationReservationProof proof;proof.asset=asset;
        if(asset.reservation.kind==ReservationRef::Kind::action){
            auto claimed=wallet?read_task_public_wallet_reservation(w.common,w.tasks,*w.catalog->tasks,original,asset.wallet_id,{asset.reservation,asset.amount},execution.value->responsibilities(),phase):read_task_public_lot_reservation(w.common,w.tasks,*w.catalog->tasks,original,asset.inventory_id,asset.lot_id,asset.reservation,execution.value->responsibilities(),phase);
            if(!claimed.ok)return {false,{},claimed.error};if(!claimed.value)return fail("PoliticalReservationSourceUnavailable");
            const auto* action=w.tasks.actions.find(asset.reservation.id);if(!action)return fail("PoliticalReservationSourceUnavailable");proof.source=OccupationReservationProof::Source::task_static_action;proof.responsibility_id=action->task_id;
        }else if(asset.reservation.kind==ReservationRef::Kind::contract){
            if(!w.public_relief_enabled()||!w.governance_tasks_enabled()||!w.politics_governance_enabled())return fail("PoliticalReservationSourceUnavailable");
            auto agencies=PoliticalGovernanceAgencyWorldAccess::create(w,phase);if(!agencies.ok)return {false,{},agencies.error};GovernanceTaskRead tasks(w.common,w.tasks,w.governance_tasks);PoliticsGovernanceAccess authority(w.common,w.politics,w.politics_governance,*w.catalog->politics_governance,w.organization,*w.catalog->organization,w.services,agencies.value?&agencies.value->read():nullptr);
            auto claimed=wallet?read_relief_public_wallet_reservation(w.common,w.local_economy,*w.catalog->public_relief,w.public_relief,tasks,authority,original,asset.wallet_id,{asset.reservation,asset.amount},phase,execution.value->economy()):read_relief_public_lot_reservation(w.common,w.local_economy,*w.catalog->public_relief,w.public_relief,tasks,authority,original,asset.inventory_id,asset.lot_id,asset.reservation,phase,execution.value->economy());
            if(!claimed.ok)return {false,{},claimed.error};if(!claimed.value)return fail("PoliticalReservationSourceUnavailable");proof.source=OccupationReservationProof::Source::relief_contract;proof.responsibility_id=asset.reservation.id;
        }else return fail("PoliticalReservationSourceUnavailable");
        return {true,{std::move(proof)},{}};
    };
}
}
