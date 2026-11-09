// 本人足迹页只显示已核旧地点与来源，不提供远程会面、追踪或自动寻路命令。
#pragma once
#include "known_contact_location.hpp"
#include "resident/views/scene_presentation.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_known_contact_locations(const KnownContactLocationAccess&,const SceneCatalog&,Integer cursor=0);
}
