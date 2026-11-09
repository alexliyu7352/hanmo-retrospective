// 地图只描述本人实际所在军团；军事名册、命令、钱粮仍归军事域及共同候选。
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "hanmo/resident/presentation_contract.hpp"
namespace hanmo::resident {
struct MapArmyCompany { StableId army_id,commander_actor_id;std::string commander_name;bool commanded_by_observer=false;Integer observed_tick=0; };
struct MapCompanyPage { WorldIdentity identity;StableId observer_actor_id;std::optional<MapArmyCompany> army; };
// 仅当前控制人可读，原active_army成员资格与本人raw位置必须一致；没有军团返回真实空值。
Outcome<MapCompanyPage> query_map_company(const CommonStore&,const MilitaryState&,const PresentationRead&,const StableId& observer);
// 本页不提供军力、库存或第二坐标，画布继续用同身份的原JourneyMapPosition。
BoundaryValue encode_map_company(const MapCompanyPage&);
}
