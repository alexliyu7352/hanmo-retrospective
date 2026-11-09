// 内层命令只借外层真实捕获；不另建事务、封存或成功回执。
#pragma once
#include "command_types.hpp"
namespace hanmo::resident {
struct WorldBundle;
class CompleteExactCommandCapture;
Outcome<ReceiptDetails> apply_command_with_complete_capture(
  WorldBundle&,const StableId&,const PreparedDomainOp&,CompleteExactCommandCapture&);
}
