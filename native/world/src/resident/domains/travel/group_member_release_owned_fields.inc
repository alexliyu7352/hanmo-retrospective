// Include in the original source_owned_fields namespace; no second owner.
template <class Visitor>
inline void
visit(const ::hanmo::resident::GroupContactMemberReleaseSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.original_contact);
  visitor(value.actual_release_source);
  visitor(value.release_schema);
  visitor(value.occurred_at);
  visitor(value.named_participants);
}
