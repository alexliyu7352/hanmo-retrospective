#include "political_continuity_codec.hpp"
#include <type_traits>
namespace hanmo::resident {
namespace {
// 结构拒绝不表示读过真实来源；业务有效性必须由后继同World完整门证明。
bool source(const SourceRef &value) { return valid_source_ref(value); }
bool source(const MomentOwnerSource &value) {
  return value.source_schema > 0 && source(value.source_ref);
}
bool sources(const std::vector<SourceRef> &values) {
  for (const auto &value : values)
    if (!source(value))
      return false;
  return true;
}
bool moment(const WorldMoment &value) { return encode_world_moment(value).ok; }
bool answer(PoliticalContinuityAnswer value) {
  return value == PoliticalContinuityAnswer::accept ||
         value == PoliticalContinuityAnswer::refuse;
}
// 各具名叶只验自身已有字段，无跨owner余额/历史/当前资格猜测。
template <class T> bool row_shape(const T &value) {
#define ORIGINAL_ID(member)                                                    \
  if constexpr (requires { value.member; })                                    \
    if (value.member.empty())                                                  \
  return false
  ORIGINAL_ID(id);
  ORIGINAL_ID(org_id);
  ORIGINAL_ID(office_id);
  ORIGINAL_ID(rule_id);
  ORIGINAL_ID(case_id);
  ORIGINAL_ID(actor_id);
  ORIGINAL_ID(candidate_actor_id);
  ORIGINAL_ID(counting_actor_id);
  ORIGINAL_ID(scope_id);
#undef ORIGINAL_ID
#define ORIGINAL_SOURCE(member)                                                \
  if constexpr (requires { value.member; })                                    \
    if (!source(value.member))                                                 \
  return false
  ORIGINAL_SOURCE(trigger_ref);
  ORIGINAL_SOURCE(nomination_ref);
  ORIGINAL_SOURCE(candidate_acceptance_ref);
  ORIGINAL_SOURCE(actual_content_receipt);
  ORIGINAL_SOURCE(actual_delivery_ref);
  ORIGINAL_SOURCE(ballot_ref);
  ORIGINAL_SOURCE(seat_control_ref);
  ORIGINAL_SOURCE(command_ref);
  ORIGINAL_SOURCE(claim_ref);
  ORIGINAL_SOURCE(recognition_ref);
  ORIGINAL_SOURCE(acting_head_or_agency);
  ORIGINAL_SOURCE(scope_continuity_ref);
  ORIGINAL_SOURCE(leadership_or_lawful_agency_ref);
  ORIGINAL_SOURCE(exile_case_ref);
  ORIGINAL_SOURCE(original_source);
  ORIGINAL_SOURCE(actual_consumer);
#undef ORIGINAL_SOURCE
#define ORIGINAL_SOURCES(member)                                               \
  if constexpr (requires { value.member; })                                    \
    if (!sources(value.member))                                                \
  return false
  ORIGINAL_SOURCES(eligibility_sources);
  ORIGINAL_SOURCES(nomination_refs);
  ORIGINAL_SOURCES(ballot_refs);
  ORIGINAL_SOURCES(accepted_followers);
  ORIGINAL_SOURCES(mobile_carrier_refs);
  ORIGINAL_SOURCES(supply_obligation_refs);
  ORIGINAL_SOURCES(accepted_political_or_military_execution_refs);
  ORIGINAL_SOURCES(usable_base_or_mobile_carrier_refs);
  ORIGINAL_SOURCES(actual_supply_basis_refs);
  ORIGINAL_SOURCES(disposition_source_refs);
  ORIGINAL_SOURCES(reviewed_change_refs);
#undef ORIGINAL_SOURCES
#define ORIGINAL_MOMENT(member)                                                \
  if constexpr (requires { value.member; })                                    \
    if (!moment(value.member))                                                 \
  return false
  ORIGINAL_MOMENT(issued_at);
  ORIGINAL_MOMENT(answered_at);
  ORIGINAL_MOMENT(withdrawn_at);
  ORIGINAL_MOMENT(cast_at);
  ORIGINAL_MOMENT(received_at);
  ORIGINAL_MOMENT(claimed_at);
  ORIGINAL_MOMENT(decided_at);
  ORIGINAL_MOMENT(reviewed_at);
#undef ORIGINAL_MOMENT
  if constexpr (requires { value.round; })
    if (value.round < 1 || value.round > 2)
      return false;
  if constexpr (requires { value.terms_revision; })
    if (value.terms_revision < 1)
      return false;
  if constexpr (requires { value.decision; })
    if (!answer(value.decision))
      return false;
  if constexpr (requires { value.authority; })
    if (value.authority.actor_id.empty() || value.authority.office_id.empty() ||
        value.authority.organization_id.empty() ||
        value.authority.authority_epoch < 0)
      return false;
  if constexpr (requires { value.authority_basis_ref; })
    if (value.authority_basis_ref.source_schema != 1 ||
        !source(value.authority_basis_ref.issued_source))
      return false;
  if constexpr (std::is_same_v<T, PoliticalContinuitySuccessionCase>) {
    if (value.office_epoch < 0 || value.rule_version < 1 ||
        value.opening_tick < 0 || value.deadline_tick < value.opening_tick ||
        static_cast<unsigned>(value.phase) >
            static_cast<unsigned>(PoliticalSuccessionPhase::closed) ||
        (value.interim_authority_ref &&
         !source(*value.interim_authority_ref)) ||
        (value.resolution_ref && !source(*value.resolution_ref)) ||
        value.original_term.valueless_by_exception())
      return false;
    const bool term = std::visit(
        [&](const auto &original) {
          using O = std::decay_t<decltype(original)>;
          if constexpr (std::is_same_v<O, AuthoredOfficeAuthoritySource>)
            return !original.catalog_identity.content_digest.empty() &&
                   original.office_id == value.office_id &&
                   original.initial_epoch == value.office_epoch;
          else
            return !original.fact_id.empty();
        },
        value.original_term);
    if (!term)
      return false;
    for (std::size_t i = 0; i < value.elector_snapshot.size(); ++i) {
      if (!row_shape(value.elector_snapshot[i]))
        return false;
      // 原每真人一票的快照身份约束，不能以多个Office复制同一选民。
      for (std::size_t j = 0; j < i; ++j)
        if (value.elector_snapshot[j].actor_id ==
            value.elector_snapshot[i].actor_id)
          return false;
    }
  }
  if constexpr (std::is_same_v<T, PoliticalContinuityExileCase>)
    if (value.next_review_tick < 0 ||
        (value.base_or_shelter_right && !source(*value.base_or_shelter_right)))
      return false;
  if constexpr (std::is_same_v<T, PoliticalContinuityViableExile>)
    if (value.accepted_political_or_military_execution_refs.empty() ||
        value.usable_base_or_mobile_carrier_refs.empty() ||
        value.actual_supply_basis_refs.empty())
      return false;
  if constexpr (std::is_same_v<T, PoliticalContinuityNoIndependentBasis>)
    if (value.disposition_source_refs.empty())
      return false;
  if constexpr (std::is_same_v<T, PoliticalContinuityExileReview>) {
    if (value.evidence.valueless_by_exception())
      return false;
    return std::visit([](const auto &evidence) { return row_shape(evidence); },
                      value.evidence);
  }
  if constexpr (std::is_same_v<T, PoliticalLegacyContinuationBinding>)
    if (static_cast<unsigned>(value.original_kind) >
        static_cast<unsigned>(PoliticalLegacyContinuationKind::term_handover))
      return false;
  return true;
}
// 当前表只能保存本型原行；variant必须选实际类型，不能空缺仍解为默认正常结果。
template <class T> bool row(const StableId &key, const T &value) {
  if constexpr (requires { value.valueless_by_exception(); }) {
    if (value.valueless_by_exception())
      return false;
    return std::visit([&](const auto &actual) { return row(key, actual); },
                      value);
  } else if constexpr (std::is_same_v<T, FoundationIntentSource>)
    return key == value.id && validate_foundation_intent_source_shape(value).ok;
  else if constexpr (std::is_same_v<T, FoundationSupportSource>)
    return key == value.id &&
           validate_foundation_support_source_shape(value).ok;
  else
    return !key.empty() && key == value.id && row_shape(value);
}
template <class T> Status table(const OrderedTable<T> &values) {
  for (const auto &entry : values.rows())
    if (!row(entry.first, entry.second))
      return common_failure("InvalidPoliticalContinuityRow", entry.first);
  return common_success();
}
} // namespace
Status
validate_political_continuity_shape(const PoliticalContinuityState &value) {
  if (value.schema_version != 1)
    return common_failure("UnsupportedPoliticalContinuitySchema");
  Status valid = table(value.succession_cases);
  if (valid.ok)
    valid = table(value.nomination_decisions);
  if (valid.ok)
    valid = table(value.ballots);
  if (valid.ok)
    valid = table(value.recognitions);
  if (valid.ok)
    valid = table(value.lifecycle_decisions);
  if (valid.ok)
    valid = table(value.exile_cases);
  if (valid.ok)
    valid = table(value.exile_reviews);
  if (valid.ok)
    valid = table(value.legacy_continuation_bindings);
  return valid;
}
} // namespace hanmo::resident
