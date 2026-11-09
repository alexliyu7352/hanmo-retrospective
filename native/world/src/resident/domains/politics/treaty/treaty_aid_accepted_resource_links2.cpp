#include "treaty_resource_source_links2.hpp"
#include <algorithm>
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
std::size_t position(const CommonStore &c, const SourceRef &r) {
  const auto *h = c.state().facts.find(r.id);
  const auto &o = c.state().facts.order();
  auto i = std::find(o.begin(), o.end(), r.id);
  if (!h || h->id != r.id || h->domain != r.domain || h->kind != r.kind ||
      i == o.end())
    fail("TreatyAidAcceptedActualSourceMissing", r.id);
  return static_cast<std::size_t>(i - o.begin());
}
} // namespace
Outcome<const TreatyAidAcceptedResourceSource2 *>
read_treaty_aid_accepted_resource2_links(const TreatyResourceSourceLinks2 &x,
                                         const MomentOwnerSource &s) {
  try {
    if (s.source_schema != 2 || s.source_ref.domain != "politics" ||
        s.source_ref.kind != "treaty_aid_resources_accepted" ||
        !valid_source_ref(s.source_ref))
      fail("TreatyAidAcceptedResourceSourceUnsupported", s.source_ref.id);
    const auto *r = x.accepted_aid_resources.find(s.source_ref.id);
    if (!r || r->id != s.source_ref.id)
      fail("TreatyAidAcceptedResourceSourceMissing", s.source_ref.id);
    take(validate_treaty_aid_accepted_resource_source2_shape(*r));
    const auto &c = x.revisions.common;
    const auto cut = position(c, s.source_ref);
    const auto *p =
        take(read_treaty_aid_resource_proposal2_links(x, r->proposal));
    if (position(c, r->proposal.source_ref) >= cut ||
        position(c, r->recipient_consent.source_ref) >= cut ||
        position(c, r->activation.source_ref) >= cut ||
        take(compare_world_moments(p->proposed_at, r->accepted_at)) > 0 ||
        take(compare_world_moments(
            r->accepted_at,
            take(x.revisions.clock.read_organization_authority_now()))) > 0)
      fail("TreatyAidAcceptedResourceSourceOrderMismatch", r->id);
    const auto revision =
        take(read_treaty_revision_source(x.revisions, p->terms_source, 1));
    if (!revision.definition)
      fail("TreatyAidAcceptedOriginalTermsMissing", r->term_id);
    const TreatyTerm *t = nullptr;
    const TreatyAidResourceSelection2 *selected = nullptr;
    for (const auto &term : revision.definition->terms.terms)
      if (term.term_id == r->term_id) {
        if (t)
          fail("TreatyAidAcceptedTermAmbiguous", r->term_id);
        t = &term;
      }
    for (const auto &choice : p->selections)
      if (choice.aid.term_id == r->term_id) {
        if (selected)
          fail("TreatyAidAcceptedSelectionAmbiguous", r->term_id);
        selected = &choice;
      }
    if (!t || !selected ||
        !std::holds_alternative<TreatyMaterialAidTerm>(t->terms))
      fail("TreatyAidAcceptedTermMismatch", r->term_id);
    take(validate_treaty_aid_resource_selection2_for_term(*selected, *t));
    for (const auto &row : x.accepted_aid_resources.rows())
      if (row.first != r->id && row.second.term_id == r->term_id &&
          same_source_ref(row.second.activation.source_ref,
                          r->activation.source_ref))
        fail("DuplicateTreatyAidAcceptedTerm", r->term_id);
    return {true, r, {}};
  } catch (const Failure &f) {
    return {false, nullptr, f.error};
  }
}
} // namespace hanmo::resident
