#include "resident/domains/military/military_command_contract.hpp"
// 接敌、会话和永久伤亡账接入同一世界候选；时钟、旅行费用和资源所有权仍由原共同服务掌管。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "military_contact.hpp"
#include "battle_request_transition.hpp"
namespace hanmo::resident {
// 海陆战读取同一实时军职/本部控制集合；返回有限权限，不复制世界或授予统帅身份。
struct MilitaryBattleControl { StableId actor_id,scope,army_id,office_id;Integer side=0;std::vector<StableId> unit_ids; };
Outcome<MilitaryBattleControl> military_battle_control(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const std::vector<BattleUnit>&,const StableId& actor);
// 部将请求的内部权源只沿同一军职裁定函数派生，不接受玩家指定统帅或AI名单。
Outcome<MilitaryRequestAuthority> military_request_authority(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryBattleRequest&);
bool military_actor_in_battle(const MilitaryState&,const StableId& actor);
std::vector<StableId> military_held_actors(const MilitaryState&);
Outcome<StableId> create_military_battle(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& source_id,const std::optional<StableId>& controller={});
Outcome<MilitaryQuote> preview_battle_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&);
Outcome<MilitaryEffect> apply_battle_command(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&);
Status validate_military_battle_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,bool supplied=false);
Status preflight_military_battles(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick,bool supplied=false);
Outcome<MilitaryBattleStep> step_military_battles(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick);
Outcome<MilitaryBattleStep> terminate_military_battle(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& battle_id,const StableId& reason,Integer tick);
Status apply_military_actor_changes(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const std::vector<MilitaryActorChange>&);
// 已落Land原来源与本次写入逐组匹配；组写完才通知条件记录方，失败整候选回滚。
Status apply_military_actor_changes_from_sources(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const std::vector<MilitaryActorChange>&,const std::vector<StableId>& actual_casualty_sources);
Outcome<StableId> install_military_contact(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,TravelState&,const MilitaryContactHit&,const std::vector<TerrainExecution>& executions={});
Outcome<MilitaryDamageSources> military_damage_sources(const MilitaryState&,const MilitaryCatalog&,const MilitaryBattleRecord&);
}

namespace hanmo::resident {
class CommonStore;
struct FieldBattleWorldState;
class FieldSegmentApplicationRead;
// Same original captured available/location consequence, restricted to an
// actual Field casualty and a no-personal-action member. S4 prepare/bind and
// S1 member ownership remain required by the caller in the same candidate.
Status apply_original_field_captured_actor_change(
    CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const SourceRef &actual_casualty, const StableId &actor_id,
    const FieldSegmentApplicationRead &);
}
