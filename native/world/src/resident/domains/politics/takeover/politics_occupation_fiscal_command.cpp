#include "resident/domains/military/military_model.hpp"
// 预览没有签发副作用；当前Grant变化也不能借旧报价继续签税。
#include "politics_occupation_fiscal_command.hpp"
#include "politics_occupation_fiscal_detail.hpp"
namespace hanmo::resident {
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_command(const OccupationFiscalContext& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetOccupationTaxCommand& command){
    auto valid=preview_political_set_occupation_tax(x,activity,actor,command);if(!valid.ok)return {false,{},valid.error};
    const auto& registration=*x.takeover.state.registrations.find(command.registration_fact_id);const auto& rule=*x.takeover.catalog.rules.find(registration.rule_id);const auto& jurisdiction=*x.politics.jurisdictions.find(command.jurisdiction_id);const auto* grant=occupation_fiscal_detail::current_grant(x,command,registration,rule,jurisdiction);
    return {true,{x.takeover.common.state().identity,actor,grant->id,command,0},{}};
}
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_command_in_transaction(CommonStore& common,OccupationFiscalState& state,const OccupationFiscalContext& x,const ActivityRead& activity,const StableId& actor,const PoliticalSetOccupationTaxCommand& command,const OccupationFiscalQuote& quote){
    if(actor!=quote.actor_id||quote.command.authority.actor_id!=actor||quote.duration!=0||!takeover_detail::same_identity(quote.source_identity,common.state().identity)||!compare_boundary(encode_political_set_occupation_tax(command),encode_political_set_occupation_tax(quote.command)).exact())return {false,{},common_failure("StaleOccupationFiscalQuote").error};
    auto current=prepare_occupation_fiscal_command(x,activity,actor,quote.command);if(!current.ok)return {false,{},current.error};if(current.value.grant_id!=quote.grant_id)return {false,{},common_failure("OccupationFiscalAuthorityChanged").error};
    return apply_political_set_occupation_tax_in_transaction(common,state,x,activity,actor,quote.command);
}
BoundaryValue encode_occupation_fiscal_quote(const OccupationFiscalQuote& quote){
    using V=BoundaryValue;StableId hex;static constexpr const char* digits="0123456789abcdef";for(auto byte:quote.source_identity.world_id){hex+=digits[byte>>4];hex+=digits[byte&15];}
    auto identity=boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{quote.source_identity.execution_epoch}},{"revision",V{quote.source_identity.revision}},{"tick",V{quote.source_identity.tick}}});
    return boundary::obj({{"source_identity",std::move(identity)},{"actor_id",V{quote.actor_id}},{"grant_id",V{quote.grant_id}},{"command_kind",V{political_occupation_fiscal_command_kind}},{"command",encode_political_set_occupation_tax(quote.command)},{"duration",V{quote.duration}}});
}
}
