// 同一typed字段用于JSON/binary/retention；原owner与N的完整规则在source validator执行。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "life_work_period_codec.hpp"
#include "life_work_period_codec_fields.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_life_work_period_source(const LifeWorkPeriodView& v){return exact_fields::encode_json(v,[](auto& c,auto& value){return life_work_fields::body(c,value);});}
Outcome<LifeWorkPeriodView> decode_life_work_period_source(const BoundaryValue& v){return exact_fields::decode_json<LifeWorkPeriodView>(v,[](auto& c,auto& value){return life_work_fields::body(c,value);});}
Status visit_life_work_period_source(CodecVisitor& c,LifeWorkPeriodView& v){return exact_fields::visit_binary(c,v,[](auto& x,auto& value){return life_work_fields::body(x,value);});}
Outcome<BoundaryValue> encode_life_work_period_source_ref(const LifeWorkPeriodSourceRef& v){return exact_fields::encode_json(v,[](auto& c,auto& value){return life_work_fields::ref(c,value);});}
Outcome<LifeWorkPeriodSourceRef> decode_life_work_period_source_ref(const BoundaryValue& v){return exact_fields::decode_json<LifeWorkPeriodSourceRef>(v,[](auto& c,auto& value){return life_work_fields::ref(c,value);});}
Status visit_life_work_period_source_ref(CodecVisitor& c,LifeWorkPeriodSourceRef& v){return exact_fields::visit_binary(c,v,[](auto& x,auto& value){return life_work_fields::ref(x,value);});}
}
