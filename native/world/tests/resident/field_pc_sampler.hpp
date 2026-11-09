// 仅显式前三 M 诊断借用；SIGPROF 只采真实被中断 PC，不生成调用栈或业务来源。
#pragma once
#include <cstdint>
namespace hanmo::resident::field_pc_sampler {
// 默认未启动。初始化失败不夺取原 timer/handler；interval_us 只允许 1000..5000。
bool start(unsigned interval_us = 5000) noexcept;
// 仅原九个完整生产窗口启停；参数只在正常上下文复制，handler 只读单一窗口 token。
bool begin_window(std::uint64_t epoch, std::int64_t target, unsigned kind,
                  std::uint64_t production_call, std::uint32_t production_depth) noexcept;
void end_window() noexcept;
// 禁记录/disarm/owner drain；恢复原 mask/空 timer，静态 disabled handler 保留至进程退出。
bool stop() noexcept;
// 只能在 stop 后正常上下文输出原 PC/maps/计数；不符号化，不走业务 job。
bool write_report(int fd) noexcept;
const char* error() noexcept;
}
