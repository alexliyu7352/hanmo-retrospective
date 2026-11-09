#include "native_spatial_carrier_sources.hpp"
#include "../actor_foundation/created_origin.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
bool subject_same(const SubjectRef &a, const SubjectRef &z) {
  return a.kind == z.kind && a.id == z.id;
}
void fact(const CommonStore &common, const SourceRef &ref) {
  const auto *head = common.state().facts.find(ref.id);
  if (!head || head->domain != ref.domain || head->kind != ref.kind)
    fail("TransportActualFactRequired", ref.id);
}
Integer plus(Integer a, Integer z) {
  if (a < 0 || z < 0 || a > std::numeric_limits<Integer>::max() - z)
    fail("TransportQuantityOverflow");
  return a + z;
}
} // namespace
// 原引用构造保全部调用兼容，只有真实地址传到可选根拥有接缝。
NativeCivilianSpatialCarrierSources::NativeCivilianSpatialCarrierSources(
    const CommonStore &c, const CommonCatalog &cc, const TravelCatalog &tc,
    const MovementExactState &m, const SpatialCarrierState &s,
    const TransportRegistrationState &r, const LocalEconomyState &e,
    const PopulationTransportState &p, const AuthoredActorFoundationState &f,
    const ActorFoundationCatalog &ac,
    const TransportRegistrationBusinessRead &b,
    const CurrentReadStampRead &stamps,
    const TransportVehicleTakeover *vehicles,
    const FamilyCatalog *family_catalog)
    : NativeCivilianSpatialCarrierSources(c, cc, tc, m, s, r, e, &p, f, ac, &b,
                                          stamps, vehicles, family_catalog) {}
// 可选根仅表达正式未选 group；选中时由实际 owning factory 拒缺根。
NativeCivilianSpatialCarrierSources::NativeCivilianSpatialCarrierSources(
    const CommonStore &c, const CommonCatalog &cc, const TravelCatalog &tc,
    const MovementExactState &m, const SpatialCarrierState &s,
    const TransportRegistrationState &r, const LocalEconomyState &e,
    const PopulationTransportState *p, const AuthoredActorFoundationState &f,
    const ActorFoundationCatalog &ac,
    const TransportRegistrationBusinessRead *b,
    const CurrentReadStampRead &stamps,
    const TransportVehicleTakeover *vehicles,
    const FamilyCatalog *family_catalog)
    : common_(c), common_catalog_(cc), travel_catalog_(tc), movement_(m),
      carriers_(s), registrations_(r), economy_(e), population_(p),
      foundation_(f), actor_catalog_(ac), business_(b), stamps_(stamps),
      vehicles_(vehicles), family_catalog_(family_catalog) {}
// 人口业务真正读取时必须存在原状态，不把空表当缺模块的替代。
const PopulationTransportState &
NativeCivilianSpatialCarrierSources::required_population() const {
  if (!population_)
    throw Failure{common_failure("PopulationTransportProviderRequired", {},
                                 ErrorKind::unsupported_capability)
                      .error};
  return *population_;
}
// 每次授权与交接都由原业务 reader 核验，缺失不得默认允许。
const TransportRegistrationBusinessRead &
NativeCivilianSpatialCarrierSources::required_business() const {
  if (!business_)
    throw Failure{
        common_failure("TransportRegistrationBusinessProviderRequired", {},
                       ErrorKind::unsupported_capability)
            .error};
  return *business_;
}

