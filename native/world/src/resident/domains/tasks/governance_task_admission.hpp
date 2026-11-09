// 治理受领只消费真实S6当前批准及S5足额预留；玩家不提交规则金额、期限或完成事实。
#pragma once
#include "governance_task_rules.hpp"
#include "resident/domains/politics/politics_governance_contract.hpp"
#include "resident/domains/local_economy/relief/public_relief.hpp"
namespace hanmo::resident {
// 此报价没有分配ID或资产写入；S6负责现场辖区/site及Office/Grant的实际资格。
Outcome<GovernanceAuthorityApproval> preview_governance_acceptance(const CommonStore&,const GovernanceTaskState&,const GovernanceAuthorityRead&,const StableId& executor,const StableId& authority_fact_id,Integer expected_world_revision);
// 接受源先在未发布候选成立，S5据此建立真实条款和contract预留；完成后才开唯一行动。
using GovernanceReserveResources=std::function<Outcome<PublicReliefTerms>(const TaskGovernanceRead&,const GovernanceTaskAcceptance&,const GovernanceAuthorityApproval&)>;
Outcome<GovernanceTaskAcceptance> begin_governance_task(CommonStore&,TaskState&,GovernanceTaskState&,const CommonCatalog&,const GovernanceAuthorityRead&,const StableId& executor,const StableId& authority_fact_id,Integer expected_world_revision,const GovernanceReserveResources&);
}
