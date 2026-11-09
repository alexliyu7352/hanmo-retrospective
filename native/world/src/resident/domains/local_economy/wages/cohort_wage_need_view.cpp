#include "resident/checkpoint/checkpoint_economic_index_copy.hpp"
// 全域映射仅供内部按已授权欠据ID关联，分页计数和游标仍由原合法源序列决定。
#include "cohort_wage_need_view.hpp"
namespace hanmo::resident {
// 已结清事项保同一身份，页面无需重查全表或推测钱包余额。
void bind_cohort_wage_need_references(LocalEconomyIndex& index,const AffairsState& affairs){
    index_memory::clear(index.text_memory,index.cohort_wage_need_by_arrear);
    for(const auto& row:affairs.needs.rows()){
        const auto& need=row.second;if(need.source_domain!="cohort_wages")continue;
        StableId status;switch(need.status){case NeedStatus::open:status="open";break;case NeedStatus::partial:status="partial";break;case NeedStatus::fulfilled:status="fulfilled";break;default:status="invalid";break;}
        if(index.cohort_wage_need_by_arrear.find(need.arrear_id)==index.cohort_wage_need_by_arrear.end()){auto key=index_memory::copy(index.text_memory,need.arrear_id);CohortWageNeedReference value{index_memory::copy(index.text_memory,need.id),index_memory::copy(index.text_memory,need.source_fact_id),index_memory::copy(index.text_memory,status),need.revision};index.cohort_wage_need_by_arrear.emplace(std::move(key),std::move(value));}
    }
}
}
