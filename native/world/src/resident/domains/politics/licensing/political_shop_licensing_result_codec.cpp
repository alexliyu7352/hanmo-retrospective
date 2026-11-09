// 新尾逐字段编码，未知版本/重字段明确拒绝；原六张法律表字节不增字段。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_shop_licensing_result_bindings.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalShopLicensingPlayerResult& v){a("source_fact_id",v.source_fact_id);a("actor_id",v.actor_id);a("command_kind",v.command_kind);a("result_id",v.result_id);a("tick",v.tick);a("request_sequence",v.request_sequence);a("result_revision",v.result_revision);}
template<class A>void record(A& a,PoliticalShopLicensingReceiptState& v){a("version",v.version);a("bindings",v.bindings);}
}
namespace hanmo::resident {
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_shop_licensing_receipts_fields(ProjectionWriter& writer,const PoliticalShopLicensingReceiptState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_political_shop_licensing_receipts(const PoliticalShopLicensingReceiptState& value){return politics_projection_detail::encode_fields(value,emit_political_shop_licensing_receipts_fields);}
Outcome<PoliticalShopLicensingReceiptState> decode_political_shop_licensing_receipts(const BoundaryValue& value){
 auto guard=validate_political_takeover_payload(value);if(!guard.ok)return {false,{},guard.error};try{PoliticalShopLicensingReceiptState out;dynamic_economy_codec::decode(value,out,"/political_shop_licensing_receipts");if(out.version!=1)return {false,{},common_failure("UnsupportedPoliticalShopLicensingReceiptsVersion").error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
Status visit_political_shop_licensing_receipts_checkpoint(CodecVisitor& visitor,PoliticalShopLicensingReceiptState& value){if(!visitor.reading()&&value.version!=1)return visitor.reject("UnsupportedPoliticalShopLicensingReceiptsVersion");auto result=dynamic_economy_codec::checkpoint(visitor,political_shop_licensing_receipts_module,value);return !result.ok?result:value.version==1?common_success():visitor.reject("UnsupportedPoliticalShopLicensingReceiptsVersion");}
}
