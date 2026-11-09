// 同一 OfferResidence 原前置与来源核，避免 preview/writer/cold 三份规则。
#pragma once
#include "estate_rest_history.hpp"
#include "residence_offer.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
namespace hanmo::resident::residence_offer_detail {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
[[noreturn]] inline void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
inline int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
inline bool same_rule(const ActorFoundationRuleRef &a,
                      const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
// 源图必须借同一真实 Common/身体/目录/钟，不能跨 job 拼一个合法世界。
WorldMoment context(const ResidenceOfferContext &);
void requested(const ResidenceOfferContext &,
               std::span<const ResidenceResidentRequest>);
void place(const ResidenceOfferContext &, const StableId &actor,
           const ResidenceTerms &, const WorldMoment &,
           const SourceRef *actual_before);
WorldMoment preflight(const ResidenceOfferContext &, const StableId &host,
                      const OfferResidenceRequest &, const ReadStamp &,
                      const ReadStampAuthority &,
                      const DomainReadVersionSource &);
// 只为已预留 fresh 字段构造；调用者先完成整行计划，不借旧 clone 信用。
StableId fresh(std::string_view);
} // namespace hanmo::resident::residence_offer_detail
