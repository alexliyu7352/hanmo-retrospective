#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 意图的对象和条款来自原作者目录，未知、重复或跨辖区引用在进入调度前拒绝。
#include "political_governance_intents.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident::dynamic_economy_codec {
// 显式一次邀请选择，不接受作者预置批准ID、受领ID或完成状态。
template<class A>void record(A& a,PoliticalGovernanceIntent& v){a("id",v.id);a("issuer_actor_id",v.issuer_actor_id);a("executor_actor_id",v.executor_actor_id);a("office_id",v.office_id);a("jurisdiction_id",v.jurisdiction_id);a("commission_policy_id",v.commission_policy_id);a("public_inventory_id",v.public_inventory_id);}
// 旧目录没有本组时不产生额外字节。
template<class A>void record(A& a,PoliticalGovernanceIntentCatalog& v){a("version",v.version);a("intents",v.intents);}
}
namespace hanmo::resident {namespace {
// 有界、具名并区分双方；自签自领继续由原玩家业务办理，不在首个NPC意图中猜阶段。
Status shape(const PoliticalGovernanceIntentCatalog& c){
 if(c.version!=1)return common_failure("UnsupportedGovernanceIntentVersion");if(c.intents.empty()||c.intents.size()>64)return common_failure("GovernanceIntentLimit");
 for(const auto& row:c.intents.rows()){const auto& v=row.second;if(row.first!=v.id||v.issuer_actor_id==v.executor_actor_id)return common_failure("InvalidGovernanceIntent",row.first);for(const auto* id:{&v.id,&v.issuer_actor_id,&v.executor_actor_id,&v.office_id,&v.jurisdiction_id,&v.commission_policy_id,&v.public_inventory_id})if(id->empty()||id->size()>4096)return common_failure("InvalidGovernanceIntent",row.first);}return common_success();
}
}
// 解码仅显式v1形状；完整政治/Task能力由共享profile再次配对。
Outcome<PoliticalGovernanceIntentCatalog> decode_political_governance_intents(const BoundaryValue& value){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};try{PoliticalGovernanceIntentCatalog out;dynamic_economy_codec::decode(value,out,"/political_governance_intents");auto valid=shape(out);return valid.ok?Outcome<PoliticalGovernanceIntentCatalog>{true,std::move(out),{}}:Outcome<PoliticalGovernanceIntentCatalog>{false,{},valid.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 序列只含作者选择，不保存重复的邀请或任务真值。
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_governance_intents_fields(ProjectionWriter& writer,const PoliticalGovernanceIntentCatalog& c){
 try{return dynamic_economy_codec::emit_fields(writer,c);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_governance_intents(const PoliticalGovernanceIntentCatalog& c){return politics_projection_detail::encode_fields(c,emit_political_governance_intents_fields);}
// 不要求今天仍任职，避免将后来离职倒写作者目录；真正命令仍核当前Office及Grant。
Status validate_political_governance_intents(const PoliticalGovernanceIntentCatalog& c,const CommonCatalog& common,const PoliticsCatalog& politics,const PoliticsGovernanceCatalog& governance,const OrganizationCatalog& organization){
 auto valid=shape(c);if(!valid.ok)return valid;if(!governance.enabled||!politics.enabled||!organization.enabled)return common_failure("GovernanceIntentProfileRequired");std::set<std::tuple<StableId,StableId,StableId,StableId,StableId,StableId>> selected;
 for(const auto& row:c.intents.rows()){const auto& v=row.second;const auto* j=politics.jurisdictions.find(v.jurisdiction_id);const auto* office=organization.offices.find(v.office_id);const auto* policy=governance.commission_policies.find(v.commission_policy_id);const auto* rule=policy?governance.appeasement_rules.find(policy->rule_id):nullptr;
  if(!j||!office||!policy||!rule||office->organization_id!=j->political_organization_id||std::find(j->public_inventory_ids.begin(),j->public_inventory_ids.end(),v.public_inventory_id)==j->public_inventory_ids.end()||std::find(common.actor_ids.begin(),common.actor_ids.end(),v.issuer_actor_id)==common.actor_ids.end()||std::find(common.actor_ids.begin(),common.actor_ids.end(),v.executor_actor_id)==common.actor_ids.end())return common_failure("GovernanceIntentSourceMismatch",row.first);
  if(!selected.emplace(v.issuer_actor_id,v.executor_actor_id,v.office_id,v.jurisdiction_id,v.commission_policy_id,v.public_inventory_id).second)return common_failure("DuplicateGovernanceIntent",row.first);
  bool grant=false;for(const auto& id:office->grant_ids){const auto* g=organization.grants.find(id);if(g&&g->office_id==v.office_id&&g->may_delegate&&g->amount_limit>=rule->cash_amount&&std::find(g->allowed_command_kinds.begin(),g->allowed_command_kinds.end(),"political_commission_governance")!=g->allowed_command_kinds.end()&&std::find(g->wallet_ids.begin(),g->wallet_ids.end(),j->public_wallet_id)!=g->wallet_ids.end()&&std::find(g->inventory_ids.begin(),g->inventory_ids.end(),v.public_inventory_id)!=g->inventory_ids.end()&&std::any_of(g->scopes.begin(),g->scopes.end(),[&](const auto& s){return s.kind==ScopeRef::Kind::site&&s.id==j->seat_place_id;}))grant=true;}
  if(!grant)return common_failure("GovernanceIntentOriginalGrantRequired",row.first);
 }return common_success();
}
}
