// 原前序先持有，消费者后构造；析构反序，不回调 Complete 工厂或制造第二知识账。
#include "native_household_purchase_report_world.hpp"
#include "native_character_family_world.hpp"
#include "native_communication_selection.hpp"
#include "native_domain_read_versions.hpp"
#include "native_exact_result_owners.hpp"
#include "native_interaction_world.hpp"
#include "native_transport_funding.hpp"
#include "original_catalog_identity.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/family/household_purchase_disclosure_history.hpp"
#include "resident/domains/interaction/face_to_face_report.hpp"
#include "resident/domains/interaction/visit_messages/visit_report_custody.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/tasks/task_report_carriage.hpp"
#include "resident/domains/tasks/task_report_carrier_terms.hpp"
#include "world_bundle.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("HouseholdReportOwnerResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("HouseholdReportOwnerResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Integer selected(const CatalogIdentity &id, const char *name) {
  const StableId *v = nullptr;
  for (const auto &r : id.module_versions)
    if (r.first == name) {
      if (v)
        fail("HouseholdReportDuplicateModuleSelection");
      v = &r.second;
    }
  if (!v)
    return 0;
  if (*v == "1")
    return 1;
  if (*v == "2")
    return 2;
  if (*v == "3")
    return 3;
  fail("HouseholdReportUnsupportedModuleSelection");
}
// 仅原三类实际渠道分派。不存在的分支拒绝；从不返回默认到达或空许可。
class Channels final : public CommunicationSourceRead {
  const CommonStore &common_;
  const SocialState &social_;
  const FaceToFaceReportCommunicationRead &face_;
  const VisitReportCustodyCommunicationRead *custody_;
  const TaskReportCarriageCommunicationRead *carrier_;
  Outcome<const CommunicationSourceRead *> owner(const SourceRef &r) const {
    if (!face_.bound_to(common_, social_) ||
        (custody_ && !custody_->bound_to(common_, social_)) ||
        (carrier_ && !carrier_->bound_to(common_, social_)))
      return {false, nullptr,
              common_failure("HouseholdReportChannelWorldMismatch").error};
    if (r.domain == "interaction" && r.kind == face_to_face_report_source_kind)
      return {true, &face_, {}};
    if (r.domain == "visit_messages" &&
        (r.kind == visit_report_deposited_kind ||
         r.kind == visit_report_picked_up_kind ||
         r.kind == visit_report_withdrawn_kind)) {
      if (custody_)
        return {true, custody_, {}};
      return {
          false, nullptr,
          common_failure("HouseholdReportOriginalCustodyOwnerRequired", r.id)
              .error};
    }
    if (r.domain == "tasks" && (r.kind == "task_report_carriage_dispatched" ||
                                r.kind == "task_report_handed_over")) {
      if (carrier_)
        return {true, carrier_, {}};
      return {
          false, nullptr,
          common_failure("HouseholdReportOriginalCarrierOwnerRequired", r.id)
              .error};
    }
    return {
        false, nullptr,
        common_failure("HouseholdReportOriginalChannelRequired", r.id).error};
  }

public:
  Channels(const CommonStore &c, const SocialState &s,
           const FaceToFaceReportCommunicationRead &f,
           const VisitReportCustodyCommunicationRead *v,
           const TaskReportCarriageCommunicationRead *t)
      : common_(c), social_(s), face_(f), custody_(v), carrier_(t) {}
  Outcome<CommunicationOrigin> origin(const SourceRef &s,
                                      Integer v) const override {
    auto p = owner(s);
    return p.ok ? p.value->origin(s, v)
                : Outcome<CommunicationOrigin>{false, {}, p.error};
  }
  Outcome<CommunicationCancellation> cancellation(const SourceRef &s,
                                                  Integer v) const override {
    auto p = owner(s);
    return p.ok ? p.value->cancellation(s, v)
                : Outcome<CommunicationCancellation>{false, {}, p.error};
  }
  Outcome<CommunicationArrival> arrival(const SourceRef &s,
                                        Integer v) const override {
    auto p = owner(s);
    return p.ok ? p.value->arrival(s, v)
                : Outcome<CommunicationArrival>{false, {}, p.error};
  }
  Outcome<CommunicationResolvedRecipient>
  resolve_arrived_address(const CommunicationAddress &a,
                          const CommunicationArrival &r) const override {
    auto p = owner(r.source);
    return p.ok ? p.value->resolve_arrived_address(a, r)
                : Outcome<CommunicationResolvedRecipient>{false, {}, p.error};
  }
  Status validate_historical_recipient(
      const CommunicationAddress &a, const CommunicationArrival &r,
      const CommunicationResolvedRecipient &v) const override {
    auto p = owner(r.source);
    return p.ok ? p.value->validate_historical_recipient(a, r, v)
                : Status{false, {}, p.error};
  }
  Outcome<CommunicationDossierAccess> dossier_access(const SourceRef &s,
                                                     Integer v) const override {
    auto p = owner(s);
    return p.ok ? p.value->dossier_access(s, v)
                : Outcome<CommunicationDossierAccess>{false, {}, p.error};
  }
  Status may_disclose(const CommunicationOrigin &o,
                      const Report &r) const override {
    auto p = owner(o.source);
    return p.ok ? p.value->may_disclose(o, r) : Status{false, {}, p.error};
  }
  Status
  validate_delivery_before_common_source(const CommunicationDeliverySource &d,
                                         const SourceRef &s) const override {
    auto p = owner(d.arrival_ref);
    return p.ok ? p.value->validate_delivery_before_common_source(d, s)
                : Status{false, {}, p.error};
  }
};
} // namespace
Status visit_native_household_purchase_report_source_pins(
    const WorldBundle &world, NativeHouseholdReportPinSink &sink) {
  auto r = guard<Done>([&] {
    if (!world.catalog ||
        selected(world.catalog->identity, "face_to_face_contacts") != 3 ||
        !world.interaction.face_to_face_contacts ||
        world.interaction.face_to_face_contacts->version != 3)
      fail("HouseholdReportPhysical3Required");
    class Prefix final : public CommonActualFactPrefixSink {
      const StableId &id_;
      NativeHouseholdReportPinSink &sink_;

    public:
      Prefix(const StableId &id, NativeHouseholdReportPinSink &sink)
          : id_(id), sink_(sink) {}
      Status actual_prefix_fact(std::size_t ordinal,
                                const FactRecord &fact) override {
        if (ordinal >
            static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
          return common_failure("HouseholdReportPinOrdinalOverflow", id_);
        return sink_.original_source(
            id_, NativeHouseholdReportPinRole::actual_contact_prefix,
            static_cast<Integer>(ordinal),
            SourceRefBuildView{fact.domain, fact.kind, fact.id});
      }
    };
    for (const auto &row : world.interaction.face_to_face_contacts
                               ->purchase_plan_disclosures.rows()) {


      auto memory = CheckpointMemoryCharge(0);
      const SourceRef source{"interaction",
                             household_purchase_report_source_kind, row.first};
      const auto *original =
          take(read_original_household_purchase_report_source(
              world.common, world.interaction, source, 1));
      take(sink.original_source(
          row.first, NativeHouseholdReportPinRole::actual_delivery, 0,
          SourceRefBuildView{source.domain, source.kind, source.id}));
      Prefix prefix(row.first, sink);
      take(visit_actual_common_fact_prefix(
          world.common, original->actual_contact_prefix, prefix));
    }
    return Done{};
  });
  return r.ok ? common_success() : Status{false, {}, std::move(r.error)};
}
struct NativeHouseholdPurchaseReportWorldRead::Sources final
    : CheckpointAllocatedObject {
  const WorldBundle &world;
  const NativeCompleteWorldCoreSources &core;
  const NativeHouseholdFoundationLease &household;
  NativeHouseholdPurchaseReportWorldDependencies dependencies;
  NativeDomainReadVersionSource versions;
  std::unique_ptr<CurrentReadStampRead> clock;
  WorldMoment actual;
  InteractionAccess access;
  std::unique_ptr<NativeExactResultOwnerRead> result_sources;
  std::shared_ptr<const ExactResultTimeRead> results;
  std::optional<ExactResultReadStamp> stamp;
  std::optional<ReportDisclosureReadContext> disclosure;
  std::optional<FaceToFaceReportContext> face_context;
  std::optional<FaceToFaceReportCommunicationRead> face;
  std::optional<VisitReportCustodyContext> custody_context;
  std::optional<VisitReportCustodyCommunicationRead> custody;
  std::unique_ptr<NativeWorldPrivateTransportFundingRead> funding;
  std::optional<NativeMovementActionTimeRead> movement;
  std::optional<TaskReportCarrierScopeContext> carrier_scope;
  std::optional<NativeTaskReportCarrierTermsRead> carrier_terms;
  std::optional<TaskReportCarriageContext> carrier_context;
  std::optional<TaskReportCarriageCommunicationRead> carrier;
  std::optional<Channels> channels;
  std::optional<ActorReportKnowledgeContext> knowledge;
  std::optional<CharacterConditionSources> controller_condition;
  std::optional<HouseholdPurchaseDisclosureHistorySources> controller_history;
  std::optional<HouseholdPurchaseReportContext> read;
  Sources(const WorldBundle &w, const NativeCompleteWorldCoreSources &c,
          const NativeHouseholdFoundationLease &h,
          NativeHouseholdPurchaseReportWorldDependencies d)
      : world(w), core(c), household(h), dependencies(std::move(d)),
        versions(w, c.characters.early.formations, c.characters.early.returns,
                 c.characters.early.periods),
        access(w.services.interaction_access()) {}
  void bindings() const {
    const auto &w = world;
    if (&core.characters.early.world != &w || !w.catalog ||
        !w.catalog->common || !w.catalog->interaction || !w.catalog->family ||
        !w.catalog->organization || !w.catalog->exact_time ||
        !exact_complete_selected(*w.catalog->exact_time) ||
        !w.catalog->complete_exact_time_runtime || !w.world_timeline ||
        !w.read_stamps || !w.social_period_sources || !w.social.dynamic ||
        !w.social.dynamic->time2 || !w.household_purchases ||
        !w.household_retail_settlements || !w.action_elapsed ||
        !w.actor_foundation || !w.catalog->actor_foundation)
      fail("HouseholdReportCompleteRootsRequired");
    // /2 尚无已交付完整原根，旧三表绝不能冒充它；由原选型函数严格判定。
    if (take(selected_native_communication_links_version(
            w.catalog->identity)) == 2)
      fail("HouseholdReportCommunicationPhysical2NotImplemented");
    if (selected(w.catalog->identity, "face_to_face_contacts") != 3 ||
        !w.interaction.face_to_face_contacts ||
        w.interaction.face_to_face_contacts->version != 3)
      fail("HouseholdReportPhysical3Required");
    if (w.catalog->selected_household_version != 2 &&
        w.catalog->selected_household_version != 3)
      fail("HouseholdReportOriginalHouseholdSelectionRequired");
    if (!household.bound_to(w.common, w.local_economy, *w.household_purchases,
                            w.private_shops, *w.household_retail_settlements,
                            *w.action_elapsed))
      fail("HouseholdReportFoundationWorldMismatch");
    take(household.validate_bindings());
    const auto &a = household.operation_spatial();
    const auto &p = household.cohort();
    if (&a.common != &w.common ||
        &a.common_catalog != w.catalog->common.get() ||
        &a.foundation != &*w.actor_foundation ||
        &a.actors != w.catalog->actor_foundation.get() ||
        &p.common != &w.common || &p.economy != &w.local_economy ||
        !w.population_transport || &p.population != &*w.population_transport ||
        &p.state != &*w.household_purchases ||
        &p.catalog != w.catalog->household_purchases.get() ||
        &a.spatial != &p.spatial)
      fail("HouseholdReportOriginalSpatialWorldMismatch");
    if (!core.characters.interaction)
      fail("HouseholdReportOriginalInteractionOwnerRequired");
    take(core.characters.interaction->validate_bindings());
  }
  void construct() {
    bindings();
    const auto &w = world;
    if (!dependencies.stamp_limits.max_versions ||
        !dependencies.stamp_limits.max_sources ||
        !dependencies.stamp_limits.max_text_bytes)
      fail("HouseholdReportReadStampLimitsRequired");
    clock =
        w.make_current_read_stamp_reader(versions, dependencies.stamp_limits);
    if (!clock)
      fail("HouseholdReportCurrentReadOwnerRequired");
    actual = take(clock->read_current_moment());
    if (take(
            compare_world_moments(actual, household.service().household().now)))
      fail("HouseholdReportFoundationMomentMismatch");
    result_sources = take(make_native_exact_result_sources_bound(w, core));
    if (!result_sources)
      fail("HouseholdReportExactResultOwnerRequired");
    auto retained = take(result_sources->retained_sources());
    results = take(capture_exact_result_time_read(
        w.common, *w.world_timeline, *result_sources, retained, actual));
    if (!results)
      fail("HouseholdReportExactResultSnapshotRequired");
    stamp.emplace(results->stamp());
    disclosure.emplace(ReportDisclosureReadContext{
        w.common, w.social, *results, *stamp, *clock, *w.read_stamps, versions,
        dependencies.disclosure_originals.get(),
        dependencies.official_contents.get()});
    face_context.emplace(FaceToFaceReportContext{
        w.common, w.interaction, *w.catalog->interaction, access,
        w.catalog->identity, *disclosure});
    face.emplace(*face_context);
    // 没有原三表时只让原知情读证明 Inspect/direct 等既有分支；
    // 消息分支自己严格要求真实三表与渠道，不制造空根或虚拟 /1。
    if (w.interaction.communication_links) {
      const auto interaction_selected =
          take(selected_native_interaction_versions(w.catalog->identity));
      if (interaction_selected.messages == 3) {
        const auto *message_owner = core.characters.interaction->messages();
        if (!message_owner || !w.interaction.visit_messages ||
            !w.catalog->interaction->visit_messages)
          fail("HouseholdReportVisitCustodyOwnerRequired");
        const auto *life = take(message_owner->context());
        if (!life || &life->common != &w.common ||
            &life->state != &*w.interaction.visit_messages)
          fail("HouseholdReportVisitCustodyWorldMismatch");
        custody_context.emplace(VisitReportCustodyContext{
            w.common, *w.interaction.visit_messages,
            *w.catalog->interaction->visit_messages, w.interaction,
            *w.catalog->interaction, access, w.catalog->identity, *disclosure,
            *life});
        custody.emplace(*custody_context);
      }
      const bool has_carrier = w.tasks.transport_agreements &&
                               w.tasks.transport_agreements->version == 2;
      if (bool(dependencies.carrier_offers) != has_carrier)
        fail("HouseholdReportCarrierOriginalSelectionMismatch");
      if (has_carrier) {
        if (!same_original_catalog_identity(
                dependencies.carrier_offers->identity, w.catalog->identity))
          fail("HouseholdReportCarrierCatalogIdentityMismatch");
        if (!w.catalog->travel || !w.movement_exact ||
            !w.private_transport_funding)
          fail("HouseholdReportCarrierOriginalRootsRequired");
        take(validate_task_report_carrier_offer_catalog(
            *dependencies.carrier_offers, *w.catalog->common,
            *w.catalog->interaction));
        funding = take(NativeWorldPrivateTransportFundingRead::create(w));
        if (!funding)
          fail("HouseholdReportCarrierOriginalFundingRequired");
        movement.emplace(w.common, w.travel, *w.movement_exact,
                         *w.catalog->travel, w.services);
        carrier_scope.emplace(TaskReportCarrierScopeContext{
            *w.catalog->common, w.tasks, *dependencies.carrier_offers,
            *face_context});
        carrier_terms.emplace(*carrier_scope, funding->funding());
        carrier_context.emplace(TaskReportCarriageContext{
            w.common, *w.catalog->common, w.tasks, *carrier_terms,
            *face_context, *movement, *w.movement_exact, *w.catalog->travel});
        carrier.emplace(*carrier_context);
      }
      channels.emplace(w.common, w.social, *face, custody ? &*custody : nullptr,
                       carrier ? &*carrier : nullptr);
    }
    if (w.catalog->actor_foundation->actors.empty())
      fail("HouseholdReportOriginalScenarioRequired");
    const auto initial = w.catalog->actor_foundation->actors.rows()
                             .front()
                             .second.scenario_origin_ref.initial_tick;
    for (const auto &r : w.catalog->actor_foundation->actors.rows())
      if (r.second.scenario_origin_ref.initial_tick != initial)
        fail("HouseholdReportScenarioBoundaryMismatch");
    knowledge.emplace(ActorReportKnowledgeContext{
        w.common, w.social, *results, *stamp,
        w.interaction.communication_links ? &*w.interaction.communication_links
                                          : nullptr,
        channels ? &*channels : nullptr, &*w.social_period_sources,
        &*w.world_timeline, &w.catalog->identity, initial});
    controller_condition.emplace(take(core.characters.family.body().sources()));
    if (&controller_condition->common != &w.common ||
        &controller_condition->state != &*w.actor_foundation ||
        &controller_condition->catalog != w.catalog->actor_foundation.get() ||
        take(compare_world_moments(controller_condition->actual_now, actual)))
      fail("HouseholdReportControllerConditionWorldMismatch");
    controller_history.emplace(HouseholdPurchaseDisclosureHistorySources{
        *controller_condition, w.family, *w.catalog->family, *w.catalog->common,
        w.organization, *w.catalog->organization,
        household.service().household().controls, w.interaction, *clock,
        w.political_office_receptions ? &*w.political_office_receptions
                                      : nullptr});
    read.emplace(HouseholdPurchaseReportContext{
        w.common, w.interaction, household.service().household(),
        household.cohort(), household.operation_spatial(), *disclosure,
        *knowledge,
        w.military_exact ? &w.military_exact->spatial_handoffs : nullptr,
        dependencies.custody_releases.get(), &*controller_history});
  }
  void current() const {
    bindings();
    if (!clock || !read || !results || !knowledge ||
        (bool(world.interaction.communication_links) != bool(channels)) ||
        take(compare_world_moments(actual, take(clock->read_current_moment()))))
      fail("HouseholdReportReadSliceExpired");
  }
};
NativeHouseholdPurchaseReportWorldRead::NativeHouseholdPurchaseReportWorldRead(
    std::unique_ptr<Sources> s)
    : sources_(std::move(s)) {}
NativeHouseholdPurchaseReportWorldRead::
    ~NativeHouseholdPurchaseReportWorldRead() = default;
Outcome<std::unique_ptr<NativeHouseholdPurchaseReportWorldRead>>
NativeHouseholdPurchaseReportWorldRead::create(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &c,
    const NativeHouseholdFoundationLease &h,
    NativeHouseholdPurchaseReportWorldDependencies d) {
  return guard<std::unique_ptr<NativeHouseholdPurchaseReportWorldRead>>([&] {
    auto s = std::unique_ptr<Sources>(new Sources(w, c, h, std::move(d)));
    s->construct();
    return std::unique_ptr<NativeHouseholdPurchaseReportWorldRead>(
        new NativeHouseholdPurchaseReportWorldRead(std::move(s)));
  });
}
bool NativeHouseholdPurchaseReportWorldRead::bound_to(
    const WorldBundle &w,
    const NativeHouseholdFoundationLease &h) const noexcept {
  return sources_ && &sources_->world == &w && &sources_->household == &h;
}
Status NativeHouseholdPurchaseReportWorldRead::validate_bindings() const {
  auto r = guard<Done>([&] {
    if (!sources_)
      fail("HouseholdReportOwnerMissing");
    sources_->current();
    return Done{};
  });
  return r.ok ? common_success() : Status{false, {}, std::move(r.error)};
}
Status NativeHouseholdPurchaseReportWorldRead::validate_sources() const {
  auto r = guard<Done>([&] {
    if (!sources_)
      fail("HouseholdReportOwnerMissing");
    sources_->current();
    const auto &w = sources_->world;
    take(validate_face_to_face_contact_sources(
        w.common, w.interaction, *w.catalog->interaction, sources_->actual));
    // /3 保留旧两表所有语义，不只验新采购告知表。有真实消息三表时即使
    // 当前报告表为空也反扫原links；已有报告但缺三表必须由原完整门拒绝。
    if (w.interaction.communication_links ||
        !w.interaction.face_to_face_contacts->report_disclosures.empty())
      take(validate_face_to_face_report_sources(*sources_->face_context));
    take(validate_household_purchase_report_sources(*sources_->read));
    return Done{};
  });
  return r.ok ? common_success() : Status{false, {}, std::move(r.error)};
}
Status NativeHouseholdPurchaseReportWorldRead::visit_controller_history_pins(
    const SourceRef &delivery, CommonActualFactPrefixSink &prefix,
    CharacterAvailabilityProvenanceSink &presence) const {
  auto r = guard<Done>([&] {
    if (!sources_)
      fail("HouseholdReportOwnerMissing");
    sources_->current();
    if (!sources_->controller_history)
      fail("HouseholdReportControllerHistoryOwnerMissing");
    const auto actual =
        take(read_household_purchase_disclosure_controller_before_fact(
            *sources_->controller_history, delivery, 1, &prefix, &presence));
    if (!actual.authored_role || !actual.role_enabled || !actual.present ||
        !actual.alive || !actual.available)
      fail("HouseholdReportHistoricalControllerUnavailable");
    return Done{};
  });
  return r.ok ? common_success() : Status{false, {}, std::move(r.error)};
}
Outcome<const HouseholdPurchaseReportContext *>
NativeHouseholdPurchaseReportWorldRead::context() const {
  return guard<const HouseholdPurchaseReportContext *>([&] {
    if (!sources_)
      fail("HouseholdReportOwnerMissing");
    sources_->current();
    return &*sources_->read;
  });
}
} // namespace hanmo::resident
