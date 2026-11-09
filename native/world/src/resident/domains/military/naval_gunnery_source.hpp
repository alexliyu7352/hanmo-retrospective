// 原接敌候选借成长纯读冻结一次；未接完整技能reader时不能启用Naval6。
#pragma once
#include "naval_gunnery_model.hpp"
#include "naval_gunnery_skill_contract.hpp"
#include "naval_battle_state.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 必须在原contact及battle头已登记之后、AI/计划产生之前调用；只原from→to阶段可入账。
// 本函数写共同事实与本域来源，晚失败由调用者原完整World候选撤回。
Outcome<StableId> record_naval_gunnery_preparation(CommonStore&,const MilitaryState&,
 const NavalAssetState&,NavalGunneryState&,const NavalContactFact&,
 const NavalTacticalFrame& initial_frame,const NavalGunnerySkillRead&,const DynamicStepInput&);
// 早期纯结构门不证明XP：原Naval建场必须交叉核绑定，World在Family/Birth/Growth后必补完整门。
Status validate_naval_gunnery_sources(const CommonStore&,const NavalGunneryState&,const NavalBattleState&);
// 完整技能史只在Family/Birth/Growth先验后调用；同一World未发布候选不可遗漏此晚门。
Status validate_naval_gunnery_history(const CommonStore&,const NavalGunneryState&,
 const NavalBattleState&,const NavalGunnerySkillRead&);
// initial_frame只能来自原真实军籍/舰队/接敌重放，不接受今日frame反填原船长。
Status validate_naval_gunnery_initial_bindings(const NavalGunneryPreparation&,const NavalTacticalFrame& initial_frame);
// 已有后续回合只核原unit/captain来源；合法capture/continue换船仍用原入战技能。
Status validate_naval_gunnery_frame_source(const NavalGunneryPreparation&,const NavalTacticalFrame&);
// 无该场返回空；重复/缺失本场来源由history门拒绝，查询不授攻击或指挥权限。
const NavalGunneryPreparation* naval_gunnery_preparation(const NavalGunneryState&,const StableId& battle_id);
// 有界冷形状与字段序由独立Naval6 codec调用，旧段不访问此表。
Status validate_naval_gunnery_shape(const NavalGunneryState&);
}
