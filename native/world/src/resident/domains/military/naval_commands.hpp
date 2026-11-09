// 泊地编队是零时长计划命令；仅绑定现军团/实船/容器，不转货、修复或移动。
#pragma once
#include "naval_world.hpp"
#include "naval_battle_state.hpp"
namespace hanmo::resident {
// 原编舰请求人从不可变成功编军条款读取；当前接任者不是旧请求的提交人。
Outcome<StableId> naval_formation_actor(const MilitaryState&,const NavalFleetRecord&);
// 同业务幂等键永久随舰队来源保存，Common命令nonce仍沿原提交边界。
struct NavalForm { StableId request_key;NavalFormationRequest formation; };
struct NavalDisband { StableId request_key,fleet_id; };
using NavalCommand=std::variant<NavalForm,NavalDisband>;
// 公开回执只保存确切来源，不包含可被客户端拿来改状态的观察或候选。
struct NavalCommandReceipt { StableId kind,request_key,receipt_id,fleet_id,army_id,actor_id;Integer tick=0;FieldOrder field_order; };
struct NavalCommandQuote { bool replayed=false;StableId berth_id;NavalCommandReceipt receipt;std::optional<NavalFormationQuote> formation;std::optional<NavalReleaseObservation> release; };
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId&,const NavalCommand&);
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const StableId&,const NavalCommand&,bool combat_enabled);
// 仅在外层未发布的完整World候选上调用；任何失败均丢弃Common和Naval整候选。
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalAssetState&,const NavalAssetCatalog&,const StableId&,const NavalCommand&);
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalAssetState&,const NavalAssetCatalog&,const StableId&,const NavalCommand&,bool combat_enabled);
// Naval2重放编舰从原战果链还原首次名单，当前已捕船名单不能改写旧请求。
Outcome<NavalCommandQuote> preview_naval_command(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const StableId&,const NavalCommand&);
Outcome<NavalCommandReceipt> apply_naval_command(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,NavalState&,const NavalCatalog&,const StableId&,const NavalCommand&);
}
