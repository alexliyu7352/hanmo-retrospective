#include "resident/domains/military/military_model.hpp"
// 真实source反验与唯一终点；不从HP、injury、今日Office或消息建立死亡。
#include "family_life_exact.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/military/field_battle/field_validation.hpp"
#include "resident/kernel/native_military_state.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 传原军域typed reader前先租真正参数文本；不是结果/事实第二拥有根。
CheckpointMemoryCharge source_memory(std::string_view domain,
                                     std::string_view kind,
                                     std::string_view id) {


  return CheckpointMemoryCharge(0);
}
struct LifeSourceArgument {
  CheckpointMemoryCharge memory;
  SourceRef value;
  LifeSourceArgument(std::string_view domain, std::string_view kind,
                     std::string_view id)
      : memory(source_memory(domain, kind, id)),
        value{checkpoint_fresh_text_copy(domain),
              checkpoint_fresh_text_copy(kind),
              checkpoint_fresh_text_copy(id)} {}
};
bool same_moment(const WorldMoment &a, const WorldMoment &b) {
  auto x = compare_world_moments(a, b);
  return x.ok && x.value == 0;
}
bool valid_ref(const SourceRef &r) {
  return !r.domain.empty() && !r.kind.empty() && !r.id.empty();
}
Status prior(const CommonStore &c, const StableId &source_id,
             const StableId &source_domain, const StableId &source_kind,
             const StableId &after) {
  const auto *h = c.state().facts.find(source_id);
  const auto &order = c.state().facts.order();
  const auto a = std::find(order.begin(), order.end(), source_id),
             b = std::find(order.begin(), order.end(), after);
  if (!h || h->domain != source_domain || h->kind != source_kind ||
      a == order.end() || b == order.end() || a >= b)
    return common_failure("FamilyDeathBeforeCause", after);
  return common_success();
}
Status actor_stamp(const CommonStore &c, const ReadStampAuthority &authority,
                   const DomainReadVersionSource &versions,
                   const ReadStamp &stamp, const StableId &actor) {
  auto valid =
      authority.validate(stamp, c.state().identity, stamp.mode(), c, versions);
  if (!valid.ok)
    return valid;
  for (const auto &r : stamp.resources())
    if (r.resource.kind == ResourceKey::Kind::actor && r.resource.id == actor)
      return common_success();
  return common_failure("FamilyDeathActorReadStampRequired", actor);
}
} // namespace
// Original dynamic2 and explicit Condition8 Field1 retain distinct owning
// roots.
Status validate_family_life4_original_military_selection(
    const CommonStore &c, const FamilyLifeCauseSources4 &sources) {
  if (!sources.catalog_selection)
    return sources.military_exact && !sources.field_battles &&
                   !sources.field_application_clock
               ? common_success()
               : common_failure("FamilyLifeOriginalMilitarySourceRequired", {},
                                ErrorKind::unsupported_capability);
  if (sources.catalog_selection->content_digest !=
      c.state().metadata.content_digest)
    return common_failure("FamilyLifeMilitaryCatalogMismatch", {},
                          ErrorKind::internal);
  auto selected =
      selected_native_military_state_versions(*sources.catalog_selection);
  if (!selected.ok)
    return {false, {}, selected.error};
  auto field = selected_field_battle_version(*sources.catalog_selection);
  if (!field.ok)
    return {false, {}, field.error};
  auto successor = condition_application_facts_selected(
      *sources.catalog_selection,
      ConditionApplicationHistoryOwner::injury_sources);
  if (!successor.ok)
    return {false, {}, successor.error};
  const bool field_admitted = field.value == 1 && successor.value;
  if (field_admitted != bool(sources.field_battles) ||
      field_admitted != bool(sources.field_application_clock))
    return common_failure("FamilyLifeSelectedFieldRootRequired", {},
                          ErrorKind::unsupported_capability);
  if (sources.field_battles) {
    if (!sources.field_application_clock->bound_to(c, sources.military,
                                                   *sources.field_battles))
      return common_failure("FamilyLifeFieldClockBindingRequired", {},
                            ErrorKind::unsupported_capability);
    if (sources.field_battles->schema != 1)
      return common_failure("FamilyLifeFieldSourceVersionMismatch", {},
                            ErrorKind::unsupported_capability);
    auto valid = validate_field_segment_settlement_sources(
        c, sources.military, *sources.field_battles,
        sources.field_application_clock);
    if (!valid.ok)
      return valid;
  }
  if (selected.value.dynamic_military && !sources.military_exact)
    return common_failure("FamilyLifeSelectedMilitaryRootRequired", {},
                          ErrorKind::unsupported_capability);
  if (!selected.value.dynamic_military && sources.military_exact)
    return common_failure("FamilyLifeUnselectedMilitaryRoot", {},
                          ErrorKind::internal);
  for (const auto &row : sources.military.casualty_facts) {
    if (sources.military_exact &&
        sources.military_exact->dynamic_casualty_facts.find(row.id))
      return common_failure("FamilyLifeAmbiguousMilitarySource", row.id);
    if (selected.value.dynamic_military)
      continue; // preserve original dynamic history admission
    if (!sources.field_battles)
      return common_failure("FamilyLifeUnselectedMilitaryHistory", row.id,
                            ErrorKind::unsupported_capability);
    const LifeSourceArgument parent_ref("military", "field_battle_segment",
                                        row.battle_id);
    auto parent = read_field_segment_settlement_source(
        c, sources.military, *sources.field_battles, parent_ref.value,
        sources.field_application_clock);
    if (!parent.ok)
      return {false, {}, parent.error};
    if (parent.value.casualties != &row)
      return common_failure("FamilyLifeFieldCasualtyOwnerMismatch", row.id);
  }
  if (!selected.value.dynamic_military)
    for (const auto &row : c.state().facts.rows())
      if (row.second.domain == "military" &&
          row.second.kind == "military_casualties") {
        std::size_t matches = 0;
        for (const auto &original : sources.military.casualty_facts)
          if (original.id == row.first)
            ++matches;
        if (!sources.field_battles || matches != 1)
          return common_failure("FamilyLifeUnselectedMilitaryHistory",
                                row.first, ErrorKind::unsupported_capability);
      }
  return common_success();
}
bool same_life_cause4(const LifeCauseRef4 &a, const LifeCauseRef4 &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<MilitaryDeathCause4>(&a)) {
    const auto &y = std::get<MilitaryDeathCause4>(b);
    return x->casualty_fact_id == y.casualty_fact_id &&
           x->battle_id == y.battle_id && x->actor_id == y.actor_id &&
           same_moment(x->occurrence_at, y.occurrence_at);
  }
  if (const auto *x = std::get_if<NaturalAgeDeathCause4>(&a))
    return x->age_check_fact_id ==
           std::get<NaturalAgeDeathCause4>(b).age_check_fact_id;
  const auto &x = std::get<EnvironmentalSeaDeathCause4>(a);
  const auto &y = std::get<EnvironmentalSeaDeathCause4>(b);
  return x.sea_loss_result_id == y.sea_loss_result_id &&
         x.actor_id == y.actor_id &&
         x.vessel_passenger_binding_ref == y.vessel_passenger_binding_ref &&
         same_moment(x.occurrence_at, y.occurrence_at);
}
namespace {
Status validate_life_cause_read_context(const CommonStore &c,
                                        const FamilyLifeCauseSources4 &sources,
                                        const WorldMoment *actual_now,
                                        MilitaryCasualtyHistoryOrder ordering) {
  if (ordering != MilitaryCasualtyHistoryOrder::legacy_serial &&
      ordering != MilitaryCasualtyHistoryOrder::actual_common_prefix)
    return {false,
            {},
            common_failure("FamilyLifeHistoryOrderRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  if (ordering == MilitaryCasualtyHistoryOrder::actual_common_prefix &&
      !actual_now)
    return {false,
            {},
            common_failure("FamilyLifeActualHistoryClockRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto selected = validate_family_life4_original_military_selection(c, sources);
  if (!selected.ok)
    return {false, {}, selected.error};
  return common_success();
}
struct FieldLifeCauseBorrowed {
  const StableId *actor_id = nullptr;
  const FactRecord *header = nullptr;
  const WorldMoment *occurred_at = nullptr;
  Integer ordinal = 0;
};
// 仅内部已过同一selection/order门后使用；nullopt只表示这不是原Field分支。
Outcome<std::optional<FieldLifeCauseBorrowed>> field_life_cause_borrowed(
    const CommonStore &c, const FamilyLifeCauseSources4 &sources,
    const MilitaryDeathCause4 &military, const WorldMoment *actual_now) {
  const MilitaryCasualtyFact *receipt = nullptr;
  if (sources.field_battles)
    for (const auto &row : sources.military.casualty_facts)
      if (row.id == military.casualty_fact_id) {
        if (receipt)
          return {false,
                  {},
                  common_failure("FamilyLifeAmbiguousMilitarySource",
                                 military.casualty_fact_id)
                      .error};
        receipt = &row;
      }
  const auto *dynamic =
      sources.military_exact
          ? sources.military_exact->dynamic_casualty_facts.find(
                military.casualty_fact_id)
          : nullptr;
  if (receipt && dynamic)
    return {false,
            {},
            common_failure("FamilyLifeAmbiguousMilitarySource",
                           military.casualty_fact_id)
                .error};
  if (!receipt)
    return {true, std::nullopt, {}};
  if (!actual_now)
    return {false,
            {},
            common_failure("FamilyLifeFieldActualClockRequired",
                           military.casualty_fact_id,
                           ErrorKind::unsupported_capability)
                .error};
  const LifeSourceArgument ref("military", "military_casualties",
                               military.casualty_fact_id);
  auto fatal = read_field_segment_fatal_source(
      c, sources.military, *sources.field_battles, ref.value, military.actor_id,
      *actual_now, sources.field_application_clock);
  if (!fatal.ok)
    return {false, {}, std::move(fatal.error)};
  if (fatal.value.source.session->battle_id != military.battle_id ||
      !same_moment(fatal.value.source.settlement->applied_at,
                   military.occurrence_at))
    return {
        false,
        {},
        common_failure("MilitaryDeathCauseMismatch", military.casualty_fact_id)
            .error};
  const auto *header = c.state().facts.find(military.casualty_fact_id);
  if (!c.actor(fatal.value.actor_change->actor_id) || !valid_ref(ref.value) ||
      !header || header->domain != ref.value.domain ||
      header->kind != ref.value.kind || fatal.value.source_ordinal < 0)
    return {false,
            {},
            common_failure("InvalidLifeDeathSource", military.casualty_fact_id)
                .error};
  return {true,
          FieldLifeCauseBorrowed{&fatal.value.actor_change->actor_id, header,
                                 &fatal.value.source.settlement->applied_at,
                                 fatal.value.source_ordinal},
          {}};
}
} // namespace
Outcome<LifeCauseView4>
read_life_cause4(const CommonStore &c, const FamilyLifeCauseSources4 &sources,
                 const LifeCauseRef4 &cause, const WorldMoment *actual_now,
                 MilitaryCasualtyHistoryOrder ordering) {
  auto context =
      validate_life_cause_read_context(c, sources, actual_now, ordering);
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  Outcome<LifeCauseView4> out;
  if (const auto *military = std::get_if<MilitaryDeathCause4>(&cause)) {
    auto field = field_life_cause_borrowed(c, sources, *military, actual_now);
    if (!field.ok)
      return {false, {}, std::move(field.error)};
    if (field.value) {
      const auto &v = *field.value;
      // 旧裸ABI只保兼容投影；真正record验证直接用同借证据，不产生此副本。
      out = {true,
             {*v.actor_id,
              {v.header->domain, v.header->kind, military->casualty_fact_id},
              *v.occurred_at,
              1,
              v.ordinal},
             {}};
    } else {
      const LifeSourceArgument argument("military", "military_casualties",
                                        military->casualty_fact_id);
      const auto &ref = argument.value;
      if (!sources.military_exact)
        return {false,
                {},
                common_failure("FamilyLifeOriginalMilitarySourceRequired",
                               ref.id, ErrorKind::unsupported_capability)
                    .error};
      // 新精确death只读动态完整2源，绝不由旧tick猜offset=0。
      std::optional<MilitaryCasualtyHistoryContext> history;
      if (actual_now)
        history.emplace(MilitaryCasualtyHistoryContext{
            c, *actual_now, sources.military_formations,
            sources.military_returns, ordering});
      auto fatal = read_military_fatal_consequence_for_actor(
          sources.military, *sources.military_exact, ref, 2, military->actor_id,
          {}, history ? &*history : nullptr);
      if (!fatal.ok)
        return {false, {}, fatal.error};
      if (fatal.value.battle_id != military->battle_id ||
          !same_moment(fatal.value.occurred_at, military->occurrence_at))
        return {false,
                {},
                common_failure("MilitaryDeathCauseMismatch",
                               military->casualty_fact_id)
                    .error};
      out = {true,
             {fatal.value.actor_id, ref, fatal.value.occurred_at, 2,
              fatal.value.source_ordinal},
             {}};
    }
  } else if (const auto *age = std::get_if<NaturalAgeDeathCause4>(&cause)) {
    if (!sources.age)
      return {false,
              {},
              common_failure("NaturalAgeDeathSourceReadRequired",
                             age->age_check_fact_id,
                             ErrorKind::unsupported_capability)
                  .error};
    out = sources.age->read_age_death_source(*age);
  } else {
    const auto &sea = std::get<EnvironmentalSeaDeathCause4>(cause);
    if (!sources.sea)
      return {false,
              {},
              common_failure("EnvironmentalSeaDeathSourceReadRequired",
                             sea.sea_loss_result_id,
                             ErrorKind::unsupported_capability)
                  .error};
    out = sources.sea->read_sea_death_source(sea);
  }
  if (!out.ok)
    return out;
  const auto *h = c.state().facts.find(out.value.cause_ref.id);
  if (!c.actor(out.value.actor_id) || !valid_ref(out.value.cause_ref) || !h ||
      h->domain != out.value.cause_ref.domain ||
      h->kind != out.value.cause_ref.kind || out.value.source_schema < 1 ||
      out.value.source_ordinal < 0)
    return {
        false,
        {},
        common_failure("InvalidLifeDeathSource", out.value.cause_ref.id).error};
  return out;
}
Status validate_family_death4(const CommonStore &c,
                              const FamilyDeathRecord &record,
                              const FamilyLifeCauseSources4 &sources,
                              const WorldMoment &current,
                              MilitaryCasualtyHistoryOrder ordering) {
  if (!record.precise || record.kind != "family_death" ||
      record.cause_kind != "military_casualty" ||
      !record.cause_fact_id.empty() || !record.battle_id.empty() ||
      record.death_tick != 0 || record.recorded_tick != 0 ||
      !record.field_order.empty())
    return common_failure("FamilyLifeProfileMismatch");
  const auto &d = *record.precise;
  const auto *actor = c.actor(record.actor_id);
  const auto *header = c.state().facts.find(d.death_id);
  if (!actor || actor->alive || actor->available || actor->hp != 0 ||
      record.world_id != c.state().identity.world_id ||
      record.scenario_id != c.state().metadata.scenario_id ||
      d.death_id.empty() || !header || header->domain != "family_life" ||
      header->kind != "family_death")
    return common_failure("InvalidFamilyDeath4", d.death_id);
  auto one = compare_world_moments(d.effective_at, d.recorded_at),
       two = compare_world_moments(d.recorded_at, current);
  if (!one.ok)
    return {false, {}, one.error};
  if (!two.ok)
    return {false, {}, two.error};
  if (one.value > 0 || two.value > 0)
    return common_failure("InvalidFamilyDeathTime", d.death_id);
  auto context =
      validate_life_cause_read_context(c, sources, &current, ordering);
  if (!context.ok)
    return context;
  if (const auto *military = std::get_if<MilitaryDeathCause4>(&d.cause)) {
    auto field = field_life_cause_borrowed(c, sources, *military, &current);
    if (!field.ok)
      return {false, {}, std::move(field.error)};
    if (field.value) {
      const auto &v = *field.value;
      if (*v.actor_id != record.actor_id ||
          !same_moment(*v.occurred_at, d.effective_at))
        return common_failure("FamilyDeathSourceMismatch", d.death_id);
      return prior(c, military->casualty_fact_id, v.header->domain,
                   v.header->kind, d.death_id);
    }
  }
  auto source = read_life_cause4(c, sources, d.cause, &current, ordering);
  if (!source.ok)
    return {false, {}, source.error};
  if (source.value.actor_id != record.actor_id ||
      !same_moment(source.value.occurred_at, d.effective_at))
    return common_failure("FamilyDeathSourceMismatch", d.death_id);
  return prior(c, source.value.cause_ref.id, source.value.cause_ref.domain,
               source.value.cause_ref.kind, d.death_id);
}
Status validate_family_life4(const CommonStore &c,
                             const FamilyDeathState &state,
                             const FamilyLifeCauseSources4 &sources,
                             const WorldMoment &current,
                             MilitaryCasualtyHistoryOrder ordering) {
  auto selected = validate_family_life4_original_military_selection(c, sources);
  if (!selected.ok)
    return selected;
  if (!state.field_order.empty())
    return common_failure("FamilyLife4UnexpectedFieldOrder");
  for (const auto &row : state.deaths.rows()) {
    if (!row.second.precise || row.first != row.second.precise->death_id)
      return common_failure("FamilyDeathKeyMismatch", row.first);
    auto valid =
        validate_family_death4(c, row.second, sources, current, ordering);
    if (!valid.ok)
      return valid;
    for (const auto &other : state.deaths.rows())
      if (other.first != row.first &&
          other.second.actor_id == row.second.actor_id)
        return common_failure("ConflictingDeathSource", row.second.actor_id);
  }
  for (const auto &row : c.state().facts.rows())
    if (row.second.domain == "family_life" &&
        row.second.kind == "family_death" && !state.deaths.find(row.first))
      return common_failure("MissingFamilyDeathRecord", row.first);
  const MilitaryCasualtyHistoryContext history{
      c, current, sources.military_formations, sources.military_returns,
      ordering};
  if (sources.military_exact)
    for (const auto &row :
         sources.military_exact->dynamic_casualty_facts.rows()) {
      const LifeSourceArgument source_ref("military", "military_casualties",
                                          row.first);
      auto actual = read_military_casualty_source(
          sources.military, *sources.military_exact, source_ref.value, 2, {},
          &history);
      if (!actual.ok)
        return {false, {}, actual.error};
      for (const auto &change : actual.value.actor_changes)
        if (change.status == "dead") {
          const auto *death = family_death_for_actor(state, change.actor_id);
          if (!death || !death->precise)
            return common_failure("MissingFamilyDeathRecord", change.actor_id);
          const auto *cause =
              std::get_if<MilitaryDeathCause4>(&death->precise->cause);
          if (!cause || cause->casualty_fact_id != row.first)
            return common_failure("ConflictingDeathSource", change.actor_id);
        }
    }
  if (sources.field_battles)
    for (const auto &session : sources.field_battles->sessions)
      for (const auto &segment : session.second.settlements) {
        const LifeSourceArgument segment_ref("military", "field_battle_segment",
                                             segment.fact_id);
        auto original = read_field_segment_settlement_source(
            c, sources.military, *sources.field_battles, segment_ref.value,
            sources.field_application_clock);
        if (!original.ok)
          return {false, {}, original.error};
        if (original.value.session != &session.second ||
            original.value.settlement != &segment || !original.value.casualties)
          return common_failure("FamilyLifeFieldSegmentOwnerMismatch",
                                segment.fact_id);
        // Every actual segment casualty is scanned, not only a final battle.
        for (const auto &change : original.value.casualties->actor_changes)
          if (change.status == "dead") {
            const LifeSourceArgument casualty_ref(
                "military", "military_casualties",
                original.value.casualties->id);
            auto fatal = read_field_segment_fatal_source(
                c, sources.military, *sources.field_battles, casualty_ref.value,
                change.actor_id, current, sources.field_application_clock);
            if (!fatal.ok)
              return {false, {}, fatal.error};
            const auto *death = family_death_for_actor(state, change.actor_id);
            if (!death || !death->precise)
              return common_failure("MissingFamilyDeathRecord",
                                    change.actor_id);
            const auto *cause =
                std::get_if<MilitaryDeathCause4>(&death->precise->cause);
            if (!cause ||
                cause->casualty_fact_id != original.value.casualties->id ||
                cause->battle_id != session.second.battle_id ||
                !same_moment(cause->occurrence_at, segment.applied_at))
              return common_failure("ConflictingDeathSource", change.actor_id);
          }
      }
  return common_success();
}
Outcome<const FamilyDeathRecord *>
read_verified_death4_record(const CommonStore &c, const FamilyDeathState &state,
                            const FamilyLifeCauseSources4 &sources,
                            const DeathRef &ref, const WorldMoment &current,
                            MilitaryCasualtyHistoryOrder ordering) {
  if (ref.domain != "family_life" || ref.kind != "family_death" ||
      ref.id.empty())
    return {false, {}, common_failure("InvalidDeathRef", ref.id).error};
  const auto *d = state.deaths.find(ref.id);
  if (!d)
    return {false, {}, common_failure("DeathRefMissing", ref.id).error};
  auto valid = validate_family_death4(c, *d, sources, current, ordering);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &other : state.deaths.rows())
    if (other.first != ref.id && other.second.actor_id == d->actor_id)
      return {false,
              {},
              common_failure("ConflictingDeathSource", d->actor_id).error};
  return {true, d, {}};
}
Outcome<VerifiedDeathView>
read_verified_death4(const CommonStore &c, const FamilyDeathState &state,
                     const FamilyLifeCauseSources4 &sources,
                     const DeathRef &ref, const WorldMoment &current,
                     MilitaryCasualtyHistoryOrder ordering) {
  auto original =
      read_verified_death4_record(c, state, sources, ref, current, ordering);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  const auto *d = original.value;
  auto source =
      read_life_cause4(c, sources, d->precise->cause, &current, ordering);
  if (!source.ok)
    return {false, {}, source.error};
  VerifiedDeathView out;
  out.death_ref = ref;
  out.actor_id = d->actor_id;
  out.effective_at.exact_moment = d->precise->effective_at;
  out.recorded_at.exact_moment = d->precise->recorded_at;
  out.cause_kind = d->precise->cause.index() == 0   ? "military_casualty"
                   : d->precise->cause.index() == 1 ? "natural_age"
                                                    : "environmental_sea";
  out.cause_ref = source.value.cause_ref;
  out.source_schema = source.value.source_schema;
  out.source_ordinal = source.value.source_ordinal;
  out.world_id = d->world_id;
  out.scenario_id = d->scenario_id;
  return {true, std::move(out), {}};
}
FamilyVerifiedDeathRead::FamilyVerifiedDeathRead(
    const CommonStore &c, const FamilyDeathState &s,
    const FamilyLifeCauseSources4 &sources, const ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions,
    MilitaryCasualtyHistoryOrder ordering)
    : common_(c), state_(s), sources_(sources), stamps_(stamps),
      versions_(versions), ordering_(ordering) {}
Outcome<VerifiedDeathView>
FamilyVerifiedDeathRead::by_ref(const DeathRef &ref,
                                const ReadStamp &stamp) const {
  const auto *record = state_.deaths.find(ref.id);
  if (!record)
    return {false, {}, common_failure("DeathRefMissing", ref.id).error};
  auto valid =
      actor_stamp(common_, stamps_, versions_, stamp, record->actor_id);
  if (!valid.ok)
    return {false, {}, valid.error};
  return read_verified_death4(common_, state_, sources_, ref, stamp.span().to,
                              ordering_);
}
Outcome<std::optional<VerifiedDeathView>>
FamilyVerifiedDeathRead::by_actor(const StableId &actor,
                                  const ReadStamp &stamp) const {
  auto valid = actor_stamp(common_, stamps_, versions_, stamp, actor);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!common_.actor(actor))
    return {false, {}, common_failure("UnknownActor", actor).error};
  const FamilyDeathRecord *found = nullptr;
  for (const auto &row : state_.deaths.rows())
    if (row.second.actor_id == actor) {
      if (found)
        return {
            false, {}, common_failure("ConflictingDeathSource", actor).error};
      found = &row.second;
    }
  if (!found)
    return {true, std::nullopt, {}};
  if (!found->precise)
    return {false, {}, common_failure("FamilyLifeProfileMismatch").error};
  auto read =
      by_ref({"family_life", "family_death", found->precise->death_id}, stamp);
  return {read.ok,
          read.ok ? std::optional<VerifiedDeathView>{std::move(read.value)}
                  : std::nullopt,
          read.error};
}
} // namespace hanmo::resident
