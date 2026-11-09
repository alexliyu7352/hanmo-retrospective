#include "native_growth_market_sources.hpp"
#include "native_action_time_world.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/military/military_exact_phase.hpp"
#include "motion_exact_world_bindings.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/tasks/task_complete_action_time.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
template<class T,class Fn>Outcome<T> growth_read(const WorldBundle&w,Fn fn){if(!w.catalog||!w.catalog->exact_time||!w.catalog->growth||!w.catalog->common||!w.action_elapsed)return {false,{},common_failure("GrowthExactWorldSourcesRequired",{},ErrorKind::internal).error};auto at=w.actual_phase_moment();if(!at.ok)return {false,{},at.error};auto market=read_native_growth_market_exact_sources(w);if(!market.ok)return {false,{},std::move(market.error)};GrowthActionTimeRead source(w.common,w.growth,*w.catalog->growth,*w.catalog->common,*w.action_elapsed,w.catalog->exact_time->scale_ref,w.catalog->identity,at.value,{},&w.interaction,w.catalog->interaction.get(),market.value?&*market.value:nullptr);return fn(source);}
// A lease survives a single candidate advance. Its reader must still recapture the actual phase clock
// on every call, because Common Calendar is normalized only after all terminal effects are applied.
class WorldGrowthActionTimeRead final:public ActionTimeOriginalSourceRead {
 const WorldBundle&w_;
public:
 explicit WorldGrowthActionTimeRead(const WorldBundle&w):w_(w){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink& sink)const override{return growth_read<Done>(w_,[&](const auto&s){return s.enumerate_action_time_owners(r,sink);});}
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&r,const StableId&id)const override{return growth_read<ActionTimeOwnerProof>(w_,[&](const auto&s){return s.read_action_time_owner(r,id);});}
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&ref)const override{return growth_read<WorldMoment>(w_,[&](const auto&s){return s.read_action_time_source(ref);});}
 Status validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&e,const std::vector<FixedServiceTransition>&h)const override{return growth_read<Done>(w_,[&](const auto&s){return s.validate_fixed_action_sources(r,p,e,h);});}
 Status validate_shared_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const SharedWorkElapsedState&e)const override{return growth_read<Done>(w_,[&](const auto&s){return s.validate_shared_action_sources(r,p,e);});}
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const override{return growth_read<RunStop>(w_,[&](const auto&s){return s.read_action_continuation_stop(r,p,at);});}
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const override{return growth_read<OriginalActionRemaining>(w_,[&](const auto&s){return s.read_original_action_remaining(r,p,at);});}
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&gate)const override{return growth_read<StableId>(w_,[&](const auto&s){return s.read_gate_result_id(gate);});}
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&id)const override{return growth_read<SharedWorkGateResult>(w_,[&](const auto&s){return s.read_original_gate_result(id);});}
 Status validate_original_gate_result(const SharedWorkGateResult&gate)const override{return growth_read<Done>(w_,[&](const auto&s){return s.validate_original_gate_result(gate);});}
};
// 只接已存在的 Field Office/Formation 原读口；不拥有第二份状态或工时。
// 每次调用重新取得实际 M 的原军事 lease，借用上下文只活在该调用栈内。
class WorldFieldMilitaryActionTimeRead final : public ActionTimeOriginalSourceRead {
  const WorldBundle &world_;
  const CompleteExactWorldBindingOwner &owner_;
  MilitaryActionTimeReadSelection selected_;

