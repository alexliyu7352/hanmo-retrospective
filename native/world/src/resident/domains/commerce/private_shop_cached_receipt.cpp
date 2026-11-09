// 回执不是经营真值；真实原事实、本人和全局结果区间一起证明一次命令，不准换挂旧收入。
#include "private_shop_cached_receipt.hpp"
#include "resident/domains/commerce/private_shop_payload.hpp"
#include "liquidation/private_shop_liquidation_cached_receipt.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
struct Source {StableId id,actor,kind;Integer tick=0;};
std::optional<Integer> serial(const StableId& id,Integer next){const auto at=id.rfind(':');if(at==StableId::npos)return {};const auto digits=id.substr(at+1);Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);if(p.ec!=std::errc{}||p.ptr!=digits.data()+digits.size()||n<1||n>=next||std::to_string(n)!=digits)return {};return n;}
bool private_fact(const StableId& id){for(const auto* prefix:{"private_shop_executor:","private_shop_executor_acceptance:","private_shop_executor_revoked:","private_shop_estate_closed:","private_shop_rent_assessed:","private_shop_rent_paid:","private_shop_rent_resumed:"})if(id.rfind(prefix,0)==0)return false;return id.rfind("private_shop:",0)==0||id.rfind("private_shop_",0)==0;}

std::vector<Source> sources(const PrivateShopState& state){std::vector<Source> out;
 for(const auto& r:state.shops.rows())out.push_back({r.first,r.second.terms.proprietor_actor_id,"private_shop_open",r.second.opened_tick});
 for(const auto& r:state.capital.rows())out.push_back({r.first,r.second.actor_id,"private_shop_fund",r.second.tick});
 for(const auto& r:state.markups.rows())out.push_back({r.first,r.second.actor_id,"private_shop_set_markup",r.second.tick});
 for(const auto& r:state.purchases.rows())if(const auto* shop=state.shops.find(r.second.shop_id))out.push_back({r.second.trade_fact_id,shop->terms.proprietor_actor_id,"private_shop_restock",r.second.tick});
 for(const auto& r:state.operations.rows())out.push_back({r.first,r.second.actor_id,"private_shop_operate",r.second.start_tick});
 for(const auto& r:state.operating_results.rows())if(!r.second.completed&&r.second.reason=="cancelled")out.push_back({r.first,r.second.actor_id,"private_shop_cancel_operation",r.second.tick});
 for(const auto& r:state.staff.contracts.rows())out.push_back({r.first,r.second.accepted_by_worker?r.second.terms.worker_actor_id:r.second.terms.employer_actor_id,r.second.accepted_by_worker?"private_shop_staff_accept":"private_shop_employ",r.second.accepted_tick});
 for(const auto& r:state.staff.decisions.rows())if(r.second.decision=="leave"||r.second.decision=="dismiss")out.push_back({r.first,r.second.actor_id,"private_shop_staff_decide",r.second.tick});
 for(const auto& r:state.orders.orders.rows())out.push_back({r.first,r.second.terms.proprietor_actor_id,"private_shop_request_order",r.second.issued_tick});
 for(const auto& r:state.closings.rows())out.push_back({r.first,r.second.actor_id,"private_shop_close",r.second.tick});return out;
}
// 下界取同一全局history的紧前result；首保留项仍须核同人同业务最近原事实。
Status interval(const CommonStore& c,const StableId& key,const CommitResult& result,const Source& source,const std::vector<Source>* recent){
 const auto split=key.rfind(':');if(split==StableId::npos)return common_failure("PrivateShopCachedKeyMismatch",key);const auto actor=key.substr(0,split),digits=key.substr(split+1);Integer request=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* sequence=c.state().request_sequences.find(actor);if(!c.actor(actor)||!sequence||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>2147483647||request>*sequence||std::to_string(request)!=digits)return common_failure("PrivateShopCachedKeyMismatch",key);
 const HistoryRecord* previous=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* selected=nullptr;for(const auto& h:c.state().history){if(h.result_id==result.result_id){if(selected||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("PrivateShopCachedHistoryMismatch",key);selected=&h;lower=previous;}previous=&h;}if(!selected||result.tick<0||result.tick>c.tick()||source.tick!=result.tick||(recent&&(source.actor!=actor||source.kind!=result.kind)))return common_failure("PrivateShopCachedHistoryMismatch",key);
 const auto next=c.state().next_instance_id;auto from=lower?serial(lower->result_id,next):std::optional<Integer>{0},to=serial(result.result_id,next),id=serial(source.id,next);if(result.result_id.rfind("result:",0)!=0||(lower&&lower->result_id.rfind("result:",0)!=0)||!from||!to||!id||*from>=*id||*id>=*to)return common_failure("PrivateShopCachedAllocationMismatch",key);
 if(recent)for(const auto& s:*recent){auto n=serial(s.id,next);if(s.actor==actor&&s.kind==source.kind&&n&&*n>*id&&*n<*to)return common_failure("PrivateShopCachedAllocationMismatch",key);}return common_success();
}
}
Status validate_private_shop_cached_results(const CommonStore& c,const PrivateShopState* state,const PrivateShopLiquidationState* liquidations,ProjectionWriter* projection){
 auto estate=validate_private_shop_liquidation_cached_results(c,liquidations);if(!estate.ok)return estate;
 for(const auto& h:c.state().history)if(private_shop_command_kind(h.kind)&&!state)return common_failure("PrivateShopProfileRequired",h.result_id);
 const auto known=state?sources(*state):std::vector<Source>{};std::set<StableId> command_sources,completion_sources;
 for(const auto& row:c.state().results.rows())if(row.second){const auto& result=*row.second;const auto* effect=result.details?std::get_if<InteractionEffect>(&result.details->data):nullptr;const auto* started=result.details?std::get_if<ActionStarted>(&result.details->data):nullptr;const bool command=private_shop_command_kind(result.kind),own_effect=effect&&private_fact(effect->settlement_result);bool own_action=false;if(started)for(const auto& source:known)if(source.id==started->action_id&&(source.kind=="private_shop_operate"||source.kind=="private_shop_restock"))own_action=true;
  if(command||own_effect||own_action){if(!state||!command||!result.ok)return common_failure("InvalidPrivateShopReceipt",row.first);const bool action=result.kind=="private_shop_operate"||result.kind=="private_shop_restock";StableId origin;if(action){if(!started||!own_action)return common_failure("InvalidPrivateShopReceipt",row.first);origin=started->action_id;}else{if(!effect||!own_effect||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("InvalidPrivateShopReceipt",row.first);origin=effect->settlement_result;}
   const Source* exact=nullptr;for(const auto& source:known)if(source.id==origin&&source.kind==result.kind){if(exact)return common_failure("DuplicatePrivateShopCachedSource",origin);exact=&source;}if(!exact||!command_sources.insert(origin).second)return common_failure("InvalidPrivateShopCachedSource",origin);if(!action){const auto* fact=c.state().facts.find(origin);if(!fact)return common_failure("PrivateShopCachedFactMissing",origin);auto valid=projection?validate_private_shop_fact_header(c,*state,*fact,*projection):validate_private_shop_fact_header(c,*state,*fact);if(!valid.ok)return valid;}auto valid=interval(c,row.first,result,*exact,&known);if(!valid.ok)return valid;
  }
  const auto* advance=result.details?std::get_if<AdvanceDetails>(&result.details->data):nullptr;if(!advance)continue;
  for(const auto& entry:advance->completed_actions.rows()){const auto* operation=state?state->operations.find(entry.first):nullptr;if(!operation)continue;const auto* day=state->operating_results.find(operation->result_fact_id);const auto& actual=entry.second;if(!day||day->reason=="cancelled"||day->reason=="PrivateShopStaffTerminated"||(result.kind!="advance"&&result.kind!="player_plan_step")||!result.ok||!actual.ok||actual.completed!=std::optional<bool>{true}||actual.success!=std::optional<bool>{day->completed}||actual.code!=(day->reason.empty()?std::optional<StableId>{}:std::optional<StableId>{day->reason})||actual.already_completed||actual.military||actual.task||actual.family||actual.growth)return common_failure("InvalidPrivateShopCompletionReceipt",entry.first);if(!completion_sources.insert(day->id).second)return common_failure("DuplicatePrivateShopCompletionSource",entry.first);const auto* head=c.state().facts.find(day->id);if(!head)return common_failure("PrivateShopCachedFactMissing",day->id);auto valid=projection?validate_private_shop_fact_header(c,*state,*head,*projection):validate_private_shop_fact_header(c,*state,*head);if(!valid.ok)return valid;valid=interval(c,row.first,result,{day->id,day->actor_id,{},day->tick},nullptr);if(!valid.ok)return valid;
  }
 }
 return common_success();
}
}
