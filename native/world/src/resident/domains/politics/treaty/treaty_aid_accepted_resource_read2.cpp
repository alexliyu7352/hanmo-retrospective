#include "treaty_aid_accepted_resource_read2.hpp"
#include "treaty_aid_source_exposure.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *c, const StableId &id = {}) {
  throw Failure{common_failure(c, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool same(const MomentOwnerSource &a, const MomentOwnerSource &b) {
  return a.source_schema == b.source_schema &&
         same_source_ref(a.source_ref, b.source_ref);
}
const TreatyResourceSourceLinks2 &
links(const TreatyAidAcceptedResourceContext2 &x) {
  return x.activations.signatures.links;
}
void context(const TreatyAidAcceptedResourceContext2 &x) {
  if (&x.recipients.links != &links(x) ||
      !x.public_assets.bound_to(links(x).revisions.common))
    fail("TreatyAidAcceptedCandidateMismatch");
}
bool owner(const Owner &a, const Owner &b) {
  return !a.extra && !b.extra && a.kind == b.kind && a.id == b.id;
}
void recipient_title(const TreatyAidAcceptedResourceContext2 &x,
                     const TreatyAidRecipientAcceptanceView &v,
                     const SourceRef &cut) {
  const auto &s = *v.selection;
  const auto &r = x.recipients;
  NativePrivateWalletTitleRead wallets{r.wallets};
  NativePrivateInventoryTitleRead inventories{r.inventories};
  for (const auto &p : s.aid.item_routes)
    if (!owner(take(inventories.read_private_inventory_title_before_source(
                        p.beneficiary_inventory_id, cut))
                   .legal_owner,
               v.recipient_owner))
      fail("TreatyAidAcceptedRecipientInventoryTitleChanged",
           p.beneficiary_inventory_id);
  if (s.aid.money_route &&
      !owner(take(wallets.read_private_wallet_title_before_source(
                      s.aid.money_route->beneficiary_wallet_id, cut))
                 .legal_owner,
             v.recipient_owner))
    fail("TreatyAidAcceptedRecipientWalletTitleChanged",
         s.aid.money_route->beneficiary_wallet_id);
  if (s.compensation &&
      !owner(take(wallets.read_private_wallet_title_before_source(
                      s.compensation->money_route.beneficiary_wallet_id, cut))
                 .legal_owner,
             v.recipient_owner))
    fail("TreatyAidAcceptedCompensationRecipientTitleChanged",
         s.compensation->money_route.beneficiary_wallet_id);
  const auto &aid = std::get<TreatyMaterialAidTerm>(v.term->terms);
  if (aid.beneficiary.kind == SubjectKind::cohort) {
    auto h = take(read_private_cohort_household_before_source(
        links(x).revisions.common, r.economy, r.economy_catalog,
        aid.beneficiary.id, cut));
    if (v.recipient_owner.kind != Owner::Kind::household ||
        h.household_id != v.recipient_owner.id)
      fail("TreatyAidAcceptedBeneficiaryHouseholdChanged", aid.beneficiary.id);
  }
}
} // namespace
Outcome<TreatyAidAcceptedResourceView2>
read_treaty_aid_accepted_resources2(const TreatyAidAcceptedResourceContext2 &x,
                                    const MomentOwnerSource &actual) {
  try {
    context(x);
    const auto &l = links(x);
    const auto &c = l.revisions.common;
    const auto *r = take(read_treaty_aid_accepted_resource2_links(l, actual));
    auto activation = take(read_original_treaty_resource_activation(
        x.activations, r->activation.source_ref, r->activation.source_schema));
    if (!activation.source || !activation.revision.definition ||
        cmp(activation.source->activated_at, r->accepted_at))
      fail("TreatyAidAcceptedActivationMomentMismatch", r->id);
    auto recipient = take(read_treaty_aid_private_recipient_acceptance(
        x.recipients, r->recipient_consent));
    if (!recipient.source || !recipient.proposal || !recipient.term ||
        !recipient.selection || recipient.source->term_id != r->term_id ||
        !same(recipient.source->proposal, r->proposal) ||
        cmp(recipient.source->accepted_at, r->accepted_at) > 0 ||
        recipient.proposal->treaty_id != activation.source->treaty_id ||
        recipient.proposal->terms_revision !=
            activation.source->terms_revision ||
        !same_source_ref(recipient.proposal->terms_source,
                         activation.source->terms_source))
      fail("TreatyAidAcceptedRecipientAssociationMismatch", r->id);
    // 本人同意必须是实际激活的先因；同 M 不允许先激活再补一条同意。
    if (take(read_actual_common_fact_position(
            c, r->recipient_consent.source_ref)) >=
        take(read_actual_common_fact_position(c, r->activation.source_ref)))
      fail("TreatyAidActivationBeforeRecipientConsent", r->id);
    bool exact_term = false;
    for (const auto &t : activation.revision.definition->terms.terms)
      if (&t == recipient.term)
        exact_term = true;
    if (!exact_term)
      fail("TreatyAidAcceptedTermForeignOwner", r->term_id);
    TreatyAidAcceptedResourceView2 out;
    out.source = r;
    out.activation = activation.source;
    out.recipient = recipient;
    for (const auto &sig : activation.source->signature_refs) {
      auto s = take(read_treaty_resource_signature2_links(l, {sig, 2}));
      if (!s->actual_aid_resource_proposal ||
          !same(*s->actual_aid_resource_proposal, r->proposal))
        fail("TreatyAidAcceptedSignatureProposalMismatch", sig.id);
      out.authority_sources.push_back({sig, 2});
    }
    out.authority_sources.push_back(r->recipient_consent);
    const auto &aid = std::get<TreatyMaterialAidTerm>(recipient.term->terms);
    auto exposure = take(read_treaty_aid_source_exposure(c, *recipient.term,
                                                         *recipient.selection));
    for (const auto &resource : exposure.source_resources) {
      auto title = take(
          x.public_assets.read_owner_before_aid_acceptance(l, *r, resource));
      if (title.extra || title.kind != Owner::Kind::organization ||
          title.id != aid.source_party)
        fail("TreatyAidAcceptedSourcePartyTitleChanged", resource.id);
    }
    recipient_title(x, recipient, actual.source_ref);
    return {true, std::move(out), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  } catch (const std::bad_alloc &) {
    return {
        false, {}, common_failure("TreatyAidAcceptedResourceExhausted").error};
  } catch (const std::length_error &) {
    return {
        false, {}, common_failure("TreatyAidAcceptedResourceExhausted").error};
  }
}
Status validate_treaty_aid_accepted_resources2(
    const TreatyAidAcceptedResourceContext2 &x) {
  try {
    context(x);
    const auto &l = links(x);
    for (const auto &row : l.accepted_aid_resources.rows()) {
      if (row.first != row.second.id)
        fail("TreatyAidAcceptedIdentityMismatch", row.first);
      take(read_treaty_aid_accepted_resources2(
          x, {treaty_aid_accepted_resource_source2(row.second), 2}));
    }
    for (const auto &row : l.revisions.common.state().facts.rows())
      if (row.second.domain == "politics" &&
          row.second.kind == "treaty_aid_resources_accepted" &&
          !l.accepted_aid_resources.find(row.first))
        fail("TreatyAidAcceptedOrphanFact", row.first);
    // 即使下游零 binding，也不能通过删除原接受条款来绕过完整覆盖。
    for (const auto &row : x.activations.activations.rows()) {
      auto a = take(read_original_treaty_resource_activation(
          x.activations, treaty_activation_source(row.second), 2));
      for (const auto &t : a.revision.definition->terms.terms)
        if (std::holds_alternative<TreatyMaterialAidTerm>(t.terms)) {
          std::size_t count = 0;
          for (const auto &r : l.accepted_aid_resources.rows())
            if (r.second.term_id == t.term_id &&
                r.second.activation.source_schema == 2 &&
                same_source_ref(r.second.activation.source_ref,
                                treaty_activation_source(*a.source)))
              ++count;
          if (count != 1)
            fail("TreatyAidAcceptedTermCoverage", t.term_id);
        }
    }
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<MomentOwnerSource> accept_treaty_aid_resources_in_candidate2(
    CommonStore &c, OrderedTable<TreatyAidAcceptedResourceSource2> &rows,
    const TreatyAidAcceptedResourceContext2 &x,
    const MomentOwnerSource &activation, const StableId &term,
    const MomentOwnerSource &consent) {
  try {
    context(x);
    const auto &l = links(x);
    if (&c != &l.revisions.common || &rows != &l.accepted_aid_resources)
      fail("TreatyAidAcceptedCandidateMismatch");
    // 精确重放重新核原完整接受源，不捕获今日产权，也不为同条款重造同意或接受源。
    for (const auto &row : rows.rows())
      if (row.second.term_id == term &&
          same(row.second.activation, activation)) {
        if (!same(row.second.recipient_consent, consent))
          fail("TreatyAidAcceptedReplayChanged", row.first);
        const MomentOwnerSource ref{
            treaty_aid_accepted_resource_source2(row.second), 2};
        take(read_treaty_aid_accepted_resources2(x, ref));
        return {true, ref, {}};
      }
    auto a = take(read_original_treaty_resource_activation(
        x.activations, activation.source_ref, activation.source_schema));
    auto recipient = take(
        read_treaty_aid_private_recipient_acceptance(x.recipients, consent));
    const auto now = take(l.revisions.clock.read_organization_authority_now());
    if (!a.source || cmp(a.source->activated_at, now) ||
        recipient.source->term_id != term)
      fail("TreatyAidAcceptedNotAtActualActivation", term);
    TreatyAidAcceptedResourceSource2 row;
    row.id = take(c.next_id("treaty_aid_accepted"));
    row.term_id = term;
    row.activation = activation;
    row.proposal = recipient.source->proposal;
    row.recipient_consent = consent;
    row.accepted_at = now;
    take(validate_treaty_aid_accepted_resource_source2_shape(row));
    const MomentOwnerSource ref{treaty_aid_accepted_resource_source2(row), 2};
    take(c.append_fact(
        {ref.source_ref.id, ref.source_ref.kind, ref.source_ref.domain}));
    if (!rows.insert(ref.source_ref.id, std::move(row)))
      fail("DuplicateTreatyAidAcceptedSource", ref.source_ref.id);
    take(read_treaty_aid_accepted_resources2(x, ref));
    return {true, ref, {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  } catch (const std::bad_alloc &) {
    return {
        false, {}, common_failure("TreatyAidAcceptedResourceExhausted").error};
  } catch (const std::length_error &) {
    return {
        false, {}, common_failure("TreatyAidAcceptedResourceExhausted").error};
  }
}
Status visit_treaty_aid_accepted_original_prefix2(
    const TreatyAidAcceptedResourceContext2 &x, const MomentOwnerSource &ref,
    CommonActualFactPrefixSink &sink) {
  auto accepted = read_treaty_aid_accepted_resources2(x, ref);
  if (!accepted.ok)
    return {false, {}, accepted.error};
  const auto &common = links(x).revisions.common;
  auto prefix = read_actual_common_fact_prefix_before(common, ref.source_ref);
  if (!prefix.ok)
    return {false, {}, prefix.error};
  return visit_actual_common_fact_prefix(common, prefix.value, sink);
}
Outcome<WorldMoment>
NativeTreatyAidAcceptedResourceResultRead2::read_selected_source_moment(
    const MomentOwnerSource &ref) const {
  auto r = read_treaty_aid_accepted_resources2(x_, ref);
  if (!r.ok)
    return {false, {}, r.error};
  return {true, r.value.source->accepted_at, {}};
}
Outcome<ExactOwnerFactRead>
NativeTreatyAidAcceptedResourceResultRead2::read_selected_fact(
    const FactRecord &head) const {
  if (head.domain != "politics" || head.kind != "treaty_aid_resources_accepted")
    return {false,
            {},
            common_failure("TreatyAidAcceptedResultUnsupported", head.id,
                           ErrorKind::unsupported_capability)
                .error};
  auto r = read_treaty_aid_accepted_resources2(
      x_, {{head.domain, head.kind, head.id}, 2});
  if (!r.ok)
    return {false, {}, r.error};
  ExactOwnerFactRead out;
  out.disposition = ExactOwnerFactRead::Disposition::no_dynamic_projection;
  return {true, std::move(out), {}};
}

} // namespace hanmo::resident
