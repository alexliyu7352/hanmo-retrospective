// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "long_rest_period.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/actor_foundation/character_origin_codec_detail.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
namespace hanmo::resident {namespace {
template<class C>struct EstateFields {
 C& codec;
 template<class T>void operator()(const char* name,T& value){
  if constexpr(std::is_same_v<T,Integer>||std::is_same_v<T,StableId>)codec.field(name,value);
  else if constexpr(std::is_same_v<T,ScenarioOriginRef>)codec.object(name,[&](auto& c){return character_origin_codec::scenario(c,value);});
  else if constexpr(std::is_same_v<T,WorldMoment>)codec.object(name,[&](auto& c){return family_life4_codec::moment(c,value);});
  else codec.optional(name,value,[](auto& c,SourceRef& ref){c.field("domain",ref.domain);c.field("kind",ref.kind);return c.field("id",ref.id);});
 }
};
template<class C>Status source(C& c,MomentOwnerSource& value){c.object("source_ref",[&](auto& x){x.field("domain",value.source_ref.domain);x.field("kind",value.source_ref.kind);return x.field("id",value.source_ref.id);});c.field("source_schema",value.source_schema);if(c.status().ok&&(value.source_schema<1||value.source_ref.domain.empty()||value.source_ref.kind.empty()||value.source_ref.id.empty()))return c.reject("LongRestMouthActualSourceRequired");return c.status();}
template<class C>Status record(C& c,LongRestActualPeriod& p,bool exact_rest_mouths){
 c.field("ordinal",p.ordinal);c.object("provided_from",[&](auto& x){return family_life4_codec::moment(x,p.provided_from);});c.object("applied_at",[&](auto& x){return family_life4_codec::moment(x,p.applied_at);});
 c.sequence("input_boundary_refs",p.input_boundary_refs,[](auto& x,auto& id){return x.field("value",id);});
 c.object("estate_basis",[&](auto& x){EstateFields<std::decay_t<decltype(x)>> fields{x};estate_rest_adoption_fields(fields,p.estate_basis);if(!x.status().ok)return x.status();auto valid=validate_estate_rest_adoption_shape(p.estate_basis);return valid.ok?x.status():x.reject(valid.error.original.code);});
 c.sequence("mouth_actor_ids",p.mouth_actor_ids,[](auto& x,auto& id){return x.field("value",id);});c.field("inventory_id",p.inventory_id);c.field("food_item_id",p.food_item_id);c.field("consumed_quantity",p.consumed_quantity);c.sequence("condition_receipt_refs",p.condition_receipt_refs,[](auto& x,auto& ref){return x.field("result_id",ref.result_id);});
 if(exact_rest_mouths)c.sequence("mouth_inputs",p.mouth_inputs,[](auto& x,auto& mouth){x.field("actor_id",mouth.actor_id);x.optional("original_adoption",mouth.original_adoption,[](auto& y,auto& value){return source(y,value);});x.object("condition_before",[&](auto& y){return actor_condition_codec::head(y,mouth.condition_before);});x.sequence("actual_location_sources",mouth.actual_location_sources,[](auto& y,auto& value){return source(y,value);});if(x.status().ok&&(mouth.actor_id.empty()||mouth.condition_before.actor_id!=mouth.actor_id||mouth.condition_before.condition_revision<0||mouth.condition_before.initial_condition_seed_ref.seed_key.empty()||mouth.actual_location_sources.empty()))return x.reject("LongRestMouthActualInputsRequired");return x.status();});
 else if(c.reading())p.mouth_inputs.clear();else if(!p.mouth_inputs.empty())return c.reject("GrowthExactRestMouthPhysicalSelectionRequired");
 if(c.status().ok&&exact_rest_mouths){if(p.mouth_inputs.size()<p.mouth_actor_ids.size())return c.reject("LongRestMouthInputCoverageMismatch");for(std::size_t n=1;n<p.mouth_inputs.size();++n)if(p.mouth_inputs[n-1].actor_id>=p.mouth_inputs[n].actor_id)return c.reject("LongRestMouthInputOrderMismatch");}
 if(!c.status().ok)return c.status();auto order=compare_world_moments(p.provided_from,p.applied_at);if(!order.ok)return {false,{},order.error};auto adopted=compare_world_moments(p.estate_basis.adopted_at,p.applied_at);if(!adopted.ok)return {false,{},adopted.error};
 if(p.ordinal<1||p.ordinal>720||order.value>=0||adopted.value!=0||p.input_boundary_refs.empty()||p.mouth_actor_ids.empty()||p.inventory_id.empty()||p.food_item_id.empty()||p.consumed_quantity<0||p.condition_receipt_refs.size()!=p.mouth_actor_ids.size())return c.reject("InvalidLongRestActualPeriod");
 for(std::size_t i=0;i<p.mouth_actor_ids.size();++i){if(p.mouth_actor_ids[i].empty()||p.condition_receipt_refs[i].result_id.empty())return c.reject("InvalidLongRestPeriodMouth");for(std::size_t j=0;j<i;++j)if(p.mouth_actor_ids[j]==p.mouth_actor_ids[i]||p.condition_receipt_refs[j].result_id==p.condition_receipt_refs[i].result_id)return c.reject("DuplicateLongRestPeriodMouth");}
 for(std::size_t i=0;i<p.input_boundary_refs.size();++i){if(p.input_boundary_refs[i].empty())return c.reject("LongRestPeriodInputRequired");for(std::size_t j=0;j<i;++j)if(p.input_boundary_refs[j]==p.input_boundary_refs[i])return c.reject("DuplicateLongRestPeriodInput");}return c.status();
}
}
Status visit_long_rest_actual_period(CodecVisitor& c,LongRestActualPeriod& value,bool exact_rest_mouths){return record(c,value,exact_rest_mouths);}
Status emit_long_rest_actual_period(ProjectionWriter& sink,std::string_view name,const LongRestActualPeriod& value,bool exact_rest_mouths){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[&](auto& c,auto& v){return record(c,v,exact_rest_mouths);});}
Outcome<LongRestActualPeriod> decode_long_rest_actual_period(const BoundaryValue& input,bool exact_rest_mouths){LongRestActualPeriod value;ActorFoundationJsonVisitor c(input);auto status=record(c,value,exact_rest_mouths);if(status.ok)status=c.finish();return {status.ok,std::move(value),status.error};}
}
