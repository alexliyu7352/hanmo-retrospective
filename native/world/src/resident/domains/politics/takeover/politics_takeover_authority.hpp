// 未来接管的作者范围不是当前产权或已发生的历史授权。
#pragma once
#include "politics_takeover_catalog.hpp"
namespace hanmo::resident {
// 仅供组织目录校验：准许声明本规则的专用Grant。实际check_authority仍严格核当前Owner。
bool political_takeover_grant_declares_resource(const CommonCatalog&,const OrganizationCatalog&,
    const PoliticsCatalog&,const MilitaryCatalog&,const PoliticalTakeoverCatalog&,
    const StableId& grant_id,const ResourceKey& resource);
}
