// Include in the original source_owned_fields namespace; no second owner.
template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMemberDeathApplication &value,
                  Visitor &&visitor) {
  visitor(value.actual_death);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::GroupMemberCaptivityApplication &value,
      Visitor &&visitor) {
  visitor(value.actual_capacity_transition);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::GroupMemberDesertionApplication &value,
      Visitor &&visitor) {
  visitor(value.actual_capacity_transition);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::GroupMemberLossHandoffSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.actor_id);
  visitor(value.original_contact);
  visitor(value.actual_casualty);
  visitor(value.casualty_ordinal);
  visitor(value.occurred_at);
  visitor(value.actual_body);
}
