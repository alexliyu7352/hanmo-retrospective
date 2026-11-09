// 军事接触只读检测，复用B的实际轨迹与binary64交点；触及未实现战斗才明确能力拒绝。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
struct MilitaryContactHit { ContactHit contact; StableId first_army_id,second_army_id;Integer source_tick=0;TravelMotion first_motion,second_motion; };
struct MilitaryContactProbe { std::vector<MilitaryContactHit> ordered_hits; Integer eligible_armies=0,candidate_pairs=0,exact_pairs=0; };
Outcome<MilitaryContactProbe> probe_military_contacts(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const TravelState&,const TravelCatalog&,const TravelServices&,const std::vector<StableId>& hold={});
Status check_military_contact_capability(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const TravelState&,const TravelCatalog&,const TravelServices&,const std::vector<StableId>& hold={});
// 仅实际军团正向位移的执行点受模型能力门约束；关闭模型或无保留位移不得发空回执。
// 海陆接敌共用原真实征讨军令；异组织本身不授攻击权，不新建敌对关系账。
bool military_armies_hostile(const MilitaryState&,const MilitaryCatalog&,const ArmyRecord&,const ArmyRecord&);
bool capture_military_march(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const StableId& actor_id);
Status check_military_march_execution(const MilitaryCatalog&,const TerrainExecution&);
}
