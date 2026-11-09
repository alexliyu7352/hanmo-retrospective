#include "resident/domains/military/military_model.hpp"
// 报价不授永久权限；提交在同一候选重新检查本人、真实Grant和逐责任选择。
#include "politics_takeover_command.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_command(const PoliticalTakeoverContext& context,const StableId& actor,const PoliticalTakeoverDecisionCommand& parsed,const WorldIdentity& expected_identity){
    if(actor!=parsed.authority.actor_id)return {false,{},common_failure("PoliticalTakeoverActorMismatch").error};
    auto command=parsed;command.expected_identity=expected_identity;
    auto permission=preview_political_responsibility_decisions(context,actor,command);if(!permission.ok)return {false,{},permission.error};
    return {true,{context.common.state().identity,actor,permission.value,std::move(command)}, {}};
}
Outcome<PoliticalTakeoverQuote> prepare_political_takeover_command(const PoliticalTakeoverContext& context,const StableId& actor,const BoundaryValue& params,const WorldIdentity& expected_identity){
    auto command=decode_political_takeover_command_params(params);if(!command.ok)return {false,{},command.error};
    return prepare_political_takeover_command(context,actor,command.value,expected_identity);
}
Outcome<StableId> commit_political_takeover_command_in_transaction(CommonStore& common,PoliticalTakeoverState& state,const PoliticalTakeoverContext& context,const StableId& actor,const PoliticalTakeoverQuote& quote,PoliticalSupplyAdoptionState* supply_adoptions,OrganizationRuntime* organization_runtime){
    if(quote.actor_id!=actor||!takeover_detail::same_identity(quote.source_identity,common.state().identity)||!takeover_detail::same_identity(quote.command.expected_identity,quote.source_identity))return {false,{},common_failure("PoliticalTakeoverRevisionMismatch").error};
    auto current=preview_political_responsibility_decisions(context,actor,quote.command);if(!current.ok)return current;if(current.value!=quote.grant_id)return {false,{},common_failure("PoliticalTakeoverAuthorityChanged").error};
    return decide_political_responsibilities_in_transaction(common,state,context,actor,quote.command,supply_adoptions,organization_runtime);
}
}
