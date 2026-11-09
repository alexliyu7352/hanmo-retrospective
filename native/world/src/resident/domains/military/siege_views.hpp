// 城防页面与批量单部草案只输出原公开DTO，不包含AI、随机流、完整观察或任何世界副本。
#pragma once
#include "siege_world.hpp"
#include "military_integration_assets.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> military_siege_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const CommonCatalog&,const MilitaryAccess&,const StableId& siege_id={},const MilitaryIntegrationAccess* integration_access=nullptr);
Outcome<BoundaryValue> military_siege_draft(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const BoundaryValue& identity,const BoundaryValue::Array& requests,bool first_ok=false);
}
