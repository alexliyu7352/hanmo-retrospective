#pragma once
#include "treaty_release_sources.hpp"
#include "treaty_resource_termination.hpp"
namespace hanmo::resident {
// 原解除规则及 reply/schema1 不变；只显式绑定实际 activation/termination2
// 原拥有者。
struct TreatyResourceReleaseContext {
  const TreatyResourceTerminationContext &treaties;
  const OrderedTable<TreatyReplyRecord> &replies;
  const TreatyReleaseOriginalRead &originals;
};
class NativeTreatyResourceReleaseIssueRead final
    : public AuthorityIssueSourceRead {
  const TreatyResourceReleaseContext &x_;

public:
  explicit NativeTreatyResourceReleaseIssueRead(
      const TreatyResourceReleaseContext &x)
      : x_(x) {}
  Outcome<WorldMoment> read_organization_authority_now() const override;
  Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &) const override;
};
Outcome<const TreatyReleaseRequestSource *>
read_treaty_resource_release_request(const TreatyResourceReleaseContext &,
                                     const SourceRef &, Integer);
Outcome<const TreatyReleaseAnswerSource *>
read_treaty_resource_release_answer(const TreatyResourceReleaseContext &,
                                    const SourceRef &, Integer);
Status
validate_treaty_resource_release_sources(const TreatyResourceReleaseContext &);
Outcome<OfficialContentRef>
read_treaty_resource_release_request_official_content(
    const TreatyResourceReleaseContext &, const SourceRef &, Integer);
Outcome<TreatyReleaseRequestBody>
read_treaty_resource_release_request_official_body(
    const TreatyResourceReleaseContext &, const OfficialContentRef &);
Outcome<SourceRef> request_treaty_resource_release_in_candidate(
    CommonStore &, OrderedTable<TreatyReplyRecord> &, AuthorityBasisRegistry &,
    const TreatyResourceReleaseContext &, const RequestTreatyReleaseCommand &);
Outcome<SourceRef> answer_treaty_resource_release_in_candidate(
    CommonStore &, OrderedTable<TreatyReplyRecord> &, AuthorityBasisRegistry &,
    const TreatyResourceReleaseContext &, const AnswerTreatyReleaseCommand &);
Outcome<std::optional<TreatyMutualReleaseReady>>
read_treaty_resource_mutual_release_ready(const TreatyResourceReleaseContext &,
                                          const SourceRef &);
Status validate_treaty_resource_mutual_release_consent(
    const TreatyResourceReleaseContext &, const TreatyActivationView &,
    const TreatyMutualReleaseTermination &, const WorldMoment &,
    const SourceRef *);
Outcome<AnswerAndReleaseTreatyResult>
answer_and_release_treaty_resource_in_candidate(
    CommonStore &, OrderedTable<TreatyReplyRecord> &, AuthorityBasisRegistry &,
    OrderedTable<TreatyTerminationSource> &,
    const TreatyResourceReleaseContext &, TreatyTerminationOriginalWrite &,
    const AnswerTreatyReleaseCommand &);
} // namespace hanmo::resident
