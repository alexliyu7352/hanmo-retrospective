// 命令参数只表达对已知责任的选择；世界身份与提交人来自原命令封套。
#pragma once
#include "politics_takeover_rules.hpp"
namespace hanmo::resident {
// 冷边界只解析具名选择；提交人及来源身份由原命令封套绑定，不从JSON授予。
Outcome<PoliticalTakeoverDecisionCommand> decode_political_takeover_command_params(const BoundaryValue&);
Outcome<PoliticalTakeoverDecisionCommand> decode_political_takeover_command(
    const BoundaryValue& params,const StableId& command_actor,const WorldIdentity& expected_identity);
BoundaryValue encode_political_takeover_command_params(const PoliticalTakeoverDecisionCommand&);
struct PoliticalTakeoverQuote {
    WorldIdentity source_identity;
    StableId actor_id,grant_id;
    PoliticalTakeoverDecisionCommand command;
};
BoundaryValue encode_political_takeover_quote(const PoliticalTakeoverQuote&);
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_command(const PoliticalTakeoverContext&,
    const StableId& actor,const PoliticalTakeoverDecisionCommand&,const WorldIdentity& expected_identity);
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_command(const PoliticalTakeoverContext&,
    const StableId& actor,const BoundaryValue& params,const WorldIdentity& expected_identity);
Outcome<StableId> commit_political_takeover_command_in_transaction(CommonStore&,PoliticalTakeoverState&,
    const PoliticalTakeoverContext&,const StableId& actor,const PoliticalTakeoverQuote&,PoliticalSupplyAdoptionState* supply_adoptions=nullptr,OrganizationRuntime* organization_runtime=nullptr);
}
