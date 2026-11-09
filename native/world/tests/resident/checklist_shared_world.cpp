// 共用配置的真实原生存读门；全部推进经
// ResidentWorld::preview/submit，冷消费者是新进程。
#include "checklist_a2_contract.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/actor_foundation/actor_foundation_codec.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/domains/family/care_root_capability_selection.hpp"
#include "resident/domains/growth/growth_command_result_retention.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/growth/growth_fixed_start.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/domains/growth/growth_market_cause.hpp"
#include "resident/domains/growth/growth_market_start_read.hpp"
#include "resident/domains/growth/growth_quiet_source.hpp"
#include "resident/domains/growth/growth_scalar_fifo_read.hpp"
#include "resident/domains/growth/growth_trade_terminal_read.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "resident/domains/social/social_knowledge_cut.hpp"
#include "resident/domains/social/social_time2_codec.hpp"
#include "resident/domains/military/field_battle/field_author_data.hpp"
#include "resident/domains/military/military_authority_time.hpp"
#include "resident/domains/military/military_exact_layout.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "resident/domains/military/military_exact_state_codec.hpp"
#include "resident/domains/military/military_fortification_node_codec.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/military_spatial_codec.hpp"
#include "resident/domains/military/naval_vessel_identity.hpp"
#include "resident/domains/military/naval_vessel_ref_codec_fields.hpp"
#include "resident/domains/player_plan/plan_commands.hpp"
#include "resident/domains/player_plan/plan_execution.hpp"
#include "resident/domains/player_plan/plan_lifecycle.hpp"
#include "resident/domains/player_plan/plan_sources.hpp"
#include "resident/domains/travel/local_move_source.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/complete_command_capture_dispatch.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/kernel/exact_time_world.hpp"
#include "resident/kernel/live_view_queries.hpp"
#include "resident/kernel/native_catalog_decode.hpp"
#include "resident/kernel/native_catalog_identity.hpp"
#include "resident/kernel/native_director_wake_world.hpp"
#include "resident/kernel/native_growth_market_sources.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/native_military_session_definitions.hpp"
#include "resident/kernel/native_military_spatial_definitions.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/kernel/world_restore_retention.hpp"
#include "resident/kernel/world_retention.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
#include "resident/storage/common_capture_read_set.hpp"
#include "resident/storage/common_command_publication.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include "resident_profile_coordinator.hpp"
#include "resident_slot_coordinator.hpp"
#include "resident_stored_slot_receipt.hpp"
#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <picosha2.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>

// 同一二进制链接未改业务断言的原组件测试，生成副本只重命名 main。
int checklist_original_route_cache();
int checklist_original_travel_catalog();
int checklist_original_battle_terrain();
// 原 Family 组件保持独立翻译单元，只改入口名，不替代完整来源/命名门。
int checklist_family_maturation_boundary();
int checklist_family_due_value_storage();

