// 第三门：只通过原 ResidentWorld preview/submit 推进。检查副本来自真实 bank，
// 不写初态、接触、战果、名册或续行状态。由中央独占编译/执行本薄宿主。
#include "hanmo/resident_world_api.hpp"
#include "field_reinforcement_component_test.hpp"
#include "field_pc_sampler.hpp"
#include "local_move_component_test.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/native_catalog_decode.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/kernel/exact_time_world.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy_initial.hpp"
#include "resident/kernel/native_catalog_identity.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/native_director_wake_world.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/kernel/typed_supply_observation.hpp"
#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/domains/actor_foundation/validated_catalog.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/military/office_exact_phase.hpp"
#include "resident/domains/military/formation_exact_phase.hpp"
#include "resident/kernel/native_field_world_scope.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
#include "resident/kernel/native_military_custody_command.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/field_battle/field_ability_sources.hpp"
#include "resident/domains/military/field_battle/field_roster_origin.hpp"
#include "resident/domains/military/battle_world_model.hpp"
#include "resident/domains/actor_foundation/injury_sources_v1.hpp"
#include "resident/domains/actor_foundation/injury_grade_effects_v3.hpp"
#include "resident/domains/growth/native_recovery_absence.hpp"
#include "resident/domains/travel/group_member_loss_handoff.hpp"
#include <picosha2.h>
#include <filesystem>
#include <array>
#include <exception>
#include <cerrno>
#include <cstdio>
#include <sys/resource.h>
#include <time.h>
#include <cstdlib>
#include <charconv>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
// 同一可执行门内的原归属来源组件反例；不改变正式 World 守门输入。
namespace hanmo::resident { void check_field_initial_settlement_controller_source();
void check_field_post_result_exit_geometry();
void check_field_army_exit_ready_source_guards(const FieldExactContactReadContext &,
    const FieldBattleCatalog &, const FieldBattleAbilityReadContext &); }
