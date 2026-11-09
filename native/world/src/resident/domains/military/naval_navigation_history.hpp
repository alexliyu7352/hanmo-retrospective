// Naval5逐舟行账只读历史门；先验Common/原军史，再与同一资产重放前值逐事件交叉核对。
#pragma once
#include "naval_navigation_geometry.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct NavalOriginalSourceContext;
// 源、向量顺序、每Action同半日唯一与跨接管累计在此核；不读取今日船损倒补启程资格。
Status validate_naval_navigation_history(const CommonStore&,const TravelState&,const NavalNavigationState&);
// 由唯一Naval资产历史重放调用；启程核原船体/军籍，步进只修改同一replay船体。
Status replay_naval_navigation_event(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,const NavalCatalog&,const NavalNavigationPolicy&,const NavalNavigationState&,const NavalState&,NavalAssetState&,const NavalHistoryAccess&,const StableId& kind,const StableId& source_id);
// 当前完整Naval历史验证后的精确终止来源；不依latest、同tick猜测或外传reason。
Outcome<std::optional<NavalNavigationTermination>> query_naval_navigation_termination(const CommonStore&,const NavalNavigationState&,const StableId& source_id,const StableId& action_id);
// 决定/轮次的真实Common头前是否已由航损终止；只核永久原step，不读今日Action缺失。
Outcome<std::optional<NavalNavigationTermination>> query_naval_navigation_termination_before(
 const CommonStore&,const NavalNavigationState&,const StableId& action_id,const StableId& actor_id,
 const StableId& before_fact_id,Integer observed_tick);

// Explicit original-source observation. Stable callers keep current equality;
// a prefix/immediate-consequence caller validates retained source history only.
Status validate_naval_navigation_source_history(const CommonStore&,const TravelState&,const NavalNavigationState&,const NavalOriginalSourceContext&,bool require_current);
Outcome<std::optional<NavalNavigationTermination>> query_naval_navigation_termination_before(const CommonStore&,const NavalNavigationState&,const StableId& action_id,const StableId& actor_id,const StableId& before_fact_id,Integer observed_tick,const NavalOriginalSourceContext&);
}
