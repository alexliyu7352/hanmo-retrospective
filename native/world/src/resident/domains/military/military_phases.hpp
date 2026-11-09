// 军事空域与后尾的显式能力闭包；每个入口核真实待处理对象，绝不以空成功占位吞掉请求。
#pragma once
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
struct MilitaryReturnCompletion { StableId order_id,receipt_id; };

Status military_preflight(const CommonRead&,const MilitaryState&);
Status military_preflight(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick);
Status military_siege_step(const MilitaryState&);
Status military_battle_step(const MilitaryState&);
Status military_battle_actor_changes(const MilitaryState&,const std::vector<MilitaryActorChange>&);
Status military_siege_battles_finished(const MilitaryState&,const std::vector<StableId>& completed_battles);
Status military_siege_results(const MilitaryState&,const std::vector<StableId>& pending_siege_results);
Status release_invalid_military_commands(const CommonRead&,const MilitaryState&,Integer tick);
Status release_invalid_military_commands(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer tick);
Status complete_military_returns(const MilitaryState&,const std::vector<MilitaryReturnCompletion>&);
Status complete_military_returns(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const std::vector<MilitaryReturnCompletion>&);
Status begin_military_journeys(const MilitaryState&,const std::vector<MilitaryJourneyRequest>&);
}
