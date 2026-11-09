// 只枚举原经济表里的实际责任，已付历史不重开；活动义务与生产库不能随私产清空。
#include "private_account_dependencies.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "fiscal/occupation_fiscal_settlement.hpp"
#include "relief/public_relief_rules.hpp"
#include <set>
#include "../estate/employment/estate_employment.hpp"
namespace hanmo::resident {
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(const CommonStore& common,const LocalEconomyState* economy,const LocalEconomyCatalog* catalog,const FiscalSettlementState* fiscal,const OccupationFiscalSettlementState* occupation,const PublicReliefState* relief,const EconomicPrivateAccountSelection& selection){return read_economic_private_account_dependencies(common,economy,catalog,fiscal,occupation,relief,selection,nullptr);}
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(const CommonStore& common,const LocalEconomyState* economy,const LocalEconomyCatalog* catalog,const FiscalSettlementState* fiscal,const OccupationFiscalSettlementState* occupation,const PublicReliefState* relief,const EconomicPrivateAccountSelection& selection,const EstateEmploymentState* employment){return read_economic_private_account_dependencies(common,economy,catalog,fiscal,occupation,relief,selection,employment,nullptr);}
Outcome<EconomicPrivateAccountDependencies> read_economic_private_account_dependencies(const CommonStore& common,const LocalEconomyState* economy,const LocalEconomyCatalog* catalog,const FiscalSettlementState* fiscal,const OccupationFiscalSettlementState* occupation,const PublicReliefState* relief,const EconomicPrivateAccountSelection& selection,const EstateEmploymentState* employment,const PrivateShopState* shops){
 using Result=Outcome<EconomicPrivateAccountDependencies>;auto fail=[&](const StableId& code,const StableId& id=StableId{})->Result{return {false,{},common_failure(code,id).error};};
 if(selection.actor_id.empty()||!common.actor(selection.actor_id)||(selection.wallet_ids.empty()&&selection.inventory_ids.empty()))return fail("InvalidEconomicPrivateSelection");
 const Owner original{Owner::Kind::actor,selection.actor_id,{}};std::set<StableId> wallets,inventories;
 for(const auto& id:selection.wallet_ids){const auto* value=common.wallet(id);if(!value||!wallets.insert(id).second||!owner_equal(value->owner,original))return fail("EconomicPrivateWalletOwnerMismatch",id);}
 for(const auto& id:selection.inventory_ids){const auto* value=common.inventory(id);if(!value||!inventories.insert(id).second||!owner_equal(value->owner,original))return fail("EconomicPrivateInventoryOwnerMismatch",id);}
 if(bool(economy)!=bool(catalog)||((fiscal||occupation||relief)&&!economy)||(economy&&(!catalog->enabled||economy->last_step_tick!=common.tick())))return fail("EconomicPrivateDependencyContextMismatch");
 if(employment){if(!economy||employment->version!=1||employment->last_step_tick!=common.tick())return fail("EconomicPrivateEmploymentContextMismatch");}else for(const auto& fact:common.state().facts.rows())if(fact.second.domain==estate_employment_module)return fail("EconomicPrivateEmploymentContextRequired");
 EconomicPrivateAccountDependencies out;out.source_identity=common.state().identity;out.actor_id=selection.actor_id;
 auto shop_dependencies=read_private_shop_private_asset_responsibilities(common,shops,{selection.actor_id,selection.wallet_ids,selection.inventory_ids});if(!shop_dependencies.ok)return {false,{},shop_dependencies.error};for(const auto& blocker:shop_dependencies.value.blockers)out.blockers.push_back({blocker.kind,blocker.source_id,0});
 auto add=[&](const StableId& kind,const StableId& id,Integer amount=0){out.blockers.push_back({kind,id,amount});};
 if(employment)for(const auto& row:employment->contracts.rows()){const auto& c=row.second;if((wallets.count(c.terms.payer_wallet_id)||wallets.count(c.terms.payee_wallet_id))&&(c.status=="offered"||c.status=="active"))add("employment_contract",c.id);}
 if(economy){
  for(const auto& row:economy->production_units.rows()){const auto& unit=row.second;bool selected=inventories.count(unit.output_inventory_id);for(const auto& id:unit.input_inventory_ids)selected|=inventories.count(id)!=0;if(selected)add("production_unit",row.first);}
  std::set<StableId> obligations;
  for(const auto& row:economy->obligations.rows()){const auto& obligation=row.second;const auto& accounts=obligation.kind==ObligationKind::ration?inventories:wallets;if(accounts.count(obligation.source_wallet_or_inventory)||accounts.count(obligation.destination)){obligations.insert(row.first);if(obligation.active)add("active_obligation",row.first);}}
  for(const auto& row:economy->arrears.rows())if(obligations.count(row.second.obligation_id)){const auto& debt=row.second;if(debt.original_due<0||debt.paid_amount<0||debt.paid_amount>debt.original_due)return fail("InvalidEconomicPrivateArrear",row.first);if(debt.paid_amount<debt.original_due)add("unpaid_obligation",row.first,debt.original_due-debt.paid_amount);}
  for(const auto& row:economy->provision_bindings.rows())if(inventories.count(row.second.payer_inventory)&&row.second.through_tick>=common.tick())add("provision_binding",row.first);
  for(const auto& row:catalog->supply_sources.rows())if(inventories.count(row.second.inventory_id))add("supply_source",row.first);
 }
 // actor不能成为正常税源；仍核显式子域的容器关联，防止漏掉历史收付款引用。
 auto debt=[&](const FiscalSourceAssessment& a,const FiscalPolicySnapshot& policy,const StableId& kind)->Status{
  for(const auto* value:{&a.cash,&a.grain})if(value->original_due<0||value->paid_amount<0||value->paid_amount>value->original_due)return common_failure("InvalidEconomicPrivateFiscalArrear",a.id);
  if((wallets.count(a.wallet_id)||wallets.count(policy.public_wallet_id))&&a.cash.paid_amount<a.cash.original_due)add(kind,a.cash.id,a.cash.original_due-a.cash.paid_amount);
  if((inventories.count(a.inventory_id)||inventories.count(policy.public_inventory_id))&&a.grain.paid_amount<a.grain.original_due)add(kind,a.grain.id,a.grain.original_due-a.grain.paid_amount);return common_success();
 };
 if(fiscal){if(fiscal->version!=1||fiscal->last_step_tick!=common.tick())return fail("EconomicPrivateFiscalContextMismatch");for(const auto& row:fiscal->assessments.rows()){const auto* p=fiscal->periods.find(row.second.period_id);if(!p||!p->policy)return fail("InvalidEconomicPrivateFiscalPeriod",row.first);auto checked=debt(row.second,*p->policy,"fiscal_assessment");if(!checked.ok)return {false,{},checked.error};}}
 if(occupation){if(occupation->version!=1||occupation->last_step_tick!=common.tick())return fail("EconomicPrivateFiscalContextMismatch");for(const auto& row:occupation->assessments.rows()){const auto* p=occupation->periods.find(row.second.assessment.period_id);if(!p)return fail("InvalidEconomicPrivateFiscalPeriod",row.first);auto checked=debt(row.second.assessment,p->policy,"occupation_fiscal_assessment");if(!checked.ok)return {false,{},checked.error};}}
 if(relief){if(relief->version!=1)return fail("EconomicPrivateReliefContextMismatch");for(const auto& row:relief->commitments.rows()){const auto& terms=row.second.terms;bool selected=wallets.count(terms.public_wallet_id)||inventories.count(terms.public_inventory_id);for(const auto& item:row.second.transfers)selected|=wallets.count(item.wallet_id)||inventories.count(item.inventory_id);if(selected)add("relief_commitment",terms.id);}}
 return {true,std::move(out),{}};
}
}
