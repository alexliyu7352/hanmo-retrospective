// 同次占领的逐预约具名证明，只读原责任，不授予政治接管权、不写冷档。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
namespace hanmo::resident {
struct OccupationReservedAsset {
 enum class Kind { wallet,inventory_lot };Kind kind=Kind::wallet;
 StableId occupation_id,garrison_id,from_organization_id;
 ReservationRef reservation;
 StableId wallet_id,inventory_id,lot_id,item_id;
 Integer amount=0,quantity=0;
};
struct OccupationReservationProof {
 enum class Source { task_static_action,relief_contract };Source source=Source::task_static_action;
 OccupationReservedAsset asset;
 StableId responsibility_id;
};
// 必须恰一责任认领。空、多重、未知来源或任一原资源字段不符一律拒绝。
using OccupationReservationRead=std::function<Outcome<std::vector<OccupationReservationProof>>(const OccupationReservedAsset&)>;
}
