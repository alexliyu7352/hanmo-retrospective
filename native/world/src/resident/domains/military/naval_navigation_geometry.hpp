// 新航损账复用原Travel纯几何与原接敌截断结果；此门不允许通过今日人物位置补历史。
#pragma once
#include "naval_navigation_model.hpp"
#include "naval_contact_history.hpp"
namespace hanmo::resident {
// contact必须是同候选实际安装中的那份原接触；冷读时同份接触另经完整Naval历史核验。
Status validate_naval_navigation_execution(const TravelState&,const TravelCatalog&,const NavalHistoryAccess&,const NavalVoyageFact&,const NavalNavigationExecution&,Integer completed_tick,const NavalContactFact* contact=nullptr);
// 精确比较原行动/时间/积分/位置，用于同步重放拒绝；字段顺序不决定业务语义。
bool same_naval_navigation_execution(const TerrainExecution&,const TerrainExecution&);
}
