// 原九键及具名叶共用的严格字段表；只做结构，不替代真实来源/发布全验。
#pragma once
#include "political_continuity_contract.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_foundation_codec_fields.hpp"
#include "resident/domains/organization/authority_basis_codec_fields.hpp"
#include "resident/domains/organization/organization_authority_codec_fields.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident::political_continuity_fields {
template <class C> Status basis_ref(C &c, AuthorityBasisRef &v) {
  c.object("issued_source", [&](auto &child) {
    return foundation_codec_fields::source(child, v.issued_source);
  });
  return c.field("source_schema", v.source_schema);
}
// 原MomentOwnerSource仍是具名source及显式schema；不是准入许可。
template <class C> Status owner(C &c, MomentOwnerSource &v) {
  c.object("source_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.source_ref);
  });
  return c.field("source_schema", v.source_schema);
}
// 新枚举用闭合名称；原OfficeAuthoritySource嵌套沿其现有唯一wire字段。
template <class C, class E, std::size_t N>
Status named(C &c, const char *key, E &value, const char *const (&tags)[N]) {

  StableId tag;
  if (!c.reading()) {
    const auto index = static_cast<std::size_t>(value);
    if (index >= N)
      return c.reject("InvalidPoliticalContinuityTag", key);
    tag = tags[index];
  }
  c.field(key, tag);
  if (!c.status().ok)
    return c.status();
  std::size_t i = 0;
  for (; i < N && tag != tags[i]; ++i) {
  }
  if (i == N)
    return c.reject("UnknownPoliticalContinuityTag", key);
  if (c.reading())
    value = static_cast<E>(i);
  return c.status();
}
// 每个实际table行只移动到同一候选；不复制fallible Moment的第二份业务值。
struct DecodedRowTransfer {
  template <class D, class R> Status operator()(D &destination, R &row) const {
    destination.emplace_back(std::move(row));
    return common_success();
  }
};
template <class C, class T, class F>
Status table(C &c, const char *key, OrderedTable<T> &rows, F visit) {
  if constexpr (std::is_same_v<C, CodecVisitor>)
    return common_checkpoint::table(c, key, rows, visit, DecodedRowTransfer{});
  else if constexpr (std::is_same_v<C, ActorFoundationJsonVisitor>)
    return c.table(key, rows, visit, DecodedRowTransfer{});
  else
    return c.table(key, rows, visit);
}
template <class C> Status fields(C &, PoliticalSuccessionElector &);
template <class C> Status fields(C &, PoliticalContinuitySuccessionCase &);
template <class C> Status fields(C &, PoliticalContinuityNomination &);
template <class C> Status fields(C &, PoliticalContinuityNominationAnswer &);
template <class C>
Status fields(C &, PoliticalContinuityNominationWithdrawal &);
template <class C> Status fields(C &, PoliticalContinuityBallotCast &);
template <class C> Status fields(C &, PoliticalContinuityBallotReceived &);
template <class C> Status fields(C &, PoliticalContinuityLeadershipClaim &);
template <class C> Status fields(C &, PoliticalContinuityRecognition &);
template <class C> Status fields(C &, PoliticalContinuityRecognitionReceived &);
template <class C> Status fields(C &, PoliticalContinuityExileCase &);
template <class C> Status fields(C &, PoliticalContinuityViableExile &);
template <class C> Status fields(C &, PoliticalContinuityNoIndependentBasis &);
template <class C> Status fields(C &, PoliticalContinuityExileReview &);
template <class C> Status fields(C &, PoliticalLegacyContinuationBinding &);
template <class C> Status fields(C &, PoliticalContinuityState &);
template <class C> Status fields(C &, PoliticalContinuityNominationDecision &);
template <class C> Status fields(C &, PoliticalContinuityBallot &);
template <class C> Status fields(C &, PoliticalContinuityRecognitionRecord &);
template <class C> Status fields(C &, PoliticalContinuityExileReviewEvidence &);
// 只有本原union的已列tag可构造对应叶，未知tag不回退首项或空成功。
template <std::size_t I = 0, class C, class V, std::size_t N>
Status select(C &c, V &v, const StableId &tag, const char *const (&tags)[N]) {
  if constexpr (I < N) {
    if (tag == tags[I]) {
      if (c.reading()) {
        using T = std::variant_alternative_t<I, V>;
        auto ready = c.template prepare_default_values<T>();
        if (!ready.ok)
          return ready;
        v = T{};
      }
      return fields(c, std::get<I>(v));
    }
    return select<I + 1>(c, v, tag, tags);
  } else
    return c.reject("UnknownPoliticalContinuitySourceKind");
}
template <class C, class V, std::size_t N>
Status named_union(C &c, V &v, const char *const (&tags)[N]) {
  if (v.valueless_by_exception())
    return c.reject("InvalidPoliticalContinuityUnion");

  StableId tag;
  if (!c.reading()) {
    if (v.index() >= N)
      return c.reject("InvalidPoliticalContinuityUnion");
    tag = tags[v.index()];
  }
  c.field("kind", tag);
  if (!c.status().ok)
    return c.status();
  return select(c, v, tag, tags);
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalSuccessionElector &v) {
  c.field("actor_id", v.actor_id);
  foundation_codec_fields::refs(c, "eligibility_sources",
                                v.eligibility_sources);
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuitySuccessionCase &v) {
  c.field("id", v.id);
  c.field("org_id", v.org_id);
  c.field("office_id", v.office_id);
  c.object("original_term", [&](auto &child) {
    return authority_basis_codec_fields::office_source(child, v.original_term);
  });
  c.field("office_epoch", v.office_epoch);
  c.field("rule_id", v.rule_id);
  c.field("rule_version", v.rule_version);
  c.object("trigger_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.trigger_ref);
  });
  c.field("opening_tick", v.opening_tick);
  {
    static constexpr const char *tags[] = {
        "nomination", "voting", "awaiting_arrival", "accessioned", "vacant",
        "disputed",   "closed"};
    named(c, "phase", v.phase, tags);
  }
  c.field("round", v.round);
  foundation_codec_fields::refs(c, "nomination_refs", v.nomination_refs);
  c.sequence("elector_snapshot", v.elector_snapshot,
             [](auto &child, auto &row) { return fields(child, row); });
  foundation_codec_fields::refs(c, "ballot_refs", v.ballot_refs);
  c.optional("interim_authority_ref", v.interim_authority_ref,
             [](auto &child, auto &row) {
               return foundation_codec_fields::source(child, row);
             });
  c.field("deadline_tick", v.deadline_tick);
  c.optional("resolution_ref", v.resolution_ref, [](auto &child, auto &row) {
    return foundation_codec_fields::source(child, row);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityNomination &v) {
  c.field("id", v.id);
  c.field("case_id", v.case_id);
  c.field("candidate_actor_id", v.candidate_actor_id);
  c.field("terms_revision", v.terms_revision);
  organization_authority_codec_fields::value(c, "authority", v.authority,
                                             false);
  c.object("authority_basis_ref", [&](auto &child) {
    return basis_ref(child, v.authority_basis_ref);
  });
  c.object("issued_at", [&](auto &child) {
    return exact_fields::moment(child, v.issued_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityNominationAnswer &v) {
  c.field("id", v.id);
  c.field("actor_id", v.actor_id);
  c.object("nomination_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.nomination_ref);
  });
  c.field("terms_revision", v.terms_revision);
  {
    static constexpr const char *tags[] = {"accept", "refuse"};
    named(c, "decision", v.decision, tags);
  }
  c.object("actual_content_receipt",
           [&](auto &child) { return owner(child, v.actual_content_receipt); });
  c.object("answered_at", [&](auto &child) {
    return exact_fields::moment(child, v.answered_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C>
Status fields(C &c, PoliticalContinuityNominationWithdrawal &v) {
  c.field("id", v.id);
  c.object("nomination_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.nomination_ref);
  });
  organization_authority_codec_fields::value(c, "authority", v.authority,
                                             false);
  c.object("authority_basis_ref", [&](auto &child) {
    return basis_ref(child, v.authority_basis_ref);
  });
  c.object("withdrawn_at", [&](auto &child) {
    return exact_fields::moment(child, v.withdrawn_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityBallotCast &v) {
  c.field("id", v.id);
  c.field("case_id", v.case_id);
  c.field("actor_id", v.actor_id);
  c.field("round", v.round);
  c.object("candidate_acceptance_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.candidate_acceptance_ref);
  });
  c.object("actual_content_receipt",
           [&](auto &child) { return owner(child, v.actual_content_receipt); });
  c.object("cast_at",
           [&](auto &child) { return exact_fields::moment(child, v.cast_at); });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityBallotReceived &v) {
  c.field("id", v.id);
  c.field("counting_actor_id", v.counting_actor_id);
  c.object("ballot_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.ballot_ref);
  });
  c.object("actual_delivery_ref",
           [&](auto &child) { return owner(child, v.actual_delivery_ref); });
  c.object("received_at", [&](auto &child) {
    return exact_fields::moment(child, v.received_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityLeadershipClaim &v) {
  c.field("id", v.id);
  c.field("case_id", v.case_id);
  c.field("actor_id", v.actor_id);
  c.object("seat_control_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.seat_control_ref);
  });
  c.object("command_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.command_ref);
  });
  c.object("candidate_acceptance_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.candidate_acceptance_ref);
  });
  c.object("claimed_at", [&](auto &child) {
    return exact_fields::moment(child, v.claimed_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityRecognition &v) {
  c.field("id", v.id);
  c.field("actor_id", v.actor_id);
  c.object("claim_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.claim_ref);
  });
  {
    static constexpr const char *tags[] = {"accept", "refuse"};
    named(c, "decision", v.decision, tags);
  }
  c.object("actual_content_receipt",
           [&](auto &child) { return owner(child, v.actual_content_receipt); });
  c.object("decided_at", [&](auto &child) {
    return exact_fields::moment(child, v.decided_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C>
Status fields(C &c, PoliticalContinuityRecognitionReceived &v) {
  c.field("id", v.id);
  c.field("counting_actor_id", v.counting_actor_id);
  c.object("recognition_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.recognition_ref);
  });
  c.object("actual_delivery_ref",
           [&](auto &child) { return owner(child, v.actual_delivery_ref); });
  c.object("received_at", [&](auto &child) {
    return exact_fields::moment(child, v.received_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityExileCase &v) {
  c.field("id", v.id);
  c.field("org_id", v.org_id);
  c.object("acting_head_or_agency", [&](auto &child) {
    return foundation_codec_fields::source(child, v.acting_head_or_agency);
  });
  foundation_codec_fields::refs(c, "accepted_followers", v.accepted_followers);
  c.optional("base_or_shelter_right", v.base_or_shelter_right,
             [](auto &child, auto &row) {
               return foundation_codec_fields::source(child, row);
             });
  foundation_codec_fields::refs(c, "mobile_carrier_refs",
                                v.mobile_carrier_refs);
  foundation_codec_fields::refs(c, "supply_obligation_refs",
                                v.supply_obligation_refs);
  c.field("next_review_tick", v.next_review_tick);
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityViableExile &v) {
  c.object("scope_continuity_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.scope_continuity_ref);
  });
  c.object("leadership_or_lawful_agency_ref", [&](auto &child) {
    return foundation_codec_fields::source(child,
                                           v.leadership_or_lawful_agency_ref);
  });
  foundation_codec_fields::refs(
      c, "accepted_political_or_military_execution_refs",
      v.accepted_political_or_military_execution_refs);
  foundation_codec_fields::refs(c, "usable_base_or_mobile_carrier_refs",
                                v.usable_base_or_mobile_carrier_refs);
  foundation_codec_fields::refs(c, "actual_supply_basis_refs",
                                v.actual_supply_basis_refs);
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C>
Status fields(C &c, PoliticalContinuityNoIndependentBasis &v) {
  foundation_codec_fields::refs(c, "disposition_source_refs",
                                v.disposition_source_refs);
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityExileReview &v) {
  c.field("id", v.id);
  c.field("scope_id", v.scope_id);
  c.object("exile_case_ref", [&](auto &child) {
    return foundation_codec_fields::source(child, v.exile_case_ref);
  });
  foundation_codec_fields::refs(c, "reviewed_change_refs",
                                v.reviewed_change_refs);
  c.object("evidence", [&](auto &child) { return fields(child, v.evidence); });
  c.object("reviewed_at", [&](auto &child) {
    return exact_fields::moment(child, v.reviewed_at);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalLegacyContinuationBinding &v) {
  c.field("id", v.id);
  c.field("case_id", v.case_id);
  {
    static constexpr const char *tags[] = {"political_succession",
                                           "successor_round", "term_handover"};
    named(c, "original_kind", v.original_kind, tags);
  }
  c.object("original_source",
           [&](auto &child) { return owner(child, v.original_source); });
  c.object("actual_consumer", [&](auto &child) {
    return foundation_codec_fields::source(child, v.actual_consumer);
  });
  return c.status();
}
// 逐实际成员保持原字段顺序；不在字段读取中生成事实或补未来引用。
template <class C> Status fields(C &c, PoliticalContinuityState &v) {
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedPoliticalContinuitySchema");
  table(c, "succession_cases", v.succession_cases,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "nomination_decisions", v.nomination_decisions,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "ballots", v.ballots,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "recognitions", v.recognitions,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "lifecycle_decisions", v.lifecycle_decisions,
        [](auto &child, auto &row) {
          return foundation_codec_fields::decision(child, row);
        });
  table(c, "exile_cases", v.exile_cases,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "exile_reviews", v.exile_reviews,
        [](auto &child, auto &row) { return fields(child, row); });
  table(c, "legacy_continuation_bindings", v.legacy_continuation_bindings,
        [](auto &child, auto &row) { return fields(child, row); });
  return c.status();
}
template <class C>
Status fields(C &c, PoliticalContinuityNominationDecision &v) {
  static constexpr const char *tags[] = {"nomination", "answer", "withdrawal"};
  return named_union(c, v, tags);
}
template <class C> Status fields(C &c, PoliticalContinuityBallot &v) {
  static constexpr const char *tags[] = {"cast", "received"};
  return named_union(c, v, tags);
}
template <class C>
Status fields(C &c, PoliticalContinuityRecognitionRecord &v) {
  static constexpr const char *tags[] = {"claim", "recognition", "received"};
  return named_union(c, v, tags);
}
template <class C>
Status fields(C &c, PoliticalContinuityExileReviewEvidence &v) {
  static constexpr const char *tags[] = {"viable_independent",
                                         "no_independent_political_basis"};
  return named_union(c, v, tags);
}
} // namespace hanmo::resident::political_continuity_fields
