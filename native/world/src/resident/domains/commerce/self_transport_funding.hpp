// E20.2/C20.2: the original person's finite transport funding acceptance.
// No money is moved by accepting these terms. Actual fees keep their S1/S5 source.
#pragma once
#include "private_wallet_title.hpp"
#include "resident/domains/travel/transport_authority.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct TaskState;class ProjectionWriter;class CodecVisitor;
enum class SelfTransportFundingContinuity { honor_committed_safe_return };
struct SelfTransportFundingTerms {
 StableId id,principal_actor_id,wallet_id;SourceRef journey_or_plan_ref;
 StableId currency_id="qian";Integer gross_cap=0;WorldMoment valid_from,until;
 std::vector<StableId> allowed_destination_scope;
 SelfTransportFundingContinuity continuity=SelfTransportFundingContinuity::honor_committed_safe_return;
 SourceRef acceptance_ref;Integer revision=1;
};
struct PrivateTransportFundingState {Integer schema_version=1;OrderedTable<SelfTransportFundingTerms> terms;};
// This is a caller's finite proposal, not a payment source or an accepted plan.
// The actual S3 owner verifies carrier/destination against its original offer.
struct SelfTransportFundingProposal {
 StableId principal_actor_id,wallet_id;SubjectRef carrier_subject;
 StableId destination_site_id;Integer gross_cap=0;WorldMoment until;
};
struct SelfTransportFundingQuote {
 SelfTransportFundingProposal proposal;WorldIdentity identity;ReadVersion wallet_version;
 WorldMoment quoted_at;
};
struct SelfTransportFundingContext {
 const CommonStore& common;const PrivateTransportFundingState& state;
 const TaskState& tasks;const PrivateWalletTitleRead& titles;const CurrentMomentRead& clock;
};
struct SelfTransportFundingReadView {
 const SelfTransportFundingTerms* terms=nullptr;
 SourceRef accepted_source;SelfFundedExpenseAuthority authority;
 SubjectRef principal,carrier;StableId destination_site_id;WorldMoment finite_until;
 // These identify the original payment authority, never a claim of money paid.
 std::vector<SourceRef> payment_source_refs;
};
SourceRef self_transport_funding_acceptance_source(const StableId& terms_id);
Status validate_self_transport_funding_terms_shape(const SelfTransportFundingTerms&);
Outcome<SelfTransportFundingQuote> preview_self_transport_funding(
 const SelfTransportFundingContext&,const SelfTransportFundingProposal&);
// Called only after the actual Task acceptance header and immutable scope exist
// in this same candidate. It does not validate the Task's funding tail recursively.
// The original Task owner fills that tail and performs the final two-way check.
Outcome<SelfTransportFundingReadView> accept_self_transport_funding_in_candidate(
 CommonStore&,PrivateTransportFundingState&,const SelfTransportFundingContext&,
 const SelfTransportFundingQuote&,const SourceRef& actual_accepted_transport_plan);
Outcome<SelfTransportFundingReadView> read_self_transport_funding(
 const SelfTransportFundingContext&,const SelfFundedExpenseAuthority&);
Status validate_self_transport_fundings(const SelfTransportFundingContext&);
Outcome<WorldMoment> read_self_transport_funding_moment(
 const SelfTransportFundingContext&,const SourceRef&,Integer source_schema);
Outcome<std::uint64_t> read_self_transport_funding_domain_version(
 const PrivateTransportFundingState&,const DomainReadKey&);
Outcome<PrivateTransportFundingState> decode_private_transport_funding(const BoundaryValue&);
Outcome<BoundaryValue> encode_private_transport_funding(const PrivateTransportFundingState&);
Status visit_private_transport_funding_checkpoint(CodecVisitor&,PrivateTransportFundingState&);
Status emit_private_transport_funding_projection(ProjectionWriter&,const StableId& name,const PrivateTransportFundingState&);
}
