// 原LifeActivity第十尾的唯一字段表；只编码真实终局，不授予市场或时间能力。
#pragma once
#include "growth_model.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident::growth_trade_terminal_codec {
// 物理字段固定source_schema、ended_at；未知版本或非规范Moment明确失败。
template<class C> Status fields(C& codec,TradeTerminalTime& value) {
  codec.field("source_schema",value.source_schema);
  if(!codec.status().ok)return codec.status();
  if(value.source_schema!=2)return codec.reject("UnsupportedTradeTerminalTimeSchema");
  return codec.object("ended_at",[&](auto& child) {
    return exact_fields::moment(child,value.ended_at);
  });
}
// Optional已经由原visitor预留并拥有默认值；不另建候选副本或重复收费。
inline Status visit(CodecVisitor& codec,TradeTerminalTime& value) {
  return exact_fields::guarded<Done>([&] {
    exact_fields::take(fields(codec,value));
    return Done{};
  });
}
// 原JSON出口只借用持值，和binary共享字段，不复制快照或生成临时值树。
inline Status emit(ProjectionWriter& writer,std::string_view name,
                   const TradeTerminalTime& value) {
  return exact_fields::guarded<Done>([&] {
    exact_fields::take(emit_projection_record<ActorFoundationProjectionArchive>(
        writer,name,value,[](auto& codec,auto& row) {return fields(codec,row);}));
    return Done{};
  });
}
// 严格字段序沿原exact JSON归档；同job默认分配与大数由原预算拥有者收费。
inline Outcome<TradeTerminalTime> decode(const BoundaryValue& input) {
  return exact_fields::decode_json<TradeTerminalTime>(
      input,[](auto& codec,auto& value) {return fields(codec,value);});
}
// 完整Time2 scalar终态必须有原时刻；旧档、服务及活动中记录仍只能为空。
// 此处只核原载荷形状，开始源/当前时刻/实际释放仍交真实业务reader闭合。
inline Status validate_activity(const LifeActivityRecord& record,
                                bool exact_trade_selected) {
  const auto& view=record.view;
  const bool required=exact_trade_selected&&view.kind==LifeActivityKind::trade&&
      view.phase!=LifeActivityPhase::active;
  if(bool(record.trade_terminal_time)!=required)
    return common_failure(required?"GrowthTradeTerminalTimeRequired":
                          "UnexpectedGrowthTradeTerminalTime",view.action_id);
  if(!record.trade_terminal_time)return common_success();
  const auto& terminal=*record.trade_terminal_time;
  if(!std::holds_alternative<TradeTerms>(record.terms)||
     (view.phase!=LifeActivityPhase::completed&&view.phase!=LifeActivityPhase::cancelled)||
     terminal.source_schema!=2||terminal.ended_at.half_day_tick!=view.completed_tick||
     view.completed_tick<view.start_tick||record.selected_trade_settlement)
    return common_failure("GrowthTradeTerminalTimeMismatch",view.action_id);
  return common_success();
}
} // namespace hanmo::resident::growth_trade_terminal_codec