  // 只能在 Complete 图发布后调用；构造适配器本身绝不回调尚未完成的图。
  template <class T, class F> Outcome<T> original(F &&read) const {
    if (!world_.catalog || !world_.catalog->military ||
        !world_.military_action_time || !world_.action_elapsed)
      return {false, {}, common_failure("FieldMilitaryActionRootsRequired", {},
                                       ErrorKind::unsupported_capability).error};
    auto at = world_.actual_phase_moment();
    if (!at.ok)
      return {false, {}, std::move(at.error)};
    auto lease = owner_.read_military(at.value);
    if (!lease.ok)
      return {false, {}, std::move(lease.error)};
    if (!lease.value)
      return {false, {}, common_failure("FieldMilitaryActionLeaseRequired", {},
                                       ErrorKind::unsupported_capability).error};
    const auto binding = lease.value->bindings();
    if (&binding.common != &world_.common ||
        &binding.military != &world_.military ||
        &binding.catalog != world_.catalog->military.get() ||
        binding.action_time != &*world_.military_action_time ||
        &binding.elapsed != &*world_.action_elapsed ||
        bool(binding.office) != selected_.office_accept ||
        bool(binding.formation) != selected_.army_form || binding.training ||
        binding.returning || binding.resupply || binding.naval_repair ||
        binding.dynamic_sources || binding.duty)
      return {false, {}, common_failure("FieldMilitaryActionCandidateMismatch", {},
                                       ErrorKind::internal).error};
    auto now = binding.clock.read_current_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto same = compare_world_moments(at.value, now.value, binding.budget);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value != 0)
      return {false, {}, common_failure("FieldMilitaryActionMomentMismatch", {},
                                       ErrorKind::internal).error};
    MilitaryActionTimeReadContexts contexts;
    if (binding.office)
      contexts.office.emplace(*binding.office);
    if (binding.formation)
      contexts.formation.emplace(*binding.formation);
    NativeMilitaryActionTimeRead source(
        binding.common, binding.military, *binding.action_time, binding.elapsed,
        selected_, std::move(contexts));
    return read(source);
  }

public:
  // 只借同一不可移动 Complete owner；不会保活父图成环或构造期获取业务源。
  WorldFieldMilitaryActionTimeRead(const WorldBundle &world,
                                  const CompleteExactWorldBindingOwner &owner,
                                  MilitaryActionTimeReadSelection selected)
      : world_(world), owner_(owner), selected_(selected) {}
  // 原军事是否支持参与历史由原 reader 决定，不伪造空成功。
  Status visit_actor_action_history(const ActionTimeRegistration &r,
                                   const StableId &actor,
                                   ActionTimeActorHistorySink &sink) const override {
    return original<Done>([&](const auto &s) {
      return s.visit_actor_action_history(r, actor, sink);
    });
  }
  // 枚举真实原业务正文，不用 elapsed 自证 owner 覆盖。
  Status enumerate_action_time_owners(const ActionTimeRegistration &r,
                                     ActionTimeOwnerIdSink &sink) const override {
    return original<Done>([&](const auto &s) {
      return s.enumerate_action_time_owners(r, sink);
    });
  }
  // 原注册及原行动证明仍由军事 reader 严格反读。
  Outcome<ActionTimeOwnerProof> read_action_time_owner(
      const ActionTimeRegistration &r, const StableId &id) const override {
    return original<ActionTimeOwnerProof>([&](const auto &s) {
      return s.read_action_time_owner(r, id);
    });
  }
  // 只读取原 military source/schema 的实际时刻。
  Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource &ref) const override {
    return original<WorldMoment>([&](const auto &s) {
      return s.read_action_time_source(ref);
    });
  }
  // 固定工时历史和原完成来源不改写。
  Status validate_fixed_action_sources(const ActionTimeRegistration &r,
      const ActionTimeOwnerProof &p, const ActionElapsedState &e,
      const std::vector<FixedServiceTransition> &history) const override {
    return original<Done>([&](const auto &s) {
      return s.validate_fixed_action_sources(r, p, e, history);
    });
  }
  // 候选必须仍是同一实际 elapsed 根，不借稳定快照或第二份账。
  Status validate_fixed_action_sources_in_candidate(const ActionTimeRegistration &r,
      const ActionTimeOwnerProof &p, const ActionElapsedState &e,
      const std::vector<FixedServiceTransition> &history,
      const ActionElapsedStore &candidate) const override {
    if (!world_.action_elapsed || &candidate != &*world_.action_elapsed)
      return common_failure("FieldMilitaryActionElapsedMismatch", {}, ErrorKind::internal);
    return original<Done>([&](const auto &s) {
      return s.validate_fixed_action_sources_in_candidate(r, p, e, history, candidate);
    });
  }
  // 共享工期若非原军事能力，保留原拒绝。
  Status validate_shared_action_sources(const ActionTimeRegistration &r,
      const ActionTimeOwnerProof &p, const SharedWorkElapsedState &e) const override {
    return original<Done>([&](const auto &s) {
      return s.validate_shared_action_sources(r, p, e);
    });
  }
  // 原行动状态决定继续与停因，不由技术时片代替。
  Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration &r,
      const ActionTimeOwnerProof &p, const WorldMoment &at) const override {
    return original<RunStop>([&](const auto &s) {
      return s.read_action_continuation_stop(r, p, at);
    });
  }
  // 剩余量及含义保持原合同。
  Outcome<OriginalActionRemaining> read_original_action_remaining(
      const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
      const WorldMoment &at) const override {
    return original<OriginalActionRemaining>([&](const auto &s) {
      return s.read_original_action_remaining(r, p, at);
    });
  }
  // 原 reader 不支持的授权窗仍拒绝，不补默认成功。
  Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(
      const ActionTimeRegistration &r, const ActionTimeOwnerProof &p,
      const WorldMoment &at) const override {
    return original<ExhaustedAuthorizationWindow>([&](const auto &s) {
      return s.read_exhausted_authorization_window(r, p, at);
    });
  }
  // 共享 gate 标识只来自原业务，不按传入 payload 合成。
  Outcome<StableId> read_gate_result_id(const SharedWorkGateResult &gate) const override {
    return original<StableId>([&](const auto &s) { return s.read_gate_result_id(gate); });
  }
  // 原结果不存在时保留真实失败。
  Outcome<SharedWorkGateResult> read_original_gate_result(const StableId &id) const override {
    return original<SharedWorkGateResult>([&](const auto &s) {
      return s.read_original_gate_result(id);
    });
  }
  // 原 gate 校验不降级为中央自报。
  Status validate_original_gate_result(const SharedWorkGateResult &gate) const override {
    return original<Done>([&](const auto &s) { return s.validate_original_gate_result(gate); });
  }
};

