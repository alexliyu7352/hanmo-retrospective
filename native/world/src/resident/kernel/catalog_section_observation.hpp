// 作者目录与原区段编码的同步观察；不改业务结果、资源或错误构造。
#pragma once
#include <cstdint>
#include <string_view>
namespace hanmo::resident::catalog_section_observation {
// 仅固定入口类别；site 对应冻结源码清单，不以标签推导选型或纯性。
enum class Kind { catalog_validation, authored_read, section_visit, section_hash, header_encode, envelope_emit };
// 正常析构只证明调用已返回，不把原 Outcome 的失败当作成功。
enum class Edge { begin, returned_unknown, abort, end_success };
struct Event {
  Edge edge;
  Kind kind;
  std::uint32_t site = 0;
  const char* category = nullptr;
  std::string_view detail{};
  // 原参数地址仅本采样窗口内诊断；不是完整 context、内容身份或 memo 键。
  const void* original_root = nullptr;
  std::int64_t codec_version = 0;
  std::uint64_t call_id = 0, parent_id = 0;
  std::uint32_t depth = 0;
  std::uint64_t bytes = 0;
  bool has_bytes = false;
};
using Sink = void (*)(void*, const Event&) noexcept;
struct Binding { Sink sink = nullptr; void* context = nullptr; };
// 宿主显式安装并恢复原绑定；默认关闭，借用数据只在同步回调中有效。
Binding exchange_binding(Binding) noexcept;
class Scope {
public:
  Scope(Kind, std::uint32_t site, const char* category, std::string_view detail,
        const void* original_root = nullptr, std::int64_t codec_version = 0) noexcept;
  ~Scope() noexcept;
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  // 只在原成功门已经返回后调用；字节必须借原已计算标量，不重编码。
  void stop(std::uint64_t bytes = 0, bool has_bytes = false) noexcept;
private:
  void emit(Edge) noexcept;
  Binding binding_{};
  Event event_{};
  Scope* previous_ = nullptr;
  int exceptions_ = 0;
};
}
