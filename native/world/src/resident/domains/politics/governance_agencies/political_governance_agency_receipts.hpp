// 首片四个玩家命令永久绑定真正提交结果，裁剪缓存后也不能借旧同tick来源冒新办理。
#pragma once
#include "political_governance_agency_codec.hpp"
namespace hanmo::resident {
// World必须核实际成功PoliticsEffects且fact恰为source；代理签发另核原唯一治理批准effect。
using PoliticalGovernanceAgencyResultMatch=std::function<bool(const CommitResult&,const StableId& source_fact_id,const StableId& authority_fact_id)>;
Outcome<StableId> political_governance_agency_new_player_source(const PoliticalGovernanceAgencyState& before,const PoliticalGovernanceAgencyState& after,const StableId& actor,const StableId& command_kind,Integer tick);
Status bind_political_governance_agency_player_result(const CommonStore&,PoliticalGovernanceAgencyState&,const StableId& source_fact_id,const StableId& actor,Integer request,const CommitResult&,const PoliticalGovernanceAgencyResultMatch&);
// 本口位于原Family控制/政治全史之后；不代替各业务来源和历史本人验证。
Status validate_political_governance_agency_player_results(const CommonStore&,const PoliticalGovernanceAgencyState&,const PoliticalGovernanceAgencyResultMatch&);
}
