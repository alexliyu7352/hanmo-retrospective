#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 独立新段固定字段；直接BoundaryValue也先递归拒重键，旧财政字节不动。
#include "politics_occupation_fiscal.hpp"
#include "politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
struct OccupationFiscalAuthorityWire { ActingAuthority& value; };
template<class A> void record(A& a,OccupationFiscalAuthorityWire& w){auto& v=w.value;a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
struct OccupationFiscalPolicyWire { FiscalPolicySnapshot& value; };
template<class A> void record(A& a,OccupationFiscalPolicyWire& w){auto& v=w.value;a("jurisdiction_id",v.jurisdiction_id);a("site_id",v.site_id);a("organization_id",v.organization_id);a("public_wallet_id",v.public_wallet_id);a("public_inventory_id",v.public_inventory_id);a("source_fact_id",v.source_fact_id);a("policy_revision",v.policy_revision);a("issued_tick",v.issued_tick);a("effective_from_tick",v.effective_from_tick);StableId mode=v.mode==FiscalTaxMode::lenient?"lenient":v.mode==FiscalTaxMode::normal?"normal":v.mode==FiscalTaxMode::surcharge?"surcharge":"invalid";a("mode",mode);FiscalTaxMode parsed;if(mode=="lenient")parsed=FiscalTaxMode::lenient;else if(mode=="normal")parsed=FiscalTaxMode::normal;else if(mode=="surcharge")parsed=FiscalTaxMode::surcharge;else boundary::fail("InvalidFiscalTaxMode","mode");if(politics_projection_detail::reading(a))v.mode=parsed;}
template<class A> void record(A& a,OccupationFiscalPolicyRecord& v){a("id",v.id);a("registration_fact_id",v.registration_fact_id);a("grant_id",v.grant_id);OccupationFiscalAuthorityWire authority{v.authority};a("authority",authority);OccupationFiscalPolicyWire policy{v.policy};a("policy",policy);}
template<class A> void record(A& a,OccupationFiscalState& v){a("version",v.version);a("policies",v.policies);}
template<class A> void record(A& a,OccupationFiscalCatalog& v){a("version",v.version);a("enabled",v.enabled);}
template<class A> void record(A& a,PoliticalSetOccupationTaxCommand& v){OccupationFiscalAuthorityWire authority{v.authority};a("authority",authority);a("jurisdiction_id",v.jurisdiction_id);a("registration_fact_id",v.registration_fact_id);a("public_inventory_id",v.public_inventory_id);a("expected_jurisdiction_revision",v.expected_jurisdiction_revision);a("expected_policy_revision",v.expected_policy_revision);StableId mode=v.mode==FiscalTaxMode::lenient?"lenient":v.mode==FiscalTaxMode::normal?"normal":v.mode==FiscalTaxMode::surcharge?"surcharge":"invalid";a("mode",mode);FiscalTaxMode parsed;if(mode=="lenient")parsed=FiscalTaxMode::lenient;else if(mode=="normal")parsed=FiscalTaxMode::normal;else if(mode=="surcharge")parsed=FiscalTaxMode::surcharge;else boundary::fail("InvalidFiscalTaxMode","mode");if(politics_projection_detail::reading(a))v.mode=parsed;}
}
namespace hanmo::resident {
namespace {
template<class T> Outcome<T> decode_occupation_fiscal(const BoundaryValue& value,const StableId& path){auto shape=validate_political_takeover_payload(value);if(!shape.ok)return {false,{},shape.error};try{T out;dynamic_economy_codec::decode(value,out,path);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
Status emit_occupation_fiscal_state_fields(ProjectionWriter& writer,const OccupationFiscalState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_occupation_fiscal_state(const OccupationFiscalState& value){return politics_projection_detail::encode_fields(value,emit_occupation_fiscal_state_fields);}
Outcome<OccupationFiscalState> decode_occupation_fiscal_state(const BoundaryValue& value){auto result=decode_occupation_fiscal<OccupationFiscalState>(value,"/political_occupation_fiscal");if(result.ok&&result.value.version!=1)return {false,{},common_failure("UnsupportedOccupationFiscalVersion").error};return result;}
Status visit_occupation_fiscal_checkpoint(CodecVisitor& codec,OccupationFiscalState& value){if(!codec.reading()&&value.version!=1)return codec.reject("UnsupportedOccupationFiscalVersion");auto result=dynamic_economy_codec::checkpoint(codec,"political_occupation_fiscal",value);if(!result.ok)return result;if(value.version!=1)return codec.reject("UnsupportedOccupationFiscalVersion");return result;}
// 原目录字段借读出口，沿同一record供值与shape发射，不生成业务状态。
Status emit_occupation_fiscal_catalog_fields(ProjectionWriter& writer,const OccupationFiscalCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_occupation_fiscal_catalog(const OccupationFiscalCatalog& value){return politics_projection_detail::encode_fields(value,emit_occupation_fiscal_catalog_fields);}
Outcome<OccupationFiscalCatalog> decode_occupation_fiscal_catalog(const BoundaryValue& value){auto result=decode_occupation_fiscal<OccupationFiscalCatalog>(value,"/political_occupation_fiscal");if(result.ok&&result.value.version!=1)return {false,{},common_failure("UnsupportedOccupationFiscalVersion").error};return result;}
BoundaryValue encode_political_set_occupation_tax(const PoliticalSetOccupationTaxCommand& value){return dynamic_economy_codec::encode(value);}
Outcome<PoliticalSetOccupationTaxCommand> decode_political_set_occupation_tax(const BoundaryValue& value){return decode_occupation_fiscal<PoliticalSetOccupationTaxCommand>(value,"/political_set_occupation_tax");}
}
