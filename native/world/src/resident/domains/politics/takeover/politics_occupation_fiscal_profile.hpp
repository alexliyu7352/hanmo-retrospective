// 新税政与实际评税段成对启用；旧回执不能借未启用模块留下政策事实。
#pragma once
#include "politics_occupation_fiscal.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <functional>
namespace hanmo::resident {
inline constexpr std::uint32_t occupation_fiscal_checkpoint_feature=8192u;
inline Status validate_occupation_fiscal_layout(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const OccupationFiscalState* state,std::optional<Integer> settlement_version={}){
    unsigned political=0,economic=0;
    for(const auto& item:modules)if(item.first=="political_occupation_fiscal"||item.first=="occupation_fiscal_settlement"){
        if(item.second!="1")return common_failure("UnsupportedOccupationFiscalVersion");
        if(++(item.first=="political_occupation_fiscal"?political:economic)!=1)return common_failure("DuplicateOccupationFiscalModule");
    }
    if(!enabled)return political||economic||state||settlement_version?common_failure("UnexpectedOccupationFiscalProfile"):common_success();
    if(political!=1||economic!=1||!state||!settlement_version)return common_failure("MissingOccupationFiscalState");
    return state->version==1&&*settlement_version==1?common_success():common_failure("UnsupportedOccupationFiscalVersion");
}
inline Status validate_occupation_fiscal_command_history(const CommonStore& common,bool enabled){
    if(enabled)return common_success();
    for(const auto& item:common.state().facts.rows())if(item.second.kind==political_occupation_fiscal_fact_kind)return common_failure("OccupationFiscalProfileRequired",item.first);
    for(const auto& item:common.state().history)if(item.kind==political_occupation_fiscal_command_kind)return common_failure("OccupationFiscalProfileRequired",item.result_id);
    for(const auto& item:common.state().results.rows())if(item.second&&item.second->kind==political_occupation_fiscal_command_kind)return common_failure("OccupationFiscalProfileRequired",item.first);
    return common_success();
}
using OccupationFiscalReceiptReader=std::function<const PoliticalFiscalEffects*(const CommitResult&)>;
Status validate_occupation_fiscal_receipts(const OccupationFiscalContext&,const OccupationFiscalReceiptReader&);
}
