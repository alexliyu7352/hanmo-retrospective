#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 新条约段显式版本与递归重键守卫，拒绝直接BoundaryValue藏第二份授权。
#include "treaty_passage_model.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<> struct EnumNames<TreatyPassageSide>{static std::vector<std::pair<TreatyPassageSide,StableId>> rows(){return {{TreatyPassageSide::grantor,"grantor"},{TreatyPassageSide::beneficiary,"beneficiary"}};}};
struct TreatyAuthorityWire{ActingAuthority& value;};
// 权源四字段全部进入独立段，不用命令字符串反解或今日Office补缺。
template<class A> void record(A& a,TreatyAuthorityWire& w){auto& v=w.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
template<class A> void record(A& a,TreatyPassageControlSnapshot& v){a("jurisdiction_id",v.jurisdiction_id);a("organization_id",v.organization_id);a("occupation_fact_id",v.occupation_fact_id);a("revision",v.revision);}
template<class A> void record(A& a,TreatyPassagePlaceBinding& v){a("place_id",v.place_id);a("jurisdiction_id",v.jurisdiction_id);}
template<class A> void record(A& a,TreatyPassageProvision& v){a("id",v.id);a("terms_id",v.terms_id);a("places",v.places);a("npc_ratifier_office_ids",v.npc_ratifier_office_ids);}
template<class A> void record(A& a,TreatyPassageCatalog& v){a("version",v.version);a("enabled",v.enabled);a("provisions",v.provisions);}
template<class A> void record(A& a,TreatyPassageContact& v){a("actor_id",v.actor_id);a("target_actor_id",v.target_actor_id);a("session_actor_id",v.session_actor_id);a("session_id",v.session_id);a("place_id",v.place_id);a("provider_actor_id",v.provider_actor_id);a("role_id",v.role_id);a("context_kind",v.context_kind);}
template<class A> void record(A& a,TreatyPassageDelivery& v){a("id",v.id);a("offer_id",v.offer_id);a("accepted_reply_id",v.accepted_reply_id);a("source_fact_id",v.source_fact_id);a("tick",v.tick);a("terms_revision",v.terms_revision);a("contact",v.contact);TreatyAuthorityWire recipient{v.recipient_authority};a("recipient_authority",recipient);a("recipient_grant_id",v.recipient_grant_id);}
template<class A> void record(A& a,TreatyPassageSignature& v){a("id",v.id);a("offer_id",v.offer_id);a("accepted_reply_id",v.accepted_reply_id);a("terms_id",v.terms_id);a("knowledge_source_fact_id",v.knowledge_source_fact_id);a("prior_signature_id",v.prior_signature_id);a("side",v.side);TreatyAuthorityWire authority{v.authority},witness{v.witness_authority};a("authority",authority);a("witness_authority",witness);a("grant_id",v.grant_id);a("witness_grant_id",v.witness_grant_id);a("tick",v.tick);a("terms_revision",v.terms_revision);a("contact",v.contact);a("controls",v.controls);}
template<class A> void record(A& a,TreatyPassageState& v){a("version",v.version);a("deliveries",v.deliveries);a("signatures",v.signatures);}
template<class A> void record(A& a,TreatyPassagePresentCommand& v){a("offer_id",v.offer_id);a("accepted_reply_id",v.accepted_reply_id);a("source_fact_id",v.source_fact_id);a("target_actor_id",v.target_actor_id);a("terms_revision",v.terms_revision);}
template<class A> void record(A& a,TreatyPassageSignCommand& v){TreatyAuthorityWire authority{v.authority};a("authority",authority);a("offer_id",v.offer_id);a("accepted_reply_id",v.accepted_reply_id);a("knowledge_source_fact_id",v.knowledge_source_fact_id);a("expected_prior_signature_id",v.expected_prior_signature_id);a("target_actor_id",v.target_actor_id);a("terms_revision",v.terms_revision);}
}
namespace hanmo::resident {
namespace {
// 字段树先验再解码；任何未知字段、重复键或未知枚举都不回填默认成功。
template<class T> Outcome<T> decode(const BoundaryValue& value,const StableId& path){auto safe=validate_political_takeover_payload(value);if(!safe.ok)return {false,{},safe.error};try{T out;dynamic_economy_codec::decode(value,out,path);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_treaty_passage_catalog_fields(ProjectionWriter& writer,const TreatyPassageCatalog& v){
 try{return dynamic_economy_codec::emit_fields(writer,v);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_treaty_passage_catalog(const TreatyPassageCatalog& v){return politics_projection_detail::encode_fields(v,emit_treaty_passage_catalog_fields);}
Outcome<TreatyPassageCatalog> decode_treaty_passage_catalog(const BoundaryValue& v){auto out=decode<TreatyPassageCatalog>(v,"/treaty_passage");if(out.ok&&out.value.version!=1)return {false,{},common_failure("UnsupportedTreatyPassageVersion").error};return out;}
Status emit_treaty_passage_state_fields(ProjectionWriter& writer,const TreatyPassageState& v){
 try{return dynamic_economy_codec::emit_fields(writer,v);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_treaty_passage_state(const TreatyPassageState& v){return politics_projection_detail::encode_fields(v,emit_treaty_passage_state_fields);}
Outcome<TreatyPassageState> decode_treaty_passage_state(const BoundaryValue& v){auto out=decode<TreatyPassageState>(v,"/treaty_passage");if(out.ok&&out.value.version!=1)return {false,{},common_failure("UnsupportedTreatyPassageVersion").error};return out;}
Status visit_treaty_passage_checkpoint(CodecVisitor& codec,TreatyPassageState& value){if(!codec.reading()&&value.version!=1)return codec.reject("UnsupportedTreatyPassageVersion");auto out=dynamic_economy_codec::checkpoint(codec,"treaty_passage",value);if(!out.ok)return out;return value.version==1?common_success():codec.reject("UnsupportedTreatyPassageVersion");}
BoundaryValue encode_treaty_passage_present(const TreatyPassagePresentCommand& v){return dynamic_economy_codec::encode(v);}
Outcome<TreatyPassagePresentCommand> decode_treaty_passage_present(const BoundaryValue& v){return decode<TreatyPassagePresentCommand>(v,"/treaty_passage_present");}
BoundaryValue encode_treaty_passage_sign(const TreatyPassageSignCommand& v){return dynamic_economy_codec::encode(v);}
Outcome<TreatyPassageSignCommand> decode_treaty_passage_sign(const BoundaryValue& v){return decode<TreatyPassageSignCommand>(v,"/treaty_passage_sign");}
}
