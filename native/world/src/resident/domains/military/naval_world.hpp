// 实船只消费同一已验证世界的现军职、编组、人物和Common容器；不接收客户端证明。
#pragma once
#include "naval_assets.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
class CommonStore;struct NavalBattleState;
// 几何占用只证明原舰队仍承载统帅，不授航行、战斗、任职或资源处分权。
struct NavalLocationSource { StableId fleet_id,army_id,commander_actor_id,formation_source_id,world_map_id; };
// 在完整Naval历史校验同栈中读取原编队事实；失能船可原地浮泊，不降级为步行。
Outcome<std::optional<NavalLocationSource>> query_naval_location_source(const CommonStore&,const MilitaryState&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id);
// 此读口供形成舰队报价；当前必须在正式泊地且全体无其它活动占用。
Outcome<NavalFormationObservation> observe_naval_formation(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id,const NavalFormationRequest&);
Outcome<NavalFormationObservation> observe_naval_formation(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id,const NavalFormationRequest&,bool combat_enabled);
// 从唯一Common/军旅账导出当前人货观察，供原载重门和真实接管共用。
Outcome<NavalFormationObservation> observe_naval_manifest(const CommonRead&,const MilitaryState&,const NavalAssetState&,const NavalFleetRecord&);
// Naval2 遗留人物只凭原伤亡锚点接受其不再随军的位置，旧 Naval1 不放宽。
Outcome<NavalFormationObservation> observe_naval_manifest(const CommonRead&,const MilitaryState&,const NavalAssetState&,const NavalFleetRecord&,const NavalBattleState&);
// 浮航状态复核真实逐船容量与占用，不要求受伤船员重新满足出航条件。
Outcome<NavalFormationQuote> validate_naval_manifest(const CommonRead&,const MilitaryState&,const NavalAssetState&,const NavalAssetCatalog&,const NavalFleetRecord&);
// 退出只解除当前军团的既有占用，伤员/失能船不需恢复才能在原泊地释放。
Outcome<NavalReleaseObservation> preview_naval_release(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetCatalog&,const NavalFleetRecord&,const StableId& actor_id);
// 无舰队返回空可沿原陆路；有舰队但失效必须报错，不得降级成陆军继续。
Outcome<std::optional<NavalTravelProof>> preview_naval_travel(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id);
// 同一真实航行守卫的显式Naval2历史形状入口，不推断boat等于启用战斗。
Outcome<std::optional<NavalTravelProof>> preview_naval_travel(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId& actor_id,bool combat_enabled);
}
