// 新段逐字段保存，直接BoundaryValue也拒重复键；旧段不借此增加可选尾字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "politics_supply_adoptions.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalSupplyAdoptionRecord& v){a("decision_fact_id",v.decision_fact_id);a("policy_issue_fact_id",v.policy_issue_fact_id);}
template<class A>void record(A& a,PoliticalSupplyAdoptionState& v){a("version",v.version);a("bindings",v.bindings);}
}
namespace hanmo::resident {
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_supply_adoptions_fields(ProjectionWriter& writer,const PoliticalSupplyAdoptionState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_supply_adoptions(const PoliticalSupplyAdoptionState& value){return politics_projection_detail::encode_fields(value,emit_political_supply_adoptions_fields);}
Outcome<PoliticalSupplyAdoptionState> decode_political_supply_adoptions(const BoundaryValue& value){auto guard=validate_political_takeover_payload(value);if(!guard.ok)return {false,{},guard.error};try{PoliticalSupplyAdoptionState out;dynamic_economy_codec::decode(value,out,"/political_supply_adoptions");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalSupplyAdoptionVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_political_supply_adoptions_checkpoint(CodecVisitor& codec,PoliticalSupplyAdoptionState& state){if(!codec.reading()&&state.version!=1)return codec.reject("UnsupportedPoliticalSupplyAdoptionVersion");auto result=dynamic_economy_codec::checkpoint(codec,"political_supply_adoptions",state);if(!result.ok)return result;return state.version==1?common_success():codec.reject("UnsupportedPoliticalSupplyAdoptionVersion");}
}
