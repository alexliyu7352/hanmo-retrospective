// 原作者绑定和保留事实序构成历史负证明；不用今天active/paid或余额回填过去。
#include "private_account_history.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "fiscal/occupation_fiscal_settlement.hpp"
#include "relief/public_relief_rules.hpp"
#include <charconv>
#include <map>
#include <set>
#include "../estate/employment/estate_employment.hpp"
namespace hanmo::resident {
namespace {
// 当前预检与既有交割历史共用扫描规则，唯一差异是当前尾界没有交割头。
Outcome<EconomicPrivateHistoricalClearance> history(const EconomicPrivateHistoryContext& x,const EconomicPrivateAccountSelection& selected,Integer tick,const StableId& boundary,bool preview){
 using Result=Outcome<EconomicPrivateHistoricalClearance>;auto unavailable=[&](const StableId& id=StableId{})->Result{return {false,{},common_failure("EconomicHistoricalClearanceUnavailable",id,ErrorKind::unsupported_capability).error};};
 const auto& c=x.common;const auto* actor=c.actor(selected.actor_id);if(!actor||tick<0||tick>c.tick()||(selected.wallet_ids.empty()&&selected.inventory_ids.empty()))return unavailable(boundary);
 if(!preview){const StableId prefix="family_inheritance:";Integer instance=0;if(boundary.rfind(prefix,0)!=0)return unavailable(boundary);const auto* first=boundary.data()+prefix.size();auto parsed=std::from_chars(first,boundary.data()+boundary.size(),instance);const auto* head=c.state().facts.find(boundary);
 if(parsed.ec!=std::errc{}||parsed.ptr!=boundary.data()+boundary.size()||instance<1||instance>=c.state().next_instance_id||boundary!=prefix+std::to_string(instance)||!head||head->id!=boundary||head->kind!="family_inheritance_settled"||head->domain!="family_inheritance")return unavailable(boundary);}
 std::map<StableId,std::size_t> order;for(std::size_t i=0;i<c.state().facts.size();++i)order.emplace(c.state().facts.rows()[i].first,i);const auto limit=preview?c.state().facts.size():order.at(boundary);
 const Owner original{Owner::Kind::actor,selected.actor_id,{}};std::set<StableId> wallets,inventories;
 // 只支持作者明确的本人私容器；动态出生或其他来源账户需后继具名原产权证明。
 for(const auto& id:selected.wallet_ids){const auto* origin=x.common_catalog.required_wallets.find(id);if(!origin||!owner_equal(origin->owner,original)||!wallets.insert(id).second||!c.wallet(id))return unavailable(id);}
 for(const auto& id:selected.inventory_ids){const auto* origin=x.common_catalog.required_inventories.find(id);if(!origin||!owner_equal(origin->owner,original)||!inventories.insert(id).second||!c.inventory(id))return unavailable(id);}
 if(bool(x.economy)!=bool(x.economy_catalog)||bool(x.fiscal)!=bool(x.fiscal_catalog)||((x.fiscal||x.occupation||x.relief)&&!x.economy)||(x.occupation&&!x.fiscal)||(x.economy&&(!x.economy_catalog->enabled||x.economy->last_step_tick!=c.tick())))return unavailable("context");
 if(x.employment){if(!x.economy||x.employment->version!=1||x.employment->last_step_tick!=c.tick())return unavailable("employment_context");}else for(const auto& fact:c.state().facts.rows())if(fact.second.domain==estate_employment_module)return unavailable("employment_context");
 PrivateShopPrivateAssetScope shop_scope{selected.actor_id,selected.wallet_ids,selected.inventory_ids};auto shop_history=preview?read_private_shop_private_asset_responsibilities(c,x.shops,shop_scope):read_private_shop_private_asset_responsibilities_before_inheritance(c,x.shops,shop_scope,boundary,tick);if(!shop_history.ok)return {false,{},shop_history.error};if(!shop_history.value.blockers.empty())return unavailable(shop_history.value.blockers.front().source_id);
 bool broken=false;
 // 同tick仍按真实Common顺序切开；未来源不得伪称过去，缺头/时间矛盾不被当作无关联。
 auto prior=[&](const StableId& id,Integer when,const StableId& kind,const StableId& domain=StableId{"local_economy"}){
  const auto* h=c.state().facts.find(id);const auto it=order.find(id);if(!h||h->id!=id||h->kind!=kind||h->domain!=domain||it==order.end()||when<0||when>c.tick()||(it->second<limit&&when>tick)||(it->second>limit&&when<tick)){broken=true;return false;}return it->second<limit;
 };
 auto accounts=[&](const StableId& wallet,const StableId& inventory){return wallets.count(wallet)||inventories.count(inventory);};
 auto obligation_selected=[&](const RecurringObligation& value){const auto& ids=value.kind==ObligationKind::ration?inventories:wallets;return ids.count(value.source_wallet_or_inventory)||ids.count(value.destination);};
 auto unit_selected=[&](const ProductionUnit& value){bool found=inventories.count(value.output_inventory_id);for(const auto& id:value.input_inventory_ids)found|=inventories.count(id)!=0;return found;};
 // 新邀约之后发生的旧交割不会被当前合同倒填；在交割之前出现过关联则保守拒未知清算史。
 if(x.employment)for(const auto& row:x.employment->contracts.rows()){const auto& contract=row.second;if((wallets.count(contract.terms.payer_wallet_id)||wallets.count(contract.terms.payee_wallet_id))&&prior(row.first,contract.offered_tick,"estate_employment_offered",estate_employment_module))return unavailable(row.first);}
 if(x.economy){const auto& catalog=*x.economy_catalog;const auto& state=*x.economy;
  for(const auto& row:catalog.obligations.rows())if(obligation_selected(row.second))return unavailable(row.first);
  for(const auto& row:catalog.production_units.rows())if(unit_selected(row.second))return unavailable(row.first);
  for(const auto& row:catalog.provision_bindings.rows())if(inventories.count(row.second.payer_inventory))return unavailable(row.first);
  for(const auto& row:catalog.supply_sources.rows())if(inventories.count(row.second.inventory_id))return unavailable(row.first);
  for(const auto& row:catalog.households.rows())if(accounts(row.second.wallet_id,row.second.inventory_id))return unavailable(row.first);
  for(const auto& row:catalog.cohorts.rows())if(accounts(row.second.private_wallet_id,row.second.private_inventory_id))return unavailable(row.first);
  // 当前无合法改库命令；人口接管只换责任而强制旧新payer同容器。未来改库须增具名历史源。
  for(const auto& row:state.production_units.rows())if(unit_selected(row.second))return unavailable(row.first);
  for(const auto& row:state.provision_bindings.rows())if(inventories.count(row.second.payer_inventory))return unavailable(row.first);
  for(const auto& row:state.obligations.rows())if(obligation_selected(row.second)){
   const EstateEmploymentContract* employment=nullptr;if(x.employment)for(const auto& item:x.employment->contracts.rows())if(item.second.obligation_id==row.first){if(employment)return unavailable(row.first);employment=&item.second;}
   if(employment){if(employment->terms.payer_wallet_id!=row.second.source_wallet_or_inventory||employment->terms.payee_wallet_id!=row.second.destination||employment->acceptance_fact_id.empty())return unavailable(row.first);if(prior(employment->id,employment->offered_tick,"estate_employment_offered",estate_employment_module))return unavailable(row.first);continue;}
   const MembershipWageBinding* binding=nullptr;for(const auto& item:state.membership_wage_bindings.rows())if(item.second.obligation_id==row.first){if(binding)return unavailable(row.first);binding=&item.second;}
   if(!binding||binding->payer_wallet_id!=row.second.source_wallet_or_inventory||binding->payee_wallet_id!=row.second.destination)return unavailable(row.first);
   if(prior(binding->decision_id,binding->joined_tick,"membership_decided","organization"))return unavailable(row.first);
  }
  for(const auto& row:state.payments.rows())if(wallets.count(row.second.from_wallet_id)||wallets.count(row.second.to_wallet_id))if(prior(row.first,row.second.tick,"obligation_payment"))return unavailable(row.first);
  for(const auto& row:state.rations.rows())if(inventories.count(row.second.inventory_id)&&prior(row.first,row.second.tick,"local_ration"))return unavailable(row.first);
  for(const auto& row:state.production_receipts.rows()){bool found=false;for(const auto* group:{&row.second.inputs,&row.second.outputs,&row.second.unrealized_output})for(const auto& item:*group)found|=inventories.count(item.inventory_id)!=0;if(found&&prior(row.first,row.second.tick,"local_production"))return unavailable(row.first);}
  for(const auto& row:state.supply_registrations.rows())if(inventories.count(row.second.inventory_id)&&prior(row.first,row.second.tick,"local_supply_source"))return unavailable(row.first);
  for(const auto& row:state.supply_observations.rows())if(inventories.count(row.second.observation.inventory_id)&&prior(row.first,row.second.tick,"local_supply_observation"))return unavailable(row.first);
 }
 if(x.fiscal_catalog)for(const auto& district:x.fiscal_catalog->districts.rows())for(const auto& row:district.second.sources.rows())if(accounts(row.second.wallet_id,row.second.inventory_id))return unavailable(row.first);
 if(x.fiscal){if(x.fiscal->version!=1||x.fiscal->last_step_tick!=c.tick())return unavailable("fiscal");for(const auto& row:x.fiscal->assessments.rows()){const auto& a=row.second;const auto* p=x.fiscal->periods.find(a.period_id);if(!p||!p->policy)return unavailable(row.first);if((accounts(a.wallet_id,a.inventory_id)||accounts(p->policy->public_wallet_id,p->policy->public_inventory_id))&&prior(row.first,p->due_tick,"fiscal_source_assessed"))return unavailable(row.first);}}
 if(x.occupation){if(x.occupation->version!=1||x.occupation->last_step_tick!=c.tick())return unavailable("occupation_fiscal");for(const auto& row:x.occupation->assessments.rows()){const auto& a=row.second.assessment;const auto* p=x.occupation->periods.find(a.period_id);if(!p)return unavailable(row.first);if((accounts(a.wallet_id,a.inventory_id)||accounts(p->policy.public_wallet_id,p->policy.public_inventory_id))&&prior(row.first,p->due_tick,"occupation_fiscal_source_assessed"))return unavailable(row.first);}}
 if(x.relief){if(x.relief->version!=1)return unavailable("relief");for(const auto& row:x.relief->commitments.rows()){const auto& value=row.second;bool found=accounts(value.terms.public_wallet_id,value.terms.public_inventory_id);for(const auto& item:value.transfers)found|=accounts(item.wallet_id,item.inventory_id);if(found&&prior(value.terms.id,value.reserved_tick,"public_relief_reserved"))return unavailable(row.first);}}
 auto estate_valid=validate_estate(c,x.estate,x.estate_catalog,x.common_catalog,x.estate_access);if(!estate_valid.ok)return {false,{},estate_valid.error};
 for(const auto& row:c.state().actions.rows())if(row.second.kind=="estate_build"&&!x.estate.actions.find(row.first))return unavailable(row.first);
 for(const auto& row:x.estate_catalog.estates.rows())if(accounts(row.second.wallet_id,row.second.inventory_id))return unavailable(row.first);
 for(const auto& row:x.estate.facts.rows()){
  if(const auto* value=std::get_if<EstateConstructionFact>(&row.second)){if((accounts(value->terms.payer_wallet_id,value->terms.inventory_id)||wallets.count(value->terms.payee_wallet_id))&&prior(row.first,value->start_tick,"estate_construction","estate"))return unavailable(row.first);}
  if(const auto* value=std::get_if<EstateManagementFact>(&row.second)){bool found=value->cash&&(wallets.count(value->cash->from_wallet_id)||wallets.count(value->cash->to_wallet_id));found|=value->transfer&&(inventories.count(value->transfer->from_inventory_id)||inventories.count(value->transfer->to_inventory_id));if(found&&prior(row.first,value->tick,value->kind,"estate"))return unavailable(row.first);}
 }
 if(broken)return unavailable("source_boundary");return {true,{c.state().identity,selected.actor_id,boundary,tick},{}};
}
}
// 报价使用现在已经存在的全部事实；不产生Common写入或预测ID。
Outcome<EconomicPrivateHistoricalClearance> preview_economic_private_account_historical_clearance(const EconomicPrivateHistoryContext& x,const EconomicPrivateAccountSelection& selection){return history(x,selection,x.common.tick(),{},true);}
// 历史必须给出已真实追加的交割头，不能以当前预检结果替代过去事实。
Outcome<EconomicPrivateHistoricalClearance> read_economic_private_account_historical_clearance(const EconomicPrivateHistoryContext& x,const EconomicPrivateAccountSelection& selection,Integer tick,const StableId& before){return history(x,selection,tick,before,false);}
}
