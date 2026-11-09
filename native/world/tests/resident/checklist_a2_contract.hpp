// A2 有限失败合同：只验证真实公开出口与登记，不宣称任何缺失业务已实现。
#pragma once
#include "resident/checkpoint/checkpoint_not_implemented.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
#include "resident/indexes/contact_model.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/exact_time_profile.hpp"
#include "resident/kernel/native_complete_world_registry.hpp"
#include "resident/kernel/native_complete_world_sources.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_codec.hpp"
#include <array>
#include <set>
#include <stdexcept>
#include <string_view>

namespace hanmo::resident::checklist_a2 {
// 逐项要求真实断言成立，失败直接终止，不降为成功。
inline void check(bool condition, const StableId &label) {
  if (!condition)
    throw std::runtime_error("A2: " + label);
}

// 分别核对错误码、种类、路径及原因；另一种失败不能冒充替身合同。
template <class Result>
void reject(const Result &result, const char *label, const char *path = "",
            const char *reason = "") {
  check(!result.ok, StableId(label) + " returned success");
  const auto &error = result.error;
  check(error.original.code == "NOT_IMPLEMENTED",
        StableId(label) + " wrong code: " + error.original.code);
  check(error.kind == ErrorKind::unsupported_capability,
        StableId(label) + " wrong kind");
  check(error.original.path == path,
        StableId(label) + " wrong path: " + error.original.path);
  check(error.reason.value_or("") == reason, StableId(label) + " wrong reason");
}

// 直接对照工厂指针与状态表；已链接路由不代表缺失来源已构建。
inline void registry_contract() {
  const auto &factories = native_complete_world_source_graph_factories();
  check(!factories.family_prerequisites && !factories.action_prerequisites &&
            !factories.domain_read && !factories.domain_write,
        "four missing prerequisite constructors must remain visibly absent");
  const NativeCompleteDomainScopeFactories *absent[] = {
      &factories.economic,       &factories.craft,     &factories.repair,
      &factories.craft_facility, &factories.household, &factories.motion,
      &factories.political,      &factories.task_work, &factories.director};
  for (const auto *slot : absent)
    check(!slot->read && !slot->write,
          "missing original domain source falsely linked");
  check(factories.state_need.read && factories.state_need.write &&
            factories.military.read && factories.military.write,
        "real state_need and military constructors must not be relabeled as "
        "stubs");

  const auto source_status = read_native_complete_source_construction_status();
  const std::array<const char *, 14> names{
      {"family_prerequisites", "professional_action_sources",
       "facility_domain_sources", "economic", "craft", "repair",
       "craft_facility", "household", "motion", "state_need", "political",
       "task_work", "military", "director"}};
  check(source_status.size() == names.size(),
        "source constructor registry extent");
  std::set<StableId> seen;
  for (std::size_t i = 0; i < names.size(); ++i) {
    const auto &status = source_status[i];
    check(status.name && StableId(status.name) == names[i],
          "source status name/order changed");
    check(seen.insert(status.name).second, "duplicate source status");
    const bool implemented = StableId(status.name) == "state_need" ||
                             StableId(status.name) == "military";
    check(status.read_constructor == implemented &&
              status.write_constructor == implemented,
          StableId(status.name) +
              " constructor flag disagrees with actual registry");
  }

  const auto &routes = native_complete_world_factory_registry();
  check(routes.sources_read && routes.sources_write,
        "source graph routing must be linked");
  check(!routes.family_complete && !routes.estate_phase &&
            !routes.commerce_phase && !routes.medicine_phase &&
            !routes.study_phase && !routes.arcana_phase,
        "six absent phase factories falsely linked");
  auto route_status = read_native_complete_world_route_status();
  check(route_status.size() == 19, "route registry extent");
  seen.clear();
  for (const auto &status : route_status) {
    check(status.name && seen.insert(status.name).second,
          "duplicate/unnamed route");
    const bool missing =
        status.route == NativeCompleteWorldRoute::family_birth_due ||
        status.route == NativeCompleteWorldRoute::family_guardianship ||
        status.route == NativeCompleteWorldRoute::estate_phase ||
        status.route == NativeCompleteWorldRoute::commerce_phase ||
        status.route == NativeCompleteWorldRoute::medicine_phase ||
        status.route == NativeCompleteWorldRoute::study_phase ||
        status.route == NativeCompleteWorldRoute::arcana_phase;
    check(status.read_route == !missing && status.write_route == !missing,
          StableId(status.name) +
              " route flags disagree with linked/absent factory");
  }
  // 正向对照只确认路由已链接，不把它解释为来源已可用。
  check(validate_native_complete_world_factory_routes(routes).ok,
        "linked-route validator was incorrectly expanded into a "
        "source-readiness claim");
}

// 只在未安装的拒绝输入上触达六项实际派发失败，不构造成功 provider。
inline void phase_failures(const PreparedCatalog &original) {
  reject(make_native_complete_family_phase_registration(
             LifeAgeExactWorldFactory{}),
         "family phase", "",
         "CompleteFamilyBirthDueAndGuardianshipConstructionRequired");
  // 具名选择仅指向既有缺失工厂和其原始原因，不提供业务实现。
  struct Phase {
    ExactProviderId id;
    const char *reason;
  };
  const Phase cases[] = {{ExactProviderId::estate,
                          "CompleteEstateOriginalPhaseConstructionRequired"},
                         {ExactProviderId::commerce,
                          "CompleteCommerceOriginalPhaseConstructionRequired"},
                         {ExactProviderId::medicine,
                          "CompleteMedicineOriginalPhaseConstructionRequired"},
                         {ExactProviderId::study,
                          "CompleteStudyOriginalPhaseConstructionRequired"},
                         {ExactProviderId::arcana,
                          "CompleteArcanaOriginalPhaseConstructionRequired"}};
  for (const auto &test : cases) {
    // 从同一目录导出的拒绝输入绝不安装、准入或推进，不替换共用配置。
    auto selected = std::make_shared<ExactTimeProfile>();
    selected->profile_id = exact_complete_profile_id;
    selected->complete.emplace();
    selected->complete->phase_providers.push_back({test.id, "", 0});
    PreparedCatalog catalog = original;
    catalog.exact_time = std::move(selected);
    std::vector<ExactPhaseRegistration> output;
    reject(append_native_complete_selected_phase_routes(catalog, output),
           test.reason, "", test.reason);
    check(output.empty(), "rejected phase must not install a handler");
  }
  PreparedCatalog unselected = original;
  unselected.exact_time.reset();
  std::vector<ExactPhaseRegistration> output;
  auto bad = append_native_complete_selected_phase_routes(unselected, output);
  check(!bad.ok &&
            bad.error.original.code ==
                "CompleteExactPhaseConstructionSelectionRequired" &&
            output.empty(),
        "unselected input must preserve its original validation error");
  auto selected = std::make_shared<ExactTimeProfile>();
  selected->profile_id = exact_complete_profile_id;
  selected->complete.emplace();
  selected->complete->phase_providers.push_back(
      {ExactProviderId::motion, "", 0});
  unselected.exact_time = std::move(selected);
  bad = append_native_complete_selected_phase_routes(unselected, output);
  check(!bad.ok &&
            bad.error.original.code ==
                "CompleteOriginalSelectedPhaseConstructionRequired" &&
            output.empty(),
        "unhandled phase must not be relabeled NOT_IMPLEMENTED");
}

// 执行三项共享的真实固定异常机制，确认不会正常返回或伪造诊断拥有者。
inline void fixed_failure_contract() {
  bool caught = false;
  try {
    throw_checkpoint_not_implemented();
  } catch (checkpoint_job_detail::FailureException &failure) {
    caught = true;
    check(failure.error.code() == checkpoint_not_implemented_code,
          "wrong fixed exception code");
    check(std::string_view(checkpoint_job_failure_code_name(
              failure.error.code())) == "NOT_IMPLEMENTED",
          "fixed NOT_IMPLEMENTED public mapping lost");
    check(failure.error.rule_error() == nullptr,
          "fixed failure invented a RuleError");
    check(!failure.error.try_materialize_rule_error(),
          "stateless failure invented a diagnostic owner");
  }
  check(caught, "fixed missing-implementation helper returned normally");
}

// 沿实际公开消费者检查三项独立失败及真实缓存/索引保留；完整快照须保持不变。
inline void independent_public_failures(WorldBundle &snapshot) {
  check(bool(snapshot.catalog) && bool(snapshot.catalog->organization),
        "shared fixture organization required");
  const auto before = export_world_state(snapshot);
  check(before.ok, "export shared world before A2");

  // 公开七参入口带真实 CommonStore 已提交时钟进入原八参分支。
  // 仅未安装的拒绝输入关闭 exact-fact 标志，不制造 grant 或可用授权。
  auto organization_catalog = *snapshot.catalog->organization;
  organization_catalog.authority_lifecycle_enabled = true;
  organization_catalog.exact_fact_clock_enabled = false;
  reject(check_authority(snapshot.common, snapshot.organization,
                         organization_catalog, ActingAuthority{}, "",
                         ScopeRef{}, 0),
         "grant proof projection", "", "AuthorityProofGrantBindingRequired");

  check(snapshot.prepare_checkpoint_read_cache().ok,
        "implemented shared-world checkpoint cache preparation failed");
  reject(terrain_local_interval_intersection(TravelMotion{}, TravelMotion{},
                                             false),
         "terrain interval", "travel/local_interval_intersection");

  auto details = std::make_shared<ReceiptDetails>();
  details->data.emplace<LocalEconomyEffects>();
  auto receipt = std::make_shared<CommitResult>();
  receipt->kind = "configure_local_production";
  receipt->details = std::move(details);
  Outcome<std::shared_ptr<const CommitResult>> input{
      true, std::move(receipt), {}};
  reject(encode_resident_receipt(input), "economic receipt projection",
         "economic/configuration/effects");

  // 失败路径仍不得改变真实世界；派生索引的内存计量入口已退役。
  const auto after = export_world_state(snapshot);
  check(after.ok && compare_boundary(before.value, after.value).exact(),
        "A2 rejection calls changed shared-world state");
}

// 只组合共用配置可合法执行的子集；51 项另经有限源码合同核对。
// 不能把这里的 9 项失败实测描述成 51 个出口逐一执行。
inline void run(WorldBundle &snapshot) {
  registry_contract();
  phase_failures(*snapshot.catalog);
  fixed_failure_contract();
  independent_public_failures(snapshot);
}
} // namespace hanmo::resident::checklist_a2
