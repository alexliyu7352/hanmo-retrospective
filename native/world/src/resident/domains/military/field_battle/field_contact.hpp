// 连续野战的真实道路接触候选；不调用旧六角会战构造器，也不预写接触事实。
#pragma once
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
struct MilitaryState;
struct MilitaryCatalog;
// 此值仅由同一接触阶段产生，安装方仍须核 source_tick 与实际原行动来源。
struct FieldRoadContact {
  Integer source_tick = 0;
  std::array<StableId, 2> army_ids;
  ContactHit contact;
};
// 只接受活跃军团、当前统帅行动与原敌对军令；不把不同组织直接当敌军。
Outcome<std::vector<FieldRoadContact>>
probe_field_road_contacts(const CommonRead &, const MilitaryState &,
                          const MilitaryCatalog &, const TravelState &,
                          const TravelCatalog &, const TravelServices &,
                          const std::vector<StableId> &held_armies,
                          const std::vector<StableId> &held_actors);
// 只在原 actions_step
// 后核实际末位置、原参数和已付本半日军粮；未走原步不得安装。
Status validate_field_road_contact_after_step(
    const CommonRead &, const MilitaryState &, const MilitaryCatalog &,
    const TravelState &, const TravelCatalog &, const FieldRoadContact &);
} // namespace hanmo::resident
