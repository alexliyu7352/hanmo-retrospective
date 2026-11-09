// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 旧/新源全部按同一原拥有图比较；没有第二字段表、JSON中转或全根临时副本。
#include "collateral_personal_preserved_rows.hpp"
#include "collateral_family_origin.hpp"
#include "resident/kernel/source_owned_fields.hpp"
#include <array>
#include <type_traits>
namespace hanmo::resident {
namespace {
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct Sequence : std::false_type {};
template <class T, class A>
struct Sequence<std::vector<T, A>> : std::true_type {};
template <class T, std::size_t N>
struct Sequence<std::array<T, N>> : std::true_type {};
// 只持新头的借用地址；精确Moment仍用原数值叶及其预算，不构造业务值树。
class Equal {
  const StableId &added_;
  std::string_view kind_;

public:
  Status status = common_success();
  Equal(const StableId &added, std::string_view kind)
      : added_(added), kind_(kind) {
  }
  bool same(const CanonicalFraction &a, const CanonicalFraction &b) {
    const auto compared = compare_fractions(a, b);
    if (!compared.ok) {
      status = {false, {}, compared.error};
      return false;
    }
    return compared.value == 0;
  }
  bool same(const WorldMoment &a, const WorldMoment &b) {
    return a.half_day_tick == b.half_day_tick &&
           same(a.sub_half_day, b.sub_half_day);
  }
  template <class T>
  bool same(const OrderedTable<T> &before, const OrderedTable<T> &after) {
    bool may_add = false;
    if constexpr (std::is_same_v<T, CollateralPersonalProposal2>)
      may_add = kind_ == CollateralPersonalProposal2::kind;
    else if constexpr (std::is_same_v<T, CollateralPersonalReply2>)
      may_add = kind_ == CollateralPersonalReply2::kind;
    else if constexpr (std::is_same_v<T, VersionedCollateralRecognitionFact>)
      may_add = kind_ == CollateralPersonalAccepted2::kind;
    if (may_add && before.find(added_))
      return false;
    std::size_t old = 0, skipped = 0;
    for (const auto &row : after.rows()) {
      if (may_add && row.first == added_) {
        ++skipped;
        continue;
      }
      if (old >= before.size() || before.rows()[old].first != row.first ||
          !same(before.rows()[old].second, row.second))
        return false;
      ++old;
    }
    return old == before.size() && skipped == (may_add ? 1u : 0u);
  }
  template <class T> bool same(const T &a, const T &b) {
    if (!status.ok)
      return false;
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> ||
                  std::is_same_v<T, StableId>)
      return a == b;
    else if constexpr (Optional<T>::value)
      return a.has_value() == b.has_value() && (!a || same(*a, *b));
    else if constexpr (Variant<T>::value) {
      if (a.index() != b.index())
        return false;
      return std::visit(
          [&](const auto &left) {
            return same(left, std::get<std::decay_t<decltype(left)>>(b));
          },
          a);
    } else if constexpr (Sequence<T>::value) {
      if (a.size() != b.size())
        return false;
      for (std::size_t i = 0; i < a.size(); ++i)
        if (!same(a[i], b[i]))
          return false;
      return true;
    } else {
      // 所有成员只取唯一copy/retention原visit。没有未知类型返回true的兜底。
      bool equal = true;
      std::size_t first = 0;
      source_owned_fields::visit(a, [&](const auto &left) {
        if (!equal)
          return;
        bool matched = false;
        std::size_t second = 0;
        source_owned_fields::visit(b, [&](const auto &right) {
          if (first == second) {
            if constexpr (std::is_same_v<std::decay_t<decltype(left)>,
                                         std::decay_t<decltype(right)>>)
              matched = same(left, right);
          }
          ++second;
        });
        equal = matched;
        ++first;
      });
      return equal;
    }
  }
};
// 输入Known必须与实际表达唯一来源相等，不能仅同演员或同proposal就放行。
bool same_known(Equal &equal, const FamilyPersonalExpression2 &expression,
                const KnowledgeSourceRef *expected) {
  if (!expected)
    return expression.known_basis_refs.empty();
  if (expression.known_basis_refs.size() != 1)
    return false;
  const auto *actual =
      std::get_if<KnowledgeSourceRef>(&expression.known_basis_refs.front());
  return actual && equal.same(*actual, *expected);
}
bool same_options(Equal &equal, const FamilyPersonalExpression2 &expression,
                  const FamilyPersonalCommunicationOptions2 &options) {
  return expression.communication_audience == options.audience &&
         equal.same(expression.communication_budget, options.budget);
}
Status request_match(Equal &equal, bool matched, const StableId &actor) {
  if (!equal.status.ok)
    return equal.status;
  return matched ? common_success()
                 : common_failure("CollateralPersonalAppliedRequestMismatch",
                                  actor);
}
} // namespace
Status validate_collateral_personal_preserved_rows2(
    const CollateralControlState &before, const CollateralControlState &after,
    const StableId &id, std::string_view kind) {
  if (id.empty() || (kind != CollateralPersonalProposal2::kind &&
                     kind != CollateralPersonalReply2::kind &&
                     kind != CollateralPersonalAccepted2::kind))
    return common_failure("OneCollateralPersonalExpressionPerRequest", id);
  Equal compare(id, kind);
  const bool unchanged = compare.same(before, after);
  if (!compare.status.ok)
    return compare.status;
  return unchanged ? common_success()
                   : common_failure("CollateralPersonalPriorSourceChanged", id);
}
// 映射只列请求实际拥有的字段；原shape/source核其余派生字段，绝不复制规则。
Status validate_collateral_personal_applied_request2(
    const ProposeCollateralRecognition2 &request,
    const CollateralPersonalProposal2 &actual) {
  Equal equal(actual.expression.actor_id, {});
  const bool matched =
      actual.terms.first_actor_id == request.first_actor_id &&
      actual.terms.second_actor_id == actual.expression.actor_id &&
      equal.same(actual.terms.template_ref, request.definition_ref) &&
      // writer重读原祖系形成canonical字段序，沿原来源比较排除仅JSON排列差异。
      collateral_family_origin_equal(actual.terms.ancestry,
                                     request.ancestry_source_ref) &&
      same_known(equal, actual.expression, &request.known_target_ref) &&
      same_options(equal, actual.expression, request.communication_options);
  return request_match(equal, matched, actual.expression.actor_id);
}
Status validate_collateral_personal_applied_request2(
    const RecognizeCollateral2 &request,
    const CollateralPersonalAccepted2 &actual) {
  Equal equal(actual.expression.actor_id, {});
  const auto *proposal =
      std::get_if<FamilyCollateralProposalRef2>(&actual.proposal_ref);
  const bool matched =
      proposal && equal.same(*proposal, request.proposal_ref) &&
      same_known(equal, actual.expression, &request.received_source_ref) &&
      same_options(equal, actual.expression, request.communication_options);
  return request_match(equal, matched, actual.expression.actor_id);
}
Status validate_collateral_personal_applied_request2(
    const RespondCollateralRecognition2 &request,
    const CollateralPersonalReply2 &actual) {
  Equal equal(actual.expression.actor_id, {});
  const auto *proposal =
      std::get_if<FamilyCollateralProposalRef2>(&actual.proposal_ref);
  const bool matched =
      proposal && equal.same(*proposal, request.proposal_ref) &&
      actual.decision == request.decision &&
      same_known(equal, actual.expression,
                 request.received_source_ref ? &*request.received_source_ref
                                             : nullptr) &&
      same_options(equal, actual.expression, request.communication_options);
  return request_match(equal, matched, actual.expression.actor_id);
}
} // namespace hanmo::resident