// 仅本诊断借原私有调度接头；与 resident_world.cpp 的真实提交签名相同。
namespace hanmo::resident {
Outcome<ReceiptDetails> apply_command_with_complete_capture(
    WorldBundle &, const StableId &, const PreparedDomainOp &,
    CompleteExactCommandCapture &);
// C 私有 CPP 默认关闭；返回旧值以便本测试同步作用域退出时恢复。
bool set_army_movement_diagnostic_enabled(bool) noexcept;
}
// 仅原测试进程的有界观测；不读业务源、不配置新 job、不写 bank/current。
namespace hanmo::resident {
namespace {
Integer observation_index = -1, observation_bank_tick = -1, observation_target = -1;
unsigned long long observation_sequence = 0;
}
// 上下文仅抄已存在的测试序号、bank 整数 tick 和请求目标，不冒精确 phase Moment。
void field_gate_observation_context(Integer index, Integer bank_tick, Integer target) noexcept {
  observation_index = index; observation_bank_tick = bank_tick; observation_target = target;
}
// 单次有界直接写 stderr（无需 stdio 缓冲刷新）；采样失败不抛错、不改原 errno。
void field_gate_observation(const char *edge, const char *label, const char *source) noexcept {
  const int saved_errno = errno;
  // 整个进程最多 16384 条明细；一次显式截断标记后静默，不能当作阶段结束。
  if (observation_sequence >= 16384) {
    if (observation_sequence == 16384) {
      static constexpr char limit[] = "FIELD_OBSERVATION LIMIT_REACHED detail_lines=16384 LATER_STAGES_UNKNOWN\n";
      (void)::write(STDERR_FILENO, limit, sizeof(limit) - 1);
      ++observation_sequence;
    }
    errno = saved_errno;
    return;
  }
  struct timespec wall{};
  struct rusage usage{};
  const bool wall_ok = ::clock_gettime(CLOCK_MONOTONIC, &wall) == 0;
  const bool usage_ok = ::getrusage(RUSAGE_SELF, &usage) == 0;
  if (observation_sequence != std::numeric_limits<unsigned long long>::max()) ++observation_sequence;
  char buffer[768];
  const int n = std::snprintf(buffer, sizeof(buffer),
      "FIELD_OBSERVATION %s seq=%llu stage=%.80s index=%lld bank_tick=%lld target=%lld "
      "wall_ok=%d wall_sec=%lld wall_nsec=%ld usage_ok=%d user_sec=%lld user_usec=%ld "
      "sys_sec=%lld sys_usec=%ld maxrss_kib=%ld source=%.128s\n",
      edge, observation_sequence, label, static_cast<long long>(observation_index),
      static_cast<long long>(observation_bank_tick), static_cast<long long>(observation_target),
      int(wall_ok), static_cast<long long>(wall.tv_sec), wall.tv_nsec, int(usage_ok),
      static_cast<long long>(usage.ru_utime.tv_sec), static_cast<long>(usage.ru_utime.tv_usec),
      static_cast<long long>(usage.ru_stime.tv_sec), static_cast<long>(usage.ru_stime.tv_usec),
      usage.ru_maxrss, source ? source : "-");
  if (n > 0) (void)::write(STDERR_FILENO, buffer,
      static_cast<std::size_t>(n) < sizeof(buffer) ? static_cast<std::size_t>(n) : sizeof(buffer) - 1);
  errno = saved_errno;
}
}
using namespace hanmo::resident;
namespace {
namespace fs = std::filesystem;
Integer checks = 0;
// 只包原表达式一次；END 表示返回而非业务成功，异常展开标 ABORT 后原异常继续。
class FieldStageObservation {
  const char *label_, *source_;
  int exceptions_;
public:
  explicit FieldStageObservation(const char *label, const char *source = nullptr) noexcept
      : label_(label), source_(source), exceptions_(std::uncaught_exceptions()) {
    field_gate_observation("BEGIN", label_, source_);
  }
  ~FieldStageObservation() noexcept {
    field_gate_observation(std::uncaught_exceptions() > exceptions_ ? "ABORT" : "END", label_, source_);
  }
};
// 私有宏不改变原表达式的求值次数/值类别；不向公共头增加观测接口。
#define FIELD_OBSERVE(label, ...) ([&]() -> decltype(auto) { FieldStageObservation observation(label); return (__VA_ARGS__); }())
#define FIELD_OBSERVE_SOURCE(label, source, ...) ([&]() -> decltype(auto) { FieldStageObservation observation(label, source); return (__VA_ARGS__); }())

// 只为原 evidence 已经到达的 session 报事件大小；无额外会话遍历或总数推断。
void field_observe_event_size_scope() noexcept {
  const int saved_errno = errno;
  char buffer[320];
  const int n = std::snprintf(buffer, sizeof(buffer),
      "FIELD_EVENT_SIZE_SCOPE index=%lld bank_tick=%lld target=%lld "
      "coverage=PARTIAL basis=original_evidence_contact_loop event_total=NOT_MEASURED\n",
      static_cast<long long>(observation_index), static_cast<long long>(observation_bank_tick),
      static_cast<long long>(observation_target));
  if (n > 0) (void)::write(STDERR_FILENO, buffer,
      static_cast<std::size_t>(n) < sizeof(buffer) ? static_cast<std::size_t>(n) : sizeof(buffer) - 1);
  errno = saved_errno;
}
void field_observe_session_event_size(const FieldBattleSession &session) noexcept {
  const int saved_errno = errno;
  static unsigned records = 0;
  if (records >= 4096) {
    if (records == 4096) {
      static constexpr char limit[] = "FIELD_SESSION_EVENT_SIZE LIMIT_REACHED records=4096 LATER_SESSIONS_UNKNOWN\n";
      (void)::write(STDERR_FILENO, limit, sizeof(limit) - 1);
      ++records;
    }
    errno = saved_errno;
    return;
  }
  const auto event_count = session.events.size();
  const auto id_bytes = session.battle_id.size();
  // 原ID可能含空白或NUL；固定字节上界的hex避免把截断/换行冒完整身份。
  const auto emitted_bytes = id_bytes < 128 ? id_bytes : std::size_t{128};
  static constexpr char digits[] = "0123456789abcdef";
  char id_hex[257];
  for (std::size_t i = 0; i < emitted_bytes; ++i) {
    const auto byte = static_cast<unsigned char>(session.battle_id[i]);
    id_hex[2 * i] = digits[byte >> 4];
    id_hex[2 * i + 1] = digits[byte & 15];
  }
  id_hex[2 * emitted_bytes] = '\0';
  char buffer[768];
  const int n = std::snprintf(buffer, sizeof(buffer),
      "FIELD_SESSION_EVENT_SIZE index=%lld bank_tick=%lld target=%lld "
      "session_id_hex=%s session_id_bytes=%zu identity_complete=%d event_count=%zu "
      "unit=FieldBattleEvent coverage=PARTIAL basis=original_evidence_contact_loop\n",
      static_cast<long long>(observation_index), static_cast<long long>(observation_bank_tick),
      static_cast<long long>(observation_target), id_hex, id_bytes,
      int(emitted_bytes == id_bytes), event_count);
  if (n > 0) (void)::write(STDERR_FILENO, buffer,
      static_cast<std::size_t>(n) < sizeof(buffer) ? static_cast<std::size_t>(n) : sizeof(buffer) - 1);
  ++records;
  errno = saved_errno;
}

void field_node_window(const char *, const char *, std::uint64_t, std::uint32_t) noexcept;
// 生产计时只经显式 warm 测试窗同步回传；固定缓冲不借世界或任务资源。
void field_production_stage_sink(void *, const char *label, const char *edge,
                                 std::uint64_t call_id, std::uint32_t depth) noexcept {
  const int saved_errno = errno;
  char source[96];
  const int n = std::snprintf(source, sizeof(source), "production_call:%llu:depth:%u",
      static_cast<unsigned long long>(call_id), static_cast<unsigned int>(depth));
  if (n > 0)
    field_gate_observation(edge, label, source);
  field_node_window(label, edge, call_id, depth);
  errno = saved_errno;
}
class FieldProductionObservationBinding {
  world_stage_observation::Binding previous_{};
  bool active_ = false;
public:
  explicit FieldProductionObservationBinding(bool enabled) noexcept : active_(enabled) {
    if (active_)
      previous_ = world_stage_observation::exchange_binding({field_production_stage_sink, nullptr});
  }
  ~FieldProductionObservationBinding() noexcept {
    if (active_)
      (void)world_stage_observation::exchange_binding(previous_);
  }
  FieldProductionObservationBinding(const FieldProductionObservationBinding &) = delete;
  FieldProductionObservationBinding &operator=(const FieldProductionObservationBinding &) = delete;
};

// 各计数仅为现成容器大小；不遍历、不筛选、不签读票，也不是处理行数。
struct FieldWorldSizeValue { bool root_present = false; std::size_t total = 0; };
struct FieldWorldSizeSnapshot {
  bool captured = false;
  std::array<FieldWorldSizeValue, 16> rows{};
};
FieldWorldSizeSnapshot field_world_size_snapshot(const WorldBundle &view) noexcept {
  FieldWorldSizeSnapshot out;
  out.captured = true;
  const auto &common = view.common.state();
  out.rows[0] = {true, common.facts.size()};
  out.rows[1] = {true, common.history.size()};
  out.rows[2] = {true, common.actors.size()};
  out.rows[3] = {true, common.actions.size()};
  out.rows[4] = {true, view.military.armies.size()};
  out.rows[5] = {true, view.military.military_units.size()};
  out.rows[6] = {true, view.military.supply_facts.size()};
  out.rows[7] = {true, view.military.action_facts.size()};
  if (view.field_battle) {
    out.rows[8] = {true, view.field_battle->sessions.size()};
    out.rows[9] = {true, view.field_battle->contacts.size()};
    out.rows[10] = {true, view.field_battle->council_facts.size()};
  }
  if (view.movement_exact) {
    out.rows[11] = {true, view.movement_exact->units.size()};
    out.rows[12] = {true, view.movement_exact->prefixes.size()};
    out.rows[13] = {true, view.movement_exact->expenses.size()};
  }
  if (view.group_movement_executions) {
    out.rows[14] = {true, view.group_movement_executions->openings.size()};
    out.rows[15] = {true, view.group_movement_executions->military_supply_rebinds.size()};
  }
  return out;
}
// 一初态加最多64个原循环点；净差不冒新增，root不存在时也不冒已选或零遍历。
void field_observe_world_sizes(const WorldBundle &view, FieldWorldSizeSnapshot &previous,
                               unsigned &capture_count) noexcept {
  const int saved_errno = errno;
  if (capture_count >= 65) {
    if (capture_count == 65) {
      static constexpr char limit[] = "FIELD_WORLD_SIZE LIMIT_REACHED captures=65 LATER_SIZES_UNKNOWN\n";
      (void)::write(STDERR_FILENO, limit, sizeof(limit) - 1);
      ++capture_count;
    }
    errno = saved_errno;
    return;
  }
  const auto current = field_world_size_snapshot(view);
  static constexpr std::array<const char *, 16> names{{
    "common_facts", "common_history", "common_actors", "common_actions",
    "military_armies", "military_units", "military_supply_facts", "military_action_facts",
    "field_sessions", "field_contacts", "field_council_facts", "movement_units",
    "movement_prefixes", "movement_expenses", "group_openings", "group_supply_rebinds"
  }};
  for (std::size_t i = 0; i < current.rows.size(); ++i) {
    const auto &value = current.rows[i];
    const auto &before = previous.rows[i];
    const bool comparable = previous.captured && before.root_present && value.root_present;
    const bool increase = comparable && value.total >= before.total;
    const std::size_t delta = !comparable ? 0 : increase ? value.total - before.total : before.total - value.total;
    char buffer[512];
    const int n = std::snprintf(buffer, sizeof(buffer),
        "FIELD_WORLD_SIZE capture=%u index=%lld bank_tick=%lld target=%lld metric=%s "
        "root_present=%d table_total=%zu net_delta_known=%d net_delta_sign=%c net_delta_abs=%zu "
        "selected_count=NOT_MEASURED processed_count=NOT_MEASURED unit=rows\n",
        capture_count, static_cast<long long>(observation_index),
        static_cast<long long>(observation_bank_tick), static_cast<long long>(observation_target),
        names[i], int(value.root_present), value.total, int(comparable),
        comparable ? (increase ? '+' : '-') : '?', delta);
    if (n > 0)
      (void)::write(STDERR_FILENO, buffer,
          static_cast<std::size_t>(n) < sizeof(buffer) ? static_cast<std::size_t>(n) : sizeof(buffer) - 1);
  }
  previous = current;
  ++capture_count;
  errno = saved_errno;
}

// 仅记录最后实际调用阶段，不改变准入或失败后的业务分支。
const char *stage = "arguments";

// 仅前三步NEW诊断的独立POD计数器。地址只在当前活跃epoch内标识Common实例。
namespace node_observation = world_stage_observation;
struct NodeMetricCount { std::uint64_t visits = 0, unique = 0; bool unknown = false; };
struct NodeClassCount {
  std::array<char, 64> name{};
  std::size_t length = 0, site_length = 0, domain_length = 0, kind_length = 0;
  std::array<char, 128> site{};
  std::array<char, 64> domain{};
  std::array<char, 96> kind{};
  std::int64_t schema = 0;
  std::array<NodeMetricCount, 6> metrics{};
};
struct NodeKeySlot {
  std::uint64_t epoch = 0, hash = 0;
  std::uint16_t length = 0;
  std::array<unsigned char, 256> key{};
  std::array<std::uint64_t, 2> observed_groups{};
};
struct NodeReplayCollector {
  bool enabled = false, active = false, overflow = false, output_failed = false;
  unsigned window_kind = 0;
  std::uint64_t epoch = 0, call_id = 0;
  std::uint32_t depth = 0;
  std::array<std::array<bool, 3>, 4> sampled{}, completed{};
  std::array<NodeClassCount, 128> classes{};
  std::size_t class_count = 0, occupied = 0;
  std::array<std::uint64_t, 6> visits{}, global_unique{};
  std::array<bool, 6> global_unknown{};
  std::array<NodeKeySlot, 8192> slots{};
  unsigned byte_rows = 0;
  bool byte_overflow = false, byte_complete = false;
};
NodeReplayCollector node_replay_collector;
const char *node_metric_name(unsigned metric) noexcept {
  static constexpr const char *names[] = {"api_attempt", "resolved_entry", "loop_candidate_visit", "edge_traversal", "cache_hit", "cache_miss"};
  return metric < 6 ? names[metric] : "UNKNOWN";
}
const char *node_window_name(unsigned kind) noexcept {
  static constexpr const char *names[] = {"apply", "validate_impl", "binary_encode"};
  return kind < 3 ? names[kind] : "UNKNOWN";
}
void node_hex(char *out, const unsigned char *bytes, std::size_t count) noexcept {
  static constexpr char digits[] = "0123456789abcdef";
  for (std::size_t i = 0; i < count; ++i) {
    out[2 * i] = digits[bytes[i] >> 4]; out[2 * i + 1] = digits[bytes[i] & 15];
  }
  out[2 * count] = '\0';
}
void node_emit(const char *buffer, int count, std::size_t capacity) noexcept {
  if (count <= 0 || static_cast<std::size_t>(count) >= capacity) {
    node_replay_collector.output_failed = true;
    return;
  }
  const auto written = ::write(STDERR_FILENO, buffer, static_cast<std::size_t>(count));
  if (written != count) node_replay_collector.output_failed = true;
}
// 两处真实 typed-supply 热路径；独立静态POD，不借业务job、不缓存业务结果。
namespace typed_observation = typed_supply_observation;
struct TypedSample {
  std::uint64_t wall_ns = 0, cpu_us = 0;
  bool wall_ok = false, cpu_ok = false;
};
TypedSample typed_sample() noexcept {
  const int saved_errno = errno;
  timespec wall{}; rusage usage{}; TypedSample out;
  out.wall_ok = ::clock_gettime(CLOCK_MONOTONIC, &wall) == 0 && wall.tv_sec >= 0;
  out.cpu_ok = ::getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_utime.tv_sec >= 0 && usage.ru_stime.tv_sec >= 0;
  if (out.wall_ok) out.wall_ns = static_cast<std::uint64_t>(wall.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(wall.tv_nsec);
  if (out.cpu_ok) out.cpu_us = (static_cast<std::uint64_t>(usage.ru_utime.tv_sec) + static_cast<std::uint64_t>(usage.ru_stime.tv_sec)) * 1000000ULL + static_cast<std::uint64_t>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
  errno = saved_errno; return out;
}
struct TypedContextKey {
  std::array<unsigned char, 3072> bytes{};
  std::size_t length = 0;
  bool complete = true;
  void number(std::uint64_t n) noexcept {
    if (length + 8 > bytes.size()) { complete = false; return; }
    for (unsigned i = 0; i < 8; ++i) bytes[length++] = static_cast<unsigned char>(n >> (8 * i));
  }
  void string(std::string_view s) noexcept {
    if (s.size() > 256 || length + 8 + s.size() > bytes.size()) { complete = false; return; }
    number(s.size());
    if (!s.empty()) std::memcpy(bytes.data() + length, s.data(), s.size());
    length += s.size();
  }
  void append(const TypedContextKey &other) noexcept {
    if (!other.complete || length + 8 + other.length > bytes.size()) { complete = false; return; }
    number(other.length);
    if (other.length) std::memcpy(bytes.data() + length, other.bytes.data(), other.length);
    length += other.length;
  }
  void source(const typed_observation::Source &s) noexcept {
    string(s.domain); string(s.kind); string(s.id); number(static_cast<std::uint64_t>(s.schema));
  }
  void moment(const EncodedWorldMoment *m) noexcept {
    number(m != nullptr);
    if (m) { number(static_cast<std::uint64_t>(m->half_day_tick)); string(m->sub_half_day.numerator); string(m->sub_half_day.denominator); }
  }
  void context(const typed_observation::Context &c) noexcept {
    // 裸地址仅当前活epoch调试token；不会据此宣称完整context unique。
    number(reinterpret_cast<std::uintptr_t>(c.common_identity));
    for (const auto *root : c.original_roots) number(reinterpret_cast<std::uintptr_t>(root));
    for (auto byte : c.world_id) number(byte);
    number(static_cast<std::uint64_t>(c.revision)); number(static_cast<std::uint64_t>(c.execution_epoch)); number(static_cast<std::uint64_t>(c.common_tick));
    number(c.fact_count); number(c.item_count); source(c.source); source(c.prefix_anchor);
    number(c.has_prefix); number(static_cast<std::uint64_t>(c.prefix_count));
    number(c.has_requested_from); number(static_cast<std::uint64_t>(c.requested_from));
    number(c.has_source_cut); number(c.source_cut); number(c.requested_id_count);
    if (c.requested_id_count > 16 || (c.requested_id_count && !c.requested_ids)) complete = false;
    else for (std::size_t i = 0; i < c.requested_id_count; ++i) string(c.requested_ids[i]);
    moment(c.actual_now);
  }
};
struct TypedTotals {
  std::uint64_t begins = 0, ends = 0, failures = 0, aborts = 0, incomplete = 0;
  std::uint64_t wall_ns = 0, cpu_us = 0, wall_unknown = 0, cpu_unknown = 0;
};
struct TypedFrame {
  std::uint64_t call_id = 0, parent_id = 0;
  std::uint32_t depth = 0;
  unsigned stage = 0, context_events = 0;
  TypedSample start{};
  TypedContextKey context{};
};
struct TypedGroup { bool used = false; unsigned stage = 0; std::uint64_t first_call_id = 0, first_parent_id = 0, last_call_id = 0; std::uint32_t production_depth = 0; TypedContextKey key{}; TypedTotals totals{}; };
struct TypedReplayCollector {
  bool active = false, failed = false, output_failed = false;
  std::size_t depth = 0, dropped_depth = 0, groups = 0;
  bool overflow = false;
  std::array<TypedFrame, 32> frames{};
  std::array<TypedGroup, 256> rows{};
  std::array<TypedTotals, 4> totals{};
  std::array<std::array<bool, 3>, 4> completed{};
};
TypedReplayCollector typed_replay_collector;
const char *typed_stage_name(unsigned n) noexcept {
  static constexpr const char *names[] = {"military.roster.all_supply_proof", "military.supply.field_reader", "military.supply.period_reader", "military.supply.calendar_identity"};
  return n < 4 ? names[n] : "UNKNOWN";
}
void typed_add(std::uint64_t &to, std::uint64_t value) noexcept {
  if (value > std::numeric_limits<std::uint64_t>::max() - to) typed_replay_collector.overflow = true;
  else to += value;
}
void typed_close(TypedTotals &totals, typed_observation::Edge edge, const TypedSample &begin, const TypedSample &end) noexcept {
  if (edge == typed_observation::Edge::end) typed_add(totals.ends, 1);
  else if (edge == typed_observation::Edge::returned_failure) typed_add(totals.failures, 1);
  else if (edge == typed_observation::Edge::abort) typed_add(totals.aborts, 1);
  else typed_add(totals.incomplete, 1);
  if (begin.wall_ok && end.wall_ok && end.wall_ns >= begin.wall_ns) typed_add(totals.wall_ns, end.wall_ns - begin.wall_ns);
  else typed_add(totals.wall_unknown, 1);
  if (begin.cpu_ok && end.cpu_ok && end.cpu_us >= begin.cpu_us) typed_add(totals.cpu_us, end.cpu_us - begin.cpu_us);
  else typed_add(totals.cpu_unknown, 1);
}
void field_typed_supply_sink(void *, const typed_observation::Event &event) noexcept {
  const int saved_errno = errno;
  auto &c = typed_replay_collector;
  if (!c.active) { errno = saved_errno; return; }
  const auto kind = static_cast<unsigned>(event.stage);
  if (kind >= c.totals.size()) { c.overflow = true; errno = saved_errno; return; }
  if (event.edge == typed_observation::Edge::begin) {
    typed_add(c.totals[kind].begins, 1);
    if (c.depth == c.frames.size() || c.dropped_depth) { if (c.dropped_depth != std::numeric_limits<std::size_t>::max()) ++c.dropped_depth; c.overflow = true; errno = saved_errno; return; }
    auto &f = c.frames[c.depth++]; f = TypedFrame{};
    f.call_id = event.call_id; f.parent_id = event.parent_id; f.depth = event.depth; f.stage = kind;
    if (c.depth > 1 && c.frames[c.depth - 2].call_id != event.parent_id) c.overflow = true;
    f.context.number(kind); f.context.context(event.context);
    f.start = typed_sample(); // 原body之前；不新取任何业务时间或身份。
  } else if (event.edge == typed_observation::Edge::context) {
    if (!c.dropped_depth && c.depth) {
      auto &f = c.frames[c.depth - 1];
      if (f.call_id != event.call_id || f.stage != kind || f.context_events >= 8) c.overflow = true;
      else { ++f.context_events; f.context.string(event.moment_name ? std::string_view(event.moment_name) : std::string_view{}); f.context.number(event.context.has_source_cut); f.context.number(event.context.source_cut); f.context.moment(event.moment); }
    }
  } else {
    const auto end = typed_sample();
    if (c.dropped_depth) --c.dropped_depth;
    else if (!c.depth || c.frames[c.depth - 1].call_id != event.call_id || c.frames[c.depth - 1].stage != kind || c.frames[c.depth - 1].depth != event.depth) c.overflow = true;
    else {
      auto &f = c.frames[c.depth - 1]; typed_close(c.totals[kind], event.edge, f.start, end);
      TypedContextKey key; key.number(c.depth);
      for (std::size_t i = 0; i < c.depth; ++i) key.append(c.frames[i].context);
      if (!key.complete) c.overflow = true;
      else {
        std::size_t i = 0;
        while (i < c.groups && !(c.rows[i].stage == kind && c.rows[i].key.length == key.length && std::memcmp(c.rows[i].key.bytes.data(), key.bytes.data(), key.length) == 0)) ++i;
        if (i == c.groups && c.groups == c.rows.size()) c.overflow = true;
        else {
          if (i == c.groups) { auto &g = c.rows[c.groups++]; g = TypedGroup{}; g.used = true; g.stage = kind; g.key = key; g.first_call_id = f.call_id; g.first_parent_id = f.parent_id; g.production_depth = f.depth; }
          auto &g = c.rows[i]; g.last_call_id = f.call_id; typed_add(g.totals.begins, 1); typed_close(g.totals, event.edge, f.start, end);
        }
      }
      --c.depth;
    }
  }
  errno = saved_errno;
}
void typed_window_begin() noexcept {
  auto &c = typed_replay_collector;
  c.active = true; c.depth = c.dropped_depth = c.groups = 0; c.overflow = false; c.totals = {};
}
void typed_window_end(const char *edge) noexcept {
  const int saved_errno = errno;
  auto &c = typed_replay_collector; const auto &n = node_replay_collector;
  c.active = false;
  bool complete = std::strcmp(edge, "END") == 0 && !c.overflow && c.depth == 0 && c.dropped_depth == 0;
  for (unsigned i = 0; i < c.totals.size(); ++i) {
    const auto &v = c.totals[i];
    complete = complete && v.wall_unknown == 0 && v.cpu_unknown == 0 && v.incomplete == 0 && v.aborts == 0 && v.begins == v.ends + v.failures;
    char line[1024];
    const int size = std::snprintf(line, sizeof(line),
        "FIELD_TYPED_SUPPLY TOTAL epoch=%llu window=%s target=%lld stage=%s role=%s begins=%llu ends=%llu returned_failure=%llu aborts=%llu incomplete=%llu inclusive_wall_ns=%llu inclusive_cpu_us=%llu wall_unknown=%llu cpu_unknown=%llu context_complete=false context_unique=NOT_OBSERVED nested_not_additive=true\n",
        static_cast<unsigned long long>(n.epoch), node_window_name(n.window_kind), static_cast<long long>(observation_target), typed_stage_name(i), (i == 0 || i == 3) ? "MEASURED_PRIMARY" : "ASSOCIATION_ONLY",
        static_cast<unsigned long long>(v.begins), static_cast<unsigned long long>(v.ends), static_cast<unsigned long long>(v.failures), static_cast<unsigned long long>(v.aborts), static_cast<unsigned long long>(v.incomplete), static_cast<unsigned long long>(v.wall_ns), static_cast<unsigned long long>(v.cpu_us), static_cast<unsigned long long>(v.wall_unknown), static_cast<unsigned long long>(v.cpu_unknown));
    node_emit(line, size, sizeof(line));
  }
  for (std::size_t i = 0; i < c.groups; ++i) {
    const auto &g = c.rows[i]; const auto &v = g.totals;
    char key[6145], line[8192]; node_hex(key, g.key.bytes.data(), g.key.length);
    const int size = std::snprintf(line, sizeof(line),
        "FIELD_TYPED_SUPPLY CONTEXT_GROUP epoch=%llu window=%s target=%lld stage=%s key_schema=1 key_hex=%s first_call_id=%llu first_parent_id=%llu last_call_id=%llu production_depth=%u calls=%llu ends=%llu returned_failure=%llu aborts=%llu incomplete=%llu inclusive_wall_ns=%llu inclusive_cpu_us=%llu wall_unknown=%llu cpu_unknown=%llu context_complete=false context_unique=NOT_OBSERVED address_scope=LIVE_WINDOW_ONLY\n",
        static_cast<unsigned long long>(n.epoch), node_window_name(n.window_kind), static_cast<long long>(observation_target), typed_stage_name(g.stage), key,
        static_cast<unsigned long long>(g.first_call_id), static_cast<unsigned long long>(g.first_parent_id), static_cast<unsigned long long>(g.last_call_id), static_cast<unsigned>(g.production_depth),
        static_cast<unsigned long long>(v.begins), static_cast<unsigned long long>(v.ends), static_cast<unsigned long long>(v.failures), static_cast<unsigned long long>(v.aborts), static_cast<unsigned long long>(v.incomplete), static_cast<unsigned long long>(v.wall_ns), static_cast<unsigned long long>(v.cpu_us), static_cast<unsigned long long>(v.wall_unknown), static_cast<unsigned long long>(v.cpu_unknown));
    node_emit(line, size, sizeof(line));
  }
  complete = complete && !n.output_failed;
  c.completed[static_cast<std::size_t>(observation_target)][n.window_kind] = complete;
  c.failed = c.failed || !complete;
  char line[512];
  const int size = std::snprintf(line, sizeof(line),
      "FIELD_TYPED_SUPPLY WINDOW epoch=%llu window=%s target=%lld status=%s groups=%zu overflow=%d open_calls=%zu group_cap=256 key_bytes_cap=3072 requested_ids_cap=16 stack_cap=32 observed_context_not_memo_key=true\n",
      static_cast<unsigned long long>(n.epoch), node_window_name(n.window_kind), static_cast<long long>(observation_target), complete ? "COMPLETE" : "UNKNOWN", c.groups, int(c.overflow), c.depth + c.dropped_depth);
  node_emit(line, size, sizeof(line));
  errno = saved_errno;
}

// 仅新同M0模式读取既有TLS整数计数；不开catalog计时、不读World、不计时钟。
namespace catalog_reuse = actor_foundation_catalog_reuse;
struct FieldCatalogReuseCounters {
  bool requested = false, active = false, failed = false;
  catalog_reuse::Counts initial{}, window_start{};
  std::uint64_t epoch = 0;
  std::array<std::array<bool, 3>, 4> completed{};
};
FieldCatalogReuseCounters field_catalog_reuse_counters;
bool catalog_reuse_difference(const catalog_reuse::Counts &a,
                              const catalog_reuse::Counts &b,
                              catalog_reuse::Counts &delta) noexcept {
  bool complete = !a.overflow && !b.overflow;
  const auto field = [&](std::uint64_t before, std::uint64_t after,
                         std::uint64_t &value) noexcept {
    if (after < before) complete = false;
    else value = after - before;
  };
  field(a.requests, b.requests, delta.requests);
  field(a.reused, b.reused, delta.reused);
  field(a.full_validation_begins, b.full_validation_begins, delta.full_validation_begins);
  field(a.full_validation_successes, b.full_validation_successes, delta.full_validation_successes);
  field(a.sealed_creations, b.sealed_creations, delta.sealed_creations);
  delta.overflow = !complete;
  return complete;
}
void catalog_reuse_emit(const char *scope, const catalog_reuse::Counts &delta,
                        bool complete, std::uint64_t epoch, const char *window,
                        Integer target) noexcept {
  char line[1024];
  const int size = std::snprintf(line, sizeof(line),
      "FIELD_CATALOG_REUSE COUNTS scope=%s epoch=%llu window=%s target=%lld status=%s requests=%llu reused=%llu full_validation_begins=%llu full_validation_successes=%llu sealed_creations=%llu overflow=%d counter_scope=TLS_CURRENT_THREAD catalog_timing=OFF SIGPROF=NOT_STARTED\n",
      scope, static_cast<unsigned long long>(epoch), window, static_cast<long long>(target), complete ? "COMPLETE" : "UNKNOWN",
      static_cast<unsigned long long>(delta.requests), static_cast<unsigned long long>(delta.reused),
      static_cast<unsigned long long>(delta.full_validation_begins), static_cast<unsigned long long>(delta.full_validation_successes),
      static_cast<unsigned long long>(delta.sealed_creations), int(delta.overflow));
  node_emit(line, size, sizeof(line));
}
void catalog_reuse_window_begin() noexcept {
  const int saved_errno = errno;
  auto &c = field_catalog_reuse_counters;
  if (c.requested) {
    if (c.active) c.failed = true;
    c.active = true; c.epoch = node_replay_collector.epoch;
    c.window_start = catalog_reuse::counts();
  }
  errno = saved_errno;
}
void catalog_reuse_window_end(const char *edge) noexcept {
  const int saved_errno = errno;
  auto &c = field_catalog_reuse_counters; const auto &n = node_replay_collector;
  if (c.requested) {
    catalog_reuse::Counts delta;
    bool complete = catalog_reuse_difference(c.window_start, catalog_reuse::counts(), delta);
    complete = complete && c.active && c.epoch == n.epoch && std::strcmp(edge, "END") == 0;
    c.active = false;
    complete = complete && !n.output_failed && bool(std::cerr);
    catalog_reuse_emit("SELECTED_WINDOW", delta, complete, n.epoch, node_window_name(n.window_kind), observation_target);
    complete = complete && !n.output_failed && bool(std::cerr);
    if (observation_target >= 1 && observation_target <= 3 && n.window_kind < 3)
      c.completed[static_cast<std::size_t>(observation_target)][n.window_kind] = complete;
    else complete = false;
    c.failed = c.failed || !complete;
  }
  errno = saved_errno;
}
class FieldCatalogReuseCountingBinding {
  bool enabled_ = false, previous_ = false;
public:
  explicit FieldCatalogReuseCountingBinding(bool enabled) noexcept : enabled_(enabled) {
    const int saved_errno = errno;
    if (enabled_) {
      auto &c = field_catalog_reuse_counters; c.requested = true;
      previous_ = catalog_reuse::set_counting_enabled(true);
      c.initial = catalog_reuse::counts();
    }
    errno = saved_errno;
  }
  bool finish_report() noexcept {
    if (!enabled_) return true;
    const int saved_errno = errno;
    auto &c = field_catalog_reuse_counters;
    catalog_reuse::Counts delta;
    bool complete = catalog_reuse_difference(c.initial, catalog_reuse::counts(), delta);
    complete = complete && !c.failed && !c.active && !node_replay_collector.output_failed && bool(std::cerr);
    for (std::size_t target = 1; target <= 3; ++target)
      for (unsigned kind = 0; kind < 3; ++kind) complete = complete && c.completed[target][kind];
    catalog_reuse_emit("MAIN_BEFORE_INPUT_THROUGH_M3_CHECKS", delta, complete, 0, "ALL_WITH_INITIALIZATION", 3);
    complete = complete && !node_replay_collector.output_failed && bool(std::cerr);
    const char *marker = complete ? "FIELD_CATALOG_REUSE REPORT_COMPLETE NINE_SELECTED_WINDOWS PLUS_MAIN_SCOPE LIGHT_COUNTER_ONLY NOT_FIELD_PASS NOT_CURRENT\n" : "FIELD_CATALOG_REUSE REPORT_INCOMPLETE NOT_PASS\n";
    const auto size = std::strlen(marker); node_emit(marker, static_cast<int>(size), size + 1);
    complete = complete && !node_replay_collector.output_failed && bool(std::cerr);
    errno = saved_errno; return complete;
  }
  ~FieldCatalogReuseCountingBinding() noexcept {
    const int saved_errno = errno;
    if (enabled_) {
      field_catalog_reuse_counters.requested = false;
      field_catalog_reuse_counters.active = false;
      (void)catalog_reuse::set_counting_enabled(previous_);
    }
    errno = saved_errno;
  }
  FieldCatalogReuseCountingBinding(const FieldCatalogReuseCountingBinding &) = delete;
  FieldCatalogReuseCountingBinding &operator=(const FieldCatalogReuseCountingBinding &) = delete;
};

// 固定容量的同步目录/区段计时；仅显式诊断模式，不借业务job或保存业务结果。
namespace catalog_observation = catalog_section_observation;
thread_local unsigned char catalog_observation_thread_token = 0;
struct CatalogAggregateTotals {
  std::uint64_t calls = 0, returned_unknown = 0, end_success = 0, aborts = 0;
  std::uint64_t inclusive_wall_ns = 0, inclusive_cpu_us = 0;
  std::uint64_t child_wall_ns = 0, child_cpu_us = 0;
  std::uint64_t residual_wall_ns = 0, residual_cpu_us = 0;
  std::uint64_t max_wall_ns = 0, max_cpu_us = 0;
  std::uint64_t wall_unknown = 0, cpu_unknown = 0, residual_unknown = 0;
  std::uint64_t bytes = 0, byte_calls = 0;
};
struct CatalogAggregateGroup {
  bool used = false;
  unsigned kind = 0;
  std::uint32_t site = 0;
  std::int64_t codec_version = 0;
  std::size_t category_length = 0, detail_length = 0;
  std::array<char, 128> category{}, detail{};
  CatalogAggregateTotals totals{};
};
struct CatalogAggregateFrame {
  std::uint64_t call = 0, parent = 0, epoch = 0;
  std::uint32_t depth = 0;
  std::size_t group = 0;
  TypedSample start{};
  std::uint64_t child_wall_ns = 0, child_cpu_us = 0;
  bool child_wall_ok = true, child_cpu_ok = true;
};
struct CatalogAggregateCollector {
  bool requested = false, active = false, overflow = false, failed = false;
  const void *owner_thread = nullptr;
  std::uint64_t epoch = 0, calls = 0;
  std::size_t depth = 0, dropped_depth = 0, groups = 0;
  std::array<CatalogAggregateGroup, 512> rows{};
  std::array<CatalogAggregateFrame, 128> frames{};
  std::array<std::array<bool, 3>, 4> completed{};
};
CatalogAggregateCollector catalog_aggregate_collector;
const char *catalog_kind_name(unsigned kind) noexcept {
  static constexpr const char *names[] = {"catalog_validation", "authored_read", "section_visit", "section_hash", "header_encode", "envelope_emit"};
  return kind < 6 ? names[kind] : "UNKNOWN";
}
void catalog_add(std::uint64_t &to, std::uint64_t value) noexcept {
  if (value > std::numeric_limits<std::uint64_t>::max() - to) catalog_aggregate_collector.overflow = true;
  else to += value;
}
std::size_t catalog_group(const catalog_observation::Event &event) noexcept {
  auto &c = catalog_aggregate_collector;
  std::size_t category_size = 0;
  if (!event.category) { c.overflow = true; return c.rows.size(); }
  while (category_size < 128 && event.category[category_size]) ++category_size;
  if (category_size == 0 || category_size == 128 || event.detail.size() > 128 || static_cast<unsigned>(event.kind) >= 6) {
    c.overflow = true; return c.rows.size();
  }
  std::uint64_t hash = 1469598103934665603ULL;
  const auto byte = [&](unsigned char value) noexcept { hash ^= value; hash *= 1099511628211ULL; };
  const auto number = [&](std::uint64_t value) noexcept { for (unsigned i = 0; i < 8; ++i) byte(static_cast<unsigned char>(value >> (8 * i))); };
  number(static_cast<unsigned>(event.kind)); number(event.site); number(static_cast<std::uint64_t>(event.codec_version));
  number(category_size); for (std::size_t i = 0; i < category_size; ++i) byte(static_cast<unsigned char>(event.category[i]));
  number(event.detail.size()); for (char value : event.detail) byte(static_cast<unsigned char>(value));
  for (std::size_t probe = 0; probe < c.rows.size(); ++probe) {
    const auto index = static_cast<std::size_t>((hash + probe) % c.rows.size());
    auto &g = c.rows[index];
    if (!g.used) {
      g.used = true; g.kind = static_cast<unsigned>(event.kind); g.site = event.site; g.codec_version = event.codec_version;
      g.category_length = category_size; g.detail_length = event.detail.size();
      if (category_size) std::memcpy(g.category.data(), event.category, category_size);
      if (!event.detail.empty()) std::memcpy(g.detail.data(), event.detail.data(), event.detail.size());
      ++c.groups; return index;
    }
    if (g.kind == static_cast<unsigned>(event.kind) && g.site == event.site && g.codec_version == event.codec_version &&
        g.category_length == category_size && g.detail_length == event.detail.size() &&
        (!category_size || std::memcmp(g.category.data(), event.category, category_size) == 0) &&
        (event.detail.empty() || std::memcmp(g.detail.data(), event.detail.data(), event.detail.size()) == 0)) return index;
  }
  c.overflow = true; return c.rows.size();
}
void field_catalog_section_sink(void *, const catalog_observation::Event &event) noexcept {
  const int saved_errno = errno;
  auto &c = catalog_aggregate_collector;
  if (!c.active) { errno = saved_errno; return; }
  if (c.owner_thread != &catalog_observation_thread_token) { c.overflow = true; errno = saved_errno; return; }
  if (event.edge == catalog_observation::Edge::begin) {
    catalog_add(c.calls, 1);
    if (c.dropped_depth || c.depth == c.frames.size()) {
      if (c.dropped_depth != std::numeric_limits<std::size_t>::max()) ++c.dropped_depth;
      c.overflow = true; errno = saved_errno; return;
    }
    const auto group = catalog_group(event);
    if (group == c.rows.size()) { ++c.dropped_depth; errno = saved_errno; return; }
    if (c.depth && (c.frames[c.depth - 1].call != event.parent_id ||
                    c.frames[c.depth - 1].depth + 1 != event.depth ||
                    c.frames[c.depth - 1].epoch != c.epoch)) c.overflow = true;
    auto &f = c.frames[c.depth++]; f = CatalogAggregateFrame{};
    f.call = event.call_id; f.parent = event.parent_id; f.depth = event.depth; f.epoch = c.epoch; f.group = group;
    catalog_add(c.rows[group].totals.calls, 1);
    f.start = typed_sample(); // 同原RUSAGE_SELF进程CPU口径；不是线程self CPU。
  } else {
    const auto end = typed_sample();
    if (c.dropped_depth) --c.dropped_depth;
    else if (!c.depth || c.frames[c.depth - 1].call != event.call_id ||
             c.frames[c.depth - 1].parent != event.parent_id || c.frames[c.depth - 1].depth != event.depth ||
             c.frames[c.depth - 1].epoch != c.epoch) c.overflow = true;
    else {
      const auto f = c.frames[c.depth - 1]; auto &g = c.rows[f.group]; auto &t = g.totals;
      if (catalog_group(event) != f.group) c.overflow = true;
      if (event.edge == catalog_observation::Edge::returned_unknown) catalog_add(t.returned_unknown, 1);
      else if (event.edge == catalog_observation::Edge::end_success) catalog_add(t.end_success, 1);
      else if (event.edge == catalog_observation::Edge::abort) catalog_add(t.aborts, 1);
      else c.overflow = true;
      const bool wall_ok = f.start.wall_ok && end.wall_ok && end.wall_ns >= f.start.wall_ns;
      const bool cpu_ok = f.start.cpu_ok && end.cpu_ok && end.cpu_us >= f.start.cpu_us;
      const auto wall = wall_ok ? end.wall_ns - f.start.wall_ns : 0;
      const auto cpu = cpu_ok ? end.cpu_us - f.start.cpu_us : 0;
      if (wall_ok) { catalog_add(t.inclusive_wall_ns, wall); if (wall > t.max_wall_ns) t.max_wall_ns = wall; }
      else catalog_add(t.wall_unknown, 1);
      if (cpu_ok) { catalog_add(t.inclusive_cpu_us, cpu); if (cpu > t.max_cpu_us) t.max_cpu_us = cpu; }
      else catalog_add(t.cpu_unknown, 1);
      const bool residual_ok = !c.overflow && wall_ok && cpu_ok && f.child_wall_ok && f.child_cpu_ok && f.child_wall_ns <= wall && f.child_cpu_us <= cpu;
      if (residual_ok) {
        catalog_add(t.child_wall_ns, f.child_wall_ns); catalog_add(t.child_cpu_us, f.child_cpu_us);
        catalog_add(t.residual_wall_ns, wall - f.child_wall_ns); catalog_add(t.residual_cpu_us, cpu - f.child_cpu_us);
      } else catalog_add(t.residual_unknown, 1);
      if (event.has_bytes) { catalog_add(t.bytes, event.bytes); catalog_add(t.byte_calls, 1); }
      --c.depth;
      if (c.depth) {
        auto &parent = c.frames[c.depth - 1];
        if (parent.call != f.parent || parent.epoch != f.epoch) c.overflow = true;
        if (wall_ok) catalog_add(parent.child_wall_ns, wall); else parent.child_wall_ok = false;
        if (cpu_ok) catalog_add(parent.child_cpu_us, cpu); else parent.child_cpu_ok = false;
        if (!residual_ok) { parent.child_wall_ok = false; parent.child_cpu_ok = false; }
      }
    }
  }
  errno = saved_errno;
}
void catalog_window_begin() noexcept {
  auto &c = catalog_aggregate_collector;
  if (!c.requested) return;
  c.active = true; c.epoch = node_replay_collector.epoch;
  c.depth = c.dropped_depth = c.groups = 0; c.calls = 0; c.overflow = false; c.rows = {};
}
void catalog_window_end(const char *edge) noexcept {
  const int saved_errno = errno;
  auto &c = catalog_aggregate_collector; const auto &n = node_replay_collector;
  if (!c.requested) { errno = saved_errno; return; }
  c.active = false;
  bool complete = std::strcmp(edge, "END") == 0 && c.owner_thread == &catalog_observation_thread_token &&
      c.epoch == n.epoch && !c.overflow && c.depth == 0 && c.dropped_depth == 0;
  for (const auto &g : c.rows) if (g.used) {
    const auto &t = g.totals;
    complete = complete && t.calls == t.returned_unknown + t.end_success + t.aborts &&
        t.wall_unknown == 0 && t.cpu_unknown == 0 && t.residual_unknown == 0;
    char category[257], detail[257], line[2048];
    node_hex(category, reinterpret_cast<const unsigned char *>(g.category.data()), g.category_length);
    node_hex(detail, reinterpret_cast<const unsigned char *>(g.detail.data()), g.detail_length);
    const int size = std::snprintf(line, sizeof(line),
        "FIELD_CATALOG_SECTION GROUP epoch=%llu window=%s target=%lld kind=%s site=%u category_hex=%s detail_hex=%s codec_version=%lld calls=%llu returned_unknown=%llu end_success=%llu aborts=%llu inclusive_wall_ns=%llu inclusive_process_cpu_us=%llu direct_observed_child_wall_ns=%llu direct_observed_child_process_cpu_us=%llu observed_residual_wall_ns=%llu observed_residual_process_cpu_us=%llu max_wall_ns=%llu max_process_cpu_us=%llu wall_unknown=%llu cpu_unknown=%llu residual_unknown=%llu reported_bytes=%llu byte_calls=%llu byte_semantics=ORIGINAL_SCALAR_BY_KIND_NOT_ADDITIVE whole_function_self=NOT_OBSERVED outcome_from_returned_unknown=NOT_OBSERVED\n",
        static_cast<unsigned long long>(n.epoch), node_window_name(n.window_kind), static_cast<long long>(observation_target), catalog_kind_name(g.kind), g.site, category, detail, static_cast<long long>(g.codec_version),
        static_cast<unsigned long long>(t.calls), static_cast<unsigned long long>(t.returned_unknown), static_cast<unsigned long long>(t.end_success), static_cast<unsigned long long>(t.aborts),
        static_cast<unsigned long long>(t.inclusive_wall_ns), static_cast<unsigned long long>(t.inclusive_cpu_us), static_cast<unsigned long long>(t.child_wall_ns), static_cast<unsigned long long>(t.child_cpu_us),
        static_cast<unsigned long long>(t.residual_wall_ns), static_cast<unsigned long long>(t.residual_cpu_us), static_cast<unsigned long long>(t.max_wall_ns), static_cast<unsigned long long>(t.max_cpu_us),
        static_cast<unsigned long long>(t.wall_unknown), static_cast<unsigned long long>(t.cpu_unknown), static_cast<unsigned long long>(t.residual_unknown), static_cast<unsigned long long>(t.bytes), static_cast<unsigned long long>(t.byte_calls));
    node_emit(line, size, sizeof(line));
  }
  complete = complete && !n.output_failed && bool(std::cerr);
  char line[768];
  const int size = std::snprintf(line, sizeof(line),
      "FIELD_CATALOG_SECTION WINDOW epoch=%llu window=%s target=%lld status=%s groups=%zu calls=%llu overflow=%d open_calls=%zu group_cap=512 stack_cap=128 key_field_bytes_cap=128 coverage=INSTRUMENTED_SCOPES_ONLY nested_inclusive_not_additive=true residual_scope=DIRECT_OBSERVED_CHILD_SUBTRACTION process_cpu=RUSAGE_SELF SIGPROF=NOT_STARTED\n",
      static_cast<unsigned long long>(n.epoch), node_window_name(n.window_kind), static_cast<long long>(observation_target), complete ? "COMPLETE" : "INCOMPLETE", c.groups, static_cast<unsigned long long>(c.calls), int(c.overflow), c.depth + c.dropped_depth);
  node_emit(line, size, sizeof(line));
  complete = complete && !n.output_failed && bool(std::cerr);
  if (observation_target >= 1 && observation_target <= 3) c.completed[static_cast<std::size_t>(observation_target)][n.window_kind] = complete;
  else complete = false;
  c.failed = c.failed || !complete;
  errno = saved_errno;
}
class FieldCatalogSectionBinding {
  catalog_observation::Binding previous_{};
  bool enabled_ = false;
public:
  explicit FieldCatalogSectionBinding(bool enabled) noexcept : enabled_(enabled) {
    const int saved_errno = errno;
    if (enabled_) {
      auto &c = catalog_aggregate_collector; c.requested = true; c.owner_thread = &catalog_observation_thread_token;
      previous_ = catalog_observation::exchange_binding({field_catalog_section_sink, nullptr});
    }
    errno = saved_errno;
  }
  bool finish_report() noexcept {
    if (!enabled_) return true;
    const int saved_errno = errno;
    const auto &c = catalog_aggregate_collector;
    bool complete = !c.failed && !c.active && !node_replay_collector.output_failed && bool(std::cerr);
    for (std::size_t target = 1; target <= 3; ++target)
      for (unsigned kind = 0; kind < 3; ++kind) complete = complete && c.completed[target][kind];
    const char *marker = complete ? "FIELD_CATALOG_SECTION REPORT_COMPLETE NINE_WINDOWS AGGREGATED_ONLY NOT_MEMO_PROOF NOT_FIELD_PASS NOT_CURRENT\n" : "FIELD_CATALOG_SECTION REPORT_INCOMPLETE NOT_PASS\n";
    const auto size = std::strlen(marker); node_emit(marker, static_cast<int>(size), size + 1);
    complete = complete && !node_replay_collector.output_failed && bool(std::cerr);
    errno = saved_errno; return complete;
  }
  ~FieldCatalogSectionBinding() noexcept {
    const int saved_errno = errno;
    if (enabled_) {
      catalog_aggregate_collector.active = false; catalog_aggregate_collector.requested = false;
      (void)catalog_observation::exchange_binding(previous_);
    }
    errno = saved_errno;
  }
  FieldCatalogSectionBinding(const FieldCatalogSectionBinding &) = delete;
  FieldCatalogSectionBinding &operator=(const FieldCatalogSectionBinding &) = delete;
};

struct NodeExactKey {
  std::array<unsigned char, 256> bytes{};
  std::size_t length = 0;
  bool complete = true;
  void number(std::uint64_t value) noexcept {
    if (length + 8 > bytes.size()) { complete = false; return; }
    for (unsigned i = 0; i < 8; ++i) bytes[length++] = static_cast<unsigned char>(value >> (8 * i));
  }
  void string(std::string_view value) noexcept {
    // 长度分段避免字段拼接歧义；所有原字节参与比较，长key不计精确unique。
    if (value.size() > bytes.size() || length + 8 + value.size() > bytes.size()) { complete = false; return; }
    number(value.size());
    if (!complete) return;
    if (!value.empty()) std::memcpy(bytes.data() + length, value.data(), value.size());
    length += value.size();
  }
  void node(const node_observation::NodeRef &value) noexcept {
    if (value.node_class.empty() || value.domain.empty() || value.kind.empty() || value.id.empty() || value.schema <= 0) complete = false;
    string(value.node_class); string(value.domain); string(value.kind); string(value.id);
    number(static_cast<std::uint64_t>(value.schema));
  }
};
void field_node_sink(void *, const node_observation::NodeEvent &event) noexcept {
  const int saved_errno = errno;
  auto &c = node_replay_collector;
  if (!c.active) { errno = saved_errno; return; }
  const auto metric = static_cast<unsigned>(event.metric);
  if (metric >= c.visits.size()) { c.overflow = true; errno = saved_errno; return; }
  if (c.visits[metric] == std::numeric_limits<std::uint64_t>::max()) c.overflow = true;
  else ++c.visits[metric];
  const auto name = event.node.node_class;
  const std::string_view site = event.site ? std::string_view(event.site) : std::string_view{};
  const auto domain = event.node.domain, kind = event.node.kind;
  if (name.empty() || name.size() >= c.classes[0].name.size() || site.empty() ||
      site.size() >= c.classes[0].site.size() || domain.size() >= c.classes[0].domain.size() ||
      kind.size() >= c.classes[0].kind.size()) { c.overflow = true; errno = saved_errno; return; }
  const auto matches = [&](const NodeClassCount &row) noexcept {
    return row.length == name.size() && row.site_length == site.size() && row.domain_length == domain.size() &&
      row.kind_length == kind.size() && row.schema == event.node.schema &&
      std::memcmp(row.name.data(), name.data(), name.size()) == 0 &&
      std::memcmp(row.site.data(), site.data(), site.size()) == 0 &&
      (domain.empty() || std::memcmp(row.domain.data(), domain.data(), domain.size()) == 0) &&
      (kind.empty() || std::memcmp(row.kind.data(), kind.data(), kind.size()) == 0);
  };
  std::size_t class_index = 0;
  while (class_index < c.class_count && !matches(c.classes[class_index])) ++class_index;
  if (class_index == c.class_count) {
    if (c.class_count == c.classes.size()) { c.overflow = true; errno = saved_errno; return; }
    auto &row = c.classes[c.class_count++]; row = NodeClassCount{};
    row.length = name.size(); row.site_length = site.size(); row.domain_length = domain.size(); row.kind_length = kind.size();
    row.schema = event.node.schema;
    std::memcpy(row.name.data(), name.data(), name.size()); std::memcpy(row.site.data(), site.data(), site.size());
    if (!domain.empty()) std::memcpy(row.domain.data(), domain.data(), domain.size());
    if (!kind.empty()) std::memcpy(row.kind.data(), kind.data(), kind.size());
  }
  auto &count = c.classes[class_index].metrics[metric];
  if (count.visits == std::numeric_limits<std::uint64_t>::max()) { count.unknown = true; c.overflow = true; }
  else ++count.visits;
  if (c.overflow) { count.unknown = true; errno = saved_errno; return; }
  NodeExactKey key;
  key.number(metric);
  key.number(reinterpret_cast<std::uintptr_t>(event.common_identity));
  key.node(event.node);
  if (event.metric == node_observation::NodeMetric::edge_traversal) key.node(event.target);
  if (!event.common_identity || !key.complete) { count.unknown = true; c.global_unknown[metric] = true; errno = saved_errno; return; }
  // context地址/cut/整数tick不冒完整fraction或lifetime身份，context_unique固定NOT_OBSERVED。
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t i = 0; i < key.length; ++i) { hash ^= key.bytes[i]; hash *= 1099511628211ULL; }
  bool found = false;
  for (std::size_t probe = 0; probe < c.slots.size(); ++probe) {
    auto &slot = c.slots[(hash + probe) % c.slots.size()];
    if (slot.epoch != c.epoch) {
      slot.epoch = c.epoch; slot.hash = hash; slot.length = static_cast<std::uint16_t>(key.length);
      std::memcpy(slot.key.data(), key.bytes.data(), key.length); slot.observed_groups = {};
      slot.observed_groups[class_index / 64] |= std::uint64_t{1} << (class_index % 64);
      ++c.occupied; ++count.unique; ++c.global_unique[metric]; found = true; break;
    }
    if (slot.hash == hash && slot.length == key.length && std::memcmp(slot.key.data(), key.bytes.data(), key.length) == 0) {
      const auto bit = std::uint64_t{1} << (class_index % 64);
      if (!(slot.observed_groups[class_index / 64] & bit)) { slot.observed_groups[class_index / 64] |= bit; ++count.unique; }
      found = true; break;
    }
  }
  if (!found) { c.overflow = true; count.unknown = true; }
  errno = saved_errno;
}
void field_byte_sink(void *, const node_observation::ByteEvent &event) noexcept {
  const int saved_errno = errno;
  auto &c = node_replay_collector;
  if (!c.active || c.window_kind != 2) { errno = saved_errno; return; }
  if ((event.metric != node_observation::ByteMetric::section && event.metric != node_observation::ByteMetric::complete) || c.byte_rows >= 257 || event.domain.size() > 128) { c.byte_overflow = true; errno = saved_errno; return; }
  if (event.metric == node_observation::ByteMetric::complete) {
    if (c.byte_complete) c.byte_overflow = true;
    c.byte_complete = true;
  }
  char domain[257]; node_hex(domain, reinterpret_cast<const unsigned char *>(event.domain.data()), event.domain.size());
  char buffer[1024];
  const int n = std::snprintf(buffer, sizeof(buffer),
      "FIELD_NODE_BYTES epoch=%llu index=%lld bank_tick=%lld target=%lld metric=%s domain_hex=%s codec_version=%lld bytes=%llu offset=%llu header_bytes=%llu directory_bytes=%llu source=EXISTING_ENCODER_SCALARS\n",
      static_cast<unsigned long long>(c.epoch), static_cast<long long>(observation_index),
      static_cast<long long>(observation_bank_tick), static_cast<long long>(observation_target),
      event.metric == node_observation::ByteMetric::section ? "section" : "complete", domain,
      static_cast<long long>(event.codec_version), static_cast<unsigned long long>(event.bytes),
      static_cast<unsigned long long>(event.offset), static_cast<unsigned long long>(event.header_bytes),
      static_cast<unsigned long long>(event.directory_bytes));
  node_emit(buffer, n, sizeof(buffer)); ++c.byte_rows;
  errno = saved_errno;
}
// PC-only 取样只由新显式诊断mode启用；signal/ring由独立测试TU所有。
// 以下仅正常上下文的窗口归属，不在handler内读World/分配/格式化。
bool pc_sampling_requested = false, pc_sampling_started = false;
bool pc_sampling_window_open = false, pc_sampling_window_failed = false;
unsigned pc_sampling_windows_started = 0, pc_sampling_windows_closed = 0;
void field_pc_window_begin(std::uint64_t epoch, std::int64_t target, unsigned kind,
                           std::uint64_t call_id, std::uint32_t depth) noexcept {
  if (!pc_sampling_requested) return;
  const int saved_errno = errno;
  if (!pc_sampling_started || pc_sampling_window_open ||
      !field_pc_sampler::begin_window(epoch, target, kind, call_id, depth))
    pc_sampling_window_failed = true;
  else { pc_sampling_window_open = true; ++pc_sampling_windows_started; }
  errno = saved_errno;
}
void field_pc_window_end() noexcept {
  if (!pc_sampling_requested) return;
  const int saved_errno = errno;
  if (pc_sampling_window_open) {
    field_pc_sampler::end_window(); pc_sampling_window_open = false;
    ++pc_sampling_windows_closed;
  } else pc_sampling_window_failed = true;
  errno = saved_errno;
}
class FieldPcSamplingBinding {
  bool enabled_ = false, stopped_ = false, okay_ = true;
public:
  explicit FieldPcSamplingBinding(bool enabled) noexcept : enabled_(enabled) {
    if (!enabled_) return;
    const int saved_errno = errno;
    pc_sampling_requested = true;
    pc_sampling_started = field_pc_sampler::start(5000);
    okay_ = pc_sampling_started;
    errno = saved_errno;
  }
  bool finish_report() noexcept {
    if (!enabled_) return true;
    const int saved_errno = errno;
    if (!stopped_) {
      const bool stopped = field_pc_sampler::stop();
      okay_ = okay_ && stopped; stopped_ = true; pc_sampling_started = false;
    }
    const bool written = field_pc_sampler::write_report(STDERR_FILENO);
    const bool complete = okay_ && written && !pc_sampling_window_failed &&
        !pc_sampling_window_open && pc_sampling_windows_started == 9 &&
        pc_sampling_windows_closed == 9 && !node_replay_collector.output_failed && bool(std::cerr);
    const char *marker = complete ?
        "FIELD_PC_PROFILE REPORT_COMPLETE NINE_WINDOWS SELF_ONLY INCLUSIVE_NOT_OBSERVED NOT_FIELD_PASS NOT_CURRENT\n" :
        "FIELD_PC_PROFILE REPORT_UNKNOWN NOT_PASS\n";
    const auto length = std::strlen(marker);
    node_emit(marker, static_cast<int>(length), length + 1);
    errno = saved_errno;
    return complete && !node_replay_collector.output_failed && bool(std::cerr);
  }
  ~FieldPcSamplingBinding() noexcept {
    const int saved_errno = errno;
    if (enabled_ && !stopped_) (void)field_pc_sampler::stop();
    if (enabled_) { pc_sampling_requested = false; pc_sampling_started = false; }
    errno = saved_errno;
  }
  FieldPcSamplingBinding(const FieldPcSamplingBinding &) = delete;
  FieldPcSamplingBinding &operator=(const FieldPcSamplingBinding &) = delete;
};

void field_node_window(const char *label, const char *edge, std::uint64_t call_id,
                       std::uint32_t depth) noexcept {
  const int saved_errno = errno;
  auto &c = node_replay_collector;
  if (!c.enabled) { errno = saved_errno; return; }
  if (!c.active && std::strcmp(edge, "BEGIN") == 0 && observation_target >= 1 && observation_target <= 3) {
    unsigned kind = 3;
    if (std::strcmp(stage, "advance_submit") == 0 && std::strcmp(label, "submit.apply") == 0) kind = 0;
    if (std::strcmp(stage, "checkpoint_after_submit") == 0 && std::strcmp(label, "world.validate") == 0) kind = 1;
    if (std::strcmp(stage, "checkpoint_after_submit") == 0 && std::strcmp(label, "checkpoint.serialize") == 0) kind = 2;
    if (kind < 3 && !c.sampled[static_cast<std::size_t>(observation_target)][kind] && c.epoch != std::numeric_limits<std::uint64_t>::max()) {
      c.active = true; c.window_kind = kind; ++c.epoch; c.call_id = call_id; c.depth = depth;
      c.sampled[static_cast<std::size_t>(observation_target)][kind] = true;
      typed_window_begin();
      catalog_window_begin();
      catalog_reuse_window_begin();
      c.class_count = c.occupied = c.byte_rows = 0; c.visits = {}; c.global_unique = {}; c.global_unknown = {}; c.overflow = c.byte_overflow = c.byte_complete = false;
      char buffer[512];
      const int n = std::snprintf(buffer, sizeof(buffer),
          "FIELD_NODE_WINDOW BEGIN epoch=%llu window=%s call_id=%llu depth=%u index=%lld bank_tick=%lld target=%lld exact_key_capacity=8192 max_key_bytes=256 collector_bytes=%zu coverage=INSTRUMENTED_LAYERS_ONLY\n",
          static_cast<unsigned long long>(c.epoch), node_window_name(kind), static_cast<unsigned long long>(call_id), depth,
          static_cast<long long>(observation_index), static_cast<long long>(observation_bank_tick),
          static_cast<long long>(observation_target), sizeof(c));
      node_emit(buffer, n, sizeof(buffer));
      field_pc_window_begin(c.epoch, observation_target, kind, call_id, depth);
    }
  } else if (c.active && call_id == c.call_id && depth == c.depth && std::strcmp(edge, "BEGIN") != 0) {
    field_pc_window_end(); // 先停PC取样，后输出原typed/node汇总。
    catalog_reuse_window_end(edge);
    catalog_window_end(edge);
    typed_window_end(edge);
    c.active = false; // 输出计数时不再接收任何业务节点。
    if (observation_target >= 1 && observation_target <= 3)
      c.completed[static_cast<std::size_t>(observation_target)][c.window_kind] = std::strcmp(edge, "END") == 0;
    for (std::size_t i = 0; i < c.class_count; ++i) {
      const auto &group = c.classes[i];
      char name[129], site[257], domain[129], kind[193];
      node_hex(name, reinterpret_cast<const unsigned char *>(group.name.data()), group.length);
      node_hex(site, reinterpret_cast<const unsigned char *>(group.site.data()), group.site_length);
      node_hex(domain, reinterpret_cast<const unsigned char *>(group.domain.data()), group.domain_length);
      node_hex(kind, reinterpret_cast<const unsigned char *>(group.kind.data()), group.kind_length);
      for (unsigned metric = 0; metric < 6; ++metric) {
        const auto &v = c.classes[i].metrics[metric];
        if (!v.visits) continue;
        char buffer[1536];
        const int n = std::snprintf(buffer, sizeof(buffer),
            "FIELD_NODE_COUNT epoch=%llu window=%s target=%lld class_hex=%s site_hex=%s domain_hex=%s kind_hex=%s schema=%lld metric=%s visits=%llu group_unique_observed=%llu unique_status=%s cross_group_sum=NOT_GLOBAL_UNIQUE context_unique=NOT_OBSERVED context_reason=NO_FULL_FRACTION_OR_LIFETIME_PROOF\n",
            static_cast<unsigned long long>(c.epoch), node_window_name(c.window_kind), static_cast<long long>(observation_target),
            name, site, domain, kind, static_cast<long long>(group.schema), node_metric_name(metric),
            static_cast<unsigned long long>(v.visits), static_cast<unsigned long long>(v.unique),
            c.overflow || v.unknown ? "UNKNOWN" : "EXACT_WITHIN_CURRENT_COMMON_EPOCH");
        node_emit(buffer, n, sizeof(buffer));
      }
    }
    for (unsigned metric = 0; metric < 6; ++metric) {
      char summary[512];
      const int n = std::snprintf(summary, sizeof(summary),
          "FIELD_NODE_GLOBAL epoch=%llu window=%s target=%lld metric=%s visits=%llu physical_unique_observed=%llu unique_status=%s context_unique=NOT_OBSERVED\n",
          static_cast<unsigned long long>(c.epoch), node_window_name(c.window_kind), static_cast<long long>(observation_target),
          node_metric_name(metric), static_cast<unsigned long long>(c.visits[metric]), static_cast<unsigned long long>(c.global_unique[metric]),
          c.overflow || c.global_unknown[metric] ? "UNKNOWN" : "EXACT_WITHIN_CURRENT_COMMON_EPOCH");
      node_emit(summary, n, sizeof(summary));
    }
    char buffer[768];
    const int n = std::snprintf(buffer, sizeof(buffer),
        "FIELD_NODE_WINDOW %s epoch=%llu window=%s call_id=%llu depth=%u target=%lld api_attempt=%llu resolved_entry=%llu loop_candidate_visit=%llu edge_traversal=%llu cache_hit=%llu cache_miss=%llu overflow=%d bytes_unknown=%d source_status=%s other_repeated_calls=NOT_SAMPLED\n",
        edge, static_cast<unsigned long long>(c.epoch), node_window_name(c.window_kind), static_cast<unsigned long long>(call_id), depth,
        static_cast<long long>(observation_target), static_cast<unsigned long long>(c.visits[0]), static_cast<unsigned long long>(c.visits[1]),
        static_cast<unsigned long long>(c.visits[2]), static_cast<unsigned long long>(c.visits[3]), static_cast<unsigned long long>(c.visits[4]),
        static_cast<unsigned long long>(c.visits[5]), int(c.overflow), int(c.byte_overflow || (c.window_kind == 2 && !c.byte_complete)),
        (c.visits[0] || c.visits[1] || c.visits[2] || c.visits[3] || c.visits[4] || c.visits[5]) ? "EXERCISED_INSTRUMENTED_LAYERS" : "NOT_EXERCISED");
    node_emit(buffer, n, sizeof(buffer));
  }
  errno = saved_errno;
}
class FieldNodeObservationBinding {
  node_observation::NodeBinding previous_node_{};
  node_observation::ByteBinding previous_byte_{};
  typed_observation::Binding previous_typed_{};
  bool active_ = false;
public:
  explicit FieldNodeObservationBinding(bool enabled) noexcept : active_(enabled) {
    if (active_) {
      node_replay_collector.enabled = true;
      previous_node_ = node_observation::exchange_node_binding({field_node_sink, nullptr});
      previous_byte_ = node_observation::exchange_byte_binding({field_byte_sink, nullptr});
      previous_typed_ = typed_observation::exchange_binding({field_typed_supply_sink, nullptr});
    }
  }
  ~FieldNodeObservationBinding() noexcept {
    if (active_) {
      node_replay_collector.enabled = false; node_replay_collector.active = false;
      typed_replay_collector.active = false;
      (void)typed_observation::exchange_binding(previous_typed_);
      (void)node_observation::exchange_byte_binding(previous_byte_);
      (void)node_observation::exchange_node_binding(previous_node_);
    }
  }
  FieldNodeObservationBinding(const FieldNodeObservationBinding &) = delete;
  FieldNodeObservationBinding &operator=(const FieldNodeObservationBinding &) = delete;
};


// 仅本测试终止诊断：固定缓冲，不 materialize、不恢复失败任务。
void diagnostic_text(const char *text) noexcept {
  const auto length = std::strlen(text);
  std::size_t sent = 0;
  while (sent < length) {
    const auto n = ::write(STDERR_FILENO, text + sent, length - sent);
    if (n <= 0)
      break;
    sent += static_cast<std::size_t>(n);
  }
}
void diagnostic_required(std::uint64_t value) noexcept {
  char buffer[32];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (converted.ec == std::errc{})
    (void)::write(STDERR_FILENO, buffer,
                  static_cast<std::size_t>(converted.ptr - buffer));
}
[[noreturn]] void diagnostic_terminate() noexcept {
  static bool entered = false;
  if (entered) {
    diagnostic_text("FIELD_THIRD_GATE TERMINATE_REENTERED\n");
    std::abort();
  }
  entered = true;
  diagnostic_text("FIELD_THIRD_GATE TERMINATE stage=");
  diagnostic_text(stage);
  diagnostic_text("\nFIELD_THIRD_GATE BACKTRACE_BEGIN\n");
  void *frames[64];
  const int count = ::backtrace(frames, 64);
  if (count > 0)
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
  diagnostic_text("FIELD_THIRD_GATE BACKTRACE_END\n");
  // terminate 因异常触发时存在隐含的当前 handler；throw; 重抛原异常，
  // 不使用可能构造 dependent exception 的 rethrow_exception。
  if (std::current_exception()) {
    diagnostic_text("FIELD_THIRD_GATE CURRENT_EXCEPTION present\n");
    try {
      throw;
    } catch (const checkpoint_job_detail::FailureException &failure) {
      diagnostic_text("FIELD_THIRD_GATE TERMINATE_CHECKPOINT code=");
      diagnostic_text(checkpoint_job_failure_code_name(failure.error.code()));
      diagnostic_text(" required=");
      diagnostic_required(failure.error.required());
      diagnostic_text("\n");
    } catch (...) {
      diagnostic_text("FIELD_THIRD_GATE TERMINATE_EXCEPTION other\n");
    }
  } else {
    diagnostic_text("FIELD_THIRD_GATE CURRENT_EXCEPTION absent\n");
  }
  std::abort();
}

void check(bool value, const std::string &why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
// 只显示原 RuleError 已有字段，不改拒绝分类、断言或继续条件。
std::string original_error_text(const RuleError &error) {
  return error.original.code + " " + error.original.path +
         (error.reason ? " reason=" + *error.reason : std::string{}) +
         (error.phase.empty() ? std::string{} : " phase=" + error.phase) +
         (error.domain.empty() ? std::string{} : " domain=" + error.domain) +
         (error.command.empty() ? std::string{} : " command=" + error.command) +
         (error.source_id.empty() ? std::string{} : " source_id=" + error.source_id);
}
template <class T> T must(Outcome<T> result, const std::string &why) {
  check(result.ok, why + ": " + original_error_text(result.error));
  return std::move(result.value);
}
void must(Status result, const std::string &why) {
  check(result.ok, why + ": " + original_error_text(result.error));
}
// 失败诊断只读提交前真实 bank；编码错误也仅打印，不覆盖原 submit 错误。
std::string diagnostic_moment(const WorldMoment &at) {
  const auto encoded = encode_world_moment(at);
  if (!encoded.ok)
    return "<" + encoded.error.original.code + ">";
  return std::to_string(encoded.value.half_day_tick) + "+" +
         encoded.value.sub_half_day.numerator + "/" +
         encoded.value.sub_half_day.denominator;
}
// 候选失败已经回滚：明确这是 prior bank，不把它冒充未发布失败候选。
void diagnostic_advance_failure(const WorldBundle &view, Integer target,
                                const RuleError &error) {
  const auto from = view.actual_phase_moment();
  std::cerr << "FIELD_THIRD_GATE ADVANCE_FAILURE prior_bank_m="
            << (from.ok ? diagnostic_moment(from.value)
                        : "<" + from.error.original.code + ">")
            << " requested_target=" << target << "+0/1"
            << " " << original_error_text(error) << '\n';
  if (!view.military_action_time || error.source_id.empty())
    return;
  const auto *fact = view.military_action_time->dynamic_action_facts.find(error.source_id);
  if (!fact) {
    std::cerr << "FIELD_THIRD_GATE PRIOR_MILITARY_ACTION absent source_id="
              << error.source_id << '\n';
    return;
  }
  std::cerr << "FIELD_THIRD_GATE PRIOR_MILITARY_ACTION source_id=" << fact->id
            << " command=" << fact->command << " actor=" << fact->actor_id
            << " phase=" << fact->phase
            << " started_at=" << diagnostic_moment(fact->started_at) << '\n';
  if (view.action_elapsed) {
    const auto *elapsed = find_fixed_elapsed(*view.action_elapsed, fact->id);
    if (elapsed) {
      std::cerr << "FIELD_THIRD_GATE PRIOR_ELAPSED source_id=" << fact->id
                << " last_settled_at=" << diagnostic_moment(elapsed->last_settled_at);
      if (const auto *running = std::get_if<ElapsedRunning>(&elapsed->timing))
        std::cerr << " due=" << diagnostic_moment(running->completion_due_at);
      std::cerr << '\n';
    }
  }
}
// 只测原命令冷解码与 typed 值复制，不冒真实玩家权限/候选写入已验收。
void field_army_exit_command_boundary_contract() {
  using V = BoundaryValue;
  const V good(V::Dict{{"battle_id", V("battle.probe")},
                         {"army_id", V("army.probe")},
                         {"exit_cell", V(Integer{4})}});
  auto decoded = must(decode_resident_command("FieldArmyExitSelect", good),
                      "typed FieldArmyExitSelect cold decode");
  auto copied = decoded;
  auto *original = std::get_if<FieldArmyExitSelect>(&decoded.data);
  const auto *copy = std::get_if<FieldArmyExitSelect>(&copied.data);
  check(original && copy && copied.kind == "FieldArmyExitSelect" &&
            copy->battle_id == "battle.probe" && copy->army_id == "army.probe" &&
            copy->exit_cell == 4, "selected exit typed payload preserved");
  original->battle_id = "different";
  check(copy->battle_id == "battle.probe", "selected exit payload owns copied IDs");
  auto reject = [&](V value, const char *code) {
    auto failed = decode_resident_command("FieldArmyExitSelect", value);
    check(!failed.ok && failed.error.original.code == code,
          std::string("selected exit rejects original cold error ") + code);
  };
  V negative = good;
  negative.find("exit_cell")->data = Integer{-1};
  reject(std::move(negative), "InvalidFieldArmyExitSelect");
  V duplicate = good;
  duplicate.as<V::Dict>()->emplace_back("army_id", V("other"));
  reject(std::move(duplicate), "DuplicateFieldArmyExitSelectKey");
  reject(V(V::Dict{{"battle_id", V("battle.probe")},
                  {"army_id", V("army.probe")}}), "MissingResidentField");
  V unknown = good;
  unknown.as<V::Dict>()->emplace_back("side", V("not-an-army"));
  reject(std::move(unknown), "UnsupportedResidentField");
  V wrong_type = good;
  wrong_type.find("exit_cell")->data = StableId("4");
  reject(std::move(wrong_type), "InvalidResidentInteger");
}
// 仅对输入副本运行真实军政目录解码；不改守门作者源、军令或真实世界。
// 全部拥有值在同一工程 job 内消费，覆盖显式活动集与被动定义的区别。
void siege_selection_contract(const BoundaryValue &input,
                              const StableId &scenario_id) {
  using V = BoundaryValue;
  const auto &root = boundary::field(input, "data", "catalog");
  const auto &definitions = boundary::object(
      boundary::field(root, "siege_topologies", "data"), "siege_topologies");
  check(definitions.size() >= 2, "selection probe retains original topologies");
  std::vector<StableId> all;
  V::Array all_ids;
  for (const auto &row : definitions) {
    all.push_back(row.first);
    all_ids.emplace_back(row.first);
  }
  auto scenario = [&](V &copy) -> V & {
    auto *scenarios = copy.find("scenarios");
    check(scenarios && scenarios->find(scenario_id), "probe original scenario");
    return *scenarios->find(scenario_id);
  };
  auto selected = [&](V ids) {
    V copy = root;
    auto *enabled = scenario(copy).find("enabled");
    check(enabled && enabled->as<V::Dict>(), "probe explicit snapshot");
    auto *table = enabled->find("siege_topologies");
    check(table, "probe original selection table");
    *table = std::move(ids);
    return copy;
  };
  auto probe = [&](const V &copy, const std::vector<StableId> &expected,
                   const StableId &error = {}) {
    auto job = CheckpointJobBudget::create(
        engineering_checkpoint_capacity_profile(), engineering_slot_capacity_profile());
    check(job.ok, "selection probe original engineering job");
    auto resource = checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
    check(resource.ok, "selection probe original candidate resource");
    ScopedCheckpointMemory memory(resource.value);
    CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(), job.value);
    ScopedCheckpointBoundaryDecode fields(budget, job.value, resource.value);
    try {
      auto result = decode_military_catalog(copy, scenario_id);
      if (!error.empty()) {
        check(!result.ok && result.error.original.code == error,
              "selection exact rejection " + error + ": " + result.error.original.code);
        return;
      }
      check(result.ok, "selection real decoder: " + result.error.original.code);
      std::vector<StableId> actual;
      for (const auto &topology : result.value.siege_topologies)
        actual.push_back(topology.id);
      check(actual == expected, "selection stable runtime topology projection");
      for (const auto &stronghold : result.value.strongholds)
        check(copy.find("siege_topologies")->find(stronghold.topology_id),
              "passive stronghold reference remains in complete author definitions");
    } catch (checkpoint_job_detail::FailureException &failure) {
      if (error.empty()) throw;
      // 原 job 的边界失败保持自己的精确规则码；资源失败不能冒充负例通过。
      failure.error.try_materialize_rule_error();
      const auto *actual = failure.error.rule_error();
      check(actual && actual->original.code == error,
            "selection original job exact rejection " + error);
    }
  };
  std::reverse(all_ids.begin(), all_ids.end());
  probe(selected(V{all_ids}), all); // 数组逆序不能重排原目录。
  probe(selected(V{V::Array{}}), {});
  probe(selected(V{V::Array{V{all.back()}}}), {all.back()});
  auto missing_table = selected(V{V::Array{}});
  auto &enabled_rows = *scenario(missing_table).find("enabled")->as<V::Dict>();
  enabled_rows.erase(std::remove_if(enabled_rows.begin(), enabled_rows.end(),
                [](const auto &row) { return row.first == "siege_topologies"; }), enabled_rows.end());
  probe(missing_table, {});
  V legacy = root;
  auto &legacy_rows = *scenario(legacy).as<V::Dict>();
  legacy_rows.erase(std::remove_if(legacy_rows.begin(), legacy_rows.end(),
                [](const auto &row) { return row.first == "enabled"; }), legacy_rows.end());
  probe(legacy, all);
  probe(selected(V{V::Array{V{StableId{}}}}), {}, "InvalidScenarioEnabled");
  probe(selected(V{V::Array{V{all.front()}, V{all.front()}}}), {},
        "InvalidScenarioEnabled");
  probe(selected(V{V::Array{V{StableId{"selection_probe::unknown_topology"}}}}), {},
        "UnknownScenarioEnabled");
  probe(selected(V{Integer{1}}), {}, "InvalidScenarioEnabled");
  probe(selected(V{V::Array{V{Integer{1}}}}), {}, "InvalidScenarioEnabled");
  auto wrong_enabled = selected(V{V::Array{}});
  *scenario(wrong_enabled).find("enabled") = V{Integer{1}};
  probe(wrong_enabled, {}, "InvalidScenarioEnabled");
  auto bad_passive = selected(V{V::Array{}});
  *bad_passive.find("siege_topologies")->find(all.front())->find("id") =
      V{StableId{"selection_probe::mismatched_definition"}};
  probe(bad_passive, {}, "InvalidSiegeTopology");
}
std::string text(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  check(bool(in), "read " + path.string());
  std::ostringstream out;
  out << in.rdbuf();
  check(!in.bad(), "read bytes " + path.string());
  return out.str();
}
void put(const fs::path &path, const std::string &bytes) {
  check(!fs::exists(path), "refuse overwrite " + path.string());
  std::ofstream out(path, std::ios::binary);
  check(bool(out), "create " + path.string());
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  check(bool(out), "write " + path.string());
}
void put_bank(const fs::path &path, const Bytes &bytes) {
  put(path,
      std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}
// NEW诊断bank：直接写原Bytes，不调用save_cut/完整导出/第二次编码。
struct NodeReplayIoError final : std::exception {
  const char *what() const noexcept override { return "NEW_REPLAY_BANK_IO_FAILURE"; }
};
void put_new_replay_bytes(const fs::path &path, const unsigned char *data, std::size_t size) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) throw NodeReplayIoError{};
  bool good = true;
  std::size_t sent = 0;
  while (sent < size) {
    const auto n = ::write(fd, data + sent, size - sent);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { good = false; break; }
    sent += static_cast<std::size_t>(n);
  }
  if (good && ::fsync(fd) != 0) good = false;
  if (::close(fd) != 0) good = false;
  if (!good) throw NodeReplayIoError{};
  const int directory = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
  if (directory < 0) throw NodeReplayIoError{};
  const bool synced = ::fsync(directory) == 0;
  const bool closed = ::close(directory) == 0;
  if (!synced || !closed) throw NodeReplayIoError{};
}
void put_new_replay_text(const fs::path &path, const std::string &value) {
  put_new_replay_bytes(path, reinterpret_cast<const unsigned char *>(value.data()), value.size());
}
bool save_new_replay_bank(const fs::path &output, Integer index, Integer actual_tick,
                          Integer target, const Bytes &bytes, const std::string &summary_sha,
                          const FieldWorldSizeSnapshot &observed) {
  // 只对已存在719c/48933固定prefix的16个同view表值作可比检查；不是旧bank字节证明。
  static constexpr std::array<std::array<std::size_t, 16>, 3> expected{{
      {{12,4,11,2,0,6,2,0,0,0,0,0,0,0,0,0}},
      {{22,9,11,2,0,6,4,0,0,0,0,0,0,0,0,0}},
      {{31,13,11,2,0,6,6,0,0,0,0,0,0,0,0,0}}
  }};
  bool same = observed.captured && index >= 1 && index <= 3 && actual_tick == index && target == index;
  if (same) {
    const auto &row = expected[static_cast<std::size_t>(index - 1)];
    for (std::size_t i = 0; i < row.size(); ++i)
      if (!observed.rows[i].root_present || observed.rows[i].total != row[i]) same = false;
  }
  const auto name = "NEW_M" + std::to_string(index);
  const auto bank_sha = picosha2::hash256_hex_string(bytes);
  put_new_replay_bytes(output / (name + ".bank"), bytes.data(), bytes.size());
  std::ostringstream metadata;
  metadata << "kind=NEW_CONTROLLED_REPLAY_NOT_RECOVERED_SNAPSHOT\nindex=" << index
      << "\nbank_tick=" << actual_tick << "\ntarget=" << target << "\nbytes=" << bytes.size()
      << "\nbank_sha256=" << bank_sha << "\nnew_full_summary_sha256=" << summary_sha
      << "\nold_bank_sha256=NOT_AVAILABLE\nold_full_summary_sha256=NOT_AVAILABLE"
      << "\ncomparison_prefix_sha256=4eef9eaec00c08ee0b67f465b628be8916b01a399efc1cdd957cd2d44d9118e4"
      << "\ntable_comparison=" << (same ? "MATCH_16_COUNTS_NOT_BYTE_EQUIVALENCE" : "MISMATCH")
      << "\ncurrent_promoted=false\nformal_gate_accepted=false\n";
  put_new_replay_text(output / (name + ".meta"), metadata.str());
  std::cerr << "FIELD_NODE_REPLAY BANK_SAVED index=" << index << " bank_tick=" << actual_tick
            << " target=" << target << " bytes=" << bytes.size() << " bank_sha256=" << bank_sha
            << " new_summary_sha256=" << summary_sha << " table_comparison=" << (same ? "MATCH" : "MISMATCH")
            << " old_bank_sha=NOT_AVAILABLE NEW_REPLAY_NOT_RECOVERED\n" << std::flush;
  return same;
}

