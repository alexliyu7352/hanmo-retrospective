#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 单表主键为原politics事实，结果只能是原Common result；不生成另一条撤职或迁移旧档。
#include "political_dismissal_codec.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalDismissalSourcePolicy& v){a("id",v.id);a("jurisdiction_id",v.jurisdiction_id);a("issuer_office_id",v.issuer_office_id);a("grant_id",v.grant_id);}
template<class A>void record(A& a,PoliticalDismissalSourceCatalog& v){a("version",v.version);a("policies",v.policies);}
template<class A>void record(A& a,PoliticalDismissalSource& v){a("policy_id",v.policy_id);a("dismissal_fact_id",v.dismissal_fact_id);a("office_change_fact_id",v.office_change_fact_id);a("grant_id",v.grant_id);a("removed_actor_id",v.removed_actor_id);a("authority",v.authority);a("tick",v.tick);a("jurisdiction_revision",v.jurisdiction_revision);a("previous_target_epoch",v.previous_target_epoch);}
template<class A>void record(A& a,PoliticalDismissalResultBinding& v){a("actor_id",v.actor_id);a("result_id",v.result_id);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
template<class A>void record(A& a,PoliticalDismissalSourceRecord& v){a("source",v.source);a("actual_result",v.actual_result);}
template<class A>void record(A& a,PoliticalDismissalSourceState& v){a("version",v.version);a("dismissals",v.dismissals);}
}
namespace hanmo::resident {
Status validate_political_dismissal_state_layout(const PoliticalDismissalSourceState& s){return s.version==1&&s.dismissals.size()<=4096?common_success():common_failure("InvalidPoliticalDismissalSourceLayout");}
Status emit_political_dismissal_sources_fields(ProjectionWriter& writer,const PoliticalDismissalSourceState& s){
 try{return dynamic_economy_codec::emit_fields(writer,s);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_dismissal_sources(const PoliticalDismissalSourceState& s){return politics_projection_detail::encode_fields(s,emit_political_dismissal_sources_fields);}
Outcome<PoliticalDismissalSourceState> decode_political_dismissal_sources(const BoundaryValue& v){auto unique=validate_political_takeover_payload(v);if(!unique.ok)return {false,{},unique.error};try{PoliticalDismissalSourceState s;dynamic_economy_codec::decode(v,s,"/political_dismissal_sources");auto checked=validate_political_dismissal_state_layout(s);return checked.ok?Outcome<PoliticalDismissalSourceState>{true,std::move(s),{}}:Outcome<PoliticalDismissalSourceState>{false,{},checked.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_dismissal_sources(CodecVisitor& c,PoliticalDismissalSourceState& s){auto valid=validate_political_dismissal_state_layout(s);if(!c.reading()&&!valid.ok)return valid;auto read=dynamic_economy_codec::checkpoint(c,political_dismissal_sources_module,s);return read.ok?validate_political_dismissal_state_layout(s):read;}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_dismissal_source_catalog_fields(ProjectionWriter& writer,const PoliticalDismissalSourceCatalog& c){
 try{return dynamic_economy_codec::emit_fields(writer,c);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_dismissal_source_catalog(const PoliticalDismissalSourceCatalog& c){return politics_projection_detail::encode_fields(c,emit_political_dismissal_source_catalog_fields);}
Outcome<PoliticalDismissalSourceCatalog> decode_political_dismissal_source_catalog(const BoundaryValue& v){auto unique=validate_political_takeover_payload(v);if(!unique.ok)return {false,{},unique.error};try{PoliticalDismissalSourceCatalog c;dynamic_economy_codec::decode(v,c,"/political_dismissal_sources");if(c.version!=1||c.policies.empty()||c.policies.size()>4096)return {false,{},common_failure("InvalidPoliticalDismissalSourceCatalog").error};return {true,std::move(c),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
