// 可见机会只投影已知来源，查询不会认领、创建ID或把远方实际库存泄漏给人物。
#include "affairs_model.hpp"
#include "affairs_supply_suspension.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
namespace {
// 已收消息的来源键可见性与具体need/offer获知共享同一只读判据。
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
bool knows(const KnownSubjectView& known,const OfferInstance& offer,const Need& need){return contains(known.known_offer_ids,offer.id)||contains(known.known_need_ids,need.id)||contains(known.known_source_keys,need.source_key);}
}
std::vector<OpportunityView> query_opportunities(const AffairsState& affairs,const TaskState& tasks,const KnownSubjectView& known){std::vector<OpportunityView> out;for(const auto& row:tasks.offer_instances.rows()){const auto& offer=row.second;auto* need=affairs.needs.find(offer.source_need_id);if(!need||affair_source_suspended(affairs,*need)||known.place_id!=offer.accept_scope.id||offer.status!=OfferStatus::published||offer.available_from_tick>known.tick||(offer.expires_tick&&*offer.expires_tick<known.tick)||!knows(known,offer,*need))continue;auto available=need_available_amount(affairs,*need);if(!available.ok||available.value<=0)continue;out.push_back({{OfferRef::Kind::runtime,offer.id},need->id,need->affair_id,need->item_id,need->unit,need->revision,std::min(available.value,offer.definition.goal.quantity)});}return out;}
Outcome<NeedClaimQuote> preview_need_claim(const CommonStore& common,const AffairsState& affairs,const TaskState& tasks,const KnownSubjectView& known,const StableId& need_id,Integer amount,const EconomicSupplyPolicyRead* issued_supply){auto* need=affairs.needs.find(need_id);if(need&&need->measure==NeedMeasure::outstanding_obligation)return {false,{},common_failure("UnsupportedNeedClaim",need_id,ErrorKind::unsupported_capability).error};if(!need||known.actor_id.empty())return {false,{},common_failure("SourceUnknown").error};for(const auto& row:tasks.offer_instances.rows()){const auto& offer=row.second;if(offer.source_need_id==need_id&&knows(known,offer,*need))return prepare_need_claim(common,affairs,tasks,need_id,amount,known.subject,known.actor_id,offer.id,need->revision,nullptr,nullptr,{},issued_supply);}return {false,{},common_failure("SourceUnknown").error};}
} // 状态错误交实际命令重报价报告；视图没有副作用。
