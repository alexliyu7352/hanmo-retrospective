#include "native_director_world_scope.hpp"
#include "director_membership.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_director.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/local_economy/household_purchase_knowledge.hpp"
#include "resident/domains/politics/politics_director.hpp"
#include "resident/kernel/native_character_family_world.hpp"
#include "resident/kernel/native_director_actor_commands.hpp"
#include "resident/kernel/native_exact_result_owners.hpp"
#include "resident/kernel/native_face_to_face_sources.hpp"
#include "resident/kernel/native_interaction_world.hpp"
#include "resident/kernel/native_task_work_catalog.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>
namespace hanmo::resident {
namespace {
// 构造失败只退出本次拥有域；不写世界，也不把缺来源降成成功空对象。
struct ConstructionFailure {
  RuleError error;
};
// 取得已成功的原返回值，失败保留原错误并退出本次构造。
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw ConstructionFailure{std::move(value.error)};
  return std::move(value.value);
}
// 原绑定检查失败时不继续创建依赖该检查的借用。
void take(Status value) {
  if (!value.ok)
    throw ConstructionFailure{std::move(value.error)};
}
// 仅报告实际缺口或身份不一致，不分配任何业务结果。
[[noreturn]] void reject(const char *code,
                         ErrorKind kind = ErrorKind::internal) {
  throw ConstructionFailure{common_failure(code, {}, kind).error};
}
// 把拥有域构造异常转成原 Outcome；资源失败不泄露半成品来源。
template <class T, class F> Outcome<T> guarded(F &&construct) {
  try {
    return {true, construct(), {}};
  } catch (ConstructionFailure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("NativeDirectorScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("NativeDirectorScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
// 原 Complete
// 只在已完成的领域取得阶段被借入；此门不安装缺失域、不重入外层工厂。
void roots(const WorldBundle &world,
           const NativeDirectorWorldScopeDependencies &dependencies,
           const WorldMoment &at) {
  const auto &early = dependencies.core.characters.early;
  if (&early.world != &world || !world.catalog || !world.catalog->exact_time ||
      !exact_complete_selected(*world.catalog->exact_time) ||
      !world.catalog->complete_exact_time_runtime || !world.world_timeline ||
      !world.catalog->director || !world.catalog->director->subjects2 ||
      !world.director.subject2 || !world.catalog->common ||
      !world.catalog->tasks || !world.catalog->growth ||
      !world.catalog->travel || !world.catalog->organization ||
      !world.catalog->local_economy || !world.catalog->actor_foundation ||
      !world.actor_foundation || !world.organization_runtime ||
      !world.read_stamps)
    reject("NativeDirectorScopeActualRootsRequired",
           ErrorKind::unsupported_capability);
  if (take(compare_world_moments(at, take(world.actual_phase_moment()))) ||
      take(compare_world_moments(at, take(early.clock.read_current_moment()))))
    reject("NativeDirectorScopeActualMomentMismatch");
  if (!dependencies.authority_owner)
    reject("NativeDirectorScopeAuthorityOwnerRequired",
           ErrorKind::unsupported_capability);
  const auto authority = dependencies.authority_owner->bindings();
  const auto &organization = authority.organization;
  if (&organization.runtime != &*world.organization_runtime ||
      &organization.market.common != &world.common ||
      &organization.market.organization != &world.organization ||
      &organization.market.catalog != world.catalog->organization.get() ||
      &organization.stamp_authority != world.read_stamps.get())
    reject("NativeDirectorScopeAuthorityCandidateMismatch");
  if (take(compare_world_moments(
          at, take(organization.clock.read_organization_authority_now()))) ||
      take(compare_world_moments(at,
                                 organization.office_sources.stamp.span().to)))
    reject("NativeDirectorScopeAuthorityMomentMismatch");
  take(organization.stamp_authority.validate(
      organization.office_sources.stamp, world.common.state().identity,
      organization.office_sources.stamp.mode(), world.common,
      organization.versions));
  // 原任职提案继续借真实政治读口；不让旧 null runtime 绕回整数时刻路径。
  if (dependencies.politics) {
    const auto &politics = *dependencies.politics;
    const auto *runtime = politics.runtime;
    if (!world.catalog->politics || &politics.politics != &world.politics ||
        &politics.catalog != world.catalog->politics.get() ||
        &politics.economy != &world.local_economy ||
        &politics.social != &world.social || !runtime ||
        &runtime->runtime != &organization.runtime ||
        &runtime->clock != &organization.clock ||
        &runtime->read_stamp != &organization.office_sources.stamp ||
        &runtime->transitions != &organization.office_sources.transitions ||
        !authority.qualifications ||
        runtime->qualifications != authority.qualifications)
      reject("NativeDirectorScopePoliticalRuntimeMismatch");
  }
  auto selected =
      take(selected_native_task_work_version(world.catalog->identity));
  if (bool(selected) != bool(dependencies.work))
    reject("NativeDirectorScopeWorkSelectionMismatch",
           ErrorKind::unsupported_capability);
  if (dependencies.work) {
    take(dependencies.work->validate_bindings());
    if (take(compare_world_moments(
            at, dependencies.work->dependencies().body.actual_now)) ||
        take(compare_world_moments(
            at, take(dependencies.work->dependencies()
                         .tasks.clock.read_current_moment()))))
      reject("NativeDirectorScopeWorkMomentMismatch");
    if (dependencies.household &&
        dependencies.household.get() !=
            &dependencies.work->dependencies().economy.household())
      reject("NativeDirectorScopeWorkHouseholdOwnerMismatch");
  }
  if (dependencies.propagation_selected != bool(dependencies.propagation))
    reject("NativeDirectorScopePropagationSelectionMismatch",
           ErrorKind::unsupported_capability);
}
// 成员依序保活：真实前序 lease 先存、最后销毁；没有跨候选缓存或持久第二套状态。
class NativeDirectorWorldScope final : public NativeCompleteWorldSourceScope {
  const WorldBundle &world_;
  WorldBundle *writable_;
  NativeDirectorWorldScopeDependencies dependencies_;
  std::shared_ptr<DirectorHouseholdRoutineWrite2> household_routine_;
  std::shared_ptr<DirectorInstitutionalWrite2> institutional_writer_;
  std::shared_ptr<PropagationChannelWrite> propagation_writer_;
  WorldMoment at_;
  const CompleteExactWorldReadBindings &actual_;
  std::optional<CharacterConditionSources> body_;
  OrganizationAccess organizations_;
  GrowthAccess growth_;
  // 基础任职读取只借真 P1 票与导航；特殊业务回调仍须外层原 writer 提供。
  std::optional<PoliticalNavigationRead> political_navigation_;
  std::optional<PoliticalOrganizationRuntimeReadContext> political_runtime_;
  std::optional<DirectorPoliticalContext> politics_;
  std::optional<DirectorMembershipContext> membership_;
  std::optional<DirectorAuthorityWakeRead2> authorities_;
  // 对象体内嵌于已记账 scope，避免另一次未记账的裸 new。
  std::optional<NativeDirectorActorCommandBindings> commands_;
  std::unique_ptr<NativeExactResultOwnerRead> result_sources_;
  std::shared_ptr<const ExactResultTimeRead> results_;
  std::optional<ExactResultReadStamp> result_stamp_;
  // 计划知情只借同 Complete 家户根；先拥有报告源，后构造消费借口。
  std::unique_ptr<NativeHouseholdPurchaseReportWorldRead> household_reports_;
  std::optional<NativeHouseholdPurchaseKnownMarketRead> knowledge_;
  // 原告示读口在本拥有域内嵌，借真实 World 和同一次实际读钟。
  std::optional<PublicSupplyInspectionReadContext> public_supply_inspection_;
  std::optional<PropagationReadContext> propagation_;
  std::optional<NativeDirectorExactDependencies> read_;
  std::optional<DirectorCohortPurchaseWrite2> cohort_write_;
  std::optional<NativeDirectorExactWriteDependencies> write_;
  NativeCompleteWorldSourceExports exports_;

  // Work 真实来源已拥有自己的 Body/Clock；借同对象，不复制一个看似相等的替身。
  const CharacterConditionSources &conditions() const {
    return dependencies_.work ? dependencies_.work->dependencies().body
                              : *body_;
  }
  // 保持 Work 验证所要求的同一个读钟对象，否则借 core 的真实世界钟。
  const CurrentMomentRead &clock() const {
    return dependencies_.work ? dependencies_.work->dependencies().tasks.clock
                              : dependencies_.core.characters.early.clock;
  }
  // 外部若已有真实别名读口则沿用；否则只借本 scope 内嵌的同世界读口。
  const PublicSupplyInspectionReadContext *inspection() const {
    return dependencies_.public_supply_inspection
               ? dependencies_.public_supply_inspection.get()
               : (public_supply_inspection_ ? &*public_supply_inspection_
                                            : nullptr);
  }
  const DirectorPoliticalContext *politics() const {
    return dependencies_.politics ? dependencies_.politics.get()
                                  : (politics_ ? &*politics_ : nullptr);
  }
  // 已选专用策略必须真有原回调；基础 P1 读口不能冒充治理或军令写者。
  void validate_political_policy(const DirectorPolicy &policy) const {
    const auto *p = politics();
    for (const auto selected : policy.templates) {
      bool present = true;
      switch (selected) {
      case DirectorTemplate::military_appointment:
        present = p && bool(p->military_appointment);
        break;
      case DirectorTemplate::field_appointment:
        present = p && bool(p->field_appointment);
        break;
      case DirectorTemplate::political_supply_renewal:
        present = p && bool(p->supply_renewal);
        break;
      case DirectorTemplate::shop_registry_acceptance:
        present = p && bool(p->shop_registry_acceptance);
        break;
      case DirectorTemplate::governance_commission:
        present = p && bool(p->governance_commission);
        break;
      case DirectorTemplate::governance_agency:
        present = p && bool(p->governance_agency);
        break;
      case DirectorTemplate::office_resignation_response:
        present = p && bool(p->office_resignation_response);
        break;
      case DirectorTemplate::political_career:
        present = p && (!world_.catalog->director->subordinate_appointments ||
                        bool(p->subordinate_appointment));
        break;
      default:
        break;
      }
      if (!present)
        reject("NativeDirectorScopePoliticalProposalSourceRequired",
               ErrorKind::unsupported_capability);
    }
  }
  // 接待仅访问已有原 S3 子图；不因对话占用另开 Complete 租约或补知情。
  DirectorConversationResponse conversation() {
    if (dependencies_.conversation_response)
      return dependencies_.conversation_response;
    const auto &catalog = *world_.catalog->director;
    if (catalog.diplomatic_response || catalog.diplomatic_delegation ||
        catalog.treaty_passage)
      reject("NativeDirectorScopeOriginalDiplomaticResponseRequired",
             ErrorKind::unsupported_capability);
    if (!catalog.visit_reception)
      return {};
    const auto *interaction = dependencies_.core.characters.interaction;
    if (!interaction || !interaction->reception())
      reject("NativeDirectorScopeOriginalReceptionRequired",
             ErrorKind::unsupported_capability);
    const auto *original = &interaction->reception()->context();
    return [this, original](const StableId &actor,
                            const DirectorPolicy &policy) -> Outcome<bool> {
      if (std::find(policy.templates.begin(), policy.templates.end(),
                    DirectorTemplate::visit_reception) ==
          policy.templates.end())
        return {true, false, {}};
      const auto *intent = visit_reception_actor_intent(
          *world_.catalog->director->visit_reception, actor);
      return intent ? visit_reception_conversation_ready(
                          *original, world_.director, actor, intent->id)
                    : Outcome<bool>{true, false, {}};
    };
  }
  // 同一次实际来源取得拥有 P10 history 票；不在外层装配期捕获待封口结果。
  void construct() {
    const auto &core = dependencies_.core;
    if (&actual_.runtime != world_.catalog->complete_exact_time_runtime.get() ||
        &actual_.character_conditions != &core.characters.family.body() ||
        actual_.interaction_sources != core.characters.interaction)
      reject("NativeDirectorScopeCompleteCandidateMismatch");
    if (!dependencies_.work)
      body_.emplace(take(core.characters.family.body().sources()));
    if (&conditions().common != &world_.common ||
        &conditions().state != &*world_.actor_foundation ||
        &conditions().catalog != world_.catalog->actor_foundation.get() ||
        take(compare_world_moments(at_, conditions().actual_now)))
      reject("NativeDirectorScopeBodyCandidateMismatch");
    const auto authority = dependencies_.authority_owner->bindings();
    const auto &organization = authority.organization;
    if (world_.catalog->director->politics && !dependencies_.politics) {
      if (!world_.catalog->politics || !authority.qualifications)
        reject("NativeDirectorScopePoliticalSourcesRequired",
               ErrorKind::unsupported_capability);
      political_navigation_.emplace(PoliticalNavigationRead{
          world_.travel, *world_.catalog->travel, world_.services});
      political_runtime_.emplace(PoliticalOrganizationRuntimeReadContext{
          organization.runtime, organization.clock,
          organization.office_sources.stamp,
          organization.office_sources.transitions, authority.qualifications});
      politics_.emplace(DirectorPoliticalContext{
          world_.politics, *world_.catalog->politics, world_.local_economy,
          world_.social, world_.services, *political_navigation_});
      politics_->runtime = &*political_runtime_;
    }
    for (const auto &row : world_.catalog->director->policies.rows())
      validate_political_policy(row.second);
    for (const auto &row : world_.catalog->director->subjects.rows())
      validate_political_policy(row.second);
    authorities_.emplace(DirectorAuthorityWakeRead2{
        world_.common, world_.organization, *world_.catalog->organization,
        organization.office_sources.stamp,
        organization.office_sources.transitions,
        organization.office_sources.deaths, organization.clock, at_});
    if (world_.catalog->director->named_membership)
      membership_.emplace(DirectorMembershipContext{
          world_.social, world_.local_economy, *world_.catalog->local_economy,
          world_.interaction, world_.services, world_.services,
          &organization.clock});
    if (writable_)
      commands_.emplace(*writable_, &actual_);
    else
      commands_.emplace(world_, &actual_);
    result_sources_ =
        take(make_native_exact_result_sources_bound(world_, core));
    if (!result_sources_)
      reject("NativeDirectorScopeOriginalResultOwnerRequired",
             ErrorKind::unsupported_capability);
    auto retained = take(result_sources_->retained_sources_owned());
    results_ = take(capture_exact_result_time_read(
        world_.common, *world_.world_timeline, *result_sources_,
        retained.sources(), at_));
    if (!results_)
      reject("NativeDirectorScopeOriginalResultSnapshotRequired");
    result_stamp_.emplace(results_->stamp());
    if (dependencies_.household) {
      const auto face_version =
          take(selected_native_face_to_face_version(world_.catalog->identity));
      if (face_version == 3) {
        const auto *foundation = actual_.household_foundation;
        if (!foundation ||
            &foundation->service() != dependencies_.household.get() ||
            actual_.original_core != &core)
          reject("NativeDirectorHouseholdReportOwnerMismatch");
        household_reports_ =
            take(NativeHouseholdPurchaseReportWorldRead::create(
                world_, core, *foundation,
                take(make_native_household_report_dependencies(world_))));
        if (!household_reports_)
          reject("NativeDirectorHouseholdReportOwnerRequired");
        const auto *reports = take(household_reports_->context());
        if (!reports)
          reject("NativeDirectorHouseholdReportContextRequired");
        // 此消费必须借报告 owner 的同一 P10 快照，不混另一个原结果 stamp。
        knowledge_.emplace(
            world_.common, *world_.catalog->common, world_.social,
            reports->disclosure.results, reports->disclosure.result_stamp,
            reports, HouseholdMarketKnowledgeSelection::accepted_plan_scope);
      } else {
        knowledge_.emplace(world_.common, *world_.catalog->common,
                           world_.social, *results_, *result_stamp_);
      }
    }
    if (dependencies_.propagation) {
      const auto &original = *dependencies_.propagation;
      if (&original.common != &world_.common ||
          &original.social != &world_.social ||
          &original.director != &world_.director ||
          &original.director_catalog != world_.catalog->director.get())
        reject("NativeDirectorScopePropagationCandidateMismatch");
      propagation_.emplace(PropagationReadContext{
          original.common, original.social, original.director,
          original.director_catalog, original.policy, original.limits,
          original.catalog_identity, *results_, *result_stamp_,
          original.disposition, original.channels, original.clock,
          original.authority, original.versions});
    }
    if (bool(world_.catalog->public_supply_inspection) !=
            bool(world_.public_supply_inspection) ||
        (dependencies_.public_supply_inspection &&
         !world_.public_supply_inspection))
      reject("NativeDirectorPublicSupplySelectionMismatch",
             ErrorKind::unsupported_capability);
    if (world_.public_supply_inspection &&
        !dependencies_.public_supply_inspection) {
      if (!world_.catalog->interaction)
        reject("NativeDirectorPublicSupplyOwnerMismatch");
      public_supply_inspection_.emplace(PublicSupplyInspectionReadContext{
          world_.common, world_.social, *world_.catalog->interaction,
          *world_.catalog->local_economy, world_.local_economy,
          *world_.catalog->public_supply_inspection,
          *world_.public_supply_inspection, clock()});
    }
    if (const auto *original = inspection()) {
      const auto &source = *original;
      if (&source.common != &world_.common ||
          &source.social != &world_.social ||
          &source.interactions != world_.catalog->interaction.get() ||
          &source.economy_catalog != world_.catalog->local_economy.get() ||
          &source.economy != &world_.local_economy ||
          &source.catalog != world_.catalog->public_supply_inspection.get() ||
          &source.state != &*world_.public_supply_inspection ||
          take(compare_world_moments(take(source.clock.read_current_moment()),
                                     at_)))
        reject("NativeDirectorPublicSupplyOwnerMismatch");
      take(validate_public_supply_inspection_sources(source));
    }
    const auto *interaction = core.characters.interaction;
    DirectorActorProposalContext2 actors{
        world_.common,
        world_.director,
        *world_.catalog->director,
        organizations_,
        world_.affairs,
        world_.tasks,
        *world_.catalog->tasks,
        *world_.catalog->common,
        *world_.catalog->growth,
        *world_.catalog->travel,
        world_.organization,
        *world_.catalog->organization,
        &growth_,
        &world_.services,
        membership_ ? &*membership_ : nullptr,
        politics(),
        dependencies_.diplomacy.get(),
        dependencies_.issued_supply.get(),
        dependencies_.task_responsibility.get(),
        interaction && interaction->reception()
            ? &interaction->reception()->context()
            : nullptr,
        interaction && interaction->attendance()
            ? &interaction->attendance()->read()
            : nullptr,
        dependencies_.family_post.get(),
        inspection(),
        inspection() ? results_.get() : nullptr,
        inspection() ? &*result_stamp_ : nullptr};
    read_.emplace(NativeDirectorExactDependencies{
        world_, std::move(actors), world_.services, *results_, *result_stamp_,
        clock(), *commands_, *authorities_, conditions(),
        dependencies_.work.get(),
        dependencies_.household ? &dependencies_.household->household()
                                : nullptr,
        knowledge_ ? &*knowledge_ : nullptr, dependencies_.institutions.get(),
        conversation(), dependencies_.propagation_selected,
        propagation_ ? &*propagation_ : nullptr,
        dependencies_.household.get()});
    // 复用原唯一绑定门，不在装配器再写一套业务选择与反向覆盖。
    NativeDirectorExactSources binding(*read_);
    take(binding.validate_bindings());
    exports_.director = &*read_;
    if (!writable_)
      return;
    if (dependencies_.household) {
      if (!writable_->household_purchases || !writable_->facility_claims ||
          !writable_->action_elapsed ||
          !writable_->household_retail_settlements)
        reject("NativeDirectorScopeMutableHouseholdRootsRequired");
      cohort_write_.emplace(DirectorCohortPurchaseWrite2{
          writable_->local_economy, *writable_->household_purchases,
          *writable_->facility_claims, *writable_->action_elapsed,
          *writable_->household_retail_settlements,
          dependencies_.household->household(), *knowledge_});
    }
    if (dependencies_.propagation_selected != bool(propagation_writer_) ||
        bool(dependencies_.institutions) != bool(institutional_writer_))
      reject("NativeDirectorScopeActualWriterRequired",
             ErrorKind::unsupported_capability);
    if ((institutional_writer_ &&
         !institutional_writer_->bound_to(world_.common, world_.director)) ||
        (household_routine_ &&
         !household_routine_->bound_to(world_.common, world_.director)))
      reject("NativeDirectorScopeWriterCandidateMismatch");
    write_.emplace(NativeDirectorExactWriteDependencies{
        *read_,
        {writable_->common, writable_->director, clock(), *commands_,
         cohort_write_ ? &*cohort_write_ : nullptr, household_routine_.get(),
         institutional_writer_.get()},
        propagation_writer_.get()});
    exports_.director_write = &*write_;
  }

public:
  // 只保存本次已核实的借用；所有构造期业务读集中在 construct，地址不再移动。
  NativeDirectorWorldScope(
      const WorldBundle &world, WorldBundle *writable,
      NativeDirectorWorldScopeDependencies dependencies, const WorldMoment &at,
      const CompleteExactWorldReadBindings &actual,
      std::shared_ptr<DirectorHouseholdRoutineWrite2> household_routine = {},
      std::shared_ptr<DirectorInstitutionalWrite2> institutions = {},
      std::shared_ptr<PropagationChannelWrite> propagation = {})
      : world_(world), writable_(writable),
        dependencies_(std::move(dependencies)),
        household_routine_(std::move(household_routine)),
        institutional_writer_(std::move(institutions)),
        propagation_writer_(std::move(propagation)), at_(at), actual_(actual),
        organizations_(world.services.organization_access()),
        growth_(world.services.growth_access()) {}
  NativeDirectorWorldScope(const NativeDirectorWorldScope &) = delete;
  NativeDirectorWorldScope &
  operator=(const NativeDirectorWorldScope &) = delete;
  NativeDirectorWorldScope(NativeDirectorWorldScope &&) = delete;
  NativeDirectorWorldScope &operator=(NativeDirectorWorldScope &&) = delete;
  // 入口只在稳定地址上接线；构造失败即销毁所有局部适配，不留下发布对象。
  void initialize() {
    // 由工厂在稳定地址完成一次接线后立即交给原 retaining lease。
    construct();
  }
  // 只认同一个世界对象，不以值相等或相同目录冒充当前候选。
  bool bound_to(const WorldBundle &world) const noexcept override {
    return &world_ == &world;
  }
  // 此拥有域只导出原 Director 路由，不能被其他域借用为替身。
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::director;
  }
  const WorldMoment &actual_at() const noexcept override {
    // 时刻值属于本次取得，引用不会指向工厂栈参数。
    return at_;
  }
  // 只借已接线的具名端口，寿命不超过本拥有域。
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return exports_;
  }
};
} // namespace
// 只读取得只构造原 reader；原 authority lease 与本次 P10 一同保活到返回 scope
// 销毁。
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_director_world_read_scope(
    const WorldBundle &world, NativeDirectorWorldScopeDependencies dependencies,
    const WorldMoment &at) {
  return guarded<std::shared_ptr<const NativeCompleteWorldSourceScope>>([&] {
    roots(world, dependencies, at);
    const auto &actual = dependencies.core.characters.early.complete.reads();
    auto scope = checkpoint_make_shared<NativeDirectorWorldScope>(
        world, nullptr, std::move(dependencies), at, actual);
    scope->initialize();
    return std::shared_ptr<const NativeCompleteWorldSourceScope>(
        std::move(scope));
  });
}
// 写取得只附同候选真实 writer；采用、封口与失败撤回仍由原 Director/S0
// 路径执行。
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_director_world_write_scope(
    WorldBundle &world, NativeDirectorWorldScopeWriteDependencies dependencies,
    const WorldMoment &at) {
  return guarded<std::shared_ptr<NativeCompleteWorldSourceScope>>([&] {
    roots(world, dependencies.read, at);
    const auto &actual =
        dependencies.read.core.characters.early.complete.reads();
    auto scope = checkpoint_make_shared<NativeDirectorWorldScope>(
        world, &world, std::move(dependencies.read), at, actual,
        std::move(dependencies.household_routine),
        std::move(dependencies.institutions),
        std::move(dependencies.propagation));
    scope->initialize();
    return std::shared_ptr<NativeCompleteWorldSourceScope>(std::move(scope));
  });
}
} // namespace hanmo::resident
