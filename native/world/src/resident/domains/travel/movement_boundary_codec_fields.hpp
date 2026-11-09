#pragma once
#include "movement_boundary_codec.hpp"
#include "movement_record_codec_fields.hpp"
#include "transport_physical_table.hpp"
namespace hanmo::resident::movement_boundary_fields {
template <class C>
Status record(C &c, const StableId &key, MovementBoundaryContact &v,
              const MovementRecordCodecSelection &selection) {
  using movement_record_fields::field;
  field(c, "id", v.id, selection);
  field(c, "prefix_id", v.prefix_id, selection);
  field(c, "original_field_source", v.original_field_source, selection);
  field(c, "field_hash", v.field_hash, selection);
  field(c, "prediction_span", v.prediction_span, selection);
  field(c, "entry_fraction", v.entry_fraction, selection);
  field(c, "occurred_at", v.occurred_at, selection);
  field(c, "motion_source", v.motion_source, selection);
  field(c, "actual_anchor", v.actual_anchor, selection);
  if (!c.status().ok)
    return c.status();
  if (key.empty() || v.id != key || v.prefix_id.empty() ||
      v.field_hash.empty() || v.original_field_source.domain != "military" ||
      v.original_field_source.kind != "field_battle_session" ||
      v.original_field_source.id.empty() || v.motion_source.unit_ref.empty() ||
      v.motion_source.basis_ref.empty() ||
      !std::holds_alternative<RunningMovementMap>(
          v.motion_source.original_mapping))
    return c.reject("InvalidMovementBoundaryOriginalShape", key);
  // Source geometry, Common order, close, exertion and Field adoption must be
  // validated on the complete original roots after all selected sections read.
  return c.status();
}
template <class C>
Status
root(C &c, OrderedTable<MovementBoundaryContact> &table,
     transport_physical_table::DecodedRows<MovementBoundaryContact> &pending,
     Integer selected, const MovementRecordCodecSelection &selection) {
  if (selected != movement_boundary_contacts_codec_revision)
    return c.reject("UnsupportedMovementBoundaryPhysicalRevision");
  Integer revision = movement_boundary_contacts_codec_revision;
  c.field("revision", revision);
  if (!c.status().ok)
    return c.status();
  if (revision != movement_boundary_contacts_codec_revision)
    return c.reject("UnsupportedMovementBoundaryPhysicalRevision");
  return transport_physical_table::all(
      c, "boundary_contacts", table, pending,
      [&](auto &row, const StableId &key, MovementBoundaryContact &v) {
        return record(row, key, v, selection);
      });
}
} // namespace hanmo::resident::movement_boundary_fields
