// 把已构造的原邀约、探询和拜访工时接入中央 ActionTime，保留原域地址和错误。
#include "native_interaction_action_time_bindings.hpp"

#include "native_complete_world_sources.hpp"
#include "native_interaction_world.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace hanmo::resident {
namespace {
// 仅转发已经存在的原业务读口，不保存第二份 elapsed、结果或参与状态。
// 外层成员逆序保证 ActionTime/本适配器先于其借用的 Interaction/Family 析构。
class NativeInteractionActionTimeRead final
    : public ActionTimeOriginalSourceRead {
  const ActionTimeOriginalSourceRead &original_;

public:
  // 只借 factory 已核同候选的原读口；不调用外层工厂，不制造选择或执行记录。
  explicit NativeInteractionActionTimeRead(
      const ActionTimeOriginalSourceRead &original)
      : original_(original) {}

  // 按原 owner 枚举真实活动及终局记录；原失败或 sink 失败原样返回。
  Status
  enumerate_action_time_owners(const ActionTimeRegistration &registration,
                               ActionTimeOwnerIdSink &sink) const override {
    return original_.enumerate_action_time_owners(registration, sink);
  }

  // 读取原 owner 的行动证明，不从中央 elapsed 倒造原执行源。
  Outcome<ActionTimeOwnerProof>
  read_action_time_owner(const ActionTimeRegistration &registration,
                         const StableId &action) const override {
    return original_.read_action_time_owner(registration, action);
  }

  // 沿原事件类型和 schema 读取实际时刻，保留原来源失配的拒绝。
  Outcome<WorldMoment>
  read_action_time_source(const ElapsedTimeSource &source) const override {
    return original_.read_action_time_source(source);
  }

  // 固定工时的历史由原领域完整复验，适配器不改变区间或完成判定。
  Status validate_fixed_action_sources(
      const ActionTimeRegistration &registration,
      const ActionTimeOwnerProof &proof, const ActionElapsedState &elapsed,
      const std::vector<FixedServiceTransition> &history) const override {
    return original_.validate_fixed_action_sources(registration, proof, elapsed,
                                                   history);
  }

  // 保留候选专用入口和实际同一 elapsed store，不退回稳定读或复制候选。
  Status validate_fixed_action_sources_in_candidate(
      const ActionTimeRegistration &registration,
      const ActionTimeOwnerProof &proof, const ActionElapsedState &elapsed,
      const std::vector<FixedServiceTransition> &history,
      const ActionElapsedStore &candidate) const override {
    return original_.validate_fixed_action_sources_in_candidate(
        registration, proof, elapsed, history, candidate);
  }

  // 共享工期仍交原 owner 判定；原固定行动无此模式时保留其真实拒绝。
  Status validate_shared_action_sources(
      const ActionTimeRegistration &registration,
      const ActionTimeOwnerProof &proof,
      const SharedWorkElapsedState &elapsed) const override {
    return original_.validate_shared_action_sources(registration, proof,
                                                    elapsed);
  }

  // 按原领域状态解释继续与停因，不用技术时片或剩余量替代实际业务状态。
  Outcome<RunStop>
  read_action_continuation_stop(const ActionTimeRegistration &registration,
                                const ActionTimeOwnerProof &proof,
                                const WorldMoment &at) const override {
    return original_.read_action_continuation_stop(registration, proof, at);
  }

  // 原样返回原合同的剩余量及语义，不新增职业倒计时。
  Outcome<OriginalActionRemaining>
  read_original_action_remaining(const ActionTimeRegistration &registration,
                                 const ActionTimeOwnerProof &proof,
                                 const WorldMoment &at) const override {
    return original_.read_original_action_remaining(registration, proof, at);
  }

  // 授权窗专用检查也沿原 owner 执行，不把不支持的模式转成成功。
  Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(
      const ActionTimeRegistration &registration,
      const ActionTimeOwnerProof &proof, const WorldMoment &at) const override {
    return original_.read_exhausted_authorization_window(registration, proof,
                                                         at);
  }

  // 原 owner 决定是否存在共享 gate，适配器不从传入 payload 合成结果地址。
  Outcome<StableId>
  read_gate_result_id(const SharedWorkGateResult &gate) const override {
    return original_.read_gate_result_id(gate);
  }

  // 按原结果地址读取专业账；缺失或不适用的 gate 保留原错误。
  Outcome<SharedWorkGateResult>
  read_original_gate_result(const StableId &id) const override {
    return original_.read_original_gate_result(id);
  }

  // 对原 gate 的反读校验不绕过原业务，当前三种固定行动仍明确拒绝该模式。
  Status validate_original_gate_result(
      const SharedWorkGateResult &gate) const override {
    return original_.validate_original_gate_result(gate);
  }

  // 仅转交原 owner 的 Life 工期口；当前固定行动没有该口，不伪造 period。
  const LifeWorkPeriodSourceRead *life_work_period_sources(
      const LifeWorkPeriodSourceRef &source) const noexcept override {
    return original_.life_work_period_sources(source);
  }
};

// 比较完整编译注册而非仅 action_kind，防止相同名字夹带不同模式或源版本。
// 返回 false 只表示选择失配，不授予任何来源能力。
bool same_registration(const ActionTimeRegistration &selected,
                       const ActionTimeRegistration &original) {
  return selected.action_kind == original.action_kind &&
         selected.owner_domain == original.owner_domain &&
         selected.profile_id == original.profile_id &&
         selected.profile_version == original.profile_version &&
         selected.source_schema == original.source_schema &&
         selected.mode == original.mode &&
         selected.remaining_meaning == original.remaining_meaning &&
         selected.shared_gate_kind == original.shared_gate_kind &&
         selected.retained_wait_phases == original.retained_wait_phases;
}

// 只识别三个既定原域，不把调度 provider 名 interaction 当作来源域名。
// 非本叶拥有的注册由中央其余专业 owner 负责，不能因此视为已有实现。
bool interaction_owner(const StableId &domain) {
  return domain == personal_appointment_module ||
         domain == contact_inquiry_module ||
         domain == visit_appointment_meeting_module;
}

// 返回构造/绑定错误，不把不完整的适配器发布给中央闭包。
// 保留 action_kind 以便定位具体失配的原专业注册。
Outcome<CheckpointVector<CompiledActionTimeOwnerBinding>>
invalid(const char *code, const StableId &kind = {}) {
  return {false, {}, common_failure(code, kind, ErrorKind::internal).error};
}
} // namespace

