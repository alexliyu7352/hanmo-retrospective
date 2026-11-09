// 独立版本段不改旧LE或生产收据字节；解码后仍须完整校验原生产事实。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "production_disruptions_codec.hpp"
#include "../local_economy_codec_detail.hpp"
#include "../economic_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A> void record(A& a,ProductionDisruptionCause& v){a("id",v.id);a("source_key",v.source_key);a("site_id",v.site_id);a("reason_key",v.reason_key);a("from_tick",v.from_tick);a("through_tick",v.through_tick);a("remaining_yield_bp",v.remaining_yield_bp);a("unit_ids",v.unit_ids);}
template<class A> void record(A& a,ProductionDisruptionCatalog& v){a("version",v.version);a("profile",v.profile);a("combination_policy",v.combination_policy);a("causes",v.causes);}
template<class A> void record(A& a,ProductionDisruptionRegistration& v){a("id",v.id);a("cause_id",v.cause_id);a("tick",v.tick);}
template<class A> void record(A& a,ProductionDisruptionOutput& v){a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("original_potential",v.original_potential);a("unproduced",v.unproduced);a("adjusted_potential",v.adjusted_potential);a("stored",v.stored);a("capacity_shortfall",v.capacity_shortfall);}
template<class A> void record(A& a,ProductionDisruptionSettlement& v){a("id",v.id);a("production_receipt_id",v.production_receipt_id);a("unit_id",v.unit_id);a("tick",v.tick);a("due_tick",v.due_tick);a("remaining_yield_bp",v.remaining_yield_bp);a("registration_fact_ids",v.registration_fact_ids);a("outputs",v.outputs);}
template<class A> void record(A& a,ProductionDisruptionState& v){a("version",v.version);a("registrations",v.registrations);a("settlements",v.settlements);}
}
namespace hanmo::resident {
namespace {
// 结构闭集与版本门在任何语义解释之前拒绝未知合同。
template<class T> Outcome<T> decode_disruption(const BoundaryValue& value){
    auto valid=validate_economic_payload_fields(value);if(!valid.ok)return {false,{},valid.error};
    try{auto out=dynamic_economy_codec::decode_root<T>(value,"/production_disruptions");if(out.version!=1)boundary::unsupported("ProductionDisruptionVersionMismatch","/production_disruptions/version");return {true,std::move(out),{}};}
    catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}
}
template<class T> BoundaryValue encode_disruption(const T& value){
    if(value.version!=1)boundary::unsupported("ProductionDisruptionVersionMismatch","/production_disruptions/version");return dynamic_economy_codec::encode(value);
}
}
Outcome<ProductionDisruptionCatalog> decode_production_disruption_catalog(const BoundaryValue& value){return decode_disruption<ProductionDisruptionCatalog>(value);}
BoundaryValue encode_production_disruption_catalog(const ProductionDisruptionCatalog& value){return encode_disruption(value);}
Outcome<ProductionDisruptionState> decode_production_disruption_state(const BoundaryValue& value){return decode_disruption<ProductionDisruptionState>(value);}
Status emit_production_disruption_state_fields(ProjectionWriter&w,const ProductionDisruptionState&value){if(value.version!=1)return {false,{},common_failure("ProductionDisruptionVersionMismatch","/production_disruptions/version",ErrorKind::unsupported_capability).error};return dynamic_economy_codec::emit_fields(w,value);}
BoundaryValue encode_production_disruption_state(const ProductionDisruptionState&value){BoundaryValue out;BoundaryProjectionWriter w(out);auto status=w.object("",[&](auto&fields){return emit_production_disruption_state_fields(fields,value);});if(status.ok)status=w.finish();if(!status.ok)throw boundary::DecodeFailure{status.error};return out;}
Status visit_production_disruption_checkpoint(CodecVisitor& visitor,ProductionDisruptionState& state){
    if(!visitor.reading()&&state.version!=1)return visitor.reject("ProductionDisruptionVersionMismatch");
    auto result=dynamic_economy_codec::checkpoint(visitor,"production_disruptions",state);if(!result.ok)return result;
    return state.version==1?visitor.status():visitor.reject("ProductionDisruptionVersionMismatch");
}
}
