// 显式NPC来源只关联原代理事实，不能代替原Office/Grant/接受窗口与治理批准的完整先验。
#pragma once
#include "political_governance_agency_intents.hpp"
namespace hanmo::resident {
// 仅内部Director可构造；外部命令decoder仍只生成原玩家variant，不能从参数传入作者来源。
struct AuthoredGovernanceAgencyCommand {PoliticalGovernanceAgencyAuthoredOrigin origin;PoliticalGovernanceAgencyCommand command;};
// 本读只用于agency2的新来源：旧agency1从不调用，不存在“缺player回执即视为NPC”的路径。
Status validate_political_governance_agency_authored_source(const CommonStore&,const PoliticalGovernanceAgencyState&,const PoliticalGovernanceAgencyIntentCatalog&,const PoliticalGovernanceAgencyAuthoredOrigin&,const StableId& actual_source_fact,const PoliticalSuccessionPlayerRead&);
// 同一次原命令热门必须绑定作者本人的明确选择；它不更改Common metadata或替换实际actor。
Status validate_political_governance_agency_authored_command(const CommonStore&,const PoliticalGovernanceAgencyState&,const PoliticalGovernanceAgencyIntentCatalog&,const PoliticalGovernanceAgencyAuthoredOrigin&,const StableId& actor,const PoliticalGovernanceAgencyCommand&);
}
