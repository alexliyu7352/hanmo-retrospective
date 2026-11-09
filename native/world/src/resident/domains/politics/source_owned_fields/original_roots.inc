// 原三根及 budget 原行唯一拥有字段。仅由中央拥有字段头在原命名空间包含。
// 不编码、不推断权利、不持第二份 spent / 市场容量 / 外交状态。
#include "resident/domains/politics/source_owned_fields/public_budget.inc"

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketAuthorityState &value,
                  Visitor &&visitor) {
  visitor(value.authorizations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicMarketAuthorityCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTreatyTermsState &value,
                  Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.offers);
  visitor(value.counteroffers);
  visitor(value.mandates);
  visitor(value.presentations);
  visitor(value.replies);
  visitor(value.signatures);
  visitor(value.activations);
  visitor(value.terminations);
  visitor(value.resource_body);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTreatyTermsCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.definitions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidBudgetSelection &value,
                  Visitor &&visitor) {
  visitor(value.execution_ref);
  visitor(value.unit);
  visitor(value.period);
  visitor(value.payer_container_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidResourceSelection &value,
                  Visitor &&visitor) {
  visitor(value.term_id);
  visitor(value.item_routes);
  visitor(value.money_route);
  visitor(value.budgets);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyAidCompensationSelection &value,
      Visitor &&visitor) {
  visitor(value.money_route);
  visitor(value.budget);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidResourceSelection2 &value,
                  Visitor &&visitor) {
  visitor(value.aid);
  visitor(value.compensation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyResourceOwnChoice &value,
                  Visitor &&visitor) {
  (void)value;
  (void)visitor;
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyResourceDeliveredChoice &value,
                  Visitor &&visitor) {
  visitor(value.actual_receipt);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyResourceSignatureSource2 &value,
      Visitor &&visitor) {
  visitor(value.signature);
  visitor(value.actual_aid_resource_proposal);
  visitor(value.resource_knowledge);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyAidResourceProposalSource2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.treaty_id);
  visitor(value.terms_revision);
  visitor(value.terms_source);
  visitor(value.proposer);
  visitor(value.proposed_at);
  visitor(value.selections);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAidRecipientAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.term_id);
  visitor(value.proposal);
  visitor(value.knowledge);
  visitor(value.household_steward_source);
  visitor(value.accepted_at);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyAidAcceptedResourceSource2 &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.term_id);
  visitor(value.activation);
  visitor(value.proposal);
  visitor(value.recipient_consent);
  visitor(value.accepted_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalTreatyResourceBody2 &value,
                  Visitor &&visitor) {
  visitor(value.proposals);
  visitor(value.signatures);
  visitor(value.recipient_acceptances);
  visitor(value.accepted_aid_resources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAuthoredTermAuthority &value,
                  Visitor &&visitor) {
  visitor(value.term_id);
  visitor(value.term_kind);
  visitor(value.command_kind);
  visitor(value.scope);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAuthoredPartyAuthority &value,
                  Visitor &&visitor) {
  visitor(value.party_id);
  visitor(value.ratification_scope);
  visitor(value.terms);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::TreatyAuthoredAuthorityDefinition &value,
      Visitor &&visitor) {
  visitor(value.terms_definition_id);
  visitor(value.parties);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::TreatyAuthorityMappingCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.definitions);
}
