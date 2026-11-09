// M-TREATY offers/counteroffers are the original immutable revision sources.
// These are leaves of the fixed eight-root body, never a second treaty state.
#pragma once
#include "treaty_terms_contract.hpp"
#include "resident/domains/organization/authority_registry.hpp"
namespace hanmo::resident {
struct TreatyOfferSource {
 StableId id,terms_definition_id,offering_party;ActingAuthority issuer;WorldMoment issued_at;
};
struct TreatyCounterofferSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef previous_terms_source;
 StableId terms_definition_id,offering_party;ActingAuthority issuer;WorldMoment issued_at;
};
struct TreatyRevisionView {
 StableId treaty_id;Integer terms_revision=0;SourceRef source;
 const TreatyTermsDefinition* definition=nullptr;const ActingAuthority* issuer=nullptr;
 WorldMoment issued_at;
};
class TreatyRevisionOriginalRead {public:virtual ~TreatyRevisionOriginalRead()=default;
 // The original locked proposal rule decides permitted definition, command,
 // scope and resource exposure. A command cannot supply its own permission list.
 // Historical requests use the actual original row's Common append boundary.
 virtual Outcome<SingleGrantAuthorityRequest> read_treaty_revision_authority_request(
  const TreatyTermsDefinition&,const StableId& party,const ActingAuthority&,
  bool counteroffer,const SourceRef* actual_boundary)const=0;
 // Requires the actual original open negotiation/withdrawal/activation history
 // and the permitted replacement rule; not just two well-shaped term records.
 virtual Status validate_counteroffer_transition(const TreatyRevisionView& previous,
  const TreatyTermsDefinition& replacement,const StableId& party,const ActingAuthority&,
  const WorldMoment&,const SourceRef* actual_boundary)const=0;
};
struct TreatyRevisionContext {
 const CommonStore& common;const OrderedTable<TreatyOfferSource>& offers;
 const OrderedTable<TreatyCounterofferSource>& counteroffers;
 const PoliticalTreatyTermsCatalog& terms_catalog;
 const OrganizationState& organization;const OrganizationCatalog& organization_catalog;
 const AuthorityBasisRegistry& authority_bases;
 const TreatyRevisionOriginalRead& originals;const TreatyTermsOriginalRead& term_sources;
 const OrganizationAuthorityClock& clock;const AuthorityBudgetRead* budgets=nullptr;
};
// No Basis validation here: this raw original issue reader is the non-recursive
// source leg used by the one AuthorityBasis validator.
class NativeTreatyRevisionIssueRead final:public AuthorityIssueSourceRead {
 const TreatyRevisionContext& context_;
public:explicit NativeTreatyRevisionIssueRead(const TreatyRevisionContext& c):context_(c){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
SourceRef treaty_offer_source(const TreatyOfferSource&);
SourceRef treaty_counteroffer_source(const TreatyCounterofferSource&);
Status validate_treaty_offer_source_shape(const TreatyOfferSource&);
Status validate_treaty_counteroffer_source_shape(const TreatyCounterofferSource&);
Outcome<TreatyRevisionView> read_treaty_revision_source(const TreatyRevisionContext&,const SourceRef&,Integer source_schema);
Outcome<TreatyRevisionView> read_current_treaty_revision(const TreatyRevisionContext&,const StableId& treaty_id);
Status validate_treaty_revision_sources(const TreatyRevisionContext&);
struct ProposeTreatyTermsCommand {ActingAuthority authority;StableId terms_definition_id;};
struct CounterTreatyTermsCommand {ActingAuthority authority;StableId treaty_id;Integer expected_terms_revision=0;SourceRef expected_terms_source;StableId terms_definition_id;};
// Internal whole-candidate writes. Original command sequencing supplies replay
// identity for a new proposal; a counteroffer additionally checks its prior cut.
// Neither source signs, activates, delivers, knows or financially accepts terms.
Outcome<SourceRef> propose_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatyOfferSource>&,
 AuthorityBasisRegistry&,const TreatyRevisionContext&,const ProposeTreatyTermsCommand&);
Outcome<SourceRef> counter_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatyCounterofferSource>&,
 AuthorityBasisRegistry&,const TreatyRevisionContext&,const CounterTreatyTermsCommand&);
Status visit_treaty_offer_source(CodecVisitor&,TreatyOfferSource&);
Status visit_treaty_counteroffer_source(CodecVisitor&,TreatyCounterofferSource&);
Status emit_treaty_offer_source_projection(ProjectionWriter&,std::string_view,const TreatyOfferSource&);
Status emit_treaty_counteroffer_source_projection(ProjectionWriter&,std::string_view,const TreatyCounterofferSource&);
Outcome<BoundaryValue> encode_treaty_offer_source(const TreatyOfferSource&);
Outcome<TreatyOfferSource> decode_treaty_offer_source(const BoundaryValue&);
Outcome<BoundaryValue> encode_treaty_counteroffer_source(const TreatyCounterofferSource&);
Outcome<TreatyCounterofferSource> decode_treaty_counteroffer_source(const BoundaryValue&);
}
