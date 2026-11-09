// 原 source/1 的有限拥有字段；没有新身体账或派生历史。
template <class Visitor>
inline void
visit(const ::hanmo::resident::CharacterBattleReceiptSourceV1 &value,
      Visitor &&visitor) {
  visitor(value.original_receipt);
  visitor(value.source_schema);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::CharacterTreatmentActionSourceV1 &value,
      Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.completion_ordinal);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterInjuryApplicationV1 &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.original_source);
  visitor(value.progress.before.injury);
  visitor(value.progress.before.points);
  visitor(value.progress.after.injury);
  visitor(value.progress.after.points);
  visitor(value.progress.offered_points);
  visitor(value.progress.used_points);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::CharacterInjurySourcesStateV1 &value,
                  Visitor &&visitor) {
  visitor(value.applications);
}
