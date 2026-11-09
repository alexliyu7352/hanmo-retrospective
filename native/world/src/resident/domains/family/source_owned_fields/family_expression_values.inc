// 两原 Family route 递归拥有图，供 S3/Care 同用，不拉入完整 Care 根。
// 原 SubjectRef/SourceRef 仍调用全局已有实现；没有 Knowledge 或决定字段。
#pragma once
// AuthoredProfileController 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::AuthoredProfileController &value,
                  Visitor &&visitor) {
  visitor(value.scenario_id);
  visitor(value.actor_id);
}
// CreatedProfileController 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::CreatedProfileController &value,
                  Visitor &&visitor) {
  visitor(value.creation_fact_id);
  visitor(value.actor_id);
}
// SuccessionProfileController 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::SuccessionProfileController &value,
                  Visitor &&visitor) {
  visitor(value.succession_fact_id);
  visitor(value.actor_id);
}
// LegacyProfileController 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyProfileController &value,
                  Visitor &&visitor) {
  visitor(value.activation_fact_id);
  visitor(value.consent_fact_id);
  visitor(value.actor_id);
}
// ContinuationProfileController 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::ContinuationProfileController &value,
                  Visitor &&visitor) {
  visitor(value.activation_id);
  visitor(value.pending_id);
  visitor(value.actor_id);
  visitor(value.control_epoch);
}
// FamilyPlayerExpressionRoute 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyPlayerExpressionRoute &value,
                  Visitor &&visitor) {
  visitor(value.original_control_source_ref);
}
// FamilyNpcExpressionRoute 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyNpcExpressionRoute &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.registration_epoch);
  visitor(value.decision_seq);
  visitor(value.policy_ref);
}
