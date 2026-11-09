// 完整法律史先核，缓存再绑定本次演员、时刻与实际全局分配区间；新业务不复用旧许可。
#include "political_shop_licensing_profile.hpp"
#include "political_shop_licensing_detail.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_shop_licensing_receipts(const PoliticalShopLicensingContext& x,const std::function<bool(const CommitResult&)>& is_done){
 const auto& common=x.common.state();std::set<StableId> used;
 for(const auto& cached:common.results.rows()){
  if(!cached.second||!political_shop_licensing_command_kind(cached.second->kind))continue;const auto& result=*cached.second;
  const auto split=cached.first.rfind(':');if(split==StableId::npos||!is_done||!is_done(result))return common_failure("InvalidPoliticalShopReceipt",cached.first);
  const auto actor=cached.first.substr(0,split);const auto request=organization_instance_sequence(cached.first,actor);const auto* last=common.request_sequences.find(actor);
  if(!x.common.actor(actor)||!request||*request>2147483647||!last||*request>*last)return common_failure("InvalidPoliticalShopReceiptKey",cached.first);
  const HistoryRecord* history=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* previous=nullptr;
  for(const auto& row:common.history){if(row.result_id==result.result_id){if(history)return common_failure("DuplicatePoliticalShopReceiptHistory",cached.first);history=&row;lower=previous;}previous=&row;}
  if(!history||history->kind!=result.kind||history->actor_id!=actor||history->tick!=result.tick||result.tick<0||result.tick>x.common.tick())return common_failure("PoliticalShopReceiptHistoryMismatch",cached.first);
  const auto upper=organization_instance_sequence(result.result_id,"result"),begin=lower?organization_instance_sequence(lower->result_id,"result"):std::optional<Integer>{0};
  if(!upper||!begin||*upper>=common.next_instance_id||*begin>=*upper)return common_failure("PoliticalShopReceiptAllocationMismatch",cached.first);
  StableId selected;Integer selected_number=0,selected_tick=-1;unsigned count=0;
  const auto take=[&](const StableId& id,const StableId& prefix,const StableId& who,Integer tick){const auto n=organization_instance_sequence(id,prefix);if(who!=actor||!n||*n<=*begin||*n>=*upper)return;++count;if(*n>selected_number){selected=id;selected_number=*n;selected_tick=tick;}};
  if(result.kind=="political_request_shop_permission")for(const auto& row:x.state.permits.rows()){const auto* source=x.state.clearances.find(row.second.clearance_fact_id);if(source)take(row.first,"political_shop_permit",source->request.actor_id,row.second.tick);}
  else if(result.kind=="political_register_wanted"){for(const auto& row:x.state.warrants.rows())if(row.second.authority)take(row.first,"political_wanted",row.second.authority->actor_id,row.second.tick);}
  else if(result.kind=="political_release_wanted")for(const auto& row:x.state.releases.rows())take(row.first,"political_wanted_release",row.second.authority.actor_id,row.second.tick);
  else for(const auto& row:x.state.revocations.rows())take(row.first,"political_shop_revocation",row.second.authority.actor_id,row.second.tick);
  // 首条保留history的旧下界已不可恢复，只认可该结果前最新同人同类的实际来源。
  if(count==0||(lower&&count!=1)||selected_tick!=result.tick||!used.insert(selected).second)return common_failure("PoliticalShopReceiptSourceMismatch",cached.first);
  if(result.kind=="political_request_shop_permission"){
   const auto* permit=x.state.permits.find(selected);const auto* clearance=permit?x.state.clearances.find(permit->clearance_fact_id):nullptr;const auto n=clearance?organization_instance_sequence(clearance->id,"political_shop_clearance"):std::optional<Integer>{};
   if(!n||*n<=*begin||*n+1!=selected_number||clearance->tick!=result.tick)return common_failure("PoliticalShopReceiptAllocationMismatch",cached.first);
  }
 }
 return common_success();
}
}
