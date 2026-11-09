// 新段显式保存接册来源；直接BoundaryValue也递归拒重名，旧法律段不追加隐藏字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_shop_registry_model.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalShopRegistrySuccessorPolicy& v){a("id",v.id);a("court_id",v.court_id);a("organization_id",v.organization_id);a("office_id",v.office_id);a("role_id",v.role_id);a("dialogue_id",v.dialogue_id);a("request_node_id",v.request_node_id);}
template<class A>void record(A& a,PoliticalShopRegistrySuccessionCatalog& v){a("enabled",v.enabled);a("version",v.version);a("policies",v.policies);}
template<class A>void record(A& a,PoliticalShopRegistryPlayerReceipt& v){a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
template<class A>void record(A& a,PoliticalShopRegistryAcceptance& v){a("id",v.id);a("policy_id",v.policy_id);a("court_id",v.court_id);a("initial_registry_fact_id",v.initial_registry_fact_id);a("previous_acceptance_fact_id",v.previous_acceptance_fact_id);a("registration_fact_id",v.registration_fact_id);a("approval_fact_id",v.approval_fact_id);a("occupation_fact_id",v.occupation_fact_id);a("authority",v.authority);a("grant_id",v.grant_id);a("tick",v.tick);a("retained_legal_fact_ids",v.retained_legal_fact_ids);a("player_receipt",v.player_receipt);}
template<class A>void record(A& a,PoliticalShopRegistrySuccessionState& v){a("version",v.version);a("acceptances",v.acceptances);}
template<class A>void record(A& a,PoliticalAcceptShopRegistry& v){a("authority",v.authority);a("policy_id",v.policy_id);a("registration_fact_id",v.registration_fact_id);a("expected_approval_fact_id",v.expected_approval_fact_id);a("expected_previous_acceptance_fact_id",v.expected_previous_acceptance_fact_id);}
}
namespace hanmo::resident {namespace {
template<class T>Outcome<T> decode_versioned(const BoundaryValue& value,const StableId& path){auto guard=validate_political_takeover_payload(value);if(!guard.ok)return {false,{},guard.error};try{T out;dynamic_economy_codec::decode(value,out,path);if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalShopRegistrySuccessionVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_shop_registry_catalog_fields(ProjectionWriter& writer,const PoliticalShopRegistrySuccessionCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_shop_registry_catalog(const PoliticalShopRegistrySuccessionCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_shop_registry_catalog_fields);}
Outcome<PoliticalShopRegistrySuccessionCatalog> decode_political_shop_registry_catalog(const BoundaryValue& v){return decode_versioned<PoliticalShopRegistrySuccessionCatalog>(v,"/political_shop_registry_successions_catalog");}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_shop_registry_successions_fields(ProjectionWriter& writer,const PoliticalShopRegistrySuccessionState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_shop_registry_successions(const PoliticalShopRegistrySuccessionState& value){return politics_projection_detail::encode_fields(value,emit_political_shop_registry_successions_fields);}
Outcome<PoliticalShopRegistrySuccessionState> decode_political_shop_registry_successions(const BoundaryValue& v){return decode_versioned<PoliticalShopRegistrySuccessionState>(v,"/political_shop_registry_successions");}
Status visit_political_shop_registry_successions_checkpoint(CodecVisitor& a,PoliticalShopRegistrySuccessionState& v){if(!a.reading()&&v.version!=1)return a.reject("UnsupportedPoliticalShopRegistrySuccessionVersion");auto result=dynamic_economy_codec::checkpoint(a,political_shop_registry_succession_module,v);return !result.ok?result:v.version==1?common_success():a.reject("UnsupportedPoliticalShopRegistrySuccessionVersion");}
BoundaryValue encode_political_accept_shop_registry(const PoliticalAcceptShopRegistry& v){return dynamic_economy_codec::encode(v);}
Outcome<PoliticalAcceptShopRegistry> decode_political_accept_shop_registry(const BoundaryValue& v){auto guard=validate_political_takeover_payload(v);if(!guard.ok)return {false,{},guard.error};try{PoliticalAcceptShopRegistry out;dynamic_economy_codec::decode(v,out,"/political_accept_shop_registry");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
