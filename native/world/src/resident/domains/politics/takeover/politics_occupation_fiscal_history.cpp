// 税政回执指向本次真实政策事实，签发者、时刻和全局分配区间逐项闭合。
#include "politics_occupation_fiscal_profile.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
Status validate_occupation_fiscal_receipts(const OccupationFiscalContext& x,const OccupationFiscalReceiptReader& read){
    auto valid=validate_occupation_fiscal(x);if(!valid.ok)return valid;
    const auto& common=x.takeover.common;std::set<StableId> seen;
    for(const auto& row:common.state().results.rows()){
        if(!row.second)continue;const auto& result=*row.second;const auto* effects=read?read(result):nullptr;
        if(result.kind!=political_occupation_fiscal_command_kind){if(effects)return common_failure("OccupationFiscalReceiptMismatch",row.first);continue;}
        if(!effects||!x.catalog.enabled)return common_failure("OccupationFiscalReceiptMismatch",row.first);
        const auto split=row.first.rfind(':');if(split==StableId::npos)return common_failure("OccupationFiscalReceiptMismatch",row.first);
        const auto actor=row.first.substr(0,split);const auto sequence=organization_instance_sequence(row.first,actor);const auto* last=common.state().request_sequences.find(actor);
        if(!sequence||!last||*sequence>*last)return common_failure("OccupationFiscalReceiptMismatch",row.first);
        const auto* record=x.state.policies.find(effects->policy_fact_id);
        if(!record||record->authority.actor_id!=actor||record->policy.issued_tick!=result.tick||record->policy.policy_revision!=effects->policy_revision||record->policy.effective_from_tick!=effects->effective_from_tick)return common_failure("OccupationFiscalReceiptMismatch",row.first);
        const HistoryRecord* history=nullptr;const HistoryRecord* before=nullptr;const HistoryRecord* previous=nullptr;
        for(const auto& item:common.state().history){if(item.result_id==result.result_id){if(history)return common_failure("OccupationFiscalReceiptMismatch",row.first);history=&item;before=previous;}previous=&item;}
        if(!history||history->actor_id!=actor||history->kind!=result.kind||history->tick!=result.tick)return common_failure("OccupationFiscalReceiptMismatch",row.first);
        const auto upper=organization_instance_sequence(result.result_id,"result"),lower=before?organization_instance_sequence(before->result_id,"result"):std::optional<Integer>{0},policy=organization_instance_sequence(record->id,political_occupation_fiscal_id_prefix);
        if(!upper||!lower||!policy||*policy<=*lower||*policy>=*upper||!seen.insert(record->id).second)return common_failure("OccupationFiscalReceiptAllocationMismatch",row.first);
    }
    return common_success();
}
}
