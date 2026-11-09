// 两处供粮来源热路径的同步纯观察；不进入世界、存档、业务 job 或结果缓存。
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace hanmo::resident { struct EncodedWorldMoment; }
namespace hanmo::resident::typed_supply_observation {
// 头身份只借原参数；schema 缺失仍为零，不由 kind 推定版本。
struct Source { std::string_view domain, kind, id; std::int64_t schema = 0; };
// 这些是可旁观参数，不是完整可复用上下文：域版本/phase/预算等未完整测量。
// 地址仅在一个宿主采样窗口内区分原根，不能作为跨生命周期身份或 memo key。
struct Context {
  const void* common_identity = nullptr;
  std::array<const void*, 6> original_roots{};
  std::array<std::uint8_t, 16> world_id{};
  std::int64_t revision = 0, execution_epoch = 0, common_tick = 0;
  std::uint64_t fact_count = 0, item_count = 0;
  Source source{}, prefix_anchor{};
  std::int64_t prefix_count = 0, requested_from = 0;
  bool has_prefix = false, has_requested_from = false;
  std::uint64_t source_cut = 0;
  bool has_source_cut = false;
  const std::string* requested_ids = nullptr;
  std::size_t requested_id_count = 0;
  // 仅在 begin 回调同步借用原已生成的正规编码，观察器不得保存指针。
  const EncodedWorldMoment* actual_now = nullptr;
  bool context_complete = false;
};
enum class Stage { roster_all_supply, field_supply_reader, period_reader, calendar_identity };
enum class Edge { begin, end, returned_failure, abort, incomplete, context };
struct Event {
  Edge edge; Stage stage;
  const char* stage_name = nullptr;
  std::uint64_t call_id = 0, parent_id = 0;
  std::uint32_t depth = 0;
  Context context{};
  // context 事件仅借原表达式已经编码的值，不增加精确运算或查询。
  const char* moment_name = nullptr;
  const EncodedWorldMoment* moment = nullptr;
};
using Sink = void (*)(void*, const Event&) noexcept;
struct Binding { Sink sink = nullptr; void* context = nullptr; };
// 显式宿主 RAII 恢复旧绑定；默认关闭，所有借用只在同步回调期间存在。
Binding exchange_binding(Binding) noexcept;
class Scope {
public:
  Scope(Stage, Context) noexcept;
  ~Scope() noexcept;
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  void stop(bool success = true) noexcept;
  void moment(const char*, const EncodedWorldMoment&) noexcept;
  void cut(std::uint64_t) noexcept;
private:
  void emit(Edge, const char* = nullptr, const EncodedWorldMoment* = nullptr) noexcept;
  Binding binding_{};
  Context context_{};
  Stage stage_;
  Scope* previous_ = nullptr;
  std::uint64_t call_id_ = 0, parent_id_ = 0;
  std::uint32_t depth_ = 0;
  int exceptions_ = 0;
};
}