Bytes bank(const fs::path &path) {
  auto value = text(path);
  return Bytes(value.begin(), value.end());
}
std::unique_ptr<WorldBundle>
inspect(const Bytes &bytes, std::shared_ptr<const PreparedCatalog> catalog) {
  stage = "inspect_decode";
  auto seed =
      must(FIELD_OBSERVE("inspect.decode", decode_world_checkpoint(bytes, *catalog)), "decode actual bank");
  stage = "inspect_construct";
  auto view =
      FIELD_OBSERVE("inspect.construct", std::make_unique<WorldBundle>(std::move(catalog), std::move(seed)));
  // 与正式冷恢复相同：只由本候选原源恢复派生值/索引，不重放业务或补造 wake。
  stage = "inspect_restore_derived";
  must(FIELD_OBSERVE("inspect.transport_restore", restore_native_transport_derived_values_in_candidate(*view)),
       "whole actual cold transport derived values");
  must(FIELD_OBSERVE("inspect.director_restore", restore_native_complete_director_wake_cache_in_candidate(*view)),
       "whole actual cold director derived cache");
  stage = "inspect_validate";
  must(FIELD_OBSERVE("inspect.validate", view->validate()), "whole actual cold source validation");
  return view;
}
std::string full_json(const WorldBundle &view) {
  stage = "export_world";
  check(!view.has_active_read_phase(),
        "cold export is not masked by an outer inspection phase");
  auto exported = must(FIELD_OBSERVE("export.world", export_world_state(view)), "full world export");
  check(!view.has_active_read_phase(),
        "cold export releases its own source validation phases");
  stage = "export_json";
  return must(FIELD_OBSERVE("export.json", render_boundary_json(exported)), "full world JSON");
}
std::string digest(const WorldBundle &view) {
  return picosha2::hash256_hex_string(full_json(view));
}
struct Trace {
  Integer index = 0, target = 0;
  std::string sha;
};
std::vector<Trace> load_trace(const fs::path &path) {
  std::istringstream in(text(path));
  std::vector<Trace> rows;
  Trace row;
  while (in >> row.index >> row.target >> row.sha) {
    check(row.index == Integer(rows.size()) && row.sha.size() == 64,
          "trace exact ordinal/hash");
    rows.push_back(row);
  }
  check(in.eof() && !rows.empty(), "complete warm trace");
  return rows;
}
void save_cut(const fs::path &dir, const std::string &name, Integer index,
              const Bytes &bytes, const WorldBundle &view) {
  const auto json = full_json(view);
  put_bank(dir / (name + ".bank"), bytes);
  put(dir / (name + ".json"), json);
  put(dir / (name + ".meta"),
      std::to_string(index) + " " + picosha2::hash256_hex_string(json) + "\n");
}
Trace load_cut(const fs::path &dir, const std::string &name) {
  std::istringstream in(text(dir / (name + ".meta")));
  Trace row;
  check(bool(in >> row.index >> row.sha) && row.index >= 0 &&
            row.sha.size() == 64,
        "cut index/hash");
  return row;
}
Bytes advance(ResidentWorld &world, const WorldBundle &view, Integer target) {
  stage = "checkpoint_before_preview";
  auto before = must(FIELD_OBSERVE(stage, world.checkpoint()), "bank before preview");
  stage = "advance_identity";
  const auto id = must(world.identity(), "actual command identity");
  const auto &actor = view.common.state().metadata.player_actor_id;
  const auto *sequence = view.common.state().request_sequences.find(actor);
  check(sequence && *sequence >= 0 && *sequence < 2147483647,
        "actual player request watermark");
  CommandEnvelope envelope;
  envelope.world_id = id.world_id;
  envelope.execution_epoch = id.execution_epoch;
  envelope.expected_revision = id.revision;
  envelope.actor_id = actor;
  envelope.request_seq = *sequence + 1;
  envelope.payload = std::make_shared<const CommandPayload>(
      CommandPayload{"advance", AdvanceCommand{WorldMoment{target, {}}}});
  stage = "advance_preview";
  must(FIELD_OBSERVE("advance.preview", world.preview(envelope)), "real advance preview");
  stage = "checkpoint_after_preview";
  check(before == must(FIELD_OBSERVE(stage, world.checkpoint()), "bank after preview"),
        "preview is byte-inert");
  stage = "advance_submit";
  auto submitted = FIELD_OBSERVE(stage, world.submit(envelope));
  if (!submitted.ok) {
    // 补充诊断若自身失败，仍由原 submit 错误终止，不让诊断覆盖业务首错。
    try {
      diagnostic_advance_failure(view, target, submitted.error);
    } catch (...) {
      diagnostic_text("FIELD_THIRD_GATE ADVANCE_FAILURE_DIAGNOSTIC_UNAVAILABLE\n");
    }
  }
  auto accepted = must(std::move(submitted), "real advance submit");
  stage = "checkpoint_after_submit";
  auto after = must(FIELD_OBSERVE(stage, world.checkpoint()), "bank after submit");
  stage = "advance_replay";
  auto replay = must(FIELD_OBSERVE(stage, world.submit(envelope)), "same request replay");
  check(accepted->result_id == replay->result_id,
        "replay exact original result");
  stage = "checkpoint_after_replay";
  check(after == must(FIELD_OBSERVE(stage, world.checkpoint()), "bank after replay"),
        "replay is byte-inert");
  return after;
}
template <class R> std::set<StableId> roster_ids(const R &r) {
  std::set<StableId> ids;
  for (const auto *group :
       {&r.fit, &r.wounded, &r.dead, &r.captured, &r.deserted})
    for (const auto &id : *group)
      check(ids.insert(id).second,
            "one original member in exactly one roster partition");
  return ids;
}
// 每笔真实发生的 actor_change 必须有自己的原身体及 S1 后果，
// 不要求未经执行证明的随机伤死数量，也不用今天身体值倒证旧段。
void actor_effects(const WorldBundle &view, const MilitaryCasualtyFact &source,
                   const FieldSegmentSettlement &segment) {
  check(view.actor_foundation && view.character_injury_sources,
        "real S4 injury roots");
  const auto &conditions =
      view.actor_foundation->character_condition_extensions;
  for (std::size_t ordinal = 0; ordinal < source.actor_changes.size();
       ++ordinal) {
    const auto &change = source.actor_changes[ordinal];
    StableId body_id;
    std::size_t count = 0;
    if (change.status == "wounded") {
      for (const auto &entry :
           view.character_injury_sources->applications.rows()) {
        const auto *battle = std::get_if<CharacterBattleReceiptSourceV1>(
            &entry.second.original_source);
        if (entry.second.actor_id != change.actor_id || !battle ||
            battle->source_schema != 1 ||
            battle->original_receipt.domain != "military" ||
            battle->original_receipt.kind != "military_casualties" ||
            battle->original_receipt.id != source.id)
          continue;
        const auto *receipt = conditions.applied_receipts.find(entry.first);
        check(receipt && receipt->actor_id == change.actor_id &&
                  receipt->capability_application &&
                  must(compare_world_moments(receipt->effective_at.exact_moment,
                                             segment.applied_at),
                       "injury actual M") == 0,
              "wounded actual Condition and Growth capability");
        ++count;
      }
      check(count == 1, "each actual wounded receipt consumed once");
      continue;
    }
    if (change.status == "dead") {
      for (const auto &entry : view.family_life.deaths.rows()) {
        const auto &death = entry.second;
        if (death.actor_id != change.actor_id || !death.precise)
          continue;
        const auto *cause =
            std::get_if<MilitaryDeathCause4>(&death.precise->cause);
        if (!cause || cause->casualty_fact_id != source.id)
          continue;
        check(cause->actor_id == change.actor_id &&
                  cause->battle_id == source.battle_id &&
                  must(compare_world_moments(cause->occurrence_at,
                                             segment.applied_at),
                       "death actual M") == 0,
              "real Life4 cause");
        body_id = entry.first;
        ++count;
      }
      check(count == 1, "each actual death has one Life4 source");
      std::size_t receipts = 0;
      for (const auto &entry : conditions.applied_receipts.rows())
        if (entry.second.actor_id == change.actor_id &&
            entry.second.life_transition_ref ==
                std::optional<StableId>{body_id}) {
          check(bool(entry.second.capability_application),
                "death actual Growth capability");
          ++receipts;
        }
      check(receipts == 1, "death actual Condition termination receipt");
    } else {
      check(change.status == "captured" || change.status == "deserted",
            "no silently ignored actor status");
      for (const auto &entry : conditions.capacity_transitions.rows()) {
        const auto *cause =
            std::get_if<MilitaryAvailabilityCauseV2>(&entry.second.cause_ref);
        if (entry.second.actor_id != change.actor_id || !cause ||
            cause->source_schema != 1 ||
            cause->source_ordinal != Integer(ordinal) ||
            cause->casualty_source.domain != "military" ||
            cause->casualty_source.kind != "military_casualties" ||
            cause->casualty_source.id != source.id)
          continue;
        check(!entry.second.to &&
                  must(compare_world_moments(
                           entry.second.effective_at.exact_moment,
                           segment.applied_at),
                       "availability actual M") == 0,
              "actual captured/deserted capacity result");
        body_id = entry.first;
        ++count;
      }
      check(count == 1, "each actual captured/deserted change consumed once");
    }
    check(bool(view.group_movement_executions), "actual S1 handoff root");
    std::size_t handoffs = 0;
    for (const auto &entry :
         view.group_movement_executions->member_loss_handoffs.rows()) {
      const auto &handoff = entry.second;
      if (handoff.actor_id != change.actor_id ||
          handoff.actual_casualty.domain != "military" ||
          handoff.actual_casualty.kind != "military_casualties" ||
          handoff.actual_casualty.id != source.id ||
          handoff.casualty_ordinal != Integer(ordinal))
        continue;
      check(must(compare_world_moments(handoff.occurred_at, segment.applied_at),
                 "handoff actual M") == 0,
            "same segment S1 handoff");
      if (change.status == "dead") {
        const auto *proof =
            std::get_if<GroupMemberDeathApplication>(&handoff.actual_body);
        check(proof && proof->actual_death.id == body_id,
              "S1 binds actual DeathRef");
      } else if (change.status == "captured") {
        const auto *proof =
            std::get_if<GroupMemberCaptivityApplication>(&handoff.actual_body);
        check(proof && proof->actual_capacity_transition.id == body_id,
              "S1 binds actual captivity transition");
      } else {
        const auto *proof =
            std::get_if<GroupMemberDesertionApplication>(&handoff.actual_body);
        check(proof && proof->actual_capacity_transition.id == body_id,
              "S1 binds actual desertion transition");
      }
      ++handoffs;
    }
    check(handoffs == 1, "each actual loss adopted by original S1 once");
  }
}
struct Evidence {
  bool contact = false, segment = false, moved = false;
  Integer wounded = 0, dead = 0, captured = 0, deserted = 0;
};
Evidence evidence(WorldBundle &view) {
  stage = "field_evidence";
  Evidence out;
  field_observe_event_size_scope();
  check(view.field_battle && view.movement_exact,
        "actual selected Field/S1 roots");
  if (view.field_battle->contacts.empty())
    return out;
  const auto now = must(view.actual_phase_moment(), "evidence actual clock");
  auto phase = must(view.enter_phase_read_context(
      "field_evidence_read", "field_battle", {now, now}, now),
      "same-world evidence read phase");
  auto owner =
      must(view.complete_exact_time_bindings(), "original complete read owner");
  check(bool(owner), "actual complete owner");
  auto field = must(NativeFieldWorldScope::create(view, owner->reads()),
                    "same-world Field read scope");
  const auto *rosters = field->abilities().original_rosters;
  check(rosters, "actual original roster source");
  std::set<StableId> contact_ids, terminal_segments;
  for (const auto &[id, contact] : view.field_battle->contacts) {
    const auto *original =
        std::get_if<FieldExactGroupContactOrigin>(&contact.source);
    check(original && original->actual_contact.domain == "travel" &&
              original->actual_contact.kind == "movement_contact",
          "real S1 two-army contact not legacy fixture contact");
    check(view.movement_exact->contacts.find(original->actual_contact.id),
          "actual exact movement contact row");
    contact_ids.insert(original->actual_contact.id);
    std::set<StableId> armies, organizations;
    for (const auto &binding : contact.original_bindings) {
      check(armies.insert(binding.army_id).second,
            "two distinct original armies");
      const ArmyRecord *army = nullptr;
      for (const auto &item : view.military.armies)
        if (item.id == binding.army_id) {
          check(!army, "unique actual army");
          army = &item;
        }
      check(army && !army->formation_source.empty(),
            "actual formation source for both armies");
      organizations.insert(army->organization_id);
    }
    check(organizations.size() == 2,
          "two original organizations formed armies");
    out.contact = true;
    const auto found = view.field_battle->sessions.find(contact.battle_id);
    check(found != view.field_battle->sessions.end(), "actual Field session");
    const auto &session = found->second;
    field_observe_session_event_size(session);
    for (const auto &[formation_id, formation] : session.formations) {
      auto original_roster =
          must(read_field_original_participation_roster(
                   view.common, view.military, *view.catalog->military,
                   *view.field_battle, session, formation, *rosters),
               "original participation roster");
      check(roster_ids(original_roster) == roster_ids(formation),
            "all original members conserved across five actual partitions");
      must(validate_field_local_roster_outputs(
               view.common, view.military, *view.catalog->military,
               *view.field_battle, session, formation, *rosters),
           "original event-by-event roster replay");
    }
    for (const auto &segment : session.settlements) {
      check(!segment.fact_id.empty() && !segment.casualty_fact_ids.empty(),
            "nonempty original segment source");
      const auto *head = view.common.state().facts.find(segment.fact_id);
      check(head && head->domain == "military" &&
                head->kind == "field_battle_segment",
            "actual segment Common head");
      bool actual_roster = false;
      for (const auto &casualty_id : segment.casualty_fact_ids) {
        const MilitaryCasualtyFact *casualty = nullptr;
        for (const auto &row : view.military.casualty_facts)
          if (row.id == casualty_id) {
            check(!casualty, "unique actual casualty source");
            casualty = &row;
          }
        check(casualty && casualty->battle_id == session.battle_id,
              "segment actual casualty source belongs to battle");
        actor_effects(view, *casualty, segment);
        for (const auto &change : casualty->actor_changes) {
          if (change.status == "wounded")
            ++out.wounded;
          else if (change.status == "dead")
            ++out.dead;
          else if (change.status == "captured")
            ++out.captured;
          else if (change.status == "deserted")
            ++out.deserted;
        }
        actual_roster |=
            !casualty->units.empty() || !casualty->actor_changes.empty();
      }
      check(actual_roster, "segment has actual original roster/actor result");
      out.segment = true;
      if (session.state == FieldBattleState::Ended && session.result &&
          segment.settled_at.tick == session.result->ended.tick &&
          segment.settled_at.t == session.result->ended.t)
        terminal_segments.insert(segment.fact_id);
    }
  }
  // 完整 continuation 校验由上面的 World.validate 原门承担；此处再断言
  // 本场真实接触解封了一个新 unit，且该 unit 自己确有正位移服务跨度。
  for (const auto &entry : view.movement_exact->contact_continuations.rows()) {
    const auto &continuation = entry.second;
    if (!contact_ids.contains(continuation.contact_ref))
      continue;
    if (continuation.held || continuation.new_unit_ref.empty())
      continue;
    const auto *head = view.common.state().facts.find(continuation.id);
    check(head && head->domain == "travel" &&
              head->kind == "movement_contact_continuation",
          "actual continuation Common head");
    check(continuation.actual_resolution_source.domain == "military" &&
              continuation.actual_resolution_source.kind ==
                  "field_battle_segment" &&
              continuation.resolution_schema == 1 &&
              terminal_segments.contains(
                  continuation.actual_resolution_source.id),
          "winner resume uses this battle actual terminal segment");
    const auto *resolution = view.common.state().facts.find(
        continuation.actual_resolution_source.id);
    check(resolution &&
              resolution->domain ==
                  continuation.actual_resolution_source.domain &&
              resolution->kind == continuation.actual_resolution_source.kind &&
              continuation.resolution_schema > 0,
          "actual resolution source not a clock increment");
    const auto *old =
        view.movement_exact->units.find(continuation.closed_unit_ref);
    const auto *next =
        view.movement_exact->units.find(continuation.new_unit_ref);
    check(old && next && old->id != next->id &&
              next->ordinal == old->ordinal + 1,
          "real next travel unit");
    const auto *group = std::get_if<GroupMovementExecution>(&next->execution);
    const auto *old_group =
        std::get_if<GroupMovementExecution>(&old->execution);
    const auto *carrier =
        group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
    check(group && old_group && carrier &&
              group->journey_id == old_group->journey_id,
          "continuation remains original Army group journey");
    bool original_winner = false;
    for (const auto &[contact_id, contact] : view.field_battle->contacts) {
      const auto *origin =
          std::get_if<FieldExactGroupContactOrigin>(&contact.source);
      if (!origin || origin->actual_contact.id != continuation.contact_ref)
        continue;
      const auto &session = view.field_battle->sessions.at(contact.battle_id);
      if (!session.result || session.state != FieldBattleState::Ended ||
          session.result->winner_side < 0)
        continue;
      bool actual_terminal = false;
      for (const auto &segment : session.settlements)
        if (segment.fact_id == continuation.actual_resolution_source.id &&
            segment.settled_at.tick == session.result->ended.tick &&
            segment.settled_at.t == session.result->ended.t)
          actual_terminal = true;
      for (const auto &[formation_id, formation] : session.formations)
        if (actual_terminal && formation.side == session.result->winner_side &&
            formation.army_id == carrier->army_id)
          original_winner = true;
    }
    check(original_winner,
          "resumed carrier is the actual winning army of this contact");
    for (const auto &span : next->activity_spans)
      if (must(compare_world_moments(span.span.from, span.span.to),
               "actual movement span") < 0 &&
          must(compare_fractions(span.cursor_from, span.cursor_to),
               "actual movement cursor") < 0 &&
          must(compare_world_moments(continuation.occurred_at, span.span.from),
               "movement after actual resume") <= 0)
        out.moved = true;
  }
  return out;
}
// 原安装闭包使用同刻预测；以真初始 bank 的同World lease核两条已选军旅口，
// 不以空provider替代来源，也不把零时查询变成可写的正片。
void selected_military_prediction_contract(WorldBundle &view) {
  const auto now = must(view.actual_phase_moment(), "prediction actual clock");
  check(now.half_day_tick > std::numeric_limits<Integer>::min() &&
            now.half_day_tick < std::numeric_limits<Integer>::max(),
        "prediction counterexample has representable neighbouring ticks");
  auto phase = must(view.enter_phase_read_context(
      "field_selected_prediction_contract", "military", {now, now}, now),
      "same-world prediction read phase");
  auto owner = must(view.complete_exact_time_bindings(),
                    "same-world prediction owner");
  check(bool(owner), "actual selected military owner");
  auto lease = must(owner->read_military(now), "original military read lease");
  check(bool(lease), "actual military prediction lease");
  const auto x = lease->bindings();
  check(&x.common == &view.common && &x.military == &view.military &&
            x.action_time && x.office && x.formation && !x.training &&
            !x.returning && !x.resupply && !x.naval_repair && !x.duty,
        "field-only exact office/formation selection and true roots");
  const WorldMoment before{now.half_day_tick - 1, now.sub_half_day};
  const WorldMoment after{now.half_day_tick + 1, now.sub_half_day};
  auto verify = [&](auto predict, const char *name, const char *clock_error) {
    auto point = must(predict(now, now), std::string(name) + " same-point query");
    check(point.empty(), std::string(name) + " validated bank has no pending due");
    (void)must(predict(now, after), std::string(name) + " forward query");
    auto reverse = predict(now, before);
    check(!reverse.ok && reverse.error.original.code == "NonPositiveMomentSpan",
          std::string(name) + " reverse query retains original rejection");
    auto wrong_now = predict(after, after);
    check(!wrong_now.ok && wrong_now.error.original.code == clock_error,
          std::string(name) + " cannot replace actual World clock");
  };
  verify([&](const WorldMoment &from, const WorldMoment &to) {
    return predict_exact_office_events(x.common, x.military, *x.action_time,
                                      *x.office, from, to);
  }, "office", "ExactOfficeCurrentMomentRequired");
  verify([&](const WorldMoment &from, const WorldMoment &to) {
    return predict_exact_formation_events(x.common, x.military, *x.action_time,
                                         *x.formation, from, to);
  }, "formation", "ExactFormationCurrentMomentRequired");
  auto zero_writer_span = make_moment_span(now, now);
  check(!zero_writer_span.ok &&
            zero_writer_span.error.original.code == "NonPositiveMomentSpan",
        "positive Advance span still rejects same-point writer");
}
// 只读同一真实初始 bank 的拥有根；反例只改局部借用参数，不写入世界。
// 第二 clock 来自同 bank 冷解出的另一真实 World，不能以空壳 provider 造反例。
void selected_field_cause_contract(
    WorldBundle &view, const Bytes &bytes,
    const std::shared_ptr<const PreparedCatalog> &catalog,
    const BoundaryValue &input, const StableId &scenario_id) {
  const auto now = must(view.actual_phase_moment(), "cause actual clock");
  auto phase = must(view.enter_phase_read_context(
      "field_selected_cause_contract", "field_battle", {now, now}, now),
      "same-world cause read phase");
  auto owner = must(view.complete_exact_time_bindings(),
                    "actual family cause owner");
  check(owner && owner->reads().family_life_age,
        "actual selected family age owner");
  const auto body = must(owner->reads().character_conditions.sources(),
                         "actual body cause sources");
  const auto age = owner->reads().family_life_age->bindings();
  check(view.field_battle && body.life_sources &&
            age.life_sources.field_battles == &*view.field_battle &&
            age.life_sources.field_battles == body.life_sources->field_battles &&
            age.life_sources.field_application_clock &&
            age.life_sources.field_application_clock ==
                body.life_sources->field_application_clock,
        "same real Field root and application clock reach age and body");
  must(validate_family_life4_original_military_selection(view.common, age.life_sources),
       "real family age Field source selection");
  must(validate_family_life4_original_military_selection(view.common, *body.life_sources),
       "real body Field source selection");
  auto reject = [&](const FamilyLifeCauseSources4 &sources, const StableId &code) {
    const auto result = validate_family_life4_original_military_selection(view.common, sources);
    check(!result.ok && result.error.original.code == code,
          "family source exact counterexample " + code);
  };
  auto missing_root = age.life_sources;
  missing_root.field_battles = nullptr;
  reject(missing_root, "FamilyLifeSelectedFieldRootRequired");
  auto missing_clock = age.life_sources;
  missing_clock.field_application_clock = nullptr;
  reject(missing_clock, "FamilyLifeSelectedFieldRootRequired");
  auto another = inspect(bytes, catalog);
  const auto other_now = must(another->actual_phase_moment(), "second World actual clock");
  auto other_phase = must(another->enter_phase_read_context(
      "field_selected_foreign_cause_contract", "field_battle",
      {other_now, other_now}, other_now), "second World cause read phase");
  auto other_owner = must(another->complete_exact_time_bindings(),
                          "second real World cause owner");
  check(other_owner && other_owner->reads().family_life_age,
        "second real World selected age owner");
  auto foreign_clock = age.life_sources;
  foreign_clock.field_application_clock =
      other_owner->reads().family_life_age->bindings().life_sources.field_application_clock;
  reject(foreign_clock, "FamilyLifeFieldClockBindingRequired");
  // 永久伤效不是空表缺席：实际初始 bank 的全身体来源正向可读，原缺席门仍拒8。
  must(validate_native_permanent_injury_calendar_sources(view, body),
       "actual selected permanent injury calendar sources");
  const auto old_absence = validate_native_condition_due_absence(view, body);
  check(!old_absence.ok && old_absence.error.original.code ==
                                "ConditionDueOriginalTypedProducerRequired",
        "selected injury not admitted through negative absence");
  auto reject_injury = [&](const CharacterConditionSources &sources,
                           const StableId &code) {
    const auto result =
        validate_native_permanent_injury_calendar_sources(view, sources);
    check(!result.ok && result.error.original.code == code,
          "permanent injury exact source rejection " + code);
  };
  const auto foreign_body = must(
      other_owner->reads().character_conditions.sources(),
      "second real World injury body sources");
  reject_injury(foreign_body, "ConditionDueSameWorldSourcesRequired");
  auto missing_injury = body;
  missing_injury.injury_sources = nullptr;
  reject_injury(missing_injury, "ConditionDueSameWorldSourcesRequired");
  auto missing_injury_clock = body;
  missing_injury_clock.injury_field_clock = nullptr;
  reject_injury(missing_injury_clock,
                "CharacterInjuryCompleteWorldSourcesRequired");
  auto foreign_injury_clock = body;
  foreign_injury_clock.injury_field_clock = foreign_body.injury_field_clock;
  reject_injury(foreign_injury_clock,
                "CharacterInjuryFieldClockBindingRequired");
  auto later_body = body;
  check(later_body.actual_now.half_day_tick <
            std::numeric_limits<Integer>::max(),
        "injury clock counterexample representable");
  ++later_body.actual_now.half_day_tick;
  reject_injury(later_body, "ConditionDueActualMomentMismatch");
  // 只改原 map 的局部副本；不改真实 catalog、作者输入、world 或已有伤效。
  check(catalog->actor_foundation->injury_capability_map.has_value(),
        "actual permanent injury map selected");
  const auto &original_map = *catalog->actor_foundation->injury_capability_map;
  must(validate_injury_grade_effects_map_v3(original_map, catalog->identity),
       "actual original injury map");
  auto wrong_map = original_map;
  wrong_map.version = 2;
  const auto wrong_map_result =
      validate_injury_grade_effects_map_v3(wrong_map, catalog->identity);
  check(!wrong_map_result.ok && wrong_map_result.error.original.code ==
                                     "OriginalInjuryGradeProfileRequired",
        "another map version cannot take permanent calendar path");
  wrong_map = original_map;
  wrong_map.entries.front().injury_rule_ref.catalog_digest = "foreign_catalog";
  const auto wrong_digest =
      validate_injury_grade_effects_map_v3(wrong_map, catalog->identity);
  check(!wrong_digest.ok && wrong_digest.error.original.code ==
                                 "OriginalInjuryGradeMapMismatch",
        "map source identity not inferred from zero episode count");
  const auto *core = owner->reads().original_core;
  check(core && &core->characters.early.world == &view &&
            core->characters.early.formations &&
            core->characters.early.field_application_clock,
        "custody borrows actual same-World early sources");
  auto field_scope = must(NativeFieldWorldScope::create_from_original_core(view, *core),
                          "real initial Field scope for ready source counterexamples");
  check(field_scope && field_scope->exact_contacts(), "real Field ready original sources");
  check_field_army_exit_ready_source_guards(*field_scope->exact_contacts(),
      *catalog->field_battle, field_scope->abilities());
  const auto &early = core->characters.early;
  const NamedMilitaryCustodyContext custody{
      view.common, view.military, *catalog->military, catalog->identity,
      &*view.field_battle, catalog->naval ? &view.naval : nullptr,
      early.formations, &early.clock, &early.military_access,
      early.field_application_clock};
  must(validate_current_named_military_custody(custody),
       "actual selected custody sources");
  auto reject_custody = [&](const NamedMilitaryCustodyContext &sources) {
    const auto result = validate_current_named_military_custody(sources);
    check(!result.ok && result.error.original.code ==
                           "NamedMilitaryCustodyFieldApplicationOwnerRequired",
          "custody cannot infer absence from empty history or foreign World");
  };
  auto missing_application = custody;
  missing_application.field_applications = nullptr;
  reject_custody(missing_application);
  auto missing_custody_clock = custody;
  missing_custody_clock.clock = nullptr;
  reject_custody(missing_custody_clock);
  auto foreign_application = custody;
  foreign_application.field_applications = foreign_clock.field_application_clock;
  reject_custody(foreign_application);
  // 原编译器按作者初态生成真正的 11 人 typed origin；不注入手造 seed。
  check(catalog->actor_social_occupancy_initial && view.interaction.social_occupancy &&
            view.interaction.social_occupancy->version == 1,
        "actual occupancy v1 seed compiler and physical state");
  const auto &originals = catalog->actor_social_occupancy_initial->authored_initial;
  const auto &scenario = boundary::field(
      boundary::field(boundary::field(input, "data", "catalog"), "scenarios", "data"),
      scenario_id, "scenario");
  const auto &actors = boundary::array(boundary::field(
      boundary::field(scenario, "initial", "scenario"), "actors", "initial"), "actors");
  check(actors.size() == 11 && originals.size() == 11,
        "eleven actual author actor seeds, no replacement initial actors");
  for (const auto &actor : actors) {
    const auto id = boundary::get_string(actor, "actor_id", "actors");
    check(originals.find(id),
          "every original author actor has compiler-generated occupancy seed");
    check(!must(read_native_military_custody_permission(
                    view, id, NamedMilitaryCustodyCommandKind::half_day_wait),
                "actual initial actor custody shared route"),
          "initial actor has no invented custody");
  }
}

