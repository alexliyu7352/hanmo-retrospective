// 组织税源的最小产权证据；经济消费它暂停原纳税责任，不导入完整政治规则。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct OccupationFiscalTaxpayerContainer {StableId id;Owner owner;std::vector<StableId> occupation_fact_ids;};
struct OccupationFiscalTaxpayerSource {
    WorldIdentity source_identity;Owner original_taxpayer;Integer due_tick=0;
    OccupationFiscalTaxpayerContainer wallet,inventory;
};
class OccupationFiscalTaxpayerRead {
public:
    virtual ~OccupationFiscalTaxpayerRead()=default;
    // 边界仅限新暂停期、新户债或新付款具名事实；传入tick由经济对应原载荷二验。
    virtual Outcome<OccupationFiscalTaxpayerSource> occupation_fiscal_taxpayer_source(const Owner& original,const StableId& wallet,const StableId& inventory,Integer due_tick,const std::optional<StableId>& boundary_fact_id={})const=0;
};
}
