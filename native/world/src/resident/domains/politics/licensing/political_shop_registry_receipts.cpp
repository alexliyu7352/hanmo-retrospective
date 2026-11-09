// 接册Done绑定本次实际提交结果；裁剪缓存历史不能把更早同tick接册挪给新请求。
#include "political_shop_registry_detail.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <set>
namespace hanmo::resident {
Status validate_political_shop_registry_fact_header(const CommonStore& common,const PoliticalShopRegistrySuccessionState& state,const FactRecord& h){
 const auto* actual=common.state().facts.find(h.id);const auto* source=state.acceptances.find(h.id);auto n=organization_instance_sequence(h.id,political_shop_registry_accept_prefix);
 if(state.version!=1||!n||*n>=common.state().next_instance_id||!actual||actual->id!=h.id||actual->kind!=h.kind||actual->domain!=h.domain||h.kind!=political_shop_registry_accept_kind||h.domain!=political_shop_registry_succession_module||!source||source->id!=h.id)return common_failure("PoliticalShopRegistryPayloadRequired",h.id);return common_success();
}
Status validate_political_shop_registry_player_bindings(const CommonStore& common,const PoliticalShopRegistrySuccessionState& state){
 if(state.version!=1)return common_failure("UnsupportedPoliticalShopRegistrySuccessionVersion");std::set<StableId> results;std::set<std::pair<StableId,Integer>> requests;std::map<StableId,Integer> last_request;Integer previous_result=0,previous_revision=0;
 for(const auto& row:state.acceptances.rows()){
  const auto& a=row.second;auto number=organization_instance_sequence(row.first,political_shop_registry_accept_prefix);
  if(!number||row.first!=a.id)return common_failure("PoliticalShopRegistryAcceptanceMismatch",row.first);
  if(previous_result&&previous_result>=*number)return common_failure("PoliticalShopRegistryPlayerBindingOrder",a.id);
  if(!a.player_receipt)continue;const auto& p=*a.player_receipt;auto result=organization_instance_sequence(p.result_id,"result");const auto* last=common.state().request_sequences.find(a.authority.actor_id);
  if(!result||*result<=*number||*result>=common.state().next_instance_id||p.request_sequence<1||p.request_sequence>2147483647||!last||p.request_sequence>*last||p.result_revision<1||p.result_revision>common.revision()||p.result_revision<=previous_revision||p.request_sequence<=last_request[a.authority.actor_id]||!results.insert(p.result_id).second||!requests.emplace(a.authority.actor_id,p.request_sequence).second)return common_failure("PoliticalShopRegistryPlayerBindingMismatch",a.id);
  unsigned histories=0;for(const auto& h:common.state().history)if(h.result_id==p.result_id){++histories;if(h.actor_id!=a.authority.actor_id||h.tick!=a.tick||h.kind!=political_shop_registry_accept_command)return common_failure("PoliticalShopRegistryPlayerBindingHistoryMismatch",a.id);}
  if(histories>1)return common_failure("PoliticalShopRegistryPlayerBindingHistoryMismatch",a.id);previous_result=*result;previous_revision=p.result_revision;last_request[a.authority.actor_id]=p.request_sequence;
 }
 return common_success();
}
Outcome<StableId> political_shop_registry_new_player_acceptance(const PoliticalShopRegistrySuccessionState& before,const PoliticalShopRegistrySuccessionState& after,const StableId& actor,Integer tick){
 const auto fail=[]()->Outcome<StableId>{return {false,{},common_failure("PoliticalShopRegistryNewPlayerSourceRequired").error};};
 if(before.version!=1||after.version!=1||after.acceptances.size()!=before.acceptances.size()+1)return fail();for(const auto& row:before.acceptances.rows())if(!after.acceptances.find(row.first))return fail();
 const PoliticalShopRegistryAcceptance* source=nullptr;for(const auto& row:after.acceptances.rows())if(!before.acceptances.find(row.first)){if(source||row.first!=row.second.id||row.second.authority.actor_id!=actor||row.second.tick!=tick||row.second.player_receipt)return fail();source=&row.second;}
 return source?Outcome<StableId>{true,source->id,{}}:fail();
}
Status bind_political_shop_registry_player_receipt(CommonStore& common,PoliticalShopRegistrySuccessionState& state,const StableId& acceptance_id,const StableId& actor,Integer request,const CommitResult& result,const std::function<bool(const CommitResult&)>& is_done){
 auto* a=state.acceptances.find(acceptance_id);const auto* cached=common.state().results.find(actor+":"+std::to_string(request));const auto* sequence=common.state().request_sequences.find(actor);
 if(!a||a->player_receipt||a->authority.actor_id!=actor||a->tick!=common.tick()||result.tick!=a->tick||result.kind!=political_shop_registry_accept_command||result.revision!=common.revision()||!is_done||!is_done(result)||!cached||cached->get()!=&result||!sequence||*sequence!=request||common.state().history.empty())return common_failure("PoliticalShopRegistryActualPlayerResultRequired",acceptance_id);
 const auto& last=common.state().history.back();if(last.actor_id!=actor||last.tick!=result.tick||last.kind!=result.kind||last.result_id!=result.result_id)return common_failure("PoliticalShopRegistryActualPlayerResultRequired",acceptance_id);
 const auto* head=common.state().facts.find(acceptance_id);if(!head)return common_failure("PoliticalShopRegistryPayloadRequired",acceptance_id);auto valid=validate_political_shop_registry_fact_header(common,state,*head);if(!valid.ok)return valid;
 a->player_receipt=PoliticalShopRegistryPlayerReceipt{result.result_id,request,result.revision};valid=validate_political_shop_registry_player_bindings(common,state);if(!valid.ok)a->player_receipt.reset();return valid;
}
Status validate_political_shop_registry_receipts(const PoliticalShopRegistryContext& x,const std::function<bool(const CommitResult&)>& is_done){
 auto valid=validate_political_shop_registry_successions(x);if(!valid.ok)return valid;const auto& common=x.legal.common.state();std::set<StableId> used;
 for(const auto& cached:common.results.rows()){
  if(!cached.second||cached.second->kind!=political_shop_registry_accept_command)continue;const auto& result=*cached.second;const auto split=cached.first.rfind(':');if(split==StableId::npos||!is_done||!is_done(result))return common_failure("InvalidPoliticalShopRegistryReceipt",cached.first);
  const auto actor=cached.first.substr(0,split);auto request=organization_instance_sequence(cached.first,actor);const auto* last=common.request_sequences.find(actor);if(!x.legal.common.actor(actor)||!request||*request>2147483647||!last||*request>*last)return common_failure("InvalidPoliticalShopRegistryReceiptKey",cached.first);
  const HistoryRecord* history=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* previous=nullptr;for(const auto& row:common.history){if(row.result_id==result.result_id){if(history)return common_failure("DuplicatePoliticalShopRegistryReceiptHistory",cached.first);history=&row;lower=previous;}previous=&row;}
  if(!history||history->kind!=result.kind||history->actor_id!=actor||history->tick!=result.tick||result.tick<0||result.tick>x.legal.common.tick())return common_failure("PoliticalShopRegistryReceiptHistoryMismatch",cached.first);
  auto upper=organization_instance_sequence(result.result_id,"result"),begin=lower?organization_instance_sequence(lower->result_id,"result"):std::optional<Integer>{0};if(!upper||!begin||*upper>=common.next_instance_id||*begin>=*upper)return common_failure("PoliticalShopRegistryReceiptAllocationMismatch",cached.first);
  const PoliticalShopRegistryAcceptance* selected=nullptr;for(const auto& row:x.state.acceptances.rows())if(row.second.player_receipt&&row.second.player_receipt->result_id==result.result_id){if(selected)return common_failure("PoliticalShopRegistryReceiptSourceMismatch",cached.first);selected=&row.second;}
  if(!selected||selected->authority.actor_id!=actor||selected->tick!=result.tick||selected->player_receipt->request_sequence!=*request||selected->player_receipt->result_revision!=result.revision||!used.insert(selected->id).second)return common_failure("PoliticalShopRegistryReceiptSourceMismatch",cached.first);
  auto source=organization_instance_sequence(selected->id,political_shop_registry_accept_prefix);if(!source||*source<=*begin||*source>=*upper)return common_failure("PoliticalShopRegistryReceiptSourceMismatch",cached.first);
 }
 return common_success();
}
}
