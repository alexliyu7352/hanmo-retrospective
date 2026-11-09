// S1后正片/实际边界完成两个cold consumer共同消费的原SafeExit闭包。
#pragma once
#include "safe_exit_progress.hpp"
namespace hanmo::resident {
// 只接受原carriers中的真实crossing；先反读实际Decision，再核其对应的
// 入境binding或退出line结果。无条约的普通通行必须没有SafeExit引用。
// 可供GroupPoliticalPassage的两个validate_after_*转发；不代替S1物理或S5付款全验。
Status validate_safe_exit_crossing_consequences(const SafeExitEntryContext&,
 const SafeExitDecisionSourceContext&,const BoundaryCrossingReceipt& actual_borrowed_crossing);
}
