#include "treaty_aid_source_exposure.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
Outcome<TreatyAidSourceExposure> read_treaty_aid_source_exposure(
 const CommonStore& common,const TreatyTerm& term,const TreatyAidResourceSelection2& selection){try{
 auto valid=validate_treaty_aid_resource_selection2_for_term(selection,term);
 if(!valid.ok)return {false,{},valid.error};
 const auto* aid=std::get_if<TreatyMaterialAidTerm>(&term.terms);
 if(!aid)return {false,{},common_failure("TreatyMaterialAidTermRequired",term.term_id).error};
 auto original=read_material_aid_signing_exposure(*aid);if(!original.ok)return {false,{},original.error};
 TreatyAidSourceExposure out;
 auto resource=[&](ResourceKey::Kind kind,const StableId& id){
  if(std::none_of(out.source_resources.begin(),out.source_resources.end(),[&](const auto&r){return r.kind==kind&&r.id==id;}))out.source_resources.push_back({kind,id});
 };
 MoneyAuthorityAmount money;money.currency_id="qian";
 auto add_money=[&](Integer amount,const StableId& wallet)->bool{
  if(amount<0||money.amount>std::numeric_limits<Integer>::max()-amount)return false;
  money.amount+=amount;if(std::find(money.wallet_ids.begin(),money.wallet_ids.end(),wallet)==money.wallet_ids.end())money.wallet_ids.push_back(wallet);
  resource(ResourceKey::Kind::wallet,wallet);return true;
 };
 for(const auto& measure:original.value.ordinary){
  if(std::holds_alternative<BudgetMoneyQian>(measure.unit)){
   if(!selection.aid.money_route||!add_money(measure.all_occurrences,selection.aid.money_route->source_wallet_id))return {false,{},common_failure("TreatyAidSourceExposureOverflow",term.term_id).error};
  }else{
   const auto& item=std::get<BudgetItemQuantity>(measure.unit);
   const auto route=std::find_if(selection.aid.item_routes.begin(),selection.aid.item_routes.end(),[&](const auto&r){return r.item_id==item.item_id;});
   if(route==selection.aid.item_routes.end())return {false,{},common_failure("TreatyAidOriginalItemRouteRequired",item.item_id).error};
   out.amount_components.push_back(ItemAuthorityAmount{item.item_id,{route->source_inventory_id},measure.all_occurrences});
   resource(ResourceKey::Kind::inventory,route->source_inventory_id);
  }
 }
 if(original.value.bounded_compensation){
  if(!selection.compensation||!add_money(original.value.bounded_compensation->all_occurrences,selection.compensation->money_route.source_wallet_id))return {false,{},common_failure("TreatyAidSourceExposureOverflow",term.term_id).error};
 }
 // Keep an explicit zero-maximum selected wallet in the authorization resource
 // set. It is not a zero payable, but its selection must not escape authority.
 if(!money.wallet_ids.empty()){
  std::sort(money.wallet_ids.begin(),money.wallet_ids.end());out.amount_components.push_back(std::move(money));
 }
 std::sort(out.source_resources.begin(),out.source_resources.end(),[](const auto&a,const auto&b){return a.kind!=b.kind?a.kind<b.kind:a.id<b.id;});
 valid=validate_authority_amount_components(common,out.amount_components,out.source_resources,std::numeric_limits<Integer>::max());
 if(!valid.ok)return {false,{},valid.error};
 return {true,std::move(out),{}};
}catch(const std::bad_alloc&){return {false,{},common_failure("TreatyAidExposureResourceExhausted").error};}
catch(const std::length_error&){return {false,{},common_failure("TreatyAidExposureResourceExhausted").error};}
}
}