Outcome<WorldMoment>
NativeCivilianSpatialCarrierSources::current_moment() const {
  return stamps_.read_current_moment();
}
Outcome<TransportPopulationSource>
NativeCivilianSpatialCarrierSources::population(const SourceRef &ref) const {
  return guard<TransportPopulationSource>([&] {
    if (ref.domain != "local_economy" ||
        ref.kind != "population_transport_binding")
      fail("TransportPopulationBindingSourceRequired", ref.id);
    auto binding = take(read_population_transport_binding(
        economy_, required_population(), ref.id));
    if (binding.state == PopulationTransportPhase::released)
      fail("ReleasedPopulationNotTransported", ref.id);
    const auto *unit =
        carriers_.transport_units.find(binding.transport_unit_id);
    const auto *registration =
        registrations_.registrations.find(binding.registration_source_ref.id);
    const auto *cohort = economy_.cohorts.find(binding.transport_cohort_id);
    if (!unit || !registration || !cohort ||
        !same_source(unit->registration_source_ref,
                     binding.registration_source_ref) ||
        registration->transport_unit_id != binding.transport_unit_id ||
        std::none_of(unit->population_binding_refs.begin(),
                     unit->population_binding_refs.end(),
                     [&](const auto &r) { return same_source(r, ref); }))
      fail("TransportPopulationBacklinkMismatch", ref.id);
    fact(common_, binding.registration_source_ref);
    const auto revision = take(read_civilian_population_ref(
        economy_, required_population(), cohort->id));
    const auto actual = take(read_population_revision_source(
        economy_, required_population(), revision));
    if (actual.location_binding_ref.domain != "travel" ||
        actual.location_binding_ref.kind != "population_transport_position" ||
        actual.location_binding_ref.id != binding.binding_id)
      fail("PopulationTransportPositionRequired", ref.id);
    const auto at = take(current_moment());
    Integer workers = 0;
    std::set<StableId> used;
    for (const auto &id : binding.labor_allocation_refs) {
      const auto *allocation = economy_.labor_allocations.find(id);
      if (!used.insert(id).second || !allocation ||
          allocation->cohort_id != cohort->id ||
          allocation->activity_or_production_id != unit->transport_unit_id ||
          allocation->commitment_id != registration->id ||
          allocation->workers < 0)
        fail("TransportLaborBacklinkMismatch", id);
      if (allocation->from_tick <= at.half_day_tick &&
          allocation->through_tick >= at.half_day_tick)
        workers = plus(workers, allocation->workers);
    }
    if (workers > actual.labor_capacity)
      fail("TransportLaborExceedsOriginalCapacity", ref.id);
    return TransportPopulationSource{
        ref, cohort->id, actual.headcount, workers, {}};
  });
}
Outcome<MovementAnchor>
NativeCivilianSpatialCarrierSources::original_actor_anchor(
    const StableId &id) const {
  return guard<MovementAnchor>([&]() -> MovementAnchor {
    const auto *actor = common_.actor(id);
    if (!actor)
      fail("SpatialActorRequired", id);
    if (take(common_.field_battle_bound(id)))
      fail("FieldBattleActorSpatialReadRequired", id);
    if (movement_.spatial_bindings.find("actor:" + id))
      return take(read_carrier_movement_anchor(movement_, ActorCarrierRef{id}));
    if (!actor_catalog_.actors.find(id)) {
      if (!family_catalog_)
        fail("ActorInitialPlacementOwnerRequired", id);
      const auto created = take(read_created_character_initial_placement_v2(
          common_, foundation_, actor_catalog_, *family_catalog_,
          common_catalog_, id, take(current_moment())));
      if (created.actor_id != id || created.source_schema != 2)
        fail("ActorCreatedPlacementSourceRequired", id);
      OriginalMovementAnchor result{created.location,
                                    created.actual_creation_source};
      if (!take(same_spatial_anchor(
              result,
              OriginalMovementAnchor{actor->location,
                                     {"common", "actor_projection", id}})))
        fail("UntrackedActorLocation", id);
      return result;
    }
    take(read_authored_character_origin_v2(common_.state(), foundation_,
                                           actor_catalog_, id));
    const auto *original = actor_catalog_.actors.find(id);
    const auto *binding =
        foundation_.actor_start_context.actor_domain_bindings.find(id);
    if (!original || !binding ||
        original->placement.kind != AuthoredStartPlacement::Kind::at_place ||
        binding->placement_ref.kind != AuthoredStartPlacement::Kind::at_place ||
        binding->placement_ref.place_ref != original->placement.place_ref)
      fail("ActorOriginalPlacementRequired", id);
    const auto *place =
        common_catalog_.places.find(original->placement.place_ref);
    if (!place)
      fail("ActorOriginalPlaceRequired", id);
    OriginalMovementAnchor result{
        SettlementLocation{place->settlement_id, place->id},
        {"actor_foundation", "scenario_placement", id}};
    if (!take(same_spatial_anchor(
            result, OriginalMovementAnchor{
                        actor->location, {"common", "actor_projection", id}})))
      fail("UntrackedActorLocation", id);
    return result;
  });
}
Outcome<MovementAnchor>
NativeCivilianSpatialCarrierSources::original_site_anchor(
    const StableId &id) const {
  return read_original_spatial_site_anchor(
      common_catalog_, travel_catalog_,
      common_catalog_.places.find(id)
          ? SourceRef{"common_catalog", "place", id}
          : SourceRef{"travel_catalog", "settlement", id});
}
Outcome<Integer> NativeCivilianSpatialCarrierSources::selected_lot_weight(
    const CargoSelection &selection) const {
  return guard<Integer>([&] {
    const auto *inventory = common_.inventory(selection.inventory_id);
    if (!inventory || selection.quantity <= 0)
      fail("CargoSelectionRequired", selection.lot_id);
    const LotView *actual = nullptr;
    for (const auto &lot : inventory->lots)
      if (lot.id == selection.lot_id) {
        if (actual)
          fail("DuplicateInventoryLot", lot.id);
        actual = &lot;
      }
    if (!actual || actual->quantity < selection.quantity)
      fail("CargoLotQuantityMismatch", selection.lot_id);
    const auto *item = common_catalog_.items.find(actual->item_id);
    if (!item || item->weight < 0)
      fail("CargoItemWeightRequired", actual->item_id);
    if (item->weight &&
        selection.quantity > std::numeric_limits<Integer>::max() / item->weight)
      fail("TransportQuantityOverflow");
    return selection.quantity * item->weight;
  });
}
Status NativeCivilianSpatialCarrierSources::verify_registration_source(
    const TransportUnitState &unit) const {
  return status([&] {
    const auto &ref = unit.registration_source_ref;
    const auto *source =
        ref.domain == "travel" && ref.kind == "transport_registration"
            ? registrations_.registrations.find(ref.id)
            : nullptr;
    if (!source || source->id != ref.id ||
        source->transport_unit_id != unit.transport_unit_id ||
        !subject_same(take(read_registered_transport_subject(
                          common_, economy_, required_population(), *source)),
                      unit.subject))
      fail("TransportRegistrationSourceMismatch", unit.transport_unit_id);
    fact(common_, ref);
    take(required_business().validate_registration_authority(
        source->accepted_scope));
    const TransportUnitState *composition = &unit;
    if (unit.state == TransportUnitLifecycle::dissolved) {
      const TransportUnitReleaseSource *release = nullptr;
      for (const auto &row : registrations_.releases.rows())
        if (row.second.transport_unit_id == unit.transport_unit_id) {
          if (release)
            fail("DuplicateTransportRelease", unit.transport_unit_id);
          release = &row.second;
        }
      if (!release ||
          release->composition_revision + 1 != unit.composition_revision)
        fail("TransportReleaseSourceRequired", unit.transport_unit_id);
      fact(common_, {"travel", "transport_release", release->id});
      take(required_business().validate_transport_handover(
          *release, release->released_composition));
      composition = &release->released_composition;
    }
    if (composition->named_participants !=
            source->accepted_scope.named_participants ||
        composition->vehicle_refs != source->accepted_scope.vehicle_refs ||
        composition->inventory_ids != source->accepted_scope.inventory_ids ||
        composition->custody_ref != source->accepted_scope.custody_ref ||
        source->actual_vehicle_takeover_refs.size() !=
            source->accepted_scope.vehicle_refs.size())
      fail("TransportAcceptedCompositionMismatch", unit.transport_unit_id);
    if (!source->actual_vehicle_takeover_refs.empty() && !vehicles_)
      throw Failure{common_failure("TransportVehicleProviderRequired",
                                   unit.transport_unit_id,
                                   ErrorKind::unsupported_capability)
                        .error};
    for (std::size_t i = 0; i < source->actual_vehicle_takeover_refs.size();
         ++i)
      take(vehicles_->validate_vehicle_takeover(
          source->actual_vehicle_takeover_refs[i],
          source->accepted_scope.vehicle_refs[i],
          {unit.subject, unit.transport_unit_id}));
  });
}
Status NativeCivilianSpatialCarrierSources::verify_initiator(
    const GroupInitiator &initiator, const TransportUnitRef &unit,
    const SubjectRef &sponsor, const SourceRef &business_source) const {
  return status([&] {
    const auto *stored = carriers_.transport_units.find(unit.transport_unit_id);
    if (!stored || !subject_same(stored->subject, unit.subject))
      fail("TransportUnitRequired", unit.transport_unit_id);
    const auto &registration_ref = stored->registration_source_ref;
    const auto *registration =
        registration_ref.domain == "travel" &&
                registration_ref.kind == "transport_registration"
            ? registrations_.registrations.find(registration_ref.id)
            : nullptr;
    if (!registration ||
        registration->transport_unit_id != unit.transport_unit_id ||
        registration->id != registration_ref.id)
      fail("TransportRegistrationSourceRequired", unit.transport_unit_id);
    fact(common_, registration_ref);
    const auto &accepted = registration->accepted_scope;
    const SourceRef accepted_business = accepted.business_source;
    if (!same_source(business_source, registration_ref) &&
        !same_source(business_source, accepted_business))
      fail("TransportInitiatorOutsideAcceptedScope", unit.transport_unit_id);
    take(verify_registration_source(*stored));
    if (stored->state == TransportUnitLifecycle::dissolved)
      fail("TransportUnitAlreadyEnded", unit.transport_unit_id);
    const auto actual_subject = take(read_registered_transport_subject(
        common_, economy_, required_population(), *registration));
    if (!subject_same(actual_subject, unit.subject) ||
        !subject_same(actual_subject, sponsor))
      fail("TransportInitiatorOutsideAcceptedScope", unit.transport_unit_id);
    take(required_business().validate_registered_initiator(
        *registration, *stored, initiator, sponsor, take(current_moment())));
    if (const auto *named = std::get_if<ActorGroupInitiator>(&initiator)) {
      const auto *actor = common_.actor(named->initiator_actor_id);
      if (!actor || !actor->alive || !actor->available ||
          named->authority_ref.empty())
        fail("TransportInitiatorUnavailable", named->initiator_actor_id);
    }
  });
}

