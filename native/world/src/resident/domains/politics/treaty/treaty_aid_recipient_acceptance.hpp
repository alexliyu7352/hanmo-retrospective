// An actor's actual acceptance of one immutable Aid proposal/term. Knowledge,
// title and Steward authority are independent original checks, not substitutes.
#pragma once
#include "resident/domains/actor_foundation/condition_application_history.hpp"
#include "resident/domains/commerce/private_wallet_title.hpp"
#include "resident/domains/family/household_aid_steward_history_v1.hpp"
#include "resident/domains/local_economy/private_cohort_household_origin.hpp"
#include "resident/domains/local_economy/private_inventory_title.hpp"
#include "treaty_aid_proposal_official.hpp"
#include "treaty_aid_recipient_acceptance_model.hpp"
namespace hanmo::resident {
class ActivityRead;
struct TreatyAidRecipientAcceptanceContext {
  const TreatyResourceSourceLinks2 &links;
  const OrderedTable<TreatyAidRecipientAcceptance> &acceptances;
  const CommonCatalog &shared;
  const ActivityRead &activity;
  const SocialState &social;
  const CommunicationLinksState &communications;
  const CommunicationSourceRead &communication_sources;
  const ExactResultTimeRead &times;
  const ExactResultReadStamp &result_stamp;
  const PrivateWalletTitleContext &wallets;
  const PrivateInventoryTitleContext &inventories;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const HouseholdAidStewardHistorySourcesV1 &stewards;
  const CharacterConditionSources &condition;
};
struct TreatyAidRecipientAcceptanceView {
  const TreatyAidRecipientAcceptance *source = nullptr;
  const TreatyAidResourceProposalSource2 *proposal = nullptr;
  const TreatyTerm *term = nullptr;
  const TreatyAidResourceSelection2 *selection = nullptr;
  Owner recipient_owner;
};
SourceRef
treaty_aid_recipient_acceptance_source(const TreatyAidRecipientAcceptance &);
Status validate_treaty_aid_recipient_acceptance_shape(
    const TreatyAidRecipientAcceptance &);
// Source-local read: never calls signature/Basis/S5 usage/whole activation.
// The private branches are concrete. Organization title/authority and named
// Actor-to-household history require their real original owners, not defaults.
Outcome<TreatyAidRecipientAcceptanceView>
read_treaty_aid_private_recipient_acceptance(
    const TreatyAidRecipientAcceptanceContext &, const MomentOwnerSource &);
Status validate_treaty_aid_private_recipient_acceptances(
    const TreatyAidRecipientAcceptanceContext &);
// Owning retention must retain the complete actual prefix as well as the row,
// proposal, explicit receipt and original Steward source.
Status visit_treaty_aid_private_recipient_original_prefix(
    const TreatyAidRecipientAcceptanceContext &, const MomentOwnerSource &,
    CommonActualFactPrefixSink &);
Outcome<MomentOwnerSource> accept_treaty_aid_private_recipient_in_candidate(
    CommonStore &, OrderedTable<TreatyAidRecipientAcceptance> &,
    const TreatyAidRecipientAcceptanceContext &, const StableId &actor,
    const MomentOwnerSource &proposal, const StableId &term,
    const TreatyResourceKnowledge &,
    const std::optional<SourceRef> &original_steward);
// Private consent is not a public Report or implicit notification. The actual
// selected result owner still handles its header and original exact Moment.
class NativeTreatyAidPrivateRecipientResultRead final
    : public ExactResultOwnerRead {
  const TreatyAidRecipientAcceptanceContext &x_;

public:
  explicit NativeTreatyAidPrivateRecipientResultRead(
      const TreatyAidRecipientAcceptanceContext &x)
      : x_(x) {}
  Outcome<ExactOwnerFactRead>
  read_selected_fact(const FactRecord &) const override;
  Outcome<WorldMoment>
  read_selected_source_moment(const MomentOwnerSource &) const override;
};
Status visit_treaty_aid_recipient_acceptance(CodecVisitor &,
                                             TreatyAidRecipientAcceptance &);
Status emit_treaty_aid_recipient_acceptance_projection(
    ProjectionWriter &, std::string_view, const TreatyAidRecipientAcceptance &);
Outcome<BoundaryValue>
encode_treaty_aid_recipient_acceptance(const TreatyAidRecipientAcceptance &);
Outcome<TreatyAidRecipientAcceptance>
decode_treaty_aid_recipient_acceptance(const BoundaryValue &);
} // namespace hanmo::resident
