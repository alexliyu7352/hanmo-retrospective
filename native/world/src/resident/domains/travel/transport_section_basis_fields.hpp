#pragma once
#include "transport_section_owned_fields.hpp"
namespace hanmo::resident::transport_section_fields {
template <class C> Status movement_bases(C &c, State &state) {
  if (c.reading()) {
    if (!state.decoded_bases.empty() || !state.pending_bases.values.empty())
      return c.reject("MovementBasisPhysicalRootAlreadyPopulated");
    auto status = transport_physical_table::append_rows(
        c, "movement_bases", state.pending_bases.values,
        [&](auto &a, std::pair<StableId, MovementBasis> &row) {
          a.field("key", row.first);
          return a.object("fields", [&](auto &b) {
            auto result = movement_physical_fields::basis(
                b, row.second, state.record_selection);
            if (result.ok &&
                (row.first.empty() ||
                 movement_detail::basis_id(row.second) != row.first))
              return b.reject("MovementPhysicalBasisKeyMismatch", row.first);
            return result;
          });
        });
    if (!status.ok)
      return status;
    for (std::size_t n = 1; n < state.pending_bases.values.size(); ++n)
      if (state.pending_bases.values[n - 1].first >=
          state.pending_bases.values[n].first)
        return c.reject("InvalidCanonicalMovementBasisOrder");
    status = transport_physical_table::adopt(
        state.decoded_bases, state.pending_bases, "movement_bases");
    if (!status.ok)
      return status;
    status = transport_physical_table::append_rows(
        c, "terrain_cost_bases", state.decoded_terrain_keys,
        [&](auto &a, State::TerrainCostKey &key) {
          a.field("key", key.key);
          auto *basis = state.decoded_bases.find(key.key);
          auto *terrain =
              basis ? std::get_if<TerrainCostBasis>(basis) : nullptr;
          if (!terrain)
            return a.reject("UnpairedTerrainCostBasis", key.key);
          return a.object("fields", [&](auto &b) {
            auto result = movement_physical_fields::terrain_cost_basis(
                b, *terrain, state.record_selection);
            if (result.ok && terrain->basis_ref != key.key)
              return b.reject("TerrainCostBasisOriginalPairMismatch", key.key);
            return result;
          });
        });
    if (!status.ok)
      return status;
    for (std::size_t n = 0; n < state.decoded_terrain_keys.size(); ++n)
      if (state.decoded_terrain_keys[n].key.empty() ||
          (n && state.decoded_terrain_keys[n - 1].key >=
                    state.decoded_terrain_keys[n].key))
        return c.reject("InvalidCanonicalTerrainCostBasisOrder");
    std::size_t expected = 0;
    for (const auto &row : state.decoded_bases.rows())
      if (std::holds_alternative<TerrainCostBasis>(row.second))
        ++expected;
    if (expected != state.decoded_terrain_keys.size())
      return c.reject("TerrainCostBasisCoverageMismatch");
    return c.status();
  }
  auto status = c.sequence("movement_bases", state.writing_bases.rows,
                           [&](auto &a, const MovementBasis *&row) {
                             const auto &key = std::visit(
                                 [](const auto &original) -> const StableId & {
                                   return original.basis_ref;
                                 },
                                 *row);
                             a.field("key", const_cast<StableId &>(key));
                             return a.object("fields", [&](auto &b) {
                               return movement_physical_fields::basis(
                                   b, const_cast<MovementBasis &>(*row),
                                   state.record_selection);
                             });
                           });
  if (!status.ok)
    return status;
  CheckpointVector<const TerrainCostBasis *> terrain;
  for (const auto *row : state.writing_bases.rows)
    if (const auto *t = std::get_if<TerrainCostBasis>(row))
      terrain.push_back(t);
  return c.sequence("terrain_cost_bases", terrain,
                    [&](auto &a, const TerrainCostBasis *&row) {
                      a.field("key", const_cast<StableId &>(row->basis_ref));
                      return a.object("fields", [&](auto &b) {
                        return movement_physical_fields::terrain_cost_basis(
                            b, const_cast<TerrainCostBasis &>(*row),
                            state.record_selection);
                      });
                    });
}
template <class C> Status travel_exact(C &c, State &state) {
  auto &m = state.roots.movement;
  auto regular = [&](auto &a, const StableId &key, auto &row) {
    return simple(a, key, row, state);
  };
  auto checked =
      transport_section_fields::all(c, "movement_bindings", m.spatial_bindings,
                                    state.spatial_bindings, regular);
  if (!checked.ok)
    return checked;
  checked = movement_bases(c, state);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "movement_prefixes", m.prefixes, state.prefixes,
      [&](auto &a, const auto &key, auto &row) {
        return movement_prefix(a, key, row, state);
      });
  if (!checked.ok)
    return checked;
  checked =
      transport_section_fields::all(c, "movement_units", m.units, state.units,
                                    [&](auto &a, const auto &key, auto &row) {
                                      return movement_unit(a, key, row, state);
                                    });
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "credit_dispositions", m.credit_dispositions, state.credits, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "actual_contact_bindings", m.contacts, state.contacts,
      [&](auto &a, const auto &key, auto &row) {
        return contact(a, key, row, state);
      });
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "passage_modifier_adoptions",
                                          m.passage_modifier_adoptions,
                                          state.modifiers, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "cargo_handling_terms", m.cargo_handling.terms, state.handling_terms,
      [&](auto &a, const StableId &key, CargoHandlingTerms &row) {
        auto result = value(a, row, state.record_selection);
        if (!result.ok)
          return result;
        if (row.terms_id != key)
          return a.reject("CargoTermsKeyMismatch", key);
        return payments(a, "actual_start_payments", state,
                        TransportPaymentPhysicalOwner::handling_terms, key);
      });
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "cargo_handling_executions",
                                          m.cargo_handling.executions,
                                          state.handling_executions, regular);
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(
      c, "cargo_handling_work_results", m.cargo_handling.work_results,
      state.handling_work,
      [&](auto &a, const StableId &key, CargoHandlingWorkResult &row) {
        auto result = value(a, row, state.record_selection);
        if (!result.ok)
          return result;
        if (row.result_id != key)
          return a.reject("CargoWorkResultKeyMismatch", key);
        return payments(a, "new_completion_payments", state,
                        TransportPaymentPhysicalOwner::handling_work_result,
                        key);
      });
  if (!checked.ok)
    return checked;
  checked = transport_section_fields::all(c, "cargo_handling_results",
                                          m.cargo_handling.results,
                                          state.handling_results, regular);
  if (!checked.ok)
    return checked;
  return state.reading ? state.adopt_travel_rows() : c.status();
}
} // namespace hanmo::resident::transport_section_fields
