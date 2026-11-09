#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 新轮目录保原首职边界，专项Grant不得借旧命令名或任何钱库授权隐式启用。
#include "political_successor_round_contract.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
// 明确原制度、专项授权与两有限窗口，未知字段由原严格codec拒绝。
template<class A>void record(A& a,PoliticalSuccessorRoundPolicy& v){a("id",v.id);a("succession_policy_id",v.succession_policy_id);a("nomination_grant_id",v.nomination_grant_id);a("response_ticks",v.response_ticks);a("accession_ticks",v.accession_ticks);}
// 独立目录尚未注册，不能从旧组织同名policy字符串自动产生本制度。
template<class A>void record(A& a,PoliticalSuccessorRoundCatalog& v){a("version",v.version);a("policies",v.policies);}
}
namespace hanmo::resident {
Status validate_political_successor_round_catalog(const CommonStore& c,const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticalSuccessionCatalog& original,const PoliticalSuccessorRoundCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 110, "PoliticalSuccessorRoundCatalog", "validate_political_successor_round_catalog", &catalog);

 auto valid=validate_political_succession_catalog(c,common,organizations,original);if(!valid.ok)return valid;if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalSuccessorRoundCatalog");std::set<StableId> selected;
 for(const auto& row:catalog.policies.rows()){const auto& p=row.second;const auto* old=original.policies.find(p.succession_policy_id);const auto* office=old?organizations.offices.find(old->leader_office_id):nullptr;const auto* grant=organizations.grants.find(p.nomination_grant_id);
  if(row.first!=p.id||p.id.empty()||!old||!office||!selected.insert(old->id).second||p.response_ticks<1||p.response_ticks>2147483647||p.accession_ticks<1||p.accession_ticks>2147483647||p.nomination_grant_id==old->nomination_grant_id||!grant||grant->office_id!=old->leader_office_id||std::find(office->grant_ids.begin(),office->grant_ids.end(),grant->id)==office->grant_ids.end()||!grant->wallet_ids.empty()||!grant->inventory_ids.empty()||grant->amount_limit!=0||std::none_of(grant->scopes.begin(),grant->scopes.end(),[&](const ScopeRef& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==old->seat_place_id;}))return common_failure("PoliticalSuccessorRoundSpecificGrantRequired",p.id);
  for(const auto* kind:{political_successor_round_nominate_command,political_successor_round_withdraw_command})if(std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),kind)==grant->allowed_command_kinds.end())return common_failure("PoliticalSuccessorRoundSpecificGrantRequired",p.id);
 }return common_success();
}
// 原重复键防护与codec必须同过；未完整接业务前不能把有效目录当已授任许可。
Outcome<PoliticalSuccessorRoundCatalog> decode_political_successor_round_catalog(const BoundaryValue& value){auto valid=validate_political_takeover_payload(value);if(!valid.ok)return {false,{},valid.error};try{PoliticalSuccessorRoundCatalog out;dynamic_economy_codec::decode(value,out,"/political_successor_rounds");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalSuccessorRoundVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_successor_round_catalog_fields(ProjectionWriter& writer,const PoliticalSuccessorRoundCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_successor_round_catalog(const PoliticalSuccessorRoundCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_successor_round_catalog_fields);}
}