// 诊断只打印已有错误的借用字节；不分配新 RuleError、不覆盖技术首错。
void diagnostic_original_rule(const char *cut, const RuleError &error) noexcept {
  diagnostic_text("DIAGNOSTIC NOT_COMMITTED cut=");
  diagnostic_text(cut);
  diagnostic_text(" code=");
  diagnostic_text(error.original.code.c_str());
  diagnostic_text(" path=");
  diagnostic_text(error.original.path.c_str());
  if (error.reason) {
    diagnostic_text(" reason=");
    diagnostic_text(error.reason->c_str());
  }
  diagnostic_text(" phase=");
  diagnostic_text(error.phase.c_str());
  diagnostic_text(" domain=");
  diagnostic_text(error.domain.c_str());
  diagnostic_text(" source_id=");
  diagnostic_text(error.source_id.c_str());
  diagnostic_text("\n");
}
// 本线程只在未发布候选存活期间启用观察；不借参数逃逸，也不以环境隐式开关。
struct ArmyMovementDiagnosticScope {
  bool previous = set_army_movement_diagnostic_enabled(true);
  ~ArmyMovementDiagnosticScope() {
    set_army_movement_diagnostic_enabled(previous);
  }
};
// 专用有限诊断不是第四个验收门：原输入重建 M5，两个真 clone 仅在本进程存活。
// 正常 warm/cold 路径不调用此函数；不写 bank/trace/PASS，也不改 current。
int diagnose_m6(int argc, char **argv) {
  diagnostic_text("DIAGNOSTIC M6_BEGIN NOT_COMMITTED NO_FORMAL_BANKS\n");
  check(argc == 6, "diagnose-m6 requires original input/scenario/terrain/output arguments");
  check(!fs::exists(fs::path(argv[5])), "diagnostic output must be a new unused path");
  check(fs::path(argv[4]).is_absolute(), "diagnostic locked absolute terrain root");
  const auto input = must(parse_boundary_json(text(argv[2])), "diagnostic original prepared JSON");
  const StableId scenario_id = argv[3];
  const auto identity = must(read_native_catalog_identity(input, scenario_id),
                             "diagnostic original catalog identity");
  DirectoryFieldBattleResources terrain(identity, fs::path(argv[4]));
  const auto &scenario = boundary::field(
      boundary::field(boundary::field(input, "data", "catalog"),
                      "scenarios", "data"), scenario_id, "scenario");
  NativeInitRequest request;
  request.scenario_id = scenario_id;
  request.world_seed = boundary::get_string(scenario, "seed_text", "scenario");
  request.expected_catalog_identity = identity;
  request.founder_selection = boundary::get_string(scenario, "player_actor", "scenario");
  auto initial = must(make_native_initial_world(input, request, &terrain),
                      "diagnostic original native initialization");
  const auto catalog = initial.catalog;
  ResidentWorld world;
  must(world.install(catalog, std::move(initial.seed), true),
       "diagnostic original resident install");
  Bytes bytes = must(world.checkpoint(), "diagnostic initial in-memory bank");
  auto view = inspect(bytes, catalog);
  // 这里只走原真实提交前缀；任何首错即停，不从未成功候选假造 M5。
  for (Integer steps = 0; view->common.tick() < 5 && steps < 64; ++steps) {
    const Integer target = view->common.tick() + 1;
    bytes = advance(world, *view, target);
    view = inspect(bytes, catalog);
    (void)digest(*view);
  }
  const auto at = must(view->actual_phase_moment(), "diagnostic actual prior clock");
  check(must(compare_world_moments(at, WorldMoment{5, {}}),
             "diagnostic exact M5 boundary") == 0,
        "diagnostic requires genuinely reached M5, no future state substitution");
  const auto before = must(world.checkpoint(), "diagnostic original M5 bank");
  check(before == bytes, "diagnostic M5 original bytes agree");
  diagnostic_text("DIAGNOSTIC REBUILT_ACTUAL_M5 IN_MEMORY_ONLY\n");
  const CommandPayload command{"advance", AdvanceCommand{WorldMoment{6, {}}}};
  const StableId actor = view->common.state().metadata.player_actor_id;
  int outcome = 1;
  {
    auto job = CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                           engineering_slot_capacity_profile());
    if (!job.ok)
      throw checkpoint_job_detail::FailureException{std::move(job.error)};
    auto resource = checkpoint_memory_resource(job.value, CheckpointJobUse::candidate);
    if (!resource.ok)
      throw checkpoint_job_detail::FailureException{std::move(resource.error)};
    ScopedCheckpointMemory memory(resource.value);
    // 普通规则 Outcome 可记录；技术异常/cancel不捕获清除或换 job 继续。
    const auto attempt = [&]() -> int {
      const auto original_preview = prepare_command(*view, actor, command);
      if (!original_preview.ok) {
        diagnostic_original_rule("M5_PREPARE", original_preview.error);
        return original_preview.error.kind == ErrorKind::internal ? 4 : 1;
      }
      check(!view->has_active_read_phase(), "diagnostic clone has no borrowed original read ticket");
      stage = "diagnostic_opening_clone";
      auto opened = checkpoint_make_shared<WorldBundle>(*view);
      if (opened->world_timeline->boundary_state == PeriodBoundaryState::ready) {
        check(opened->common.revision() < std::numeric_limits<Integer>::max(),
              "diagnostic original opening revision representable");
        stage = "diagnostic_original_opening";
        auto opening = open_exact_period_in_candidate(*opened);
        if (!opening.ok) {
          diagnostic_original_rule("ORIGINAL_OPENING", opening.error);
          return opening.error.kind == ErrorKind::internal ? 4 : 1;
        }
        ++opened->common.state().identity.revision;
        auto valid = opened->validate();
        if (!valid.ok) {
          diagnostic_original_rule("ORIGINAL_OPENING_VALIDATE", valid.error);
          return valid.error.kind == ErrorKind::internal ? 4 : 1;
        }
      }
      diagnostic_text("DIAGNOSTIC ORIGINAL_OPENING_READY PRIVATE_CLONE_ONLY\n");
      // 原 submit 在 Opening 独立完成后再次 clone；绝不在原 world 调 submit(M6)。
      check(!opened->has_active_read_phase(), "diagnostic opened clone releases read tickets");
      stage = "diagnostic_advance_clone";
      auto candidate = checkpoint_make_shared<WorldBundle>(*opened);
      opened.reset();
      auto prepared = prepare_command(*candidate, actor, command);
      if (!prepared.ok) {
        diagnostic_original_rule("OPENED_PREPARE", prepared.error);
        return prepared.error.kind == ErrorKind::internal ? 4 : 1;
      }
      auto captured = CompleteExactCommandCapture::begin(*candidate);
      if (!captured.ok) {
        diagnostic_original_rule("ORIGINAL_COMPLETE_CAPTURE", captured.error);
        return captured.error.kind == ErrorKind::internal ? 4 : 1;
      }
      ArmyMovementDiagnosticScope observation;
      stage = "diagnostic_original_apply_not_commit";
      // 保原 dispatcher 的读票写界、custody/adopt及后投影；只省略未获授权的 seal/发布。
      auto applied = apply_command_with_complete_capture(
          *candidate, actor, *prepared.value->operation, *captured.value);
      if (!applied.ok) {
        diagnostic_original_rule("ORIGINAL_APPLY_M6", applied.error);
        return applied.error.kind == ErrorKind::internal ? 4 : 1;
      }
      diagnostic_text("DIAGNOSTIC APPLY_REACHED_NOT_COMMITTED NO_SEAL_NO_SUCCESS_STATE\n");
      return 2;
    };
    outcome = attempt();
    // 所有原 owners/capture/candidate先退出同一 job；不以失败候选再跑 writer。
  }
  if (outcome == 4) {
    diagnostic_text("DIAGNOSTIC TECHNICAL_OR_INTERNAL_STOP NO_FURTHER_READS\n");
    return outcome;
  }
  check(before == must(world.checkpoint(), "diagnostic original M5 after private discard"),
        "diagnostic leaves original ResidentWorld byte-identical");
  diagnostic_text("DIAGNOSTIC ORIGINAL_M5_UNCHANGED NOT_COMMITTED\n");
  if (outcome == 1)
    diagnostic_text("DIAGNOSTIC ACCEPTANCE_BLOCKED payment_prefix contact_install field_segment winner_continuation resumed_positive_span DEPENDS_ON_SUCCESSFUL_M6\n");
  diagnostic_text("DIAGNOSTIC BLOCKED final_seal next_macro cold_before cold_after current NO_COMMITTED_M6\n");
  diagnostic_text("DIAGNOSTIC REPORT_ONLY NO_FIELD_PASS NO_FORMAL_BANKS\n");
  return outcome;
}

