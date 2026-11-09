// 仅显式薄宿主窗口借用同步观察器；不进入World、作者、存档或CheckpointJob。
#pragma once
#include <cstdint>
#include <string_view>
#include <limits>

namespace hanmo::resident::world_stage_observation {
// 观察器只接字面阶段名和标量身份；回调不得抛异常或保存业务借用。
using Sink = void (*)(void*, const char*, const char*, std::uint64_t,
                     std::uint32_t) noexcept;
struct Binding { Sink sink = nullptr; void* context = nullptr; };
// 宿主负责在原调用栈上恢复返回的旧绑定；不安装默认观察器。
Binding exchange_binding(Binding) noexcept;
// 仅同步旁观已有来源值；无业务求值、所有权转移或缓存结果。
enum class NodeMetric : std::uint8_t {
  api_attempt, resolved_entry, loop_candidate_visit, edge_traversal,
  cache_hit, cache_miss
};
struct NodeRef {
  std::string_view node_class, domain, kind, id;
  std::int64_t schema = 0;
};
struct NodeEvent {
  NodeMetric metric;
  const char* site;
  const void* common_identity = nullptr;
  const void* context_identity = nullptr;
  NodeRef node{}, target{};
  std::int64_t cut = -1, actual_tick = 0;
  bool has_actual_tick = false;
};
using NodeSink = void (*)(void*, const NodeEvent&) noexcept;
struct NodeBinding { NodeSink sink = nullptr; void* context = nullptr; };
NodeBinding exchange_node_binding(NodeBinding) noexcept;
void observe_node(const NodeEvent&) noexcept;
// 仅旁观已生成的原段目录和总长，不再编码或计算摘要。
enum class ByteMetric : std::uint8_t { section, complete };
struct ByteEvent {
  ByteMetric metric;
  std::string_view domain;
  std::int64_t codec_version = 0;
  std::uint64_t bytes = 0, offset = 0, header_bytes = 0, directory_bytes = 0;
};
using ByteSink = void (*)(void*, const ByteEvent&) noexcept;
struct ByteBinding { ByteSink sink = nullptr; void* context = nullptr; };
ByteBinding exchange_byte_binding(ByteBinding) noexcept;
void observe_bytes(const ByteEvent&) noexcept;
// 原索引未能无损容入观测槽则未知；不得窄化成虚假的历史边界。
inline std::int64_t observed_cut(std::uint64_t value) noexcept {
  return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
      ? static_cast<std::int64_t>(value) : -1;
}
// 固定五项提交、三项保存与其必要辅助，不提供任意业务回调或动态阶段注册。
enum class Stage {
  submit_call, submit_prepare, submit_candidate_copy, submit_source_capture,
  submit_apply, submit_commit_tail, commit_tail_call, commit_tail_validate,
  opening_call, opening_candidate_copy, opening_apply, opening_validate,
  capture_fallback_begin, capture_settle, capture_finish,
  checkpoint_validate, checkpoint_snapshot, checkpoint_serialize, world_validate
};
// 只同步记原边界；无观察器时不取时间、不分配；异常不捕获，析构只标ABORT。
class Scope {
public:
  explicit Scope(Stage) noexcept;
  ~Scope() noexcept;
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
  // 显式结束必须在原栈顺序；重复结束不重复输出。
  void stop() noexcept;
private:
  Binding binding_{};
  const char* stage_ = nullptr;
  std::uint64_t call_id_ = 0;
  std::uint32_t depth_ = 0;
  int exceptions_ = 0;
};
}
