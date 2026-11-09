#pragma once
#include "transport_section_owned_fields.hpp"
namespace hanmo::resident::transport_section_fields {
inline bool entry_owner(const TransportEntryManifestSource &entry,
                        const BoundaryCrossingReceipt &crossing) {
  return movement_detail::execution_key(entry.execution) ==
             movement_detail::execution_key(crossing.execution) &&
         entry.entry_ordinal == crossing.entry_ordinal &&
         entry.boundary.kind == crossing.boundary.kind &&
         entry.boundary.id == crossing.boundary.id &&
         entry.boundary.map_id == crossing.boundary.map_id &&
         entry.boundary.from_site_id == crossing.boundary.from_site_id &&
         entry.boundary.to_site_id == crossing.boundary.to_site_id;
}
template <class C> Status composition_changes(C &c, State &state) {
  auto &registrations = *state.roots.registrations;
  if (c.reading()) {
    auto checked = transport_physical_table::append_rows(
        c, "composition_change_sources", state.decoded_composition,
        [&](auto &a, State::CompositionChange &row) {
          a.field("key", row.key);
          return a.object("fields", [&](auto &b) {
            Integer kind = 0;
            b.field("kind", kind);
            if (kind < 0 || kind > 1)
              return b.reject("InvalidTransportCompositionChangeKind", row.key);
            if (kind == 0) {
              row.source.template emplace<TransportRegistrationSource>();
              auto &source = std::get<TransportRegistrationSource>(row.source);
              auto result = b.object("registration", [&](auto &fields) {
                return value(fields, source, state.record_selection);
              });
              if (result.ok && source.id != row.key)
                return b.reject("TransportRegistrationKeyMismatch", row.key);
              return result;
            }
            row.source.template emplace<TransportUnitReleaseSource>();
            auto &source = std::get<TransportUnitReleaseSource>(row.source);
            auto result = b.object("release", [&](auto &fields) {
              return value(fields, source, state.record_selection);
            });
            if (result.ok && source.id != row.key)
              return b.reject("TransportReleaseKeyMismatch", row.key);
            return result;
          });
        });
    if (!checked.ok)
      return checked;
    for (std::size_t n = 0; n < state.decoded_composition.size(); ++n)
      if (state.decoded_composition[n].key.empty() ||
          (n && state.decoded_composition[n - 1].key >=
                    state.decoded_composition[n].key))
        return c.reject("InvalidCanonicalCompositionSourceOrder");
    for (auto &row : state.decoded_composition) {
      if (auto *source = std::get_if<TransportRegistrationSource>(&row.source))
        checked = append_original_row(
            c, state.registrations.values,
            std::make_pair(std::move(row.key), std::move(*source)),
            "composition_change_sources");
      else
        checked = append_original_row(
            c, state.releases.values,
            std::make_pair(
                std::move(row.key),
                std::move(std::get<TransportUnitReleaseSource>(row.source))),
            "composition_change_sources");
      if (!checked.ok)
        return checked;
    }
    return c.status();
  }
  transport_physical_table::retain(c, registrations.registrations);
  transport_physical_table::retain(c, registrations.releases);
  struct Borrowed {
    const StableId *key;
    const TransportRegistrationSource *registration;
    const TransportUnitReleaseSource *release;
  };
  CheckpointVector<Borrowed> rows;
  for (const auto &r : registrations.registrations.rows())
    rows.push_back({&r.first, &r.second, nullptr});
  for (const auto &r : registrations.releases.rows())
    rows.push_back({&r.first, nullptr, &r.second});
  std::sort(rows.begin(), rows.end(),
            [](const auto &a, const auto &b) { return *a.key < *b.key; });
  for (std::size_t n = 1; n < rows.size(); ++n)
    if (*rows[n - 1].key >= *rows[n].key)
      return c.reject("DuplicateTransportCompositionSource", *rows[n].key);
  return c.sequence(
      "composition_change_sources", rows, [&](auto &a, Borrowed &row) {
        a.field("key", const_cast<StableId &>(*row.key));
        return a.object("fields", [&](auto &b) {
          Integer kind = row.registration ? 0 : 1;
          b.field("kind", kind);
          if (row.registration)
            return b.object("registration", [&](auto &fields) {
              return value(
                  fields,
                  const_cast<TransportRegistrationSource &>(*row.registration),
                  state.record_selection);
            });
          return b.object("release", [&](auto &fields) {
            return value(fields,
                         const_cast<TransportUnitReleaseSource &>(*row.release),
                         state.record_selection);
          });
        });
      });
}
template <class C>
Status boundary_crossing(C &c, const StableId &key,
                         BoundaryCrossingReceipt &row, State &state) {
  auto checked = value(c, row, state.record_selection);
  if (!checked.ok)
    return checked;
  if (key != row.receipt_id)
    return c.reject("BoundaryCrossingKeyMismatch", key);
  auto &groups = *state.roots.executions;
  auto belongs = [&](const auto &, const TransportEntryManifestSource &entry) {
    return entry_owner(entry, row);
  };
  const auto before = state.entry_manifests.values.size();
  checked = optional_child(
      c, "entry_manifest_source", groups.entry_manifests, state.entry_manifests,
      [&](auto &a, const StableId &id, TransportEntryManifestSource &v) {
        auto result = movement_physical_fields::entry_manifest(
            a, v, state.record_selection);
        if (result.ok && v.id != id)
          return a.reject("EntryManifestKeyMismatch", id);
        return result;
      },
      belongs);
  if (!checked.ok)
    return checked;
  if (c.reading() && state.entry_manifests.values.size() != before + 1)
    return c.reject("BoundaryCrossingEntryManifestRequired", key);
  if (!c.reading()) {
    Integer count = 0;
    for (const auto &entry : groups.entry_manifests.rows())
      if (entry_owner(entry.second, row))
        ++count;
    if (count != 1)
      return c.reject("BoundaryCrossingEntryManifestRequired", key);
  }
  return payments(c, "toll_payment_sources", state,
                  TransportPaymentPhysicalOwner::boundary_crossing, key);
}
template <class C> Status manifest_bindings(C &c, State &state) {
  if (!state.roots.carriers || !state.roots.executions ||
      !state.roots.registrations || !state.roots.payments)
    return c.reject("TransportManifestOriginalRootsRequired");
  auto &carriers = *state.roots.carriers;
  auto checked = transport_section_fields::all(
      c, "assignments", carriers.inventory_bindings, state.assignments,
      [&](auto &a, const StableId &key, InventorySpatialBinding &v) {
        auto result = value(a, v, state.record_selection);
        if (result.ok && v.inventory_id != key)
          return a.reject("InventoryPlacementKeyMismatch", key);
        return result;
      });
  if (!checked.ok)
    return checked;
  checked = composition_changes(c, state);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "boundary_crossing_receipts", carriers.crossings, state.crossings,
      [&](auto &a, const auto &key, auto &row) {
        return boundary_crossing(a, key, row, state);
      });
  if (!checked.ok)
    return checked;
  return state.reading ? state.adopt_manifest_rows() : c.status();
}
} // namespace hanmo::resident::transport_section_fields
