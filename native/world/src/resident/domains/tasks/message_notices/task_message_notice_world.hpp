// 当前World调用栈的只读桥；不持久缓存S4/S2对象，不向页面暴露WorldBundle。
#pragma once
#include "task_message_notice.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<std::optional<TaskMessageNoticeServices>> make_task_message_notice_services(const WorldBundle&,const DynamicStepInput* phase=nullptr);
}
