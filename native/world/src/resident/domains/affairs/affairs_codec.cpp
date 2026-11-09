// 作者映射只在冷边界解析一次；未知验收口径和取消政策明确拒绝。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "affairs_codec.hpp"
#include "affairs_projection.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "state_need_catalog.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp"
namespace hanmo::resident {
Outcome<AffairsCatalog> decode_affairs_catalog(const BoundaryValue& input){try{using namespace boundary;fields(input,{"need_policies"},{"obligation_needs_version","obligation_policies","fiscal_need_policies","cohort_wage_need_policies","state_need_policies"},"affairs");AffairsCatalog catalog;
if(const auto* state=input.find("state_need_policies")){auto decoded=decode_state_need_catalog(*state);if(!decoded.ok)return {false,{},decoded.error};catalog.state_need_policies=checkpoint_make_shared<const StateNeedCatalog>(std::move(decoded.value));}
if(const auto* wages=input.find("cohort_wage_need_policies")){auto decoded=decode_cohort_wage_need_catalog(*wages);if(!decoded.ok)return {false,{},decoded.error};catalog.cohort_wage_need_policies=checkpoint_make_shared<const CohortWageNeedCatalog>(std::move(decoded.value));}
if(const auto* fiscal=input.find("fiscal_need_policies")){auto decoded=decode_fiscal_need_catalog(*fiscal);if(!decoded.ok)return {false,{},decoded.error};catalog.fiscal_need_policies=checkpoint_make_shared<const FiscalNeedCatalog>(std::move(decoded.value));}
if(input.find("obligation_needs_version")){
    if(get_int(input,"obligation_needs_version","affairs")!=1)fail("UnsupportedObligationNeedsVersion","affairs");catalog.obligation_needs=true;
    for(const auto& row:object(field(input,"obligation_policies","affairs"),"obligation_policies")){fields(row.second,{"id","obligation_id","organization_id","semantic_key"},{},row.first);ObligationNeedPolicy policy{get_string(row.second,"id",row.first),get_string(row.second,"obligation_id",row.first),get_string(row.second,"organization_id",row.first),get_string(row.second,"semantic_key",row.first)};if(row.first!=policy.id||!catalog.obligation_policies.insert(row.first,std::move(policy)))fail("InvalidObligationNeedPolicy",row.first);}
}else if(input.find("obligation_policies"))fail("ObligationNeedsCapabilityRequired","affairs");
for(const auto& row:object(field(input,"need_policies","affairs"),"need_policies")){const auto& v=row.second;fields(v,{"id","source_domain","source_key","semantic_key","organization_id","site_id","inventory_id","item_id","unit","template_id","terms_version","funding_wallet_id","reward_wallet_id","measure","max_claim_quantity","purchase_budget_per_unit","reward_per_unit","deadline_ticks","accept_role","report_role","cancellation_policy_id"},{"reward_total"},row.first);NeedPolicy p;p.id=get_string(v,"id",row.first);p.source_domain=get_string(v,"source_domain",row.first);p.source_key=get_string(v,"source_key",row.first);p.semantic_key=get_string(v,"semantic_key",row.first);p.organization_id=get_string(v,"organization_id",row.first);p.site_id=get_string(v,"site_id",row.first);p.inventory_id=get_string(v,"inventory_id",row.first);p.item_id=get_string(v,"item_id",row.first);p.unit=get_string(v,"unit",row.first);p.template_id=get_string(v,"template_id",row.first);p.terms_version=get_string(v,"terms_version",row.first);p.funding_wallet_id=get_string(v,"funding_wallet_id",row.first);p.reward_wallet_id=get_string(v,"reward_wallet_id",row.first);p.max_claim_quantity=get_int(v,"max_claim_quantity",row.first);p.purchase_budget_per_unit=get_int(v,"purchase_budget_per_unit",row.first);p.reward_per_unit=get_int(v,"reward_per_unit",row.first);p.deadline_ticks=get_int(v,"deadline_ticks",row.first);p.accept_role=get_string(v,"accept_role",row.first);p.report_role=get_string(v,"report_role",row.first);p.cancellation_policy_id=get_string(v,"cancellation_policy_id",row.first);if(const auto* reward=v.find("reward_total"))p.reward_total=integer(*reward,row.first+"/reward_total");const auto measure=get_string(v,"measure",row.first);if(measure=="stock_gap")p.measure=NeedMeasure::stock_gap;else if(measure=="cumulative_delivery")p.measure=NeedMeasure::cumulative_delivery;else fail("UnknownNeedMeasure",row.first);if(p.id!=row.first||!catalog.policies.insert(row.first,std::move(p)))fail("DuplicateNeedPolicy",row.first);}auto checked=make_initial_affairs(catalog);if(!checked.ok)return {false,{},checked.error};return {true,std::move(catalog),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<AffairsState> decode_affairs_state(const BoundaryValue& input,bool obligation_needs,bool fiscal_needs,bool cohort_wage_needs){AffairsState state;state.cohort_wage_needs=cohort_wage_needs;state.obligation_needs=obligation_needs;state.fiscal_needs=fiscal_needs;dynamic_codec::JsonArchive codec(input);auto status=affairs_codec::state(codec,state);if(status.ok)status=codec.finish();return status.ok?Outcome<AffairsState>{true,std::move(state),{}}:Outcome<AffairsState>{false,{},status.error};}
// 数量 Affairs 原条件字段沿同 selector 和同 state visitor，无整表副本。
Status emit_affairs_state_fields(ProjectionWriter& writer,const AffairsState& value){
    DynamicProjectionArchive archive(writer);return affairs_codec::state(archive,const_cast<AffairsState&>(value));
}
BoundaryValue encode_affairs_state(const AffairsState& value){
    BoundaryValue result;BoundaryProjectionWriter writer(result);
    auto checked=writer.object("",[&](auto& child){return emit_affairs_state_fields(child,value);});
    if(checked.ok)checked=writer.finish();
    if(!checked.ok){boundary::DecodeFailure error(checked.error.original.code,checked.error.original.path);error.error=checked.error;throw error;}
    return result;
}
} // 冷目录不产生运行需求；make_initial只核对规则登记。
