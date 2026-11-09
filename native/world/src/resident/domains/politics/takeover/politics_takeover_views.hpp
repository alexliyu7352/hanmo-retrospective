// 本人场所内的公职责任账投影；不接受任意观察者或远方登记ID。
#pragma once
#include "politics_takeover_rules.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_political_takeover_responsibilities(
    const PoliticalTakeoverContext&,const StableId& observer_actor_id,const StableId& place_id);
}
