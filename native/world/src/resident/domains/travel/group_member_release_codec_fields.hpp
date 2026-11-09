#pragma once
#include "group_member_release_codec.hpp"
#include "group_member_release_decoded_transfer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "transport_physical_table.hpp"
#include <algorithm>
namespace hanmo::resident::group_member_release_fields {
template <class C>
Status record(C &c, const StableId &key, GroupContactMemberReleaseSource &v,
              const MovementRecordCodecSelection &selection) {
  using movement_record_fields::field;
  field(c, "id", v.id, selection);
  field(c, "journey_id", v.journey_id, selection);
  field(c, "original_contact", v.original_contact, selection);
  field(c, "actual_release_source", v.actual_release_source, selection);
  field(c, "release_schema", v.release_schema, selection);
  field(c, "occurred_at", v.occurred_at, selection);
  field(c, "named_participants", v.named_participants, selection);

  if (!c.status().ok)
    return c.status();
  if (key.empty() || v.id != key || v.journey_id.empty() ||
      v.release_schema != 1 || v.named_participants.empty() ||
      v.original_contact.domain != "travel" ||
      (v.original_contact.kind != "movement_contact" &&
       v.original_contact.kind != "movement_boundary_contact") ||
      v.original_contact.id.empty() ||
      v.actual_release_source.domain != "military" ||
      v.actual_release_source.kind != "field_battle_exit" ||
      v.actual_release_source.id.empty())
    return c.reject("InvalidGroupMemberReleaseOriginalShape", key);
  for (const auto &actor : v.named_participants)
    if (actor.empty() || std::count(v.named_participants.begin(),
                                    v.named_participants.end(), actor) != 1)
      return c.reject("InvalidGroupMemberReleaseParticipants", key);
  // Physical shape is not proof of original entry, location capture or release.
  return c.status();
}
template <class C>
Status
root(C &c, OrderedTable<GroupContactMemberReleaseSource> &table,
     transport_physical_table::DecodedRows<GroupContactMemberReleaseSource>
         &pending,
     Integer selected, const MovementRecordCodecSelection &selection) {
  if (selected != movement_group_member_releases_codec_revision)
    return c.reject("UnsupportedGroupMemberReleasePhysicalRevision");
  Integer revision = movement_group_member_releases_codec_revision;
  c.field("revision", revision);
  if (!c.status().ok)
    return c.status();
  if (revision != movement_group_member_releases_codec_revision)
    return c.reject("UnsupportedGroupMemberReleasePhysicalRevision");
  return transport_physical_table::all(
      c, "contact_member_releases", table, pending,
      [&](auto &row, const StableId &key, GroupContactMemberReleaseSource &v) {
        return record(row, key, v, selection);
      });
}
} // namespace hanmo::resident::group_member_release_fields
