#include "treaty_aid_accepted_batch2.hpp"
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
} // namespace
Outcome<std::vector<MomentOwnerSource>>
accept_treaty_aid_resource_set_in_candidate2(
    CommonStore &common, OrderedTable<TreatyAidAcceptedResourceSource2> &rows,
    const TreatyAidAcceptedResourceContext2 &x,
    const MomentOwnerSource &activation,
    const std::vector<TreatyAidActualRecipientInput2> &input) {
  try {
    const auto &links = x.activations.signatures.links;
    if (&common != &links.revisions.common ||
        &rows != &links.accepted_aid_resources || &x.recipients.links != &links)
      fail("TreatyAidAcceptedSetCandidateMismatch");
    auto actual = take(read_original_treaty_resource_activation(
        x.activations, activation.source_ref, activation.source_schema));
    if (!actual.source || !actual.revision.definition)
      fail("TreatyAidAcceptedSetOriginalActivationRequired",
           activation.source_ref.id);
    std::vector<const TreatyTerm *> terms;
    for (const auto &t : actual.revision.definition->terms.terms)
      if (std::holds_alternative<TreatyMaterialAidTerm>(t.terms))
        terms.push_back(&t);
    if (terms.empty() || input.size() != terms.size())
      fail("TreatyAidAcceptedSetTermCoverage", activation.source_ref.id);
    // 先核整组输入与原接受源关联，避免以索引或后生 Schedule 代替同意身份。
    for (std::size_t i = 0; i < terms.size(); ++i) {
      if (input[i].term_id != terms[i]->term_id)
        fail("TreatyAidAcceptedSetTermOrderMismatch", input[i].term_id);
      auto recipient = take(read_treaty_aid_private_recipient_acceptance(
          x.recipients, input[i].actual_recipient_consent));
      if (!recipient.source || !recipient.proposal ||
          recipient.term != terms[i] ||
          recipient.source->term_id != input[i].term_id ||
          recipient.proposal->treaty_id != actual.source->treaty_id ||
          recipient.proposal->terms_revision != actual.source->terms_revision ||
          !same_source_ref(recipient.proposal->terms_source,
                           actual.source->terms_source))
        fail("TreatyAidAcceptedSetRecipientMismatch", input[i].term_id);
    }
    std::vector<MomentOwnerSource> out;
    out.reserve(input.size());
    for (const auto &in : input)
      out.push_back(take(accept_treaty_aid_resources_in_candidate2(
          common, rows, x, activation, in.term_id,
          in.actual_recipient_consent)));
    take(validate_treaty_aid_accepted_resources2(x));
    return {true, std::move(out), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("TreatyAidAcceptedSetResourceExhausted").error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("TreatyAidAcceptedSetResourceExhausted").error};
  }
}
} // namespace hanmo::resident
