// Time2与Fixed elapsed2冷边界；仅还原拥有值，不运行owner、推进时钟或补造历史。
#pragma once
#include "world_timeline.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {
// JSON严格完整根与有限tag，规范分数沿冻结数值叶；失败不返回部分候选。
Outcome<WorldTimelineState> decode_world_timeline2(const BoundaryValue&);
Outcome<BoundaryValue> encode_world_timeline2(const WorldTimelineState&);
Outcome<ActionElapsedStore> decode_action_elapsed2(const BoundaryValue&);
Outcome<BoundaryValue> encode_action_elapsed2(const ActionElapsedStore&);
// 连续许可直接复用唯一RWM typed叶，不另造分数或时间布局。
Outcome<WorldMoment> decode_world_moment2(const BoundaryValue&);
Outcome<BoundaryValue> encode_world_moment2(const WorldMoment&);
// 热DTO只投影同一规范typed数值；错误不能回退成整数展示。
Outcome<BoundaryValue> encode_work_units2(const WorkUnits&);
Outcome<BoundaryValue> encode_half_day_duration2(const HalfDayDuration&);
Status visit_world_moment2(CodecVisitor&,WorldMoment&);
Status visit_half_day_duration2(CodecVisitor&,HalfDayDuration&);
// 外层组合checkpoint可嵌入此逐字段visitor；调用方仍负责finish与原owner引用闭包。
Status visit_world_timeline2(CodecVisitor&,WorldTimelineState&);
Status visit_action_elapsed2(CodecVisitor&,ActionElapsedStore&);
// 独立段包装消费完整输入；不会接纳尾随字节，也不兼容猜测其它物理版本。
Outcome<CodecVisitor::Buffer> encode_world_timeline2_checkpoint(const WorldTimelineState&);
Outcome<WorldTimelineState> decode_world_timeline2_checkpoint(const CodecVisitor::Buffer&);
Outcome<CodecVisitor::Buffer> encode_action_elapsed2_checkpoint(const ActionElapsedStore&);
Outcome<ActionElapsedStore> decode_action_elapsed2_checkpoint(const CodecVisitor::Buffer&);
} // compose仍须同时选择两段及真实Growth/Condition来源，codec成功不代表可运行。
