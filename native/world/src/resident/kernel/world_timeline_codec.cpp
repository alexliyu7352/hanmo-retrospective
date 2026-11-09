// 时间codec只封装同一字段visitor；来源/数学均由独占业务叶提供。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "world_timeline_codec.hpp"
#include "world_timeline_codec_fields.hpp"
namespace hanmo::resident {
namespace {
using namespace exact_fields;
using namespace time_fields;
// JSON包装在局部候选完成全部字段校验后才返回，读操作从不调用业务handler。
template<class T,class Fn> Outcome<T> read_json(const BoundaryValue& value,Fn fn){return guarded<T>([&]{T candidate;JsonBudget budget;JsonCodec reader(value,budget);take(fn(reader,candidate));take(reader.finish());return candidate;});}
template<class T,class Fn> Outcome<BoundaryValue> write_json(const T& value,Fn fn){return guarded<BoundaryValue>([&]{T copy=value;BoundaryValue output;JsonBudget budget;JsonCodec writer(output,budget);take(fn(writer,copy));take(writer.finish());return output;});}
// binary包装仍调用原CodecVisitor真实类型/字段标签；不额外创造持久封装或世界身份。
template<class T,class Fn> Outcome<CodecVisitor::Buffer> write_binary(const T& value,Fn fn){return guarded<CodecVisitor::Buffer>([&]{CodecVisitor::Buffer output;CodecVisitor writer(output);take(fn(writer,const_cast<T&>(value)));take(writer.finish());return output;});}
template<class T,class Fn> Outcome<T> read_binary(const CodecVisitor::Buffer& value,Fn fn){return guarded<T>([&]{T output;CodecVisitor reader(value);take(fn(reader,output));take(reader.finish());return output;});}
// 候选发布只交换无分配拥有值，避免聚合move中的fraction复制失败半改调用者。
void publish(WorldMoment& a,WorldMoment& b)noexcept{std::swap(a.half_day_tick,b.half_day_tick);a.sub_half_day.swap(b.sub_half_day);}
void publish(HalfDayDuration& a,HalfDayDuration& b)noexcept{a.value.swap(b.value);}
void publish(WorldTimelineState& target,WorldTimelineState& candidate)noexcept{target.scale_ref.definition_id.swap(candidate.scale_ref.definition_id);std::swap(target.scale_ref.version,candidate.scale_ref.version);std::swap(target.scale_ref.units_per_half_day,candidate.scale_ref.units_per_half_day);target.sub_half_day.swap(candidate.sub_half_day);target.period_id.swap(candidate.period_id);target.opening_receipt_ref.swap(candidate.opening_receipt_ref);target.close_receipt_ref.swap(candidate.close_receipt_ref);std::swap(target.boundary_state,candidate.boundary_state);}
void publish(ActionElapsedStore& target,ActionElapsedStore& candidate)noexcept{target.entries.swap(candidate.entries);target.gate_results.swap(candidate.gate_results);target.terminal_bindings.swap(candidate.terminal_bindings);target.fixed_service_transitions.swap(candidate.fixed_service_transitions);}
// 嵌入visitor也先用局部候选，错误不能把半行对象泄入调用者状态。
template<class T,class Fn> Status visit_time_binary(CodecVisitor& codec,T& value,Fn fn){return guarded<Done>([&]{if(codec.reading()){T candidate{};take(fn(codec,candidate));publish(value,candidate);}else take(fn(codec,value));return Done{};});}
}
Outcome<WorldMoment> decode_world_moment2(const BoundaryValue& v){return read_json<WorldMoment>(v,[](auto& c,auto& x){return moment(c,x);});}
Outcome<BoundaryValue> encode_world_moment2(const WorldMoment& v){return write_json(v,[](auto& c,auto& x){return moment(c,x);});}
Outcome<BoundaryValue> encode_work_units2(const WorkUnits& v){return write_json(v,[](auto& c,auto& x){return work_units(c,x);});}
Outcome<BoundaryValue> encode_half_day_duration2(const HalfDayDuration& v){return write_json(v,[](auto& c,auto& x){return c.object("value",[&](auto& z){return fraction(z,x.value);});});}
Status visit_world_moment2(CodecVisitor& c,WorldMoment& v){return visit_time_binary(c,v,[](auto& x,auto& y){return moment(x,y);});}
Status visit_half_day_duration2(CodecVisitor& c,HalfDayDuration& v){return visit_time_binary(c,v,[](auto& x,auto& y){return x.object("value",[&](auto& z){return fraction(z,y.value);});});}
// 公开入口只分派同一schema，全部分配都在明确技术失败防护内。
Outcome<WorldTimelineState> decode_world_timeline2(const BoundaryValue& v){return read_json<WorldTimelineState>(v,[](auto& c,auto& x){return timeline(c,x);});}
Outcome<BoundaryValue> encode_world_timeline2(const WorldTimelineState& v){return write_json(v,[](auto& c,auto& x){return timeline(c,x);});}
Outcome<ActionElapsedStore> decode_action_elapsed2(const BoundaryValue& v){return read_json<ActionElapsedStore>(v,[](auto& c,auto& x){return elapsed(c,x);});}
Outcome<BoundaryValue> encode_action_elapsed2(const ActionElapsedStore& v){return write_json(v,[](auto& c,auto& x){return elapsed(c,x);});}
Status visit_world_timeline2(CodecVisitor& c,WorldTimelineState& v){return visit_time_binary(c,v,[](auto& x,auto& y){return timeline(x,y);});}
Status visit_action_elapsed2(CodecVisitor& c,ActionElapsedStore& v){return visit_time_binary(c,v,[](auto& x,auto& y){return elapsed(x,y);});}
Outcome<CodecVisitor::Buffer> encode_world_timeline2_checkpoint(const WorldTimelineState& v){return write_binary(v,[](auto& c,auto& x){return timeline(c,x);});}
Outcome<WorldTimelineState> decode_world_timeline2_checkpoint(const CodecVisitor::Buffer& v){return read_binary<WorldTimelineState>(v,[](auto& c,auto& x){return timeline(c,x);});}
Outcome<CodecVisitor::Buffer> encode_action_elapsed2_checkpoint(const ActionElapsedStore& v){return write_binary(v,[](auto& c,auto& x){return elapsed(c,x);});}
Outcome<ActionElapsedStore> decode_action_elapsed2_checkpoint(const CodecVisitor::Buffer& v){return read_binary<ActionElapsedStore>(v,[](auto& c,auto& x){return elapsed(c,x);});}
} // 来源存在性、完整真实服务窗、费用once与候选发布仍由原owner联合validator负责。
