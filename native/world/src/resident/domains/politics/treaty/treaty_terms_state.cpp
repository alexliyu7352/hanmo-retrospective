// 八根只作原Source/Authority/Knowledge/履约真实闭包组合，不新增状态机或伪执行者。
#include "treaty_terms_state.hpp"
namespace hanmo::resident {
Status validate_political_treaty_terms_state(
    const PoliticalTreatyTermsReadContext &x) {
  const auto &s = x.state;
  const auto &r = x.revisions;
  if (s.schema_version != 1 || s.resource_body)
    return common_failure("UnsupportedPoliticalTreatyTermsSchema");
  if (&r.offers != &s.offers || &r.counteroffers != &s.counteroffers ||
      &x.mandates.revisions != &r || &x.mandates.mandates != &s.mandates ||
      &x.recognition.revisions != &r ||
      &x.recognition.presentations != &s.presentations ||
      &x.recognition.replies != &s.replies || &x.signatures.revisions != &r ||
      &x.signatures.signatures != &s.signatures ||
      &x.activations.signatures != &x.signatures ||
      &x.activations.activations != &s.activations ||
      &x.terminations.activations != &x.activations ||
      &x.terminations.terminations != &s.terminations ||
      &x.releases.treaties != &x.terminations ||
      &x.releases.replies != &s.replies)
    return common_failure("PoliticalTreatyTermsCandidateBindingMismatch");
  // 两个知情消费者借同一实际S0/S3与read
  // stamp，不能换成另一世界/另一阶段已知集合。
  if (&x.recognition.social != &x.signatures.social ||
      &x.recognition.communications != &x.signatures.communications ||
      &x.recognition.communication_sources !=
          &x.signatures.communication_sources ||
      &x.recognition.times != &x.signatures.times ||
      &x.recognition.result_stamp != &x.signatures.result_stamp)
    return common_failure("PoliticalTreatyKnowledgeBindingMismatch");
  auto valid = validate_treaty_revision_sources(r);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_mandate_sources(x.mandates);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_recognition_sources(x.recognition);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_release_sources(x.releases);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_signature_sources(x.signatures);
  if (!valid.ok)
    return valid;
  valid = validate_treaty_activation_sources(x.activations);
  if (!valid.ok)
    return valid;
  return validate_treaty_termination_sources(x.terminations);
}
} // namespace hanmo::resident
