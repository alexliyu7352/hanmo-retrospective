// 旁支source2仍由原三表唯一拥有。只枚举实际成员，不按codec字段数估算内存。
// 在source_owned_fields命名空间内引用，所有原类型须已声明。
#include "family_terms_template_ref_values.inc"

#include "family_proposal_ref_values.inc"

// CollateralPersonalTerms2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralPersonalTerms2 &value,
                  Visitor &&visitor) {
  visitor(value.template_ref);
  visitor(value.terms_version);
  visitor(value.first_actor_id);
  visitor(value.second_actor_id);
  visitor(value.ancestry);
  visitor(value.field_order);
}

// CollateralPersonalProposal2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralPersonalProposal2 &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.expression);
  visitor(value.previous_proposal_ref);
  visitor(value.negotiated_reply_ref);
  visitor(value.negotiation_round);
  visitor(value.field_order);
}

// CollateralPersonalReply2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralPersonalReply2 &value,
                  Visitor &&visitor) {
  visitor(value.proposal_ref);
  visitor(value.decision);
  visitor(value.expression);
  visitor(value.counter_proposal_ref);
  visitor(value.field_order);
}

// CollateralPersonalAccepted2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::CollateralPersonalAccepted2 &value,
                  Visitor &&visitor) {
  visitor(value.proposal_ref);
  visitor(value.expression);
  visitor(value.field_order);
}

// VersionedCollateralRecognitionFact 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void
visit(const ::hanmo::resident::VersionedCollateralRecognitionFact &value,
      Visitor &&visitor) {
  visitor(value.payload);
  visitor(value.field_order);
}