using namespace hanmo::resident;
namespace {
using V = BoundaryValue;
namespace fs = std::filesystem;
// 唯一 F 的原剧本与受控人物；不能把仍存在的 NPC 当作玩家。
const StableId scenario = "complete_market::scenario.complete_market";
const StableId player = "actor.he";

// Release 也保留断言；原错误码及路径不得降成假通过。
void require(bool value, const StableId &message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}
// 保留生产错误身份并取出成功值，不吞异常。
template <class T> T must(Outcome<T> value, const StableId &message) {
  require(value.ok, message + ": " + value.error.original.code + " " +
                        value.error.original.path + " " +
                        value.error.reason.value_or(""));
  return std::move(value.value);
}
// 只接受真实成功状态。
void must(Status value, const StableId &message) {
  require(value.ok, message + ": " + value.error.original.code + " " +
                        value.error.original.path + " " +
                        value.error.reason.value_or(""));
}

// 真实文件读写只保存原生检查点及完整只读摘要，不制造或修补世界状态。
StableId read_text(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  require(bool(input), "open " + path.string());
  std::ostringstream text;
  text << input.rdbuf();
  require(!input.bad(), "read " + path.string());
  return text.str();
}
// 从保存文件解析原边界值，保留字段顺序。
V read_json(const fs::path &path) {
  return must(parse_boundary_json(read_text(path)), "parse " + path.string());
}
// 读取磁盘中的原生二进制银行。
Bytes read_bytes(const fs::path &path) {
  const auto text = read_text(path);
  return {text.begin(), text.end()};
}
// 完整写入并检查关闭结果，文件失败不能通过。
void write_text(const fs::path &path, const StableId &text) {
  std::ofstream output(path, std::ios::binary);
  require(bool(output), "create " + path.string());
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  output.close();
  require(bool(output), "finish " + path.string());
}
// 导出完整只读状态而非选取少数字段。
void write_json(const fs::path &path, const V &value) {
  write_text(path, must(render_boundary_json(value), "render state") + "\n");
}
// 原封保存检查点字节。
void write_bytes(const fs::path &path, const Bytes &bytes) {
  write_text(path, StableId(bytes.begin(), bytes.end()));
}
// 同时比较值和顺序，报告第一个真实差异。
void exact(const V &expected, const V &actual, const StableId &message) {
  auto comparison = compare_boundary(expected, actual);
  const auto difference = comparison.value_difference
                              ? comparison.value_difference
                              : comparison.order_difference;
  require(comparison.exact(),
          message + (difference ? ": " + difference->path : ""));
}

// 只读原完整选型身份；正式初始化和冷恢复仍独立派生并严格比较它。
CatalogIdentity expected_identity(const V &catalog) {
  return must(read_native_catalog_identity(catalog, scenario),
              "read original shared catalog identity");
}

// 只读解码由正在测试的原生检查点取得；不把临时 WorldBundle 安装或写回。
class Run {
  std::shared_ptr<const FieldBattleResourceRead> resources_;

public:
  // 只借宿主显式授予的原资源读口；没有授权目录时不补造地形。
  explicit Run(std::shared_ptr<const FieldBattleResourceRead> resources = {})
      : resources_(std::move(resources)) {}
  std::shared_ptr<const PreparedCatalog> catalog;
  ResidentWorld world;

