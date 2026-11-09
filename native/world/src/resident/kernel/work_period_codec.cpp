// 每个对外函数调用同一个固定字段遍历；技术错误保持首错，冷读从不补造工作/费用。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "work_period_codec.hpp"
#include "work_period_codec_fields.hpp"
namespace hanmo::resident {
#define WORK_CODEC(NAME,TYPE,FIELDS) \
Outcome<BoundaryValue> encode_##NAME(const TYPE& v){return exact_fields::encode_json(v,[](auto& c,auto& x){return work_fields::FIELDS(c,x);});} \
Outcome<TYPE> decode_##NAME(const BoundaryValue& v){return exact_fields::decode_json<TYPE>(v,[](auto& c,auto& x){return work_fields::FIELDS(c,x);});} \
Status visit_##NAME(CodecVisitor& c,TYPE& v){return exact_fields::visit_binary(c,v,[](auto& codec,auto& x){return work_fields::FIELDS(codec,x);});}
WORK_CODEC(work_execution_binding,WorkExecutionBinding,binding)
WORK_CODEC(work_period_ref,WorkPeriodRef,period)
WORK_CODEC(work_period_view,WorkPeriodView,view)
WORK_CODEC(work_execution_state,WorkExecutionState,state)
WORK_CODEC(craft_work_gate,CraftWorkGate,craft)
WORK_CODEC(craft_work_gate_result,CraftWorkGateResult,craft_result)
WORK_CODEC(original_half_day_gate,OriginalHalfDayGate,original)
WORK_CODEC(original_half_day_gate_result,OriginalHalfDayGateResult,original_result)
WORK_CODEC(life_half_day_stage_gate,LifeHalfDayStageGateState,life)
WORK_CODEC(life_half_day_stage_gate_result,LifeHalfDayStageGateResult,life_result)
#undef WORK_CODEC
}