Outcome<std::shared_ptr<const ReadStamp>>
NativeCivilianSpatialCarrierSources::capture_manifest_reads(
    const TransportManifestRead &manifest) const {
  return guard<std::shared_ptr<const ReadStamp>>([&] {
    std::vector<ResourceKey> resources;
    std::vector<DomainReadKey> domains;
    std::vector<StableId> facts;
    std::set<std::pair<int, StableId>> seen;
    std::set<std::tuple<StableId, StableId, StableId>> domain_seen;
    std::set<StableId> fact_seen;
    auto domain = [&](const StableId &d, const StableId &k,
                      const StableId &id) {
      if (domain_seen.emplace(d, k, id).second)
        domains.push_back({d, k, id});
    };
    for (const auto &id : manifest.named_actor_refs)
      if (seen.insert({0, id}).second)
        resources.push_back({ResourceKey::Kind::actor, id});
    for (const auto &id : manifest.inventory_refs)
      if (seen.insert({1, id}).second)
        resources.push_back({ResourceKey::Kind::inventory, id});
    if (const auto *party = std::get_if<PartyCarrierRef>(&manifest.carrier))
      resources.push_back({ResourceKey::Kind::party, party->party_id});
    if (const auto *carrier =
            std::get_if<TransportUnitCarrierRef>(&manifest.carrier)) {
      const auto *unit = carriers_.transport_units.find(
          carrier->transport_unit_ref.transport_unit_id);
      if (!unit)
        fail("TransportUnitRequired");
      domain("travel", "transport_unit", unit->transport_unit_id);
      domain("travel", "carrier_position", unit->position_ref);
    }
    for (const auto &ref : manifest.source_refs) {
      const auto *head = common_.state().facts.find(ref.id);
      if (head && head->domain == ref.domain && head->kind == ref.kind &&
          fact_seen.insert(ref.id).second)
        facts.push_back(ref.id);
      if (ref.domain == "travel" && (ref.kind == "transport_registration" ||
                                     ref.kind == "transport_release"))
        domain(ref.domain, ref.kind, ref.id);
      if (ref.domain == "local_economy" &&
          ref.kind == "population_transport_binding") {
        domain(ref.domain, ref.kind, ref.id);
      }
      if (ref.domain == "local_economy" &&
          ref.kind == "population_transport_binding") {
        const auto *binding =
            required_population().transport_bindings.find(ref.id);
        if (!binding)
          fail("PopulationTransportBindingRequired", ref.id);
        domain("local_economy", "population_source_revision",
               binding->transport_cohort_id);
        for (const auto &id : binding->labor_allocation_refs)
          domain("local_economy", "labor_allocation", id);
      }
    }
    return take(stamps_.capture(resources, domains, facts));
  });
}
Outcome<std::uint64_t> read_spatial_carrier_domain_revision(
    const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations, const DomainReadKey &key) {
  return guard<std::uint64_t>([&]() -> std::uint64_t {
    if (key.domain != "travel")
      fail("SpatialDomainRevisionNotOwned", key.domain);
    Integer revision = 0;
    if (key.kind == "transport_unit") {
      const auto *unit = carriers.transport_units.find(key.id);
      if (!unit)
        fail("TransportUnitRequired", key.id);
      revision = unit->composition_revision;
    } else if (key.kind == "carrier_position") {
      const auto *position = carriers.positions.find(key.id);
      if (!position)
        fail("CarrierPositionRequired", key.id);
      revision = position->revision;
    } else if (key.kind == "transport_registration") {
      if (!registrations.registrations.find(key.id))
        fail("TransportRegistrationSourceRequired", key.id);
      revision = 1;
    } else if (key.kind == "transport_release") {
      if (!registrations.releases.find(key.id))
        fail("TransportReleaseSourceRequired", key.id);
      revision = 1;
    } else
      fail("UnsupportedSpatialDomainRevision", key.kind);
    if (revision < 1)
      fail("InvalidSpatialDomainRevision", key.id);
    return static_cast<std::uint64_t>(revision);
  });
}
Status NativeCivilianSpatialCarrierSources::validate_actor_carried_location(
    const StableId &actor_id, const CarriedLocation &location) const {
  return status([&] {
    if (location.carrier_kind == CarriedLocation::CarrierKind::fleet)
      fail("ActorCannotSkipVesselBinding", actor_id);
    if (location.carrier_kind != CarriedLocation::CarrierKind::transport_unit)
      throw Failure{common_failure("VesselPassengerBindingProviderRequired",
                                   location.carrier_id,
                                   ErrorKind::unsupported_capability)
                        .error};
    const auto *actor = common_.actor(actor_id);
    const auto *unit = carriers_.transport_units.find(location.carrier_id);
    const auto *binding = movement_.spatial_bindings.find("actor:" + actor_id);
    const auto *carried =
        binding ? std::get_if<CarriedMovementWriter>(&binding->writer)
                : nullptr;
    const auto *reference =
        carried ? std::get_if<TransportUnitCarrierRef>(&carried->carrier_ref)
                : nullptr;
    if (!actor || !unit || unit->state == TransportUnitLifecycle::dissolved ||
        !reference ||
        reference->transport_unit_ref.transport_unit_id !=
            unit->transport_unit_id ||
        !subject_same(reference->transport_unit_ref.subject, unit->subject) ||
        reference->composition_revision != unit->composition_revision ||
        !same_source(binding->binding_source, unit->registration_source_ref) ||
        std::find(unit->named_participants.begin(),
                  unit->named_participants.end(),
                  actor_id) == unit->named_participants.end())
      fail("TransportParticipantCarriedBindingMismatch", actor_id);
    take(verify_registration_source(*unit));
  });
}
// 原固定合同没有给出非军事陆运capacity参数；该子能力保持未就绪，不能拿仓容/人数作物理值。
Outcome<TransportCapacitySource>
NativeCivilianSpatialCarrierSources::carrier_capacity(
    const SpatialCarrierRef &ref) const {
  return {false,
          {},
          common_failure("OriginalLandCarrierCapacityAuthorRequired",
                         spatial_carrier_key(ref),
                         ErrorKind::unsupported_capability)
              .error};
}
Outcome<Integer> NativeCivilianSpatialCarrierSources::vehicle_capacity(
    const StableId &id) const {
  return {false,
          {},
          common_failure("OriginalVehicleCapacityAuthorRequired", id,
                         ErrorKind::unsupported_capability)
              .error};
}
Outcome<TransportManifestRead>
NativeCivilianSpatialCarrierSources::military_manifest(
    const SpatialCarrierRef &ref) const {
  return {false,
          {},
          common_failure("MilitarySpatialManifestProviderRequired",
                         spatial_carrier_key(ref),
                         ErrorKind::unsupported_capability)
              .error};
}
} // namespace hanmo::resident
