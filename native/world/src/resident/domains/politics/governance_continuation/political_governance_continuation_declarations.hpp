// 作者条件性Grant只声明未来同ID公库，绝不等于当前产权或已批准续办。
#pragma once
#include "resident/domains/politics/takeover/politics_takeover_catalog.hpp"
namespace hanmo::resident {
bool governance_continuation_grant_declares_resource(const CommonCatalog&,const OrganizationCatalog&,const PoliticsCatalog&,const MilitaryCatalog&,const PoliticalTakeoverCatalog&,bool enabled,const StableId& grant_id,const ResourceKey&);
}