// 独立原M0诊断：不在三轮性能case内，只有局部mutable目录副本可变。
int diagnose_catalog_reuse_mutable(int argc, char **argv) {
  check(argc == 7, "catalog mutable diagnostic requires actual fixture/scenario/terrain/output/M0");
  const fs::path output = argv[5];
  check(!fs::exists(output) && fs::create_directories(output), "fresh catalog negative output");
  stage = "catalog_reuse_negative_prepare";
  const auto input = must(parse_boundary_json(text(argv[2])), "negative actual prepared JSON");
  const StableId scenario_id = argv[3];
  const auto identity = must(read_native_catalog_identity(input, scenario_id), "negative locked catalog identity");
  check(fs::path(argv[4]).is_absolute(), "negative absolute locked terrain root");
  DirectoryFieldBattleResources terrain(identity, fs::path(argv[4]));
  auto catalog = must(prepare_native_catalog_for_install(input, scenario_id, identity, &terrain), "negative real catalog");
  const auto bytes = bank(fs::path(argv[6]));
  auto view = inspect(bytes, catalog);
  check(view->common.tick() == 0 && view->actor_foundation && catalog->actor_foundation,
        "negative uses real M0 authored roots");
  auto mutable_catalog = *catalog->actor_foundation;
  const auto &definitions = mutable_catalog.actors.rows();
  check(!definitions.empty(), "negative real authored actor exists");
  const StableId actor_id = definitions.begin()->first;
  struct CountingRestore {
    bool previous;
    ~CountingRestore() noexcept { (void)catalog_reuse::set_counting_enabled(previous); }
  } counting{catalog_reuse::set_counting_enabled(true)};
  const auto before_valid = catalog_reuse::counts();
  auto valid = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, mutable_catalog, actor_id);
  const auto after_valid = catalog_reuse::counts();
  catalog_reuse::Counts valid_delta;
  check(catalog_reuse_difference(before_valid, after_valid, valid_delta), "negative valid-copy counters known");
  must(std::move(valid), "real origin with ordinary mutable catalog copy");
  check(valid_delta.requests == 1 && valid_delta.reused == 0 && valid_delta.full_validation_begins == 1 &&
        valid_delta.full_validation_successes == 1 && valid_delta.sealed_creations == 0,
        "ordinary mutable copy does not inherit validation proof");
  mutable_catalog.scenario_id.clear();
  const auto before_invalid = catalog_reuse::counts();
  auto invalid = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, mutable_catalog, actor_id);
  const auto after_invalid = catalog_reuse::counts();
  catalog_reuse::Counts invalid_delta;
  check(catalog_reuse_difference(before_invalid, after_invalid, invalid_delta), "negative invalid-copy counters known");
  check(!invalid.ok && invalid.error.original.code == "AuthoredActorFoundationCatalogRequired",
        "invalid mutable catalog preserves exact original rejection");
  check(invalid_delta.requests == 1 && invalid_delta.reused == 0 && invalid_delta.full_validation_begins == 1 &&
        invalid_delta.full_validation_successes == 0 && invalid_delta.sealed_creations == 0,
        "invalid mutable copy reaches original full validation and never reuses success");
  const auto require_uncached = [&](const catalog_reuse::Counts &a, const catalog_reuse::Counts &b,
                                    std::uint64_t calls, std::uint64_t successes, const char *scope) {
    catalog_reuse::Counts delta;
    check(catalog_reuse_difference(a, b, delta) && delta.requests == calls && delta.reused == 0 &&
              delta.full_validation_begins == calls && delta.full_validation_successes == successes && delta.sealed_creations == 0,
          std::string(scope) + " reaches original full body, not cached success");
    catalog_reuse_emit(scope, delta, true, 0, "OUTSIDE_PERFORMANCE_CASE", 0);
  };
  auto borrowed_value = *catalog->actor_foundation;
  auto *borrowed_definition = borrowed_value.actors.find(actor_id);
  check(borrowed_definition != nullptr, "preseal borrowed real actor definition");
  const auto borrowed_sealed = seal_actor_foundation_catalog(std::move(borrowed_value));
  check(borrowed_value.actors.find(actor_id) == borrowed_definition,
        "fixed sealing factory retains original mutable copy ownership");
  borrowed_definition->identity_definition_ref.id.clear();
  const auto before_borrowed_sealed = catalog_reuse::counts();
  auto borrowed_valid = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *borrowed_sealed, actor_id);
  const auto after_borrowed_sealed = catalog_reuse::counts();
  must(std::move(borrowed_valid), "preseal mutable element alias cannot alter sealed content");
  require_uncached(before_borrowed_sealed, after_borrowed_sealed, 1, 1, "NEGATIVE_PRESEAL_ALIAS_ISOLATED_FULL");
  const auto before_borrowed_hit = catalog_reuse::counts();
  auto borrowed_hit = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *borrowed_sealed, actor_id);
  const auto after_borrowed_hit = catalog_reuse::counts();
  must(std::move(borrowed_hit), "same live sealed owner reuses only its successful pure catalog proof");
  catalog_reuse::Counts hit_delta;
  check(catalog_reuse_difference(before_borrowed_hit, after_borrowed_hit, hit_delta) && hit_delta.requests == 1 &&
            hit_delta.reused == 1 && hit_delta.full_validation_begins == 0 && hit_delta.full_validation_successes == 0 && hit_delta.sealed_creations == 0,
        "actual sealed success reuse distinct from full validation");
  catalog_reuse_emit("NEGATIVE_PRESEAL_ALIAS_SEALED_HIT", hit_delta, true, 0, "OUTSIDE_PERFORMANCE_CASE", 0);
  const auto before_borrowed_plain = catalog_reuse::counts();
  const auto borrowed_bad = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, borrowed_value, actor_id);
  const auto after_borrowed_plain = catalog_reuse::counts();
  check(!borrowed_bad.ok && borrowed_bad.error.original.code == "AuthoredActorFoundationDefinitionInvalid" &&
            borrowed_bad.error.original.path == actor_id,
        "original mutable definition keeps its real original rejection");
  require_uncached(before_borrowed_plain, after_borrowed_plain, 1, 0, "NEGATIVE_PRESEAL_ALIAS_PLAIN_BAD_FULL");
  auto invalid_sealed_value = *catalog->actor_foundation;
  invalid_sealed_value.scenario_id.clear();
  const auto invalid_sealed = seal_actor_foundation_catalog(std::move(invalid_sealed_value));
  const auto before_failed_twice = catalog_reuse::counts();
  const auto first_failed = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *invalid_sealed, actor_id);
  const auto second_failed = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *invalid_sealed, actor_id);
  const auto after_failed_twice = catalog_reuse::counts();
  check(!first_failed.ok && !second_failed.ok &&
            first_failed.error.original.code == "AuthoredActorFoundationCatalogRequired" &&
            second_failed.error.original.code == "AuthoredActorFoundationCatalogRequired",
        "sealed invalid catalog never caches either failure as success");
  require_uncached(before_failed_twice, after_failed_twice, 2, 0, "NEGATIVE_SEALED_FAILURE_TWICE");
  auto growth_value = *catalog->actor_foundation;
  check(bool(growth_value.original_growth), "real selected catalog has shared Growth source");
  auto growth_alias = std::make_shared<GrowthCatalog>(*growth_value.original_growth);
  growth_value.original_growth = growth_alias;
  auto growth_sealed = seal_actor_foundation_catalog(std::move(growth_value));
  const auto before_growth_valid = catalog_reuse::counts();
  auto growth_valid = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *growth_sealed, actor_id);
  const auto after_growth_valid = catalog_reuse::counts();
  must(std::move(growth_valid), "sealed catalog before real mutable Growth selection change");
  require_uncached(before_growth_valid, after_growth_valid, 1, 1, "NEGATIVE_GROWTH_ALIAS_INITIAL_FULL");
  growth_alias->complete_study_sources = !growth_alias->complete_study_sources;
  const auto before_growth_changed = catalog_reuse::counts();
  const auto changed_sealed = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *growth_sealed, actor_id);
  const auto after_growth_changed = catalog_reuse::counts();
  auto ordinary_same_growth = *growth_sealed;
  const auto before_growth_plain = catalog_reuse::counts();
  const auto changed_plain = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, ordinary_same_growth, actor_id);
  const auto after_growth_plain = catalog_reuse::counts();
  const auto same_error = [](const RuleError &a, const RuleError &b) {
    return std::tie(a.original.code, a.original.path, a.kind, a.phase, a.domain, a.command, a.source_id,
                    a.army_id, a.reason, a.limit, a.feasibility, a.maximum, a.new_total, a.required_level,
                    a.actual_level, a.skill_id, a.actor_id, a.battle_id, a.battle_id_after_path) ==
           std::tie(b.original.code, b.original.path, b.kind, b.phase, b.domain, b.command, b.source_id,
                    b.army_id, b.reason, b.limit, b.feasibility, b.maximum, b.new_total, b.required_level,
                    b.actual_level, b.skill_id, b.actor_id, b.battle_id, b.battle_id_after_path);
  };
  check(changed_sealed.ok == changed_plain.ok && (changed_sealed.ok || same_error(changed_sealed.error, changed_plain.error)),
        "Growth mutation preserves actual original catalog admission and complete RuleError fields");
  require_uncached(before_growth_changed, after_growth_changed, 1, changed_sealed.ok ? 1 : 0, "NEGATIVE_GROWTH_ALIAS_CHANGED_FULL");
  require_uncached(before_growth_plain, after_growth_plain, 1, changed_plain.ok ? 1 : 0, "NEGATIVE_GROWTH_PLAIN_CONTROL_FULL");
  const auto destroyed_owner_address = reinterpret_cast<std::uintptr_t>(growth_sealed.get());
  growth_sealed.reset();
  auto replacement_value = *catalog->actor_foundation;
  replacement_value.scenario_id.clear();
  const auto replacement_owner = seal_actor_foundation_catalog(std::move(replacement_value));
  const bool observed_address_reuse = reinterpret_cast<std::uintptr_t>(replacement_owner.get()) == destroyed_owner_address;
  const auto before_replacement = catalog_reuse::counts();
  const auto replacement_result = read_authored_character_origin_v2(view->common.state(), *view->actor_foundation, *replacement_owner, actor_id);
  const auto after_replacement = catalog_reuse::counts();
  check(!replacement_result.ok && replacement_result.error.original.code == "AuthoredActorFoundationCatalogRequired",
        "new owner after destruction does not inherit previous success");
  require_uncached(before_replacement, after_replacement, 1, 0, "NEGATIVE_DESTROYED_OWNER_NEW_BAD_FULL");
  catalog_reuse_emit("NEGATIVE_VALID_MUTABLE_COPY", valid_delta, true, 0, "OUTSIDE_PERFORMANCE_CASE", 0);
  catalog_reuse_emit("NEGATIVE_INVALID_MUTABLE_COPY", invalid_delta, true, 0, "OUTSIDE_PERFORMANCE_CASE", 0);
  check(!node_replay_collector.output_failed && bool(std::cerr), "negative counter report output intact");
  put_new_replay_text(output / "CATALOG_REUSE_MUTABLE_NEGATIVES.txt",
      "DIAGNOSTIC_ONLY MUTABLE_COPY_ORIGINAL_GUARDS_PASS\n"
      "valid_copy_requests=1 reused=0 full_validation_begins=1 full_validation_successes=1\n"
      "invalid_copy_requests=1 reused=0 full_validation_begins=1 full_validation_successes=0\n"
      "invalid_code=AuthoredActorFoundationCatalogRequired\n"
      "preseal_alias_isolated_full=1 sealed_hit=1 mutable_original_error=AuthoredActorFoundationDefinitionInvalid\n"
      "sealed_invalid_twice_requests=2 reused=0 full_validation_begins=2 full_validation_successes=0\n"
      "growth_changed_vs_plain=OK_AND_ALL_RULE_ERROR_FIELDS_MATCH_NOT_FULL_VALUE\n"
      "growth_changed_full=1 growth_plain_full=1 destroyed_owner_new_bad_full=1\n"
      "sealed_creations_outside_subcase_count_windows=true\n"
      "same_M0_input=true measured_case=false formal_gate_accepted=false current_promoted=false\n" +
      std::string("growth_changed_ok=") + (changed_sealed.ok ? "true\n" : "false\n") +
      "owner_address_reuse_observed=" + (observed_address_reuse ? "true\n" : "false\n"));
  const char *marker = "FIELD_CATALOG_REUSE MUTABLE_NEGATIVES_COMPLETE VALID_MUTABLE_INVALID_MUTABLE_PRESEAL_ALIAS_SEALED_FAILURE_GROWTH_ALIAS_OWNER_LIFETIME OUTSIDE_PERFORMANCE_CASE NOT_FIELD_PASS NOT_CURRENT\n";
  const auto size = std::strlen(marker); node_emit(marker, static_cast<int>(size), size + 1);
  check(!node_replay_collector.output_failed && bool(std::cerr), "negative final report intact");
  return 0;
}

} // namespace
int main(int argc, char **argv) {
  std::set_terminate(diagnostic_terminate);
  diagnostic_text("FIELD_THIRD_GATE MAIN_ENTERED\n");
  // backtrace 首次调用可能加载 libgcc/分配；在业务前显式预热，不宣称
  // unwind 库在任意环境下零分配。终止时先保固定 stage，再尽力取真实栈。
  stage = "diagnostic_stack_prewarm";
  void *warm_frames[64];
  const int warm_count = ::backtrace(warm_frames, 64);
  if (warm_count > 0)
    diagnostic_text("FIELD_THIRD_GATE STACK_PREWARMED\n");
  stage = "arguments";
  try {
    if (argc >= 2 && std::string(argv[1]) == "diagnose-m6")
      return diagnose_m6(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "diagnose-catalog-reuse-mutable-negatives")
      return diagnose_catalog_reuse_mutable(argc, argv);
    check(argc == 6 || argc == 7,
          "usage: field_third_gate_test MODE PREPARED SCENARIO TERRAIN_ROOT "
          "OUTPUT_DIR [WARM_DIR]");
    const std::string mode = argv[1];
    const bool shared_m0_create = mode == "diagnose-shared-m0-create";
    const bool pc_sample = mode == "diagnose-shared-m0-pc-sample-first-three";
    const bool catalog_section = mode == "diagnose-shared-m0-catalog-section-first-three";
    const bool catalog_reuse_counting = mode == "diagnose-shared-m0-catalog-reuse-first-three";
    const bool shared_m0_replay = mode == "diagnose-shared-m0-first-three" || pc_sample || catalog_section || catalog_reuse_counting;
    const bool node_replay = mode == "diagnose-first-three" || shared_m0_replay;
    const bool warm = mode == "warm" || node_replay || shared_m0_create;
    check(warm || mode == "cold-before-contact" || mode == "cold-after-segment",
          "finite real process mode");
    check(argc == (shared_m0_replay ? 7 : warm ? 6 : 7),
          "cold requires warm evidence directory; shared-M0 replay requires its actual bank");
    FieldProductionObservationBinding production_observation(warm);
    FieldNodeObservationBinding node_observation_binding(node_replay);
    FieldPcSamplingBinding pc_sampling_binding(pc_sample);
    FieldCatalogSectionBinding catalog_section_binding(catalog_section);
    FieldCatalogReuseCountingBinding catalog_reuse_counting_binding(catalog_reuse_counting);
    FieldWorldSizeSnapshot previous_world_sizes;
    unsigned world_size_capture_count = 0;
    const fs::path output = argv[5];
    check(!fs::exists(output), "new independent output directory required");
    check(fs::create_directories(output), "create evidence directory");
    stage = "parse_input";
    const auto input =
        must(parse_boundary_json(text(argv[2])), "actual prepared JSON");
    const StableId scenario_id = argv[3];
    stage = "catalog_identity";
    const auto identity = must(read_native_catalog_identity(input, scenario_id),
                               "real catalog identity");
    if (warm) {
      stage = "siege_selection_contract";
      siege_selection_contract(input, scenario_id);
      diagnostic_text("FIELD_THIRD_GATE SIEGE_SELECTION_CONTRACT_PASS\n");
      stage = "original_settlement_controller_contract";
      check_field_initial_settlement_controller_source();
      diagnostic_text("FIELD_THIRD_GATE SETTLEMENT_CONTROLLER_CONTRACT_PASS\n");
      stage = "field_army_exit_command_boundary_contract";
      field_army_exit_command_boundary_contract();
      diagnostic_text("FIELD_THIRD_GATE EXIT_COMMAND_BOUNDARY_COMPONENT_PASS\n");
      stage = "post_result_exit_geometry_contract";
      check_field_post_result_exit_geometry();
      diagnostic_text("FIELD_THIRD_GATE EXIT_GEOMETRY_COMPONENT_PASS\n");
    }
    check(fs::path(argv[4]).is_absolute(),
          "explicit absolute locked terrain repository root");
    DirectoryFieldBattleResources terrain(identity, fs::path(argv[4]));
    ResidentWorld world;
    std::shared_ptr<const PreparedCatalog> catalog;
    Bytes bytes;
    std::vector<Trace> expected;
    Integer start = 0;
    std::string player;
    if (shared_m0_replay) {
      // 两个真实实现必须读同一原bank；不改WorldId/header，也不重新native init。
      stage = "shared_m0_prepare";
      catalog = must(prepare_native_catalog_for_install(input, scenario_id,
                                                        identity, &terrain),
                     "same locked shared-M0 catalog");
      bytes = bank(fs::path(argv[6]));
      auto original = inspect(bytes, catalog);
      check(original->common.tick() == 0, "shared input is a real M0 bank");
      stage = "shared_m0_restore";
      must(FIELD_OBSERVE(stage, world.restore(bytes, catalog)),
           "real shared-M0 restore");
      stage = "shared_m0_restored_checkpoint";
      auto restored = must(FIELD_OBSERVE(stage, world.checkpoint()),
                           "shared-M0 restored checkpoint");
      check(restored == bytes, "shared-M0 restore preserves complete bank bytes");
      bytes = std::move(restored);
      diagnostic_text("FIELD_SHARED_M0 RESTORE_BYTE_IDENTITY_PASS DIAGNOSTIC_ONLY\n");
    } else if (warm) {
      const auto &scenario = boundary::field(
          boundary::field(boundary::field(input, "data", "catalog"),
                          "scenarios", "data"),
          scenario_id, "scenario");
      NativeInitRequest request;
      request.scenario_id = scenario_id;
      request.world_seed =
          boundary::get_string(scenario, "seed_text", "scenario");
      request.expected_catalog_identity = identity;
      request.founder_selection =
          boundary::get_string(scenario, "player_actor", "scenario");
      stage = "native_init";
      auto initial = must(FIELD_OBSERVE("native.init", make_native_initial_world(input, request, &terrain)),
                          "original native initialization");
      catalog = initial.catalog;
      stage = "native_install";
      must(FIELD_OBSERVE("native.install", world.install(catalog, std::move(initial.seed), true)),
           "original resident install");
      stage = "initial_checkpoint";
      bytes = must(FIELD_OBSERVE(stage, world.checkpoint()), "initial bank");
    } else {
      stage = "cold_prepare";
      catalog = must(prepare_native_catalog_for_install(input, scenario_id,
                                                        identity, &terrain),
                     "same locked cold catalog");
      const fs::path prior = argv[6];
      expected = load_trace(prior / "trace.txt");
      const auto name = mode.substr(5);
      const auto cut = load_cut(prior, name);
      check(cut.index < Integer(expected.size()) &&
                expected[cut.index].sha == cut.sha,
            "cut matches actual warm trace");
      start = cut.index;
      bytes = bank(prior / (name + ".bank"));
      auto original = inspect(bytes, catalog);
      check(digest(*original) == cut.sha &&
                full_json(*original) == text(prior / (name + ".json")),
            "cold input complete export exact");
      stage = "cold_restore";
      must(world.restore(bytes, catalog), "fresh-process real restore");
      stage = "restored_checkpoint";
      check(must(FIELD_OBSERVE(stage, world.checkpoint()), "restored bank") == bytes,
            "restore complete bank byte identity");
    }
    if (shared_m0_create) {
      // 只保存该旧实现真实init/install/checkpoint已经产生的Bytes，不重新编码。
      auto initial_view = inspect(bytes, catalog);
      check(initial_view->common.tick() == 0, "created shared input is M0");
      const auto sha = picosha2::hash256_hex_string(bytes);
      put_new_replay_bytes(output / "NEW_SHARED_M0.bank", bytes.data(), bytes.size());
      std::ostringstream metadata;
      metadata << "kind=NEW_SHARED_M0_FROM_REAL_NATIVE_INITIALIZATION\n"
               << "bank_tick=0\nbytes=" << bytes.size() << "\nbank_sha256=" << sha
               << "\nold_bank_recovered=false\nformal_gate_accepted=false\ncurrent_promoted=false\n";
      put_new_replay_text(output / "NEW_SHARED_M0.meta", metadata.str());
      diagnostic_text("FIELD_SHARED_M0 CREATED_FROM_REAL_INIT CHECKPOINT_BYTES_SAVED DIAGNOSTIC_ONLY NOT_FIELD_PASS NOT_CURRENT\n");
      return 0;
    }
    auto view = inspect(bytes, catalog);
    FieldReinforcementComponentCoverage reinforcement_components;
    LocalMoveComponentCoverage local_move_components;
    field_gate_observation_context(start, view->common.tick(), view->common.tick());
    if (warm)
      field_observe_world_sizes(*view, previous_world_sizes, world_size_capture_count);
    stage = "reinforcement_component_contract";
    FIELD_OBSERVE("components.reinforcement", check_field_reinforcement_components(*view, bytes, reinforcement_components));
    stage = "local_move_component_contract";
    FIELD_OBSERVE("components.local_move", check_local_move_components(*view, local_move_components));
    if (warm) {
      stage = "selected_military_prediction_contract";
      FIELD_OBSERVE("contract.military_prediction", selected_military_prediction_contract(*view));
      diagnostic_text("FIELD_THIRD_GATE MILITARY_POINT_PREDICTION_CONTRACT_PASS\n");
      stage = "selected_field_cause_contract";
      FIELD_OBSERVE("contract.field_cause", selected_field_cause_contract(*view, bytes, catalog, input, scenario_id));
      diagnostic_text("FIELD_THIRD_GATE FAMILY_FIELD_ROOT_CONTRACT_PASS OCCUPANCY_SEEDS=11\n");
    }
    player = view->common.state().metadata.player_actor_id;
    check(!player.empty() &&
              player.find("actor.qingchuan.") == std::string::npos &&
              player.find("actor.blackwater.") == std::string::npos,
          "fixed original market observer remains player");
    std::ostringstream trace;
    trace << start << ' '
          << (warm ? view->common.tick() : expected[start].target) << ' '
          << FIELD_OBSERVE("digest", digest(*view)) << '\n';
    bool saved_before = false, saved_after = false;
    Integer after_segment_index = -1;
    Evidence found = FIELD_OBSERVE("evidence", evidence(*view));
    if (warm)
      check(!found.contact && !found.segment,
            "no preseeded contact or battle result");
    else
      check(FIELD_OBSERVE("digest", digest(*view)) == expected[start].sha,
            "restore full summary unchanged");
    Integer index = start;
    const Integer stop = node_replay ? 3 : warm ? 64 : Integer(expected.size() - 1);
    for (; index < stop;) {
      check(view->common.tick() < std::numeric_limits<Integer>::max(),
            "advance target representable");
      const Integer target =
          warm ? view->common.tick() + 1 : expected[index + 1].target;
      field_gate_observation_context(index, view->common.tick(), target);
      auto previous = std::move(view);
      auto prior_bytes = std::move(bytes);
      bytes = advance(world, *previous, target);
      view = inspect(bytes, catalog);
      field_gate_observation_context(index + 1, view->common.tick(), target);
      if (warm)
        field_observe_world_sizes(*view, previous_world_sizes, world_size_capture_count);
      stage = "reinforcement_component_sources";
      FIELD_OBSERVE("components.reinforcement", check_field_reinforcement_components(*view, bytes, reinforcement_components));
      stage = "local_move_component_sources";
      FIELD_OBSERVE("components.local_move", check_local_move_components(*view, local_move_components));
      ++index;
      check(view->common.state().metadata.player_actor_id == player,
            "no NPC/player takeover in test");
      const auto sha = FIELD_OBSERVE("digest", digest(*view));
      trace << index << ' ' << target << ' ' << sha << '\n';
      const auto now = FIELD_OBSERVE("evidence", evidence(*view));
      if (warm && !node_replay) {
        if (!saved_before && !found.contact && now.contact) {
          FIELD_OBSERVE("cut.before_contact", save_cut(output, "before-contact", index - 1, prior_bytes, *previous));
          saved_before = true;
        }
        if (!saved_after && now.segment) {
          FIELD_OBSERVE("cut.after_segment", save_cut(output, "after-segment", index, bytes, *view));
          saved_after = true;
          after_segment_index = index;
        }
      } else if (!node_replay)
        check(sha == expected[index].sha,
              "cold continuation complete state equals warm at command " +
                  std::to_string(index));
      found = now;
      if (node_replay) {
        stage = "node_replay_save_existing_bank";
        if (!save_new_replay_bank(output, index, observation_bank_tick, target, bytes, sha, previous_world_sizes)) {
          diagnostic_text("FIELD_NODE_REPLAY DETERMINISM_TABLE_MISMATCH NOT_PASS\n");
          return 6;
        }
      }
      if (warm && !node_replay && found.contact && found.segment && found.moved &&
          index > after_segment_index)
        break;
    }
    if (node_replay) {
      check(start == 0 && index == 3, "NEW replay exactly three original successful commands");
      bool windows_observed = true;
      for (std::size_t target = 1; target <= 3; ++target)
        for (unsigned kind = 0; kind < 3; ++kind) {
          const bool captured = node_replay_collector.completed[target][kind];
          windows_observed = windows_observed && captured;
          std::cerr << "FIELD_NODE_REPLAY WINDOW_STATUS target=" << target << " window=" << node_window_name(kind)
                    << " status=" << (captured ? "OBSERVED" : "NOT_CAPTURED") << '\n';
        }
      windows_observed = windows_observed && !typed_replay_collector.failed;
      for (std::size_t target = 1; target <= 3; ++target)
        for (unsigned kind = 0; kind < 3; ++kind)
          windows_observed = windows_observed && typed_replay_collector.completed[target][kind];
      windows_observed = windows_observed && !node_replay_collector.output_failed && bool(std::cerr);
      const bool pc_report_complete = pc_sampling_binding.finish_report();
      windows_observed = windows_observed && pc_report_complete;
      const bool catalog_report_complete = catalog_section_binding.finish_report();
      windows_observed = windows_observed && catalog_report_complete;
      const bool catalog_reuse_report_complete = catalog_reuse_counting_binding.finish_report();
      windows_observed = windows_observed && catalog_reuse_report_complete;
      const int typed_report_errno = errno;
      const char *typed_report = windows_observed ? "FIELD_TYPED_SUPPLY REPORT_COMPLETE NINE_WINDOWS CONTEXT_PARTIAL NOT_MEMO_PROOF\n" : "FIELD_TYPED_SUPPLY REPORT_UNKNOWN NOT_PASS\n";
      const auto typed_report_size = std::strlen(typed_report);
      node_emit(typed_report, static_cast<int>(typed_report_size), typed_report_size + 1);
      errno = typed_report_errno;
      windows_observed = windows_observed && !node_replay_collector.output_failed && bool(std::cerr);
      put_new_replay_text(output / "NEW_trace.txt", trace.str());
      put_new_replay_text(output / "NEW_REPLAY_REPORT.txt",
          std::string(windows_observed ? "measurement_report=COMPLETE\n" : "measurement_report=INCOMPLETE\n") +
          "DIAGNOSTIC_ONLY NEW_CONTROLLED_REPLAY_COMPLETE commands=3\n"
          "formal_gate_accepted=false cold_run=false current_promoted=false\n"
          "old_bank_sha256=NOT_AVAILABLE table_match_is_not_byte_equivalence\n");
      diagnostic_text(windows_observed ?
          "FIELD_NODE_REPLAY REPORT_COMPLETE NEW_BANKS=3 DIAGNOSTIC_ONLY NOT_FIELD_PASS NOT_CURRENT\n" :
          "FIELD_NODE_REPLAY REPORT_INCOMPLETE MISSING_WINDOW NEW_BANKS=3 DIAGNOSTIC_ONLY NOT_FIELD_PASS NOT_CURRENT\n");
      return windows_observed ? 0 : 7;
    }
    check(found.contact && found.segment && found.moved,
          "bounded run requires true contact, segment, conserved rosters and "
          "resumed unit movement");
    if (warm)
      check(saved_before && saved_after, "both genuine cold cuts captured");
    else
      check(
          index == stop && index > start,
          "all warm continuation commands replayed, including a real advance");
    // 原终态 bank 再走同一完整 source1 身体链；读票/owner在导出前一起释放。
    {
    const auto final_now = must(view->actual_phase_moment(), "final actual clock");
    auto final_phase = must(view->enter_phase_read_context(
        "field_final_condition_read", "field_battle",
        {final_now, final_now}, final_now), "final same-world read phase");
    const auto final_owner = must(view->complete_exact_time_bindings(),
                                  "final actual condition owner");
    check(bool(final_owner), "final actual condition owner exists");
    const auto final_body = must(
        final_owner->reads().character_conditions.sources(),
        "final actual condition sources");
    must(validate_native_permanent_injury_calendar_sources(*view, final_body),
         "final real permanent injury calendar sources");
    }
    FIELD_OBSERVE("cut.final", save_cut(output, "final", index, bytes, *view));
    put(output / "trace.txt", trace.str());
    put(output / "reinforcement-components.txt",
        field_reinforcement_component_report(reinforcement_components));
    put(output / "local-move-components.txt",
        "selection_checks=" + std::to_string(local_move_components.selection_cases) +
        "\nactual_bank_row_checks=" + std::to_string(local_move_components.actual_source_rows) +
        "\ncodec_checks=" + std::to_string(local_move_components.codec_cases) +
        "\nnegative_checks=" + std::to_string(local_move_components.negative_cases) +
        "\nsource_status=" + (local_move_components.actual_source_rows ?
            "EXERCISED_REAL_BANK_ROWS\n" : "NOT_EXERCISED\n"));
    put(output / "actual-effects.txt",
        "wounded=" + std::to_string(found.wounded) +
            "\ndead=" + std::to_string(found.dead) +
            "\ncaptured=" + std::to_string(found.captured) +
            "\ndeserted=" + std::to_string(found.deserted) + "\n");
    put(output / "result.txt",
        "PASS " + mode + " commands=" + std::to_string(index - start) +
            " checks=" + std::to_string(checks) +
            " final_sha256=" + FIELD_OBSERVE("digest", digest(*view)) + "\n");
    std::cout << "FIELD_THIRD_GATE PASS " << mode << " checks=" << checks
              << "\n";
    return 0;
  } catch (const NodeReplayIoError &error) {
    std::cerr << "FIELD_NODE_REPLAY DIAGNOSTIC_IO_FAILURE stage=" << stage << " error=" << error.what() << " NOT_PASS\n";
    return 5;
  } catch (checkpoint_job_detail::FailureException &failure) {
    // 原薄宿主技术首错诊断；不吞错、不放宽断言，也不建立全程外层 job。
    std::cerr << "FIELD_THIRD_GATE TECHNICAL_FAILURE stage=" << stage
              << " code="
              << checkpoint_job_failure_code_name(failure.error.code())
              << " required=" << failure.error.required() << '\n';
    if (failure.error.try_materialize_rule_error()) {
      if (const auto *error = failure.error.rule_error()) {
        std::cerr << "FIELD_THIRD_GATE ORIGINAL_FAILURE code="
                  << error->original.code << " path=" << error->original.path
                  << '\n';
      }
    }
    return 4;
  } catch (const std::exception &error) {
    std::cerr << "FIELD_THIRD_GATE FAIL " << error.what() << '\n';
    return 1;
  }
}

#undef FIELD_OBSERVE_SOURCE
#undef FIELD_OBSERVE
