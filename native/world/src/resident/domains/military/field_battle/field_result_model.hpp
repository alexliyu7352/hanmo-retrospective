// Field 公开终局与会话状态值；不暴露内部事件或权威会话。
#pragma once
#include "field_time.hpp"
namespace hanmo::resident {
struct FieldBattleResult {
  Integer winner_side = -1;
  StableId reason;
  FieldBattleTime ended;
};
// 首版尚未发布：保留旧枚举数值只避免标签重排，不承诺旧 /1 草稿银行兼容。
// 同作者身份的旧 section 也须由独立 codec revision 门拒绝，不猜缺失 Council
// 字段。
enum class FieldBattleState { Deploying, Running, Ended, Council };
} // namespace hanmo::resident