  // 从唯一共用配置走正式新局创建与安装。
  void initialize(const V &prepared) {
    NativeInitRequest request;
    request.scenario_id = scenario;
    const auto &selected = boundary::field(
        boundary::field(boundary::field(prepared, "data", "catalog"),
                        "scenarios", "data"),
        scenario, "scenarios");
    request.world_seed = boundary::get_string(selected, "seed_text", scenario);
    request.founder_selection = player;
    request.expected_catalog_identity = expected_identity(prepared);
    auto initial =
        must(make_native_initial_world(prepared, request, resources_.get()),
             "native new game");
    catalog = std::move(initial.catalog);
    must(world.install(catalog, std::move(initial.seed), true),
         "install native new game");
  }
  // 冷进程重新准入相同目录并恢复磁盘银行。
  void restore(const V &prepared, const Bytes &saved) {
    require(!world.identity().ok, "cold consumer must have no installed world");
    catalog = must(
        prepare_native_catalog_for_install(
            prepared, scenario, expected_identity(prepared), resources_.get()),
        "prepare original catalog for cold restore");
    must(world.restore(saved, catalog), "cold restore");
  }
  // 调用原生产检查点编码器。
  Bytes bytes() { return must(world.checkpoint(), "native checkpoint"); }
  // 解码实际银行并完整校验，只供摘要读取。
  std::unique_ptr<WorldBundle> snapshot() {
    auto seed = must(decode_world_checkpoint(bytes(), *catalog),
                     "decode real checkpoint");
    auto state = std::make_unique<WorldBundle>(catalog, std::move(seed));
    must(restore_native_transport_derived_values_in_candidate(*state),
         "restore transport derived values for read-only snapshot");
    must(restore_native_complete_director_wake_cache_in_candidate(*state),
         "restore director wake cache for read-only snapshot");
    must(state->validate(), "validate full read-only snapshot");
    return state;
  }
  // 导出全部持久状态，包含各领域结果和来源。
  V state() {
    return must(export_world_state(*snapshot()), "export full state");
  }
  // 读取真实身份、修订和请求序号，构造下一个原生命令。
  CommandEnvelope advance_request() {
    const auto identity = must(world.identity(), "current identity");
    const auto source = snapshot();
    const auto *sequence =
        source->common.state().request_sequences.find(player);
    require(sequence != nullptr, "real player request sequence");
    return {identity.world_id,
            identity.execution_epoch,
            *sequence + 1,
            identity.revision,
            player,
            std::make_shared<const CommandPayload>(
                CommandPayload{"advance", AdvanceCommand{}})};
  }
  // 每半日复用统一提交核账和原回执重放；实现置于共用 helper 后。
  void advance(Integer half_days);
  // 原过期修订沿统一失败合同和自动留证核验。
  void reject_stale_revision();
};
#include "checklist_checkpoint_cache.inc"
#include "checklist_core_foundation.inc"

// 真实推进只经共用 helper，所有行共享回执、Opening 与失败留证合同。
void Run::advance(Integer half_days) {
  const auto initial_tick = must(world.identity(), "advance origin").tick;
  for (Integer index = 0; index < half_days; ++index) {
    auto command = advance_request();
    const Integer expected_tick = initial_tick + index + 1;
    const auto receipt =
        checklist_core::commit(*this, command, nullptr, &expected_tick);
    checklist_core::replay(*this, command, receipt);
  }
}

// 版本拒绝发生于 Opening 前，必须完整保留已发布世界。
void Run::reject_stale_revision() {
  auto command = advance_request();
  ++command.expected_revision;
  checklist_core::rejected(*this, command, "StalePreview");
}
// 实现片段按真实调用依赖排列，格式化不得重排。
// clang-format off
#include "checklist_k5_trade.inc"
#include "checklist_k1_retry.inc"
#include "checklist_trade_capture.inc"
#include "checklist_pending_started.inc"
#include "checklist_pending_common_paths.inc"
// clang-format on

// ECO-6 复用上方真实路线和来源 helper，必须在其定义后接入。
#include "checklist_eco_6.inc"
#include "checklist_publication_revision.inc"
#include "checklist_command_accounting_regression.inc"

#include "checklist_k2_run_diagnostic.inc"
#include "checklist_slot2.inc"

// 后台门复用上方正式槽 helper，必须在其定义后接入。
// clang-format off
#include "checklist_frozen_condition_inputs.inc"
#include "checklist_k2_session_definition.inc"
#include "checklist_k2_spatial_catalog.inc"
#include "checklist_k2_canonical_layout.inc"
#include "checklist_k2_fortification_nodes.inc"
#include "checklist_k9_4.inc"
#include "checklist_vessel_reference.inc"
// clang-format on
} // namespace