// 仅装配同一 complete graph 已存在的原交互读口，不扩展实际模块选择。
// 所有成功登记都持有真实转发对象，原源和时钟仍只属于同一 WorldBundle。
Outcome<CheckpointVector<CompiledActionTimeOwnerBinding>>
make_native_interaction_action_time_bindings(
    const NativeCompleteWorldCharacterSources &characters) {
  try {
    const auto &world = characters.early.world;
    if (!world.catalog || !world.catalog->exact_time ||
        !exact_complete_selected(*world.catalog->exact_time) ||
        !world.catalog->complete_exact_time_runtime)
      return invalid("InteractionActionTimeRuntimeMismatch");
    const auto &registrations =
        world.catalog->complete_exact_time_runtime->action_times;
    const auto valid = validate_action_time_registry(registrations);
    if (!valid.ok)
      return {false, {}, valid.error};

    bool selected = false;
    for (const auto &registration : registrations)
      selected = selected || interaction_owner(registration.owner_domain);
    if (!selected)
      return {true, {}, {}};

    const auto *owner = characters.interaction;
    if (!owner || !owner->bound_to(world, characters.family.body(),
                                   characters.family.people(),
                                   characters.family.control()))
      return invalid("InteractionActionTimeOriginalOwnerMismatch");
    const auto binding = owner->validate_bindings();
    if (!binding.ok)
      return {false, {}, binding.error};

    CheckpointVector<CompiledActionTimeOwnerBinding> out;
    for (const auto &registration : registrations) {
      if (!interaction_owner(registration.owner_domain))
        continue;
      const ActionTimeOriginalSourceRead *original = nullptr;
      ActionTimeRegistration expected;
      if (registration.owner_domain == personal_appointment_module) {
        expected = personal_appointment_action_time_registration();
        if (owner->personal())
          original = &owner->personal()->actions();
      } else if (registration.owner_domain == contact_inquiry_module) {
        expected = contact_inquiry_action_time_registration();
        if (owner->inquiry())
          original = &owner->inquiry()->actions();
      } else {
        expected = visit_appointment_meeting_action_time_registration();
        if (owner->visit())
          original = &owner->visit()->actions();
      }
      if (!original || !same_registration(registration, expected))
        return invalid("InteractionActionTimeSelectedOwnerMismatch",
                       registration.action_kind);
      for (const auto &existing : out)
        if (existing.owner_domain == registration.owner_domain)
          return invalid("InteractionActionTimeDuplicateOwner",
                         registration.action_kind);
      out.push_back(
          {registration.owner_domain,
           checkpoint_make_shared<NativeInteractionActionTimeRead>(*original)});
    }
    return {true, std::move(out), {}};
  } catch (const std::bad_alloc &) {
    return invalid("InteractionActionTimeResourceExhausted");
  } catch (const std::length_error &) {
    return invalid("InteractionActionTimeResourceExhausted");
  }
}
} // namespace hanmo::resident
