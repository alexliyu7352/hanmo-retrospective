// 原个人表达八型的唯一递归成员；不带Care表或ready门。
#pragma once
#include "family_expression_values.inc"
// KnowledgeSourceRef 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::KnowledgeSourceRef &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source_id);
  visitor(value.source_subject);
}
// FamilyActualContactRef 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyActualContactRef &value,
                  Visitor &&visitor) {
  visitor(value.original_contact_ref);
}
// FamilyActualDisclosureRef 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyActualDisclosureRef &value,
                  Visitor &&visitor) {
  visitor(value.original_disclosure_ref);
}
// FamilyExplicitPlayerDecision 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyExplicitPlayerDecision &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}
// FamilyFiniteNpcDecision 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyFiniteNpcDecision &value,
                  Visitor &&visitor) {
  visitor(value.family_policy_ref);
  visitor(value.rule_branch);
  visitor(value.source_input_refs);
}
// FamilyNoFeeCommunication 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyNoFeeCommunication &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}
// FamilyOwnWalletCommunicationCap 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FamilyOwnWalletCommunicationCap &value,
      Visitor &&visitor) {
  visitor(value.max_money);
}
// FamilyPersonalExpression2 的所有实际成员进入同一copy/retention拥有图。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyPersonalExpression2 &value,
                  Visitor &&visitor) {
  visitor(value.source_schema);
  visitor(value.actor_id);
  visitor(value.expressed_at);
  visitor(value.committed_revision);
  visitor(value.command_binding);
  visitor(value.route_basis);
  visitor(value.known_basis_refs);
  visitor(value.decision_basis);
  visitor(value.communication_budget);
  visitor(value.communication_audience);
  visitor(value.field_order);
}
