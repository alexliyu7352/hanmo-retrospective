// 真实Travel步之后、共同候选提交之前的航损写口；退出Action由中央原释放与完成回执链执行。
#pragma once
#include "naval_navigation.hpp"
#include "naval_navigation_geometry.hpp"
namespace hanmo::resident {
// 不接受任意tick：仅当前from→to的原单步与原有Action；接敌入场必须先定锚再调用本口。
Outcome<NavalNavigationStepFact> apply_naval_navigation_step(CommonWrite&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TravelState&,NavalState&,const NavalCatalog&,const NavalNavigationPolicy&,NavalNavigationState&,const NavalHistoryAccess&,const NavalNavigationExecution&,const DynamicStepInput&,const NavalContactFact* contact=nullptr);
}