// 比较完整原注册；同名但 schema/mode/语义不同不能借这条已实现的桥。
bool same_field_action_registration(const ActionTimeRegistration &a,
                                    const ActionTimeRegistration &b) {
  return a.action_kind == b.action_kind && a.owner_domain == b.owner_domain &&
         a.profile_id == b.profile_id && a.profile_version == b.profile_version &&
         a.source_schema == b.source_schema && a.mode == b.mode &&
         a.remaining_meaning == b.remaining_meaning &&
         a.shared_gate_kind == b.shared_gate_kind &&
         a.retained_wait_phases == b.retained_wait_phases;
}

// 仅将原 Field 已实现的 Office/Formation 接入；缺少专业能力时继续拒绝。
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>>
make_field_military_action_reader(const WorldBundle &world,
                                 const CompleteExactTimeRuntime &runtime,
                                 const CompleteExactWorldBindingOwner &owner) {
  if (!world.catalog || !world.catalog->field_group_passage ||
      !world.catalog->field_battle || !world.field_battle ||
      !world.military_action_time || !world.action_elapsed)
    return {false, {}, common_failure("FieldMilitaryActionOriginalOwnerRequired", {},
                                     ErrorKind::unsupported_capability).error};
  auto selected = read_military_exact_phase_selection(*world.catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  const auto &s = selected.value;
  if (!s.field_original || s.dynamic_schema || s.actions.military_train ||
      s.actions.army_return || s.actions.army_resupply || s.naval_repair || s.station)
    return {false, {}, common_failure("FieldMilitaryActionSelectedOriginalRequired", {},
                                     ErrorKind::unsupported_capability).error};
  const auto originals = military_exact_action_time_registrations(s);
  std::size_t count = 0;
  for (const auto &actual : runtime.action_times) {
    if (actual.owner_domain != "military")
      continue;
    ++count;
    if (std::none_of(originals.begin(), originals.end(), [&](const auto &expected) {
          return same_field_action_registration(actual, expected);
        }))
      return {false, {}, common_failure("FieldMilitaryActionRegistrationMismatch",
                                       actual.action_kind, ErrorKind::internal).error};
  }
  if (count == 0 || count != originals.size())
    return {false, {}, common_failure("FieldMilitaryActionRegistrationCoverage", {},
                                     ErrorKind::internal).error};
  return {true, checkpoint_make_shared<WorldFieldMilitaryActionTimeRead>(
                    world, owner, s.actions), {}};
}
}
namespace {
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_action_time_sources_bound(const WorldBundle&w,const CompleteExactTimeRuntime&runtime,CheckpointVector<CompiledActionTimeOwnerBinding>owners,TaskWorkWorldFactory task_work,std::shared_ptr<const MotionCargoReadDependencies> cargo,const CharacterConditionWorldRead* original_body,const CompleteExactWorldBindingOwner* owner){
 if(!w.catalog||!w.catalog->exact_time||!w.world_timeline||!w.action_elapsed)return {false,{},common_failure("ExactActionWorldSourcePairRequired",{},ErrorKind::internal).error};
 auto selected=[&](const char*domain){return std::any_of(runtime.action_times.begin(),runtime.action_times.end(),[&](const auto&r){return r.owner_domain==domain;});};
 for(const auto&owner:owners)if(owner.owner_domain=="growth"||owner.owner_domain=="tasks"||owner.owner_domain=="travel")return {false,{},common_failure("DuplicateNativeCoreActionTimeOwner",owner.owner_domain,ErrorKind::internal).error};
 if(selected("growth")){if(!w.catalog->growth)return {false,{},common_failure("GrowthExactWorldSourcesRequired",{},ErrorKind::internal).error};owners.push_back({"growth",std::allocate_shared<WorldGrowthActionTimeRead>(CheckpointAllocator<WorldGrowthActionTimeRead>{},w)});}
 if(selected("tasks")){auto task=owner?make_complete_task_action_time_reader_in_owner(w,*owner,original_body):make_complete_task_action_time_reader(w,task_work,original_body);if(!task.ok)return {false,{},task.error};owners.push_back({"tasks",std::move(task.value)});}
 if(selected("travel")){auto movement=make_native_travel_action_time_reader(w,std::move(cargo));if(!movement.ok)return {false,{},movement.error};owners.push_back({"travel",std::move(movement.value)});}
 // 只在同 owner 的 Field 原子图接现成军事 reader；普通缺源不获空 provider。
 if(owner&&selected("military")&&w.catalog->field_group_passage){
  if(std::any_of(owners.begin(),owners.end(),[](const auto& v){return v.owner_domain=="military";}))return {false,{},common_failure("DuplicateNativeFieldMilitaryActionTimeOwner",{},ErrorKind::internal).error};
  auto military=make_field_military_action_reader(w,runtime,*owner);if(!military.ok)return {false,{},std::move(military.error)};
  owners.push_back({"military",std::move(military.value)});
 }
 auto source=std::allocate_shared<NativeActionTimeSourceRead>(CheckpointAllocator<NativeActionTimeSourceRead>{},runtime.action_times,std::move(owners));auto valid=source->validate_owner_bindings();if(!valid.ok)return {false,{},valid.error};return {true,std::move(source),{}};
}
}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_action_time_sources(const WorldBundle&w,const CompleteExactTimeRuntime&runtime,CheckpointVector<CompiledActionTimeOwnerBinding>owners,TaskWorkWorldFactory task_work,std::shared_ptr<const MotionCargoReadDependencies>cargo,const CharacterConditionWorldRead*body){return make_native_action_time_sources_bound(w,runtime,std::move(owners),task_work,std::move(cargo),body,nullptr);}
Outcome<std::shared_ptr<const ActionTimeOriginalSourceRead>> make_native_action_time_sources_in_owner(const WorldBundle&w,const CompleteExactTimeRuntime&runtime,CheckpointVector<CompiledActionTimeOwnerBinding>owners,const CompleteExactWorldBindingOwner&owner,std::shared_ptr<const MotionCargoReadDependencies>cargo,const CharacterConditionWorldRead*body){return make_native_action_time_sources_bound(w,runtime,std::move(owners),{},std::move(cargo),body,&owner);}
}
