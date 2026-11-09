// 开局/指定日数/双冷/续跑的薄宿主。只调用唯一 Resident
// session，不生成世界事实或绕领域门。
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/checkpoint_phase_timing.hpp"
#include "resident/kernel/native_catalog_identity.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
#include "resident/storage/common_store.hpp"
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace hanmo::resident;
using V = BoundaryValue;
namespace fs = std::filesystem;
struct Failed {
  RuleError error;
};
struct ReceiptRejected {
  V receipt;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failed{std::move(value.error)};
  return std::move(value.value);
}
V object(V::Dict value) { return V{std::move(value)}; }
const V &field(const V &v, const char *name) {
  const auto *p = v.find(name);
  if (!p)
    throw std::runtime_error(std::string("MissingActualStateField:") + name);
  return *p;
}
Integer integer(const V &v) {
  const auto *p = v.as<Integer>();
  if (!p)
    throw std::runtime_error("ActualIntegerRequired");
  return *p;
}
StableId string(const V &v) {
  const auto *p = v.as<StableId>();
  if (!p)
    throw std::runtime_error("ActualStringRequired");
  return *p;
}
std::string bytes(const fs::path &path) {
  const auto size = fs::file_size(path);
  if (size > 64u * 1024u * 1024u)
    throw std::runtime_error("ThinInputSizeLimit");
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("ThinInputOpenFailed");
  std::string out(size, '\0');
  in.read(out.data(), static_cast<std::streamsize>(size));
  if (!in && size)
    throw std::runtime_error("ThinInputReadFailed");
  return out;
}
V load(const fs::path &path) { return take(parse_boundary_json(bytes(path))); }
void write(const fs::path &path, const std::string &value) {
  std::ofstream out(path, std::ios::binary);
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
  if (!out)
    throw std::runtime_error("ThinOutputWriteFailed");
}
void save(const fs::path &path, const V &value) {
  write(path, take(render_boundary_json(value)) + "\n");
}
// 只在宿主输出现成结果和外层时钟；不改变会话规则、计时源或世界状态。
void measure_note(V::Dict fields) {
  fields.emplace_back("measurement", V{true});
  std::cout << take(render_boundary_json(object(std::move(fields))))
            << std::endl;
}
Integer elapsed_usec(std::chrono::steady_clock::time_point from) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now() - from)
      .count();
}
// 内部阶段只输出诊断；各行是 inclusive，binary 子段不能与 save.encode_api
// 重复相加。
void measure_checkpoint_phases(const checkpoint_phase_timing::Frame &frame,
                               const char *operation) {
  using namespace checkpoint_phase_timing;
  for (std::size_t i = 0; i < frame.samples.size(); ++i) {
    const auto &sample = frame.samples[i];
    if (!sample.calls)
      continue;
    const bool encode_child =
        i >= static_cast<std::size_t>(Stage::binary_prepare) &&
        i <= static_cast<std::size_t>(Stage::binary_write);
    measure_note({{"metric", V{"checkpoint_internal_phase"}},
                  {"operation", V{operation}},
                  {"phase", V{names[i]}},
                  {"parent", V{encode_child ? "save.encode_api" : operation}},
                  {"inclusive", V{true}},
                  {"calls", V{static_cast<Integer>(sample.calls)}},
                  {"usec", V{static_cast<Integer>(sample.nanoseconds / 1000)}},
                  {"diagnostic_overflow", V{frame.overflow}}});
  }
}
// 读取本进程 RSS；样本在原会话已安装且瞬态导出已释放的边界取得。
void measure_resident_rss(const char *phase) {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      measure_note(
          {{"metric", V{"resident_rss"}},
           {"phase", V{phase}},
           {"kib", V{static_cast<Integer>(std::stoll(line.substr(6)))}}});
      return;
    }
  }
  throw std::runtime_error("MeasurementProcessRssUnavailable");
}
V error_value(const RuleError &error) {
  V::Dict out{{"code", V{error.original.code}},
              {"path", V{error.original.path}},
              {"kind", V{error.kind == ErrorKind::rule ? "rule"
                         : error.kind == ErrorKind::unsupported_capability
                             ? "unsupported_capability"
                             : "internal"}},
              {"phase", V{error.phase}},
              {"domain", V{error.domain}},
              {"command", V{error.command}},
              {"source_id", V{error.source_id}}};
  if (error.reason)
    out.emplace_back("reason", V{*error.reason});
  return object(std::move(out));
}
// 仅把原身份读取叶的实际返回值投影到原 initialize_native
// 请求，绝不手算模块或摘要。
V identity_value(const CatalogIdentity &id) {
  V::Array modules;
  for (const auto &[name, version] : id.module_versions)
    modules.push_back(object({{"name", V{name}}, {"version", V{version}}}));
  V::Dict out{{"content_id", V{id.content_id}},
              {"content_version", V{id.content_version}},
              {"content_digest", V{id.content_digest}},
              {"ruleset_id", V{id.ruleset_id}},
              {"schema_sha256", V{id.schema_sha256}},
              {"artifact_sha256", V{id.artifact_sha256}},
              {"module_versions", V{std::move(modules)}}};
  if (id.pack_set_digest)
    out.emplace_back("pack_set_digest", V{*id.pack_set_digest});
  return object(std::move(out));
}
// 正常安装后的封闭捕获只读同一实际目录；不调用待验 helper，也不另建世界。
void check_original_role_dialogue_projection(ResidentReplaySession &session,
                                             const V &checked_catalog,
                                             const StableId &scenario_id,
                                             const fs::path &out,
                                             const StableId &mode) {
  // 本次新增正例只属于原崤函；其它样本继续原薄门，不强加未选存储租约能力。
  if (scenario_id != "xiaohan_dynamic::scenario.chuping_xiaohan_dynamic")
    return;
  const auto &capacity = engineering_checkpoint_capacity_profile();
  const auto &slot = engineering_slot_capacity_profile();
  auto created = CheckpointJobBudget::create(capacity, slot);
  if (!created.ok)
    throw checkpoint_job_detail::FailureException{std::move(created.error)};
  auto job = std::move(created.value);
  auto captured = session.capture_storage_checkpoint(capacity, slot, job);
  if (!captured.ok)
    throw checkpoint_job_detail::FailureException{std::move(captured.error)};
  const auto &prepared = *captured.value->catalog();
  if (!prepared.common || !prepared.interaction ||
      prepared.interaction->common.get() != prepared.common.get())
    throw std::runtime_error("ThinOriginalRoleDialogueSameCatalogRequired");
  const auto &data = field(checked_catalog, "data");
  const auto &actors = field(data, "actors");
  const auto &dialogues = field(data, "dialogues");
  const auto &roles = field(data, "roles");
  Integer derived = 0;
  V::Array checked;
  for (const auto &actor : prepared.interaction->actor_dialogues) {
    if (!actor.dialogue_id)
      continue;
    const auto &original_actor = field(actors, actor.actor_id.c_str());
    if (string(field(original_actor, "dialogue_id")) != *actor.dialogue_id)
      throw std::runtime_error("ThinOriginalActorDialogueMismatch");
    const auto &original_dialogue =
        field(dialogues, actor.dialogue_id->c_str());
    const auto *context = original_dialogue.find("context_role");
    if (!context || string(*context).empty())
      continue;
    const auto role_id = string(*context);
    const auto &original_role = field(roles, role_id.c_str());
    const auto *role = prepared.common->roles.find(role_id);
    if (!role || role->initial_actor != actor.actor_id)
      throw std::runtime_error("ThinOriginalRoleActorMismatch");
    const auto *explicit_dialogue = original_role.find("dialogue_id");
    if (explicit_dialogue && explicit_dialogue->as<std::monostate>())
      explicit_dialogue = nullptr;
    const auto expected =
        explicit_dialogue ? string(*explicit_dialogue) : *actor.dialogue_id;
    if (!role->dialogue_id || *role->dialogue_id != expected)
      throw std::runtime_error("ThinOriginalRoleDialogueProjectionMismatch");
    if (!explicit_dialogue)
      ++derived;
    checked.push_back(
        object({{"actor_id", V{actor.actor_id}},
                {"role_id", V{role_id}},
                {"dialogue_id", V{*role->dialogue_id}},
                {"derived_from_original", V{!explicit_dialogue}}}));
  }
  if (scenario_id == "xiaohan_dynamic::scenario.chuping_xiaohan_dynamic" &&
      derived == 0)
    throw std::runtime_error("ThinOriginalRoleDialoguePositiveRequired");
  save(out / (mode + ".role-dialogue-projection.json"),
       object({{"ok", V{true}},
               {"derived", V{derived}},
               {"rows", V{std::move(checked)}}}));
  // 本函数退出先释放真实捕获，再继续原 advance；不延长已安装世界的读租约。
}
V advance(ResidentReplaySession &session, Integer half_days,
          const fs::path &out, const StableId &name) {
  auto state = take(session.export_state());
  const auto initial = integer(field(state, "tick"));
  if (initial > std::numeric_limits<Integer>::max() - half_days)
    throw std::runtime_error("ThinTickOverflow");
  const auto target = initial + half_days;
  V::Array receipts;
  // 一个半日内可能含多次真实精确边界。只由原 advance 决定，宿主不跳事件或改
  // tick。
  for (Integer calls = 0; integer(field(state, "tick")) < target; ++calls) {
    if (calls == 10000)
      throw std::runtime_error("ThinAdvanceCallLimit");
    // 每次只請求下一個整半日邊界；一天總目標不變，原核心仍決定所有精確切片。
    const auto request_tick = integer(field(state, "tick")) + 1;
    const auto actor = string(field(state, "player_actor_id"));
    const auto seq =
        integer(field(field(state, "request_sequences"), actor.c_str()));
    // 只依据原会话已验证导出的时间根选请求形状，不改作者 profile。
    // 精确时间保原上界；旧整半日世界用原空参数
    // advance，每次仍由核心推进一半日。
    V::Dict params;
    if (state.find("world_timeline"))
      params.emplace_back("requested_to", take(encode_world_moment2(
                                              WorldMoment{request_tick, {}})));
    auto command = object({{"kind", V{"advance"}},
                           {"actor_id", V{actor}},
                           {"request_seq", V{seq + 1}},
                           {"expected_revision", field(state, "revision")},
                           {"params", object(std::move(params))}});
    auto result = take(session.execute(command));
    V::Array measured_stages;
    for (const auto &[label, usec] : result.timing.stages)
      measured_stages.push_back(
          object({{"stage", V{label}}, {"usec", V{usec}}}));
    measure_note(
        {{"metric", V{"command_timing"}},
         {"phase", V{name}},
         {"kind", field(command, "kind")},
         {"request_seq", field(command, "request_seq")},
         {"decode_usec", V{static_cast<Integer>(result.timing.decode_usec)}},
         {"preview_usec", V{static_cast<Integer>(result.timing.preview_usec)}},
         {"submit_usec", V{static_cast<Integer>(result.timing.submit_usec)}},
         {"dto_usec", V{static_cast<Integer>(result.timing.dto_usec)}},
         {"stages", V{std::move(measured_stages)}}});
    receipts.push_back(result.receipt);
    const auto *ok = field(result.receipt, "ok").as<bool>();
    save(out / (name + ".receipts.json"), V{receipts});
    if (!ok || !*ok)
      throw ReceiptRejected{std::move(result.receipt)};
    state = take(session.export_state());
  }
  if (integer(field(state, "tick")) != target)
    throw std::runtime_error("ThinAdvanceTargetMismatch");
  save(out / (name + ".json"), state);
  return state;
}
// 显式测试日数只决定请求终点；不改世界时钟、规则或原无参数模式的跨度。
Integer requested_half_days(int argc, char **argv, const std::string &mode) {
  if (argc == 5)
    return mode == "new" ? 2 : 1;
  if (argc != 7 || std::string(argv[5]) != "--days")
    throw std::runtime_error("ThinExplicitDaysArgumentRequired");
  const std::string days_text = argv[6];
  Integer days = 0;
  const auto parsed = std::from_chars(
      days_text.data(), days_text.data() + days_text.size(), days);
  if (parsed.ec != std::errc{} ||
      parsed.ptr != days_text.data() + days_text.size() || days <= 0 ||
      days > std::numeric_limits<Integer>::max() / 2)
    throw std::runtime_error("ThinPositiveDaysRequired");
  return days * 2;
}
} // namespace
int main(int argc, char **argv) {
  std::string stage = "arguments";
  fs::path output;
  try {
    if (argc != 5 && argc != 7)
      throw std::runtime_error(
          "Usage: resident_complete_thin new|binary|json checked_catalog.json "
          "scenario_id output_directory [--days positive_integer]");
    const std::string mode = argv[1];
    const auto half_days = requested_half_days(argc, argv, mode);
    output = argv[4];
    fs::create_directories(output);
    stage = "checked_catalog_input";
    const auto catalog = load(argv[2]);
    // 此文件必须由真正原作者验证链的 checked adapter 产出；本宿主不接受 raw
    // INDEX。
    const auto *checked = catalog.find("ok");
    const auto *checked_ok = checked ? checked->as<bool>() : nullptr;
    if (catalog.find("format") || !checked_ok || !*checked_ok ||
        !catalog.find("data"))
      throw std::runtime_error("CheckedCatalogDtoRequired");
    auto session = create_resident_replay_session();
    if (!session)
      throw std::runtime_error("ResidentSessionRequired");
    if (mode == "new") {
      stage = "native_catalog_identity";
      const auto identity =
          take(read_native_catalog_identity(catalog, argv[3]));
      save(output / "actual.catalog-identity.json", identity_value(identity));
      stage = "initialize_native";
      const auto &scenario =
          field(field(field(catalog, "data"), "scenarios"), argv[3]);
      take(session->initialize_native(
          catalog,
          object({{"scenario_id", V{argv[3]}},
                  {"world_seed", field(scenario, "seed_text")},
                  {"founder_selection", field(scenario, "player_actor")},
                  {"expected_catalog_identity", identity_value(identity)}})));
      stage = "original_role_dialogue_projection";
      const auto role_start = std::chrono::steady_clock::now();
      check_original_role_dialogue_projection(*session, catalog, argv[3],
                                              output, mode);
      const auto role_usec = elapsed_usec(role_start);
      measure_note({{"metric", V{"role_verification"}},
                    {"mode", V{mode}},
                    {"usec", V{role_usec}}});
      // 原安裝已成功；導出失敗應報真階段，不再冒作初始化首錯。
      stage = "export_initial_state";
      save(output / "initial.json", take(session->export_state()));
      measure_resident_rss("installed_initial");
      stage = argc == 5 ? "advance_one_day" : "advance_requested_days";
      advance(*session, half_days, output, "day");
      measure_resident_rss("installed_day");
      stage = "checkpoint";
      checkpoint_phase_timing::Frame checkpoint_phases;
      checkpoint_phase_timing::Capture checkpoint_capture(checkpoint_phases);
      const auto checkpoint_start = std::chrono::steady_clock::now();
      const auto checkpoint = take(session->checkpoint());
      const auto checkpoint_usec = elapsed_usec(checkpoint_start);
      checkpoint_capture.stop();
      measure_note(
          {{"metric", V{"checkpoint_api"}}, {"usec", V{checkpoint_usec}}});
      measure_checkpoint_phases(checkpoint_phases, "checkpoint_api");
      const auto write_start = std::chrono::steady_clock::now();
      write(output / "day.checkpoint",
            std::string(checkpoint.begin(), checkpoint.end()));
      const auto write_usec = elapsed_usec(write_start);
      measure_note(
          {{"metric", V{"checkpoint_copy_write"}}, {"usec", V{write_usec}}});
    } else if (mode == "binary") {
      stage = "initialize_checkpoint";
      const auto file_start = std::chrono::steady_clock::now();
      const auto raw = bytes(output / "day.checkpoint");
      const auto file_usec = elapsed_usec(file_start);
      measure_note(
          {{"metric", V{"checkpoint_file_read"}}, {"usec", V{file_usec}}});
      {
        const Bytes checkpoint_input(raw.begin(), raw.end());
        checkpoint_phase_timing::Frame restore_phases;
        checkpoint_phase_timing::Capture restore_capture(restore_phases);
        const auto restore_start = std::chrono::steady_clock::now();
        take(session->initialize_checkpoint(catalog, checkpoint_input));
        const auto restore_usec = elapsed_usec(restore_start);
        restore_capture.stop();
        // 原 API 含目录准备、解码及世界全校验；不能称纯 decoder 时间。
        measure_note({{"metric", V{"initialize_checkpoint_api"}},
                      {"usec", V{restore_usec}}});
        measure_checkpoint_phases(restore_phases, "initialize_checkpoint_api");
      }
      stage = "restored_role_dialogue_projection";
      const auto role_start = std::chrono::steady_clock::now();
      check_original_role_dialogue_projection(*session, catalog, argv[3],
                                              output, mode);
      const auto role_usec = elapsed_usec(role_start);
      measure_note({{"metric", V{"role_verification"}},
                    {"mode", V{mode}},
                    {"usec", V{role_usec}}});
      save(output / "restored.binary.json", take(session->export_state()));
      measure_resident_rss("installed_restored");
      stage = "continue_binary";
      advance(*session, half_days, output, "continued.binary");
    } else if (mode == "json") {
      stage = "initialize_json";
      take(session->initialize(catalog, load(output / "day.json")));
      save(output / "restored.json.json", take(session->export_state()));
      stage = "continue_json";
      advance(*session, half_days, output, "continued.json");
    } else
      throw std::runtime_error("UnknownThinMode");
    const auto report =
        object({{"ok", V{true}},
                {"mode", V{mode}},
                {"stage", V{stage}},
                {"requested_half_days", V{half_days}},
                {"scope", V{"Only this actual market sample; not all Complete "
                            "domain acceptance"}}});
    save(output / (mode + ".result.json"), report);
    std::cout << take(render_boundary_json(report)) << '\n';
    return 0;
  } catch (const ReceiptRejected &failure) {
    const auto report = object({{"ok", V{false}},
                                {"stage", V{stage}},
                                {"actual_receipt", failure.receipt}});
    if (!output.empty())
      save(output / (std::string(argc > 1 ? argv[1] : "thin") + ".result.json"),
           report);
    std::cout << take(render_boundary_json(report)) << '\n';
    return 2;
  } catch (const Failed &failure) {
    const auto report = object({{"ok", V{false}},
                                {"stage", V{stage}},
                                {"error", error_value(failure.error)}});
    if (!output.empty())
      save(output / (std::string(argc > 1 ? argv[1] : "thin") + ".result.json"),
           report);
    std::cout << take(render_boundary_json(report)) << '\n';
    return 2;
  } catch (const std::exception &error) {
    const auto report = object({{"ok", V{false}},
                                {"stage", V{stage}},
                                {"host_error", V{error.what()}}});
    if (!output.empty())
      save(output / (std::string(argc > 1 ? argv[1] : "thin") + ".result.json"),
           report);
    std::cout << take(render_boundary_json(report)) << '\n';
    return 3;
  } catch (checkpoint_job_detail::FailureException &failure) {
    // 保留同一任务拥有的技术首错；只读诊断，不改准入、容量或世界状态。
    std::cerr << "ThinTechnicalFailure stage=" << stage << " code="
              << checkpoint_job_failure_code_name(failure.error.code())
              << " required=" << failure.error.required() << '\n';
    if (failure.error.try_materialize_rule_error()) {
      if (const auto *error = failure.error.rule_error()) {
        std::cerr << "ThinOriginalFailure code=" << error->original.code
                  << " path=" << error->original.path << '\n';
      }
    }
    return 4;
  } catch (...) {
    std::cerr << "ThinTechnicalFailure stage=" << stage << '\n';
    return 4;
  }
}
