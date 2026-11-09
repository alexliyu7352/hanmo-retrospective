#pragma once
#include "movement_boundary_continuation_codec.hpp"
#include "movement_record_codec_fields.hpp"
#include "transport_physical_table.hpp"
namespace hanmo::resident::movement_boundary_continuation_fields {
template <class C>
Status record(C &c, const StableId &key, MovementBoundaryContinuationSource &v,
              const MovementRecordCodecSelection &selection) {
  using movement_record_fields::field;
  field(c, "id", v.id, selection);
  field(c, "prefix_id", v.prefix_id, selection);
  field(c, "closed_unit_ref", v.closed_unit_ref, selection);
  field(c, "new_unit_ref", v.new_unit_ref, selection);
  field(c, "boundary_contact", v.boundary_contact, selection);
  field(c, "field_contact", v.field_contact, selection);
  field(c, "terminal_segment", v.terminal_segment, selection);
  field(c, "occurred_at", v.occurred_at, selection);
  field(c, "held", v.held, selection);
  if (!c.status().ok)
    return c.status();
  if (key.empty() || v.id != key || v.prefix_id.empty() || v.held ||
      v.closed_unit_ref.empty() || v.new_unit_ref.empty() ||
      v.closed_unit_ref == v.new_unit_ref ||
      v.boundary_contact.domain != "travel" ||
      v.boundary_contact.kind != "movement_boundary_contact" ||
      v.boundary_contact.id.empty() || v.field_contact.domain != "military" ||
      v.field_contact.kind != "field_battle_contact" ||
      v.field_contact.id.empty() || v.terminal_segment.domain != "military" ||
      v.terminal_segment.kind != "field_battle_segment" ||
      v.terminal_segment.id.empty())
    return c.reject("InvalidMovementBoundaryContinuationShape", key);
  // Source geometry, Common order, close, exertion and Field adoption must be
  // validated on the complete original roots after all selected sections read.
  return c.status();
}
template <class C>
Status
root(C &c, OrderedTable<MovementBoundaryContinuationSource> &table,
     transport_physical_table::DecodedRows<MovementBoundaryContinuationSource>
         &pending,
     Integer selected, const MovementRecordCodecSelection &selection) {
  if (selected != movement_boundary_continuations_codec_revision)
    return c.reject("UnsupportedMovementBoundaryContinuationPhysicalRevision");
  Integer revision = movement_boundary_continuations_codec_revision;
  c.field("revision", revision);
  if (!c.status().ok)
    return c.status();
  if (revision != movement_boundary_continuations_codec_revision)
    return c.reject("UnsupportedMovementBoundaryContinuationPhysicalRevision");
  return transport_physical_table::all(
      c, "boundary_continuations", table, pending,
      [&](auto &row, const StableId &key,
          MovementBoundaryContinuationSource &v) {
        return record(row, key, v, selection);
      });
}
} // namespace hanmo::resident::movement_boundary_continuation_fields
