#pragma once
#include "group_member_loss_handoff_codec.hpp"
#include "group_member_loss_handoff_decoded_transfer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "transport_physical_table.hpp"
namespace hanmo::resident::group_member_loss_fields {
template <class C>
Status body(C &c, GroupMemberLossHandoffSource &v,
            const MovementRecordCodecSelection &selection) {
  Integer tag = Integer(v.actual_body.index());
  c.field("variant", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("InvalidGroupMemberLossBodyVariant");
  if (c.reading()) {
    if (tag == 0)
      v.actual_body = GroupMemberDeathApplication{};
    else if (tag == 1)
      v.actual_body = GroupMemberCaptivityApplication{};
    else
      v.actual_body = GroupMemberDesertionApplication{};
  }
  if (auto *d = std::get_if<GroupMemberDeathApplication>(&v.actual_body)) {
    c.object("actual_death", [&](auto &a) {
      a.field("domain", d->actual_death.domain);
      a.field("kind", d->actual_death.kind);
      a.field("id", d->actual_death.id);
      if (a.status().ok && (d->actual_death.domain != "family_life" ||
                            d->actual_death.kind != "family_death" ||
                            d->actual_death.id.empty()))
        return a.reject("InvalidGroupMemberLossDeathRef");
      return a.status();
    });
  } else {
    // 两个独立有限 tag 共用原 SourceRef 字段，不把逃散改记为被俘。
    auto &source =
        tag == 1 ? std::get<GroupMemberCaptivityApplication>(v.actual_body)
                       .actual_capacity_transition
                 : std::get<GroupMemberDesertionApplication>(v.actual_body)
                       .actual_capacity_transition;
    movement_record_fields::field(c, "actual_capacity_transition", source,
                                  selection);
    if (c.status().ok &&
        (source.domain != "actor_foundation" ||
         source.kind != "capacity_transition" || source.id.empty()))
      return c.reject("InvalidGroupMemberLossCapacityRef");
  }
  return c.status();
}
template <class C>
Status record(C &c, const StableId &key, GroupMemberLossHandoffSource &v,
              const MovementRecordCodecSelection &selection) {
  using movement_record_fields::field;
  field(c, "id", v.id, selection);
  field(c, "journey_id", v.journey_id, selection);
  field(c, "actor_id", v.actor_id, selection);
  field(c, "original_contact", v.original_contact, selection);
  field(c, "actual_casualty", v.actual_casualty, selection);
  field(c, "casualty_ordinal", v.casualty_ordinal, selection);
  field(c, "occurred_at", v.occurred_at, selection);
  c.object("actual_body", [&](auto &a) { return body(a, v, selection); });
  if (!c.status().ok)
    return c.status();
  if (key.empty() || v.id != key || v.journey_id.empty() ||
      v.actor_id.empty() || v.casualty_ordinal < 0 ||
      v.original_contact.domain != "travel" ||
      (v.original_contact.kind != "movement_contact" &&
       v.original_contact.kind != "movement_boundary_contact") ||
      v.original_contact.id.empty() || v.actual_casualty.domain != "military" ||
      v.actual_casualty.kind != "military_casualties" ||
      v.actual_casualty.id.empty())
    return c.reject("InvalidGroupMemberLossOriginalShape", key);
  // This physical shape is not the original body/capture/membership proof.
  return c.status();
}
template <class C>
Status root(C &c, OrderedTable<GroupMemberLossHandoffSource> &table,
            transport_physical_table::DecodedRows<GroupMemberLossHandoffSource>
                &pending,
            Integer selected, const MovementRecordCodecSelection &selection) {
  if (selected != movement_group_member_losses_codec_revision)
    return c.reject("UnsupportedGroupMemberLossPhysicalRevision");
  Integer revision = movement_group_member_losses_codec_revision;
  c.field("revision", revision);
  if (!c.status().ok)
    return c.status();
  if (revision != movement_group_member_losses_codec_revision)
    return c.reject("UnsupportedGroupMemberLossPhysicalRevision");
  return transport_physical_table::all(
      c, "member_loss_handoffs", table, pending,
      [&](auto &row, const StableId &key, GroupMemberLossHandoffSource &v) {
        return record(row, key, v, selection);
      });
}
} // namespace hanmo::resident::group_member_loss_fields
