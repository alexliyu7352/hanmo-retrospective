#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
// 此索引允许持有全域来源键，query只能通过本人已授权的assessment行取出对应引用。
#include "resident/domains/local_economy/fiscal/fiscal_need_view.hpp"
namespace hanmo::resident {
// 不依据余额或观察页重新生成Need，旧工资事项也不在此映射。
void bind_fiscal_need_references(FiscalSettlementIndex& index,const AffairsState& affairs){
    index_memory::clear(index.text_memory,index.need_by_arrear);
    for(const auto& row:affairs.needs.rows()){
        const auto& need=row.second;if(need.source_domain!="fiscal_settlement")continue;
        StableId status;switch(need.status){case NeedStatus::open:status="open";break;case NeedStatus::partial:status="partial";break;case NeedStatus::fulfilled:status="fulfilled";break;default:status="invalid";break;}
        if(index.need_by_arrear.find(need.arrear_id)==index.need_by_arrear.end()){auto key=index_memory::copy(index.text_memory,need.arrear_id);FiscalNeedReference value{index_memory::copy(index.text_memory,need.id),index_memory::copy(index.text_memory,need.source_fact_id),index_memory::copy(index.text_memory,status),need.revision};index.need_by_arrear.emplace(std::move(key),std::move(value));}
    }
}
}