// 两个进程分别保存/读取相同文件；30 日是原历法的 60 半日，不是空循环或改时钟。
int main(int argc, char **argv) {
  try {
    require(
        argc == 5 || argc == 7,
        "usage: checklist_shared_world "
        "produce|consume|a2|F-INITIALIZE|K1-1|K1-1-CONSUME|K1-2|K1-2-"
        "CONSUME|K5-1|K5-1-"
        "CONSUME|ECO-6|ECO-6-CONSUME|TRADE-CAPTURE|PENDING-STARTED|"
        "PENDING-COMMON-PATHS|PENDING-STARTED-TEST-DERIVED|"
        "PENDING-COMMON-PATHS-TEST-DERIVED|CACHE|"
        "COMPONENTS|RETENTION|"
        "SLOT2-PRODUCE|SLOT2-CONSUME|K9-4-PRODUCE|K9-4-CONSUME|NATIVE-"
        "BOUNDARY|K2-SPATIAL|K2-SESSION|K2-CANONICAL|K2-FORTIFICATION|"
        "FROZEN-CONDITION|VESSEL-REFERENCE|K2-RUN-DIAGNOSTIC|FAMILY-DUE-"
        "COMPONENTS|PUBLICATION-REVISION-PRODUCE|PUBLICATION-REVISION-REPEAT|"
        "PUBLICATION-REVISION-COLD|PUBLICATION-REVISION-LATE|"
        "PUBLICATION-REVISION-COMPONENT|COMMAND-ACCOUNTING-INSPECT|"
        "COMMAND-ACCOUNTING-INSPECT-RED "
        "PREPARED OUTPUT DAYS [--terrain-root ABSOLUTE_DIRECTORY]");
    const StableId mode = argv[1];
    const auto prepared_bytes = read_text(argv[2]);
    const auto prepared =
        must(parse_boundary_json(prepared_bytes), "parse " + StableId{argv[2]});
    const bool derived_pending = mode == "PENDING-STARTED-TEST-DERIVED" ||
                                 mode == "PENDING-COMMON-PATHS-TEST-DERIVED";
    require(derived_pending ||
                picosha2::hash256_hex_string(prepared_bytes) !=
                    checklist_pending_started::derived_fixture_sha256,
            "TEST-PRECONDITION derived fixture is restricted to its two "
            "auxiliary modes");
    const fs::path output = argv[3];
    checklist_commit_observation::configure(output, mode, argv[2],
                                            prepared_bytes);
    const StableId days_text = argv[4];
    require(days_text == "0" || days_text == "30",
            "only smoke (0) or original A4 (30) days");
    const Integer half_days = days_text == "30" ? 60 : 0;
    if (mode == "SLOT2-PRODUCE" || mode == "SLOT2-CONSUME") {
      require(days_text == "0", "Slot/2 has its own real command sequence");
      if (mode == "SLOT2-PRODUCE")
        checklist_slot2::produce(prepared, output);
      else
        checklist_slot2::consume(prepared, output);
      return 0;
    }
    if (mode == "K9-4-PRODUCE" || mode == "K9-4-CONSUME") {
      require(days_text == "0", "K9-4 uses its actual worker command sequence");
      if (mode == "K9-4-PRODUCE")
        checklist_k9_4::produce(prepared, output);
      else
        checklist_k9_4::consume(prepared, output);
      return 0;
    }
    if (mode == "NATIVE-BOUNDARY") {
      require(days_text == "0", "boundary regression is not K9 acceptance");
      auto session = create_resident_replay_session();
      must(session->initialize_native(
               prepared, checklist_slot2::native_request(prepared)),
           "real shared-world native session");
      checklist_slot2::advance_with_boundary_contract(*session);
      std::cout << "BOUNDARY auxiliary: same-world public preview, authority, "
                   "identity, replay and pure-state preservation PASS\n";
      return 0;
    }
    std::shared_ptr<const FieldBattleResourceRead> resources;
    if (argc == 7) {
      require(StableId{argv[5]} == "--terrain-root",
              "only an explicit original terrain root is accepted");
      const fs::path terrain_root = argv[6];
      require(terrain_root.is_absolute() && fs::is_directory(terrain_root),
              "explicit absolute original terrain directory required");
      resources = std::make_shared<DirectoryFieldBattleResources>(
          expected_identity(prepared), terrain_root);
    }
    Run run(resources);
    if (mode == "COMMAND-ACCOUNTING-INSPECT" ||
        mode == "COMMAND-ACCOUNTING-INSPECT-RED") {
      require(days_text == "0", "inspection regression does not advance time");
      checklist_command_accounting_regression::run_mode(
          run, prepared, prepared_bytes, output,
          mode == "COMMAND-ACCOUNTING-INSPECT-RED");
      return 0;
    }
    // 共用封口定向回归与 K1-2 清单窗独立，只读清单中原历史银行。
    if (mode == "PUBLICATION-REVISION-PRODUCE" ||
        mode == "PUBLICATION-REVISION-REPEAT" ||
        mode == "PUBLICATION-REVISION-COLD" ||
        mode == "PUBLICATION-REVISION-LATE" ||
        mode == "PUBLICATION-REVISION-COMPONENT") {
      require(days_text == "0",
              "publication regression is not an elapsed-day gate");
      checklist_publication_revision::run_mode(run, prepared, prepared_bytes,
                                               output, mode);
      return 0;
    }
    // 共同前置只安装一次原 F；不推进、不提交业务，也不顺带执行存读门。
    if (mode == "F-INITIALIZE") {
      require(days_text == "0", "F initialization does not advance time");
      run.initialize(prepared);
      must(run.world.identity(), "initialized original F world identity");
      std::cout << "F-INITIALIZE: PASS (original native world installed; "
                   "no advance, business submission or checkpoint)\n";
      return 0;
    }
    if (mode == "K2-RUN-DIAGNOSTIC") {
      require(days_text == "0", "K2 entry diagnostic is not a named gate");
      run.initialize(prepared);
      return checklist_k2_run_diagnostic::observe(run, output);
    }
    if (mode == "K2-FORTIFICATION") {
      require(days_text == "0", "node component is not K2 acceptance");
      run.initialize(prepared);
      checklist_k2_fortification_nodes::run(run);
      return 0;
    }
    if (mode == "K2-CANONICAL") {
      require(days_text == "0", "canonical component is not K2 acceptance");
      run.initialize(prepared);
      checklist_k2_canonical_layout::run(run);
      return 0;
    }
    if (mode == "K2-SESSION") {
      require(days_text == "0",
              "session definition component is not K2 acceptance");
      run.initialize(prepared);
      checklist_k2_session_definition::run(run);
      return 0;
    }
    if (mode == "VESSEL-REFERENCE") {
      require(days_text == "0", "vessel reference is not Care acceptance");
      run.initialize(prepared);
      checklist_vessel_reference::run(run);
      return 0;
    }
    if (mode == "FROZEN-CONDITION") {
      require(days_text == "0",
              "frozen input component is not full source acceptance");
      run.initialize(prepared);
      checklist_frozen_condition_inputs::run(run);
      return 0;
    }
    if (mode == "K2-SPATIAL") {
      require(days_text == "0",
              "spatial catalog component is not K2 acceptance");
      run.initialize(prepared);
      checklist_k2_spatial_catalog::run(run);
      return 0;
    }
    if (mode == "RETENTION") {
      require(days_text == "0",
              "retention graph probe is not full Slot/2 acceptance");
      run.initialize(prepared);
      checklist_cache::retention_graph(run);
      return 0;
    }
    if (mode == "FAMILY-DUE-COMPONENTS") {
      require(days_text == "0",
              "Family values are not complete phase acceptance");
      run.initialize(prepared);
      const auto before = run.bytes();
      // 真实未选情景不伪造 Care marker；只证明正式 prepare 后仍无选择 owner。
      const auto selected = must(
          read_native_care_root_capability_selection(*run.catalog, scenario),
          "actual shared catalog Care selection");
      require(!selected.physical_version && !selected.family_education &&
                  !run.catalog->care_root_capability_selection,
              "actual legacy scenario keeps Care unselected without owner");
      require(checklist_family_maturation_boundary() == 0,
              "original Family maturation boundary component");
      require(checklist_family_due_value_storage() == 0,
              "original Family typed value component");
      require(run.bytes() == before,
              "Family value components leave the shared world unchanged");
      return 0;
    }
    if (mode == "COMPONENTS") {
      require(days_text == "0", "component tests do not replace K9 scenarios");
      require(checklist_original_route_cache() == 0,
              "original route cache test");
      require(checklist_original_travel_catalog() == 0,
              "original travel catalog test");
      require(checklist_original_battle_terrain() == 0,
              "original battle terrain test");
      return 0;
    }
    if (mode == "CACHE") {
      require(days_text == "0",
              "cache auxiliary must not replace a K9 scenario");
      run.initialize(prepared);
      checklist_cache::boundary(run);
      return 0;
    }
    if (mode == "K1-1" || mode == "K1-1-CONSUME") {
      require(days_text == "0", "K1-1 is the original zero-time command");
      if (mode == "K1-1")
        checklist_core::produce_submission(run, prepared, output);
      else
        checklist_core::consume_submission(run, prepared, output);
      return 0;
    }
    // 共享捕获辅助门独立运行；不把局部所有权证据计入任一命名整行。
    if (mode == "TRADE-CAPTURE") {
      require(days_text == "0", "capture auxiliary retains original durations");
      checklist_trade_capture::run(run, prepared, output);
      return 0;
    }
    // 原辅助模式仍只用 F；衍生模式独立命名，仅获准用于消费者与回滚率量测。
    if (mode == "PENDING-STARTED" || mode == "PENDING-COMMON-PATHS" ||
        derived_pending) {
      require(days_text == "0", "pending auxiliary retains original durations");
      const auto input = checklist_pending_started::fixture_input(
          argv[2], prepared_bytes, prepared, derived_pending);
      if (mode == "PENDING-STARTED" || mode == "PENDING-STARTED-TEST-DERIVED")
        checklist_pending_started::run(run, prepared, output, input);
      else
        checklist_pending_common::run(run, prepared, output, input);
      return 0;
    }
    if (mode == "K5-1" || mode == "K5-1-CONSUME") {
      require(days_text == "0",
              "K5-1 uses its original travel and trade durations");
      if (mode == "K5-1")
        checklist_k5::produce(run, prepared, output);
      else
        checklist_k5::consume(run, prepared, output);
      return 0;
    }
    if (mode == "ECO-6" || mode == "ECO-6-CONSUME") {
      require(days_text == "0",
              "ECO-6 retains original travel and trade durations");
      if (mode == "ECO-6")
        checklist_eco::produce(run, prepared, output);
      else
        checklist_eco::consume(run, prepared, output);
      return 0;
    }
    if (mode == "K1-2" || mode == "K1-2-CONSUME") {
      require(days_text == "0",
              "K1-2 retains its real travel and trade duration");
      if (mode == "K1-2")
        checklist_k1::produce(run, prepared, output);
      else
        checklist_k1::consume(run, prepared, output);
      return 0;
    }
    if (mode == "a2") {
      require(days_text == "0",
              "A2 does not substitute an elapsed-time fixture");
      run.initialize(prepared);
      const auto before = run.bytes();
      auto request = run.advance_request();
      const auto quote =
          must(run.world.preview(request), "A2 real shared-world preview");
      require(quote && run.bytes() == before,
              "A2 preview preserves installed state");
      run.reject_stale_revision();
      auto snapshot = run.snapshot();
      checklist_a2::run(*snapshot);
      require(run.bytes() == before,
              "A2 rejection probes preserve installed world");
      std::cout << "A2 public runtime: registry flags; 6 phase failures; "
                   "3 independent failures; real cache/index retention; "
                   "fixed exception helper. "
                   "The complete source registry is checked separately.\n";
      return 0;
    }
    if (mode == "produce") {
      fs::create_directories(output);
      run.initialize(prepared);
      const auto origin = must(run.world.identity(), "initial identity");
      run.reject_stale_revision();
      run.advance(half_days);
      write_bytes(output / "saved.hmbank", run.bytes());
      write_json(output / "saved-state.json", run.state());
      run.advance(half_days);
      write_bytes(output / "uninterrupted.hmbank", run.bytes());
      write_json(output / "uninterrupted-state.json", run.state());
      require(must(run.world.identity(), "producer end").tick ==
                  origin.tick + 2 * half_days,
              "producer advances the complete 60-day baseline");
    } else {
      require(mode == "consume", "unknown process mode");
      const auto saved = read_bytes(output / "saved.hmbank");
      run.restore(prepared, saved);
      require(run.bytes() == saved,
              "cold restore preserves exact checkpoint bytes");
      exact(read_json(output / "saved-state.json"), run.state(),
            "cold restored state exact");
      run.reject_stale_revision();
      run.advance(half_days);
      require(run.bytes() == read_bytes(output / "uninterrupted.hmbank"),
              "cold continuation equals uninterrupted checkpoint bytes");
      exact(read_json(output / "uninterrupted-state.json"), run.state(),
            "cold continuation equals uninterrupted full state");
      write_json(output / "restored-state.json", run.state());
    }
    std::cout << "shared_world " << mode << ": PASS (" << days_text
              << " days per segment, real native "
                 "preview/submit/checkpoint/restore)\n";
    return 0;
  } catch (checkpoint_job_detail::FailureException &failure) {
    // 瞬态检查点失败保留固定码；不能因其不继承 std::exception 而丢失诊断。
    failure.error.try_materialize_rule_error();
    if (const auto *error = failure.error.rule_error()) {
      std::cerr << "shared_world: FAIL " << error->original.code << " "
                << error->original.path << '\n';
    } else {
      std::cerr << "shared_world: FAIL "
                << checkpoint_job_failure_code_name(failure.error.code())
                << '\n';
    }
    return 1;
  } catch (const std::exception &error) {
    std::cerr << "shared_world: FAIL " << error.what() << '\n';
    return 1;
  }
}
