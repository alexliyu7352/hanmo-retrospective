// 原册结构读不反调法律签权或Shop资格；完整World另验每项原处分，防接册/原案互相递归证成。
#pragma once
#include "political_shop_registry_contract.hpp"
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
struct PoliticalShopRegistryLegalSources {
    StableId initial_registry_fact_id;
    std::vector<StableId> fact_ids;
};
// 当前只取已提交时钟；历史仅取实际political_shop_registry_accepted头前的精确全院源。
Outcome<PoliticalShopRegistryLegalSources> read_political_shop_registry_legal_sources(
    const CommonStore&,const PoliticalShopLicensingCatalog&,const PoliticalShopLicensingState&,
    const StableId& court_id,const StableId& acceptance_boundary={},std::optional<Integer> accepted_tick=std::nullopt);
}
