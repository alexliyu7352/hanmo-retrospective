#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_trade_terminal_codec.hpp"
#include "growth_capability_application_codec.hpp"
#include "growth_life_definition_read.hpp"
#include "growth_state_projection.hpp"
#include "long_rest_period.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/storage/common_projection.hpp"
#include "study_adjustment_time2_codec.hpp"
#include "study_service_codec.hpp"
// 只在冷装载、导出与有限响应边界解析旧合同；常驻行动、事实和题面无动态字典兜底。
#include "growth_challenge_definition_codec.hpp"
#include "growth_effect_projection.hpp"
#include "growth_fixed_service_codec.hpp"
#include "growth_fixed_service_state_codec.hpp"
#include "growth_internal.hpp"
#include "growth_study_teaching_requests_codec.hpp"
#include "growth_teaching_requests_codec.hpp"
#include "resident/domains/family/innate_study_source.hpp"
#include "resident/domains/family/succession_personal_assets.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include <algorithm>
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace {
using namespace boundary;
using interaction_codec::decode_context;
using interaction_codec::encode_context;
void add(BoundaryValue &v, const StableId &key, BoundaryValue value) {
  std::get<BoundaryValue::Dict>(v.data).emplace_back(key, std::move(value));
}
BoundaryValue scalar(const StableId &x) {
  return BoundaryValue{x};
}
BoundaryValue scalar(Integer x) {
  return BoundaryValue{x};
}
BoundaryValue scalar(bool x) {
  return BoundaryValue{x};
}

Status emit_training_fields(ProjectionWriter &out, const TrainingTerms &t) {
#define F(x) out.field(#x, t.x)
  F(skill_id);
  F(student_id);
  F(teacher_id);
  F(place_id);
  F(duration);
  F(mastery_before);
  F(mastery_after);
  F(student_fatigue);
  F(teacher_fatigue);
  F(fee);
  F(payer_wallet_id);
  F(recipient_wallet_id);
#undef F
  if (t.teaching_agreement)
    out.object("teaching_agreement", [&](auto &w) {
      return emit_growth_teaching_agreement_fields(w, *t.teaching_agreement);
    });
  return out.order_fields(t.field_order);
}
Status emit_service_fields(ProjectionWriter &out, const ServiceTerms &t) {
  out.field("duration", t.duration);
  out.field("cost", t.cost);
  out.field("wage", t.wage);
  out.field("hp_gain", t.hp_gain);
  out.field("fatigue_delta", t.fatigue_delta);
  out.field("wallet_id", t.wallet_id);
  return out.order_fields(t.field_order);
}
Status emit_segment_fields(ProjectionWriter &out, const StudySegment &s,
                           bool exact_periods = false,
                           bool exact_command_cuts = false,
                           bool capability_chain = false, GrowthContributionWireVersion wire = GrowthContributionWireVersion::original) {
  out.field("ordinal", s.ordinal);
  out.field("settled_tick", s.settled_tick);
  out.field("xp_before", s.xp_before);
  out.field("performance_bp", s.performance_bp);
  out.field("xp_gained", s.xp_gained);
  out.field("performance_pending", s.performance_pending);
  if (s.adjustment) {
    const auto &a = *s.adjustment;
    if (exact_periods) {
      auto status = emit_study_adjustment_time2(
          out, "adjustment", a, exact_command_cuts, capability_chain, wire);
      if (!status.ok)
        return status;
    } else {
      if (a.exact_binding)
        return out.reject("StudyAdjustmentTime2PhysicalSelectionRequired",
                          "study/adjustment");
      out.object("adjustment", a.field_order, [&](auto &w) {
        w.field("settled_tick", a.settled_tick);
        w.field("xp_before", a.xp_before);
        w.field("performance_bp", a.performance_bp);
        return w.field("xp_gained", a.xp_gained);
      });
    }
  }
  out.order_fields(s.field_order);
  if (capability_chain) {
    if (bool(s.capability_application) != (s.xp_gained > 0))
      return out.reject("StudyLessonCapabilityApplicationMismatch");
    if (s.capability_application)
      return emit_growth_capability_application(out, "capability_application",
                                                *s.capability_application, wire);
    return out.null("capability_application");
  }
  if (s.capability_application)
    return out.reject("GrowthCapabilityPhysicalSelectionRequired");
  return out.status();
}
Status emit_study_fields(ProjectionWriter &out, const StudyTerms &t,
                         bool permanent = true) {
  out.field("skill_id", t.skill_id);
  out.field("mode", t.mode);
  out.field("student_id", t.student_id);
  out.field("teacher_id", t.teacher_id);
  out.object("context", [&](auto &w) {
    return emit_interaction_context_fields(w, t.context);
  });
#define F(x) out.field(#x, t.x)
  F(duration);
  F(xp_before);
  F(teacher_xp);
  F(student_fatigue);
  F(teacher_fatigue);
  F(xp_limit);
  F(fee);
  F(payer_wallet_id);
  F(recipient_wallet_id);
#undef F
  if (t.challenge)
    out.object(t.challenge->field, [&](auto &w) {
      return growth::emit_growth_frozen_fields(w, *t.challenge);
    });
  if (t.settlement_policy)
    out.field("settlement_policy", *t.settlement_policy);
  if (t.study_teaching_agreement)
    out.object("study_teaching_agreement", [&](auto &w) {
      return emit_growth_study_teaching_agreement_fields(
          w, *t.study_teaching_agreement);
    });
  if (t.innate_source) {
    if (permanent)
      out.object("innate_source", [&](auto &w) {
        return emit_innate_study_source_fields(w, *t.innate_source);
      });
    else {
      out.field("learning_bonus_bp", t.innate_source->bonus_bp);
      auto order = t.field_order;
      order.erase(std::remove(order.begin(), order.end(), "innate_source"),
                  order.end());
      order.push_back("learning_bonus_bp");
      return out.order_fields(order);
    }
  }
  return out.order_fields(t.field_order);
}

TrainingTerms decode_training(const BoundaryValue &v, const StableId &path,
                              bool teaching_requests) {
  fields(v,
         {"skill_id", "student_id", "teacher_id", "place_id", "duration",
          "mastery_before", "mastery_after", "student_fatigue",
          "teacher_fatigue", "fee", "payer_wallet_id", "recipient_wallet_id"},
         {"teaching_agreement"}, path);
  if (v.find("teaching_agreement") && !teaching_requests)
    fail("TeachingRequestsCapabilityRequired", path + "/teaching_agreement");
  TrainingTerms t;
  t.field_order = order(v);
  t.skill_id = get_string(v, "skill_id", path);
  t.student_id = get_string(v, "student_id", path);
  t.teacher_id = get_string(v, "teacher_id", path);
  t.place_id = get_string(v, "place_id", path);
  t.duration = get_int(v, "duration", path);
  t.mastery_before = get_int(v, "mastery_before", path);
  t.mastery_after = get_int(v, "mastery_after", path);
  t.student_fatigue = get_int(v, "student_fatigue", path);
  t.teacher_fatigue = get_int(v, "teacher_fatigue", path);
  t.fee = get_int(v, "fee", path);
  t.payer_wallet_id = get_string(v, "payer_wallet_id", path);
  t.recipient_wallet_id = get_string(v, "recipient_wallet_id", path);
  if (const auto *value = v.find("teaching_agreement")) {
    auto decoded = decode_growth_teaching_agreement(*value);
    if (!decoded.ok) {
      DecodeFailure failure{decoded.error.original.code,
                            decoded.error.original.path};
      failure.error = decoded.error;
      throw failure;
    }
    t.teaching_agreement = std::move(decoded.value);
  }
  return t;
}
BoundaryValue encode_training(const TrainingTerms &t) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status =
      out.object("", [&](auto &w) { return emit_training_fields(w, t); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
ServiceTerms decode_service(const BoundaryValue &v, const StableId &path,
                            bool author_definition = false) {
  if (author_definition)
    fields(
        v,
        {"duration", "cost", "wage", "hp_gain", "fatigue_delta", "wallet_id"},
        {"definition_ref"}, path);
  else
    fields(
        v,
        {"duration", "cost", "wage", "hp_gain", "fatigue_delta", "wallet_id"},
        {}, path);
  ServiceTerms t;
  t.field_order = order(v);
  if (author_definition)
    t.field_order.erase(std::remove(t.field_order.begin(), t.field_order.end(),
                                    "definition_ref"),
                        t.field_order.end());
  t.duration = get_int(v, "duration", path);
  t.cost = get_int(v, "cost", path);
  t.wage = get_int(v, "wage", path);
  t.hp_gain = get_int(v, "hp_gain", path);
  t.fatigue_delta = get_int(v, "fatigue_delta", path);
  t.wallet_id = get_string(v, "wallet_id", path);
  return t;
}
BoundaryValue encode_service(const ServiceTerms &t) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status =
      out.object("", [&](auto &w) { return emit_service_fields(w, t); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
// 教学与迟答补差分开读取，拒绝未知字段；合法性由同一领域证据门校验。
StudySegment decode_segment(const BoundaryValue &v, const StableId &path,
                            bool exact_periods = false,
                            bool exact_command_cuts = false,
                            bool capability_chain = false, GrowthContributionWireVersion wire = GrowthContributionWireVersion::original) {
  if (capability_chain)
    fields(v,
           {"ordinal", "settled_tick", "xp_before", "performance_bp",
            "xp_gained", "performance_pending", "capability_application"},
           {"adjustment"}, path);
  else
    fields(v,
           {"ordinal", "settled_tick", "xp_before", "performance_bp",
            "xp_gained", "performance_pending"},
           {"adjustment"}, path);
  StudySegment s;
  s.ordinal = get_int(v, "ordinal", path);
  s.settled_tick = get_int(v, "settled_tick", path);
  s.xp_before = get_int(v, "xp_before", path);
  s.performance_bp = get_int(v, "performance_bp", path);
  s.xp_gained = get_int(v, "xp_gained", path);
  s.performance_pending = get_bool(v, "performance_pending", path);
  s.field_order = order(v);
  if (auto *a = v.find("adjustment")) {
    if (exact_periods) {
      auto decoded = decode_study_adjustment_time2(*a, exact_command_cuts,
                                                   capability_chain, wire);
      if (!decoded.ok) {
        DecodeFailure failure{decoded.error.original.code,
                              decoded.error.original.path};
        failure.error = decoded.error;
        throw failure;
      }
      s.adjustment = std::move(decoded.value);
    } else {
      fields(*a, {"settled_tick", "xp_before", "performance_bp", "xp_gained"},
             {}, path);
      s.adjustment = StudyPerformanceAdjustment{
          get_int(*a, "settled_tick", path), get_int(*a, "xp_before", path),
          get_int(*a, "performance_bp", path), get_int(*a, "xp_gained", path),
          order(*a)};
    }
  }
  if (capability_chain) {
    if (s.field_order.empty() ||
        s.field_order.back() != "capability_application")
      fail("GrowthCapabilityTailOrderMismatch", path);
    s.field_order.pop_back();
    const auto &value = field(v, "capability_application", path);
    if (!value.as<std::monostate>()) {
      auto decoded = decode_growth_capability_application(value, wire);
      if (!decoded.ok) {
        DecodeFailure failure(decoded.error.original.code,
                              decoded.error.original.path);
        failure.error = decoded.error;
        throw failure;
      }
      s.capability_application = std::move(decoded.value);
    }
    if (bool(s.capability_application) != (s.xp_gained > 0))
      fail("StudyLessonCapabilityApplicationMismatch", path);
  }
  return s;
}
// 分段冷边界不重算奖励，保存不可变基础收据及至多一次的迟答差额。
BoundaryValue encode_segment(const StudySegment &s) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status =
      out.object("", [&](auto &w) { return emit_segment_fields(w, s); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
StudyTerms decode_study(const BoundaryValue &v, const StableId &path,
                        bool study_teaching_requests, bool innate_learning) {
  fields(v,
         {"skill_id", "mode", "student_id", "teacher_id", "context", "duration",
          "xp_before", "teacher_xp", "student_fatigue", "teacher_fatigue",
          "xp_limit", "fee", "payer_wallet_id", "recipient_wallet_id"},
         {"etiquette", "challenge", "settlement_policy",
          "study_teaching_agreement", "innate_source"},
         path);
  if (v.find("innate_source") && !innate_learning)
    fail("InnateStudyCapabilityRequired", path);
  if (innate_learning) {
    auto unique = validate_succession_json_unique(v);
    if (!unique.ok) {
      DecodeFailure failure{"InvalidInnateStudySource", path};
      failure.error = unique.error;
      throw failure;
    }
  }
  if (v.find("study_teaching_agreement") && !study_teaching_requests)
    fail("StudyTeachingCapabilityRequired", path + "/study_teaching_agreement");
  StudyTerms t;
  t.field_order = order(v);
  t.skill_id = get_string(v, "skill_id", path);
  t.mode = get_string(v, "mode", path);
  t.student_id = get_string(v, "student_id", path);
  t.teacher_id = get_string(v, "teacher_id", path);
  t.payer_wallet_id = get_string(v, "payer_wallet_id", path);
  t.recipient_wallet_id = get_string(v, "recipient_wallet_id", path);
  t.duration = get_int(v, "duration", path);
  t.xp_before = get_int(v, "xp_before", path);
  t.teacher_xp = get_int(v, "teacher_xp", path);
  t.student_fatigue = get_int(v, "student_fatigue", path);
  t.teacher_fatigue = get_int(v, "teacher_fatigue", path);
  t.xp_limit = get_int(v, "xp_limit", path);
  t.fee = get_int(v, "fee", path);
  t.context = decode_context(field(v, "context", path), path);
  if (v.find("settlement_policy"))
    t.settlement_policy = get_string(v, "settlement_policy", path);
  if (const auto *source = v.find("innate_source")) {
    auto read = decode_innate_study_source(*source);
    if (!read.ok) {
      DecodeFailure failure{"InvalidInnateStudySource", path};
      failure.error = read.error;
      throw failure;
    }
    t.innate_source = std::move(read.value);
  }
  if (v.find("etiquette") && v.find("challenge"))
    fail("InvalidStudyChallengeTerms", path);
  for (auto key : {"etiquette", "challenge"})
    if (auto *p = v.find(key))
      t.challenge = growth::decode_frozen(*p, key, path);
  if (const auto *source = v.find("study_teaching_agreement")) {
    auto decoded = decode_growth_study_teaching_agreement(*source);
    if (!decoded.ok) {
      DecodeFailure failure{"InvalidStudyTeachingAgreement", path};
      failure.error = decoded.error;
      throw failure;
    }
    t.study_teaching_agreement = std::move(decoded.value);
  }
  if (bool(t.innate_source) !=
      (t.settlement_policy && *t.settlement_policy == "innate_per_lesson_v1"))
    fail("InnateStudyPolicySourceMismatch", path);
  return t;
}
BoundaryValue encode_study(const StudyTerms &t, bool permanent = true) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status = out.object(
      "", [&](auto &w) { return emit_study_fields(w, t, permanent); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
TradeTerms decode_trade(const BoundaryValue &v, const StableId &path) {
  fields(v,
         {"ok", "shop_id", "item_id", "quantity", "side", "payer_wallet_id",
          "owner", "total", "market_revision", "valid_tick", "context",
          "duration"},
         {"bargain_quote_id", "discount_bp", "bargain_basis_sha256"}, path);
  if (!get_bool(v, "ok", path) || get_int(v, "duration", path) != 1)
    fail("InvalidTradeReceipt", path);
  TradeTerms t;
  t.field_order = order(v);
  t.shop_id = get_string(v, "shop_id", path);
  t.item_id = get_string(v, "item_id", path);
  t.side = get_string(v, "side", path);
  t.payer_wallet_id = get_string(v, "payer_wallet_id", path);
  t.quantity = get_int(v, "quantity", path);
  t.total = get_int(v, "total", path);
  t.market_revision = get_int(v, "market_revision", path);
  t.valid_tick = get_int(v, "valid_tick", path);
  t.owner = decode_owner(field(v, "owner", path), path);
  t.context = decode_context(field(v, "context", path), path);
  if (auto *x = v.find("bargain_quote_id"))
    t.bargain_quote_id = string(*x, path);
  if (auto *x = v.find("bargain_basis_sha256"))
    t.bargain_basis_sha256 = string(*x, path);
  if (auto *x = v.find("discount_bp"))
    t.discount_bp = integer(*x, path);
  return t;
}
BoundaryValue encode_trade(const TradeTerms &t) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status = out.object(
      "", [&](auto &w) { return emit_growth_trade_terms_fields(w, t); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
} // namespace
namespace growth {
BoundaryValue encode_trade_terms(const TradeTerms &t) {
  return encode_trade(t);
}
TradeTerms decode_trade_terms(const BoundaryValue &v, const StableId &p) {
  return decode_trade(v, p);
}
} // namespace growth
static Outcome<GrowthCatalog>
decode_growth_catalog_impl(const BoundaryValue &data,
                           const CommonCatalog &common, bool regional_life,
                           bool innate_learning, const CatalogIdentity *family2,
                           const CatalogIdentity *life_loops) {
  try {
    using namespace boundary;
    GrowthCatalog result;
    result.innate_learning = innate_learning;
    result.regional_life = regional_life;
    if (auto *capabilities = data.find("required_capabilities")) {
      auto values = strings(*capabilities, "required_capabilities");
      result.cultivation = std::find(values.begin(), values.end(),
                                     "cultivation") != values.end();
      result.economy =
          std::find(values.begin(), values.end(), "economy") != values.end();
      result.family =
          std::find(values.begin(), values.end(), "family") != values.end();
    }
    for (const auto &row : object(field(data, "roles", "data"), "roles")) {
      GrowthRole role;
      if (auto *lessons = row.second.find("lessons"))
        for (const auto &value : array(*lessons, row.first)) {
          GrowthLesson l;
          l.skill_id = get_string(value, "skill_id", row.first);
          l.wallet_id = get_string(value, "wallet_id", row.first);
          l.required_trust = get_int(value, "required_trust", row.first);
          l.study_ticks = get_int(value, "study_ticks", row.first);
          l.fee = get_int(value, "fee", row.first);
          if (!common.personal_skill_kinds.find(l.skill_id) ||
              !common.required_wallets.find(l.wallet_id) || l.study_ticks < 1 ||
              l.study_ticks > 30 || l.fee < 0 || l.fee > 10000)
            fail("InvalidLesson", row.first);
          role.lessons.push_back(std::move(l));
        }
      if (auto *courses = row.second.find("skill_courses"))
        for (const auto &value : array(*courses, row.first)) {
          if (life_loops)
            fields(value, {"skill_id", "mode", "wallet_id"},
                   {"etiquette_challenge_id", "challenge", "settlement_policy",
                    "definition_ref"},
                   row.first);
          else
            fields(value, {"skill_id", "mode", "wallet_id"},
                   {"etiquette_challenge_id", "challenge", "settlement_policy"},
                   row.first);
          GrowthCourse c;
          if (const auto *definition = value.find("definition_ref")) {
            std::size_t count = 0;
            for (const auto &field : object(value, row.first))
              if (field.first == "definition_ref")
                ++count;
            if (count != 1)
              fail("DuplicateGrowthLifeDefinitionField", row.first);
            auto decoded = decode_growth_life_definition_ref1(*definition);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            c.definition_ref = std::move(decoded.value);
          }
          c.skill_id = get_string(value, "skill_id", row.first);
          c.mode = get_string(value, "mode", row.first);
          c.wallet_id = get_string(value, "wallet_id", row.first);
          if (value.find("settlement_policy")) {
            c.settlement_policy =
                get_string(value, "settlement_policy", row.first);
            if ((*c.settlement_policy != "per_lesson_v1" &&
                 (!innate_learning ||
                  *c.settlement_policy != "innate_per_lesson_v1")) ||
                !result.cultivation)
              fail("InvalidStudySettlementPolicy", row.first);
            result.study_segments = true;
          }
          if (!growth_skill_defined(c.skill_id, common.extended_skills) ||
              (c.mode != "teacher" && c.mode != "facility") ||
              !common.required_wallets.find(c.wallet_id))
            fail("InvalidStudyCourse", row.first);
          if (auto *id = value.find("etiquette_challenge_id")) {
            if (value.find("challenge"))
              fail("InvalidStudyChallengeCourse", row.first);
            c.challenge = StudyChallengeSource{
                "etiquette", "etiquette", string(*id, row.first), {}};
          }
          if (auto *source = value.find("challenge")) {
            StudyChallengeSource a;
            a.field = "challenge";
            a.kind = get_string(*source, "kind", row.first);
            a.id = get_string(*source, "id", row.first);
            if (a.kind == "bargain") {
              auto &ctx = field(*source, "context", row.first);
              fields(ctx, {"budget", "quantity", "relation_discount_bp"}, {},
                     row.first);
              a.context = StudyChallengeContext{
                  get_int(ctx, "budget", row.first),
                  get_int(ctx, "quantity", row.first),
                  get_int(ctx, "relation_discount_bp", row.first),
                  order(ctx),
                  {}};
            } else if (a.kind != "etiquette")
              fail("InvalidStudyChallengeCourse", row.first);
            c.challenge = std::move(a);
          }
          for (const auto &old : role.courses)
            if (old.skill_id == c.skill_id)
              fail("InvalidStudyCourse", row.first);
          role.courses.push_back(std::move(c));
        }
      if (auto *activities = row.second.find("activities"))
        for (const auto &value : object(*activities, row.first)) {
          auto terms =
              decode_service(value.second, row.first, life_loops != nullptr);
          if (terms.duration < 1 || terms.duration > 30 || terms.cost < 0 ||
              terms.wage < 0 || !common.required_wallets.find(terms.wallet_id))
            fail("InvalidServiceTerms", row.first);
          GrowthActivityDefinition authored(std::move(terms));
          if (const auto *definition = value.second.find("definition_ref")) {
            std::size_t count = 0;
            for (const auto &field : object(value.second, row.first))
              if (field.first == "definition_ref")
                ++count;
            if (count != 1)
              fail("DuplicateGrowthLifeDefinitionField", row.first);
            if (value.first != "rest")
              fail("GrowthLifeRestDefinitionRequired", value.first);
            auto decoded = decode_growth_life_definition_ref1(*definition);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            authored.definition_ref = std::move(decoded.value);
          }
          if (!role.activities.insert(value.first, std::move(authored)) &&
              life_loops)
            fail("DuplicateGrowthActivityDefinition", value.first);
        }
      result.roles.insert(row.first, std::move(role));
    }
    if (auto *shops = data.find("shops"))
      for (const auto &row : object(*shops, "shops")) {
        GrowthShop s;
        s.id = get_string(row.second, "id", row.first);
        s.place_id = get_string(row.second, "place_id", row.first);
        s.inventory_id = get_string(row.second, "inventory_id", row.first);
        s.wallet_id = get_string(row.second, "wallet_id", row.first);
        s.representative_roles = strings(
            field(row.second, "representative_roles", row.first), row.first);
        if (s.id != row.first || !common.places.find(s.place_id) ||
            !common.required_inventories.find(s.inventory_id) ||
            !common.required_wallets.find(s.wallet_id))
          fail("InvalidShop", row.first);
        for (const auto &id : s.representative_roles)
          if (!common.roles.find(id))
            fail("InvalidShopRepresentative", id);
        if (auto *policy = row.second.find("bargaining")) {
          fields(*policy, {"challenge_id", "condition_sources"}, {}, row.first);
          BargainPolicy b;
          b.field_order = order(*policy);
          b.challenge_id = get_string(*policy, "challenge_id", row.first);
          for (const auto &source : array(
                   field(*policy, "condition_sources", row.first), row.first)) {
            BargainConditionSource x;
            x.field_order = order(source);
            x.condition_id = get_string(source, "condition_id", row.first);
            x.source = get_string(source, "source", row.first);
            if (auto *value = source.find("value"))
              x.value = integer(*value, row.first);
            if (x.source == "quantity_at_most")
              fields(source, {"condition_id", "source", "value"}, {},
                     row.first);
            else
              fields(source, {"condition_id", "source"}, {}, row.first);
            b.condition_sources.push_back(std::move(x));
          }
          s.bargaining = std::move(b);
        }
        result.shops.insert(row.first, std::move(s));
      }
    for (const auto &row : object(field(data, "items", "data"), "items"))
      if (get_string(row.second, "kind", row.first) == "commodity")
        result.tradeable_items.push_back(row.first);
    for (auto kind : {"etiquette", "bargain"}) {
      StableId key = StableId(kind) + "_challenges";
      if (auto *table = data.find(key))
        for (const auto &row : object(*table, key)) {
          auto decoded = family2 && std::string_view(kind) == "etiquette"
                             ? decode_family_etiquette_definition2(
                                   row.second, common, *family2)
                             : decode_original_growth_challenge_definition(
                                   row.second, kind, common);
          if (!decoded.ok)
            throw boundary::DecodeFailure(decoded.error);
          auto d = std::move(decoded.value);
          if (d.id != row.first)
            fail("InvalidChallengeDefinition", row.first);
          (d.kind == "etiquette" ? result.etiquette : result.bargain)
              .insert(row.first, std::move(d));
        }
    }
    for (const auto &row : object(field(data, "places", "data"), "places")) {
      StableId name = "场所未记";
      if (auto *value = row.second.find("display_name"))
        name = string(*value, row.first);
      else if (auto *key = row.second.find("name_key")) {
        if (auto *value = common.texts.find(string(*key, row.first)))
          name = *value;
      }
      result.place_names.insert(row.first, std::move(name));
    }
    if (auto *estates = data.find("estates"))
      for (const auto &row : object(*estates, "estates"))
        result.estate_ids.push_back(row.first);
    if (auto *organizations = data.find("organizations"))
      for (const auto &row : object(*organizations, "organizations")) {
        StableId name = row.first;
        if (auto *x = row.second.find("display_name"))
          name = string(*x, row.first);
        else if (auto *x = row.second.find("name_key")) {
          if (auto *text = common.texts.find(string(*x, row.first)))
            name = *text;
        }
        result.organization_names.insert(row.first, std::move(name));
      }
    for (auto &row : result.shops.rows())
      if (row.second.bargaining) {
        auto &policy = *row.second.bargaining;
        auto *definition = result.bargain.find(policy.challenge_id);
        if (!definition)
          fail("MissingBargainDefinition", policy.challenge_id);
        policy.source_sha256 =
            growth::bargain_source_digest(*definition, policy, common);
      }
    for (const auto &row : result.roles.rows())
      for (const auto &course : row.second.courses)
        if (course.challenge) {
          const auto &x = *course.challenge;
          if ((x.kind == "etiquette" && course.skill_id != "rites" &&
               course.skill_id != "debate") ||
              (x.kind == "bargain" && course.skill_id != "commerce" &&
               course.skill_id != "negotiation"))
            fail("InvalidStudyChallengeCourse", row.first);
          if (!(x.kind == "etiquette" ? result.etiquette : result.bargain)
                   .find(x.id))
            fail("MissingStudyChallenge", x.id);
        }
    if (life_loops) {
      auto valid = validate_growth_life_definition_rows1(result);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    return {true, std::move(result), {}};
  } catch (const DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
// 旧公开loader保持原字段接受域，新作者版本不能按字段存在自动迁移。
Outcome<GrowthCatalog> decode_growth_catalog(const BoundaryValue &data,
                                             const CommonCatalog &common,
                                             bool regional_life,
                                             bool innate_learning) {
  return decode_growth_catalog_impl(data, common, regional_life,
                                    innate_learning, nullptr, nullptr);
}
// 仅中央真正选择自治2的冷作者入口；本函数不安装World或授予运行时来源许可。
Outcome<GrowthCatalog> decode_growth_family_etiquette_catalog2(
    const BoundaryValue &data, const CommonCatalog &common,
    const CatalogIdentity &identity, bool regional_life, bool innate_learning) {
  auto selected = validate_growth_family_etiquette_selection2(identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  const bool loops = std::any_of(
      identity.module_versions.begin(), identity.module_versions.end(),
      [](const auto &row) { return row.first == "director_life_loops"; });
  if (loops) {
    auto valid = validate_growth_life_definition_selection1(identity);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return decode_growth_catalog_impl(data, common, regional_life,
                                    innate_learning, &identity,
                                    loops ? &identity : nullptr);
}
// 只供原LC01实际选择的作者安装；不把带字段的旧作者自动升级。
Outcome<GrowthCatalog> decode_growth_life_catalog1(
    const BoundaryValue &data, const CommonCatalog &common,
    const CatalogIdentity &identity, bool regional_life, bool innate_learning) {
  auto selected = validate_growth_life_definition_selection1(identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  const bool family2 = std::any_of(
      identity.module_versions.begin(), identity.module_versions.end(),
      [](const auto &row) {
        return row.first == "director_family_autonomy" && row.second == "2";
      });
  if (family2) {
    selected = validate_growth_family_etiquette_selection2(identity);
    if (!selected.ok)
      return {false, {}, selected.error};
  }
  return decode_growth_catalog_impl(data, common, regional_life,
                                    innate_learning,
                                    family2 ? &identity : nullptr, &identity);
}
// 命令只承认当前领域固定参数；不存在把不明参数存入热extras的退路。
Outcome<GrowthCommand> decode_growth_command(const StableId &kind,
                                             const BoundaryValue &params) {
  try {
    using namespace boundary;
    GrowthCommand c;
    c.kind = kind;
    c.field_order = order(params);
    if (kind == "trade_bargain_open") {
      fields(params, {"trade", "mode"}, {}, kind);
      auto inner = decode_growth_command("trade", field(params, "trade", kind));
      if (!inner.ok)
        return inner;
      auto order = c.field_order;
      c = std::move(inner.value);
      c.trade_field_order = c.field_order;
      c.field_order = std::move(order);
      c.kind = kind;
      c.challenge_mode = get_string(params, "mode", kind);
      if (c.bargain_quote_id || c.quoted_total || c.market_revision ||
          c.trade_field_order.size() != 6 || !c.context ||
          c.context->field_order.size() != 3)
        fail("InvalidBargainTrade", kind);
    } else if (kind == "trade_bargain_choose" || kind == "trade_bargain_exit") {
      if (kind == "trade_bargain_choose") {
        fields(params, {"quote_id", "step_id", "option_id"}, {}, kind);
        c.step_id = get_string(params, "step_id", kind);
        c.option_id = get_string(params, "option_id", kind);
      } else
        fields(params, {"quote_id"}, {}, kind);
      c.bargain_quote_id = get_string(params, "quote_id", kind);
    } else if (kind == "long_rest") {
      fields(params, {"estate_id", "duration"}, {}, kind);
      c.estate_id = get_string(params, "estate_id", kind);
      c.duration = get_int(params, "duration", kind);
    } else if (kind == "rest_cancel") {
      fields(params, {}, {}, kind);
    } else if (kind == "study_skill" || kind == "teach_study") {
      fields(params, {"skill_id", "ticks", "context"},
             kind == "study_skill"
                 ? std::initializer_list<const char *>{"challenge_mode"}
                 : std::initializer_list<const char *>{},
             kind);
      c.skill_id = get_string(params, "skill_id", kind);
      c.ticks = get_int(params, "ticks", kind);
      c.context = decode_context(field(params, "context", kind), kind);
      if (auto *mode = params.find("challenge_mode"))
        c.challenge_mode = string(*mode, kind);
    } else if (kind == "study_cancel" || kind == "study_pause" ||
               kind == "study_resume") {
      fields(params, {"action_id"}, {}, kind);
      c.action_id = get_string(params, "action_id", kind);
    } else if (kind == "recovery_cancel") {
      fields(params, {"action_id"}, {}, kind);
      if (object(params, kind).size() != 1)
        fail("InvalidRecoveryCancellation", kind);
      c.action_id = get_string(params, "action_id", kind);
    } else if (growth_busy_command(kind)) {
      bool choose =
          kind == "study_etiquette_choose" || kind == "study_challenge_choose";
      if (choose) {
        const auto step =
            kind == "study_etiquette_choose" ? "node_id" : "step_id";
        if (kind == "study_etiquette_choose")
          fields(params, {"action_id", "node_id", "option_id"}, {}, kind);
        else
          fields(params, {"action_id", "step_id", "option_id"}, {}, kind);
        c.step_id = get_string(params, step, kind);
        c.option_id = get_string(params, "option_id", kind);
      } else
        fields(params, {"action_id"}, {}, kind);
      c.action_id = get_string(params, "action_id", kind);
    } else if (kind == "equip_weapon") {
      fields(params, {"weapon_id"}, {"kind"}, kind);
      c.weapon_id = get_string(params, "weapon_id", kind);
    } else if (kind == "equip_armor") {
      fields(params, {"armor_id"}, {}, kind);
      c.armor_id = get_string(params, "armor_id", kind);
    } else if (kind == "configure_heart") {
      fields(params, {"skill_id"}, {"kind"}, kind);
      c.skill_id = get_string(params, "skill_id", kind);
    } else if (kind == "practice_skill") {
      fields(params, {"skill_id", "ticks"}, {"kind"}, kind);
      c.skill_id = get_string(params, "skill_id", kind);
      c.ticks = get_int(params, "ticks", kind);
    } else if (kind == "learn_skill" || kind == "teach_skill") {
      fields(params, {"skill_id", "context"}, {"kind"}, kind);
      c.skill_id = get_string(params, "skill_id", kind);
      c.context = decode_context(field(params, "context", kind), kind);
    } else if (kind == "service") {
      fields(params, {"activity", "context"}, {"kind"}, kind);
      c.activity = get_string(params, "activity", kind);
      c.context = decode_context(field(params, "context", kind), kind);
    } else if (kind == "trade" && params.find("bargain_quote_id")) {
      fields(params, {"bargain_quote_id"},
             {"quoted_total", "market_revision", "context"}, kind);
      c.bargain_quote_id = get_string(params, "bargain_quote_id", kind);
      if (bool(params.find("quoted_total")) !=
          bool(params.find("market_revision")))
        fail("InvalidBargainTradeConfirmation", kind);
      if (auto *x = params.find("quoted_total"))
        c.quoted_total = integer(*x, kind);
      if (auto *x = params.find("market_revision"))
        c.market_revision = integer(*x, kind);
      if (auto *x = params.find("context"))
        c.context = decode_context(*x, kind);
    } else if (kind == "trade") {
      fields(params,
             {"shop_id", "item_id", "quantity", "side", "payer_wallet_id",
              "context"},
             {"quoted_total", "market_revision", "bargain_quote_id", "kind"},
             kind);
      c.shop_id = get_string(params, "shop_id", kind);
      c.item_id = get_string(params, "item_id", kind);
      c.quantity = get_int(params, "quantity", kind);
      c.side = get_string(params, "side", kind);
      c.payer_wallet_id = get_string(params, "payer_wallet_id", kind);
      c.context = decode_context(field(params, "context", kind), kind);
      if (auto *v = params.find("quoted_total"))
        c.quoted_total = integer(*v, kind);
      if (auto *v = params.find("market_revision"))
        c.market_revision = integer(*v, kind);
      if (auto *v = params.find("bargain_quote_id"))
        c.bargain_quote_id = string(*v, kind);
    } else
      return {false, {}, common_failure("UnsupportedAction").error};
    return {true, std::move(c), {}};
  } catch (const boundary::DecodeFailure &e) {
    auto error = e.error;
    if (kind == "study_skill")
      error.original.code = "InvalidStudyRequest";
    else if (kind == "study_cancel")
      error.original.code = "InvalidStudyCancellation";
    else if (kind == "recovery_cancel")
      error.original.code = "InvalidRecoveryCancellation";
    else if (kind == "study_pause" || kind == "study_resume")
      error.original.code = "InvalidStudyControl";
    else if (growth_busy_command(kind))
      error.original.code = "InvalidStudyChallengeRequest";
    else if (kind == "trade")
      error.original.code = "InvalidTrade";
    else if (kind == "equip_armor")
      error.original.code = "InvalidArmorChoice";
    else if (kind == "equip_weapon")
      error.original.code = "InvalidWeaponChoice";
    return {false, {}, error};
  }
}
Status emit_growth_trade_terms_fields(ProjectionWriter &out,
                                      const TradeTerms &t) {
  out.field("ok", true);
  out.field("shop_id", t.shop_id);
  out.field("item_id", t.item_id);
  out.field("quantity", t.quantity);
  out.field("side", t.side);
  out.field("payer_wallet_id", t.payer_wallet_id);
  emit_common_owner(out, "owner", t.owner);
  out.field("total", t.total);
  out.field("market_revision", t.market_revision);
  out.field("valid_tick", t.valid_tick);
  out.object("context", [&](auto &w) {
    return emit_interaction_context_fields(w, t.context);
  });
  out.field("duration", Integer{1});
  if (t.bargain_quote_id)
    out.field("bargain_quote_id", *t.bargain_quote_id);
  if (t.discount_bp)
    out.field("discount_bp", *t.discount_bp);
  if (t.bargain_basis_sha256)
    out.field("bargain_basis_sha256", *t.bargain_basis_sha256);
  return out.order_fields(t.field_order);
}
namespace {
Status emit_command_as(ProjectionWriter &out, const GrowthCommand &c,
                       const StableId &k, const FieldOrder &order) {
  if (k == "trade_bargain_open") {
    out.object("trade", [&](auto &w) {
      return emit_command_as(w, c, "trade", c.trade_field_order);
    });
    out.field("mode", c.challenge_mode);
    return out.order_fields(order);
  }
  if (k == "trade_bargain_choose" || k == "trade_bargain_exit") {
    if (c.bargain_quote_id)
      out.field("quote_id", *c.bargain_quote_id);
    else
      out.text("quote_id", "");
    if (k == "trade_bargain_choose") {
      out.field("step_id", c.step_id);
      out.field("option_id", c.option_id);
    }
    return out.order_fields(order);
  }
  if (k == "long_rest") {
    out.field("estate_id", c.estate_id);
    out.field("duration", c.duration);
  } else if (k == "rest_cancel") {
  } else if (k == "equip_weapon")
    out.field("weapon_id", c.weapon_id);
  else if (k == "equip_armor")
    out.field("armor_id", c.armor_id);
  else if (growth_busy_command(k)) {
    out.field("action_id", c.action_id);
    if (k == "study_etiquette_choose" || k == "study_challenge_choose") {
      out.field(k == "study_etiquette_choose" ? "node_id" : "step_id",
                c.step_id);
      out.field("option_id", c.option_id);
    }
  } else if (k == "service")
    out.field("activity", c.activity);
  else if (k == "trade" && c.bargain_quote_id) {
    out.field("bargain_quote_id", *c.bargain_quote_id);
    if (c.quoted_total)
      out.field("quoted_total", *c.quoted_total);
    if (c.market_revision)
      out.field("market_revision", *c.market_revision);
  } else if (k == "trade") {
    out.field("shop_id", c.shop_id);
    out.field("item_id", c.item_id);
    out.field("quantity", c.quantity);
    out.field("side", c.side);
    out.field("payer_wallet_id", c.payer_wallet_id);
    if (c.quoted_total)
      out.field("quoted_total", *c.quoted_total);
    if (c.market_revision)
      out.field("market_revision", *c.market_revision);
    if (c.bargain_quote_id)
      out.field("bargain_quote_id", *c.bargain_quote_id);
  } else {
    out.field("skill_id", c.skill_id);
    if (k == "study_skill" || k == "teach_study" || k == "practice_skill")
      out.field("ticks", c.ticks);
  }
  if (std::find(order.begin(), order.end(), "kind") != order.end())
    out.field("kind", k);
  if (c.context)
    out.object("context", [&](auto &w) {
      return emit_interaction_context_fields(w, *c.context);
    });
  if (k == "study_skill" &&
      (c.challenge_mode != "automatic" ||
       std::find(order.begin(), order.end(), "challenge_mode") != order.end()))
    out.field("challenge_mode", c.challenge_mode);
  return out.order_fields(order);
}
} // namespace
Status emit_growth_command_fields(ProjectionWriter &out,
                                  const GrowthCommand &c) {
  return emit_command_as(out, c, c.kind, c.field_order);
}

BoundaryValue encode_growth_command(const GrowthCommand &c) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status =
      out.object("", [&](auto &w) { return emit_growth_command_fields(w, c); });
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
// 冻结报价对外保持旧字段序；私有条款虽可被kernel返回，但视图绝不复用整份条款。
Outcome<BoundaryValue> encode_growth_quote(const GrowthQuote &q) {
  try {
    using namespace boundary;
    auto v = obj({{"ok", scalar(true)}, {"duration", scalar(q.duration)}});
    const auto &kind = q.command.kind;
    if (auto *x = std::get_if<EquipmentChoice>(&q.data)) {
      add(v,
          kind == "equip_weapon"  ? "weapon_id"
          : kind == "equip_armor" ? "armor_id"
                                  : "skill_id",
          scalar(x->id));
      if (kind == "equip_armor")
        add(v, "armor_defense", scalar(x->defense));
    } else if (auto *x = std::get_if<TrainingTerms>(&q.data)) {
      add(v, "cost", scalar(q.cost));
      add(v, "training", encode_training(*x));
      if (q.context)
        add(v, "context", encode_context(*q.context));
    } else if (auto *x = std::get_if<StudyTerms>(&q.data)) {
      add(v, "cost", scalar(q.cost));
      add(v, "study", encode_study(*x, false));
      add(v, "context", encode_context(x->context));
      add(v, "expected_xp", scalar(q.expected_xp));
      add(v, "target_level", scalar(q.target_level));
      add(v, "payment_policy", scalar(StableId{"reserve_then_pay_per_lesson"}));
      add(v, "cancel_supported", scalar(true));
    } else if (auto *x = std::get_if<ServiceTerms>(&q.data)) {
      add(v, "cost", scalar(q.cost));
      add(v, "wage", scalar(q.wage));
      if (q.context)
        add(v, "context", encode_context(*q.context));
      add(v, "terms", encode_service(*x));
    } else if (auto *x = std::get_if<TradeTerms>(&q.data))
      return {true, encode_trade(*x), {}};
    else if (auto *x = std::get_if<StudyCancellation>(&q.data)) {
      add(v, "action_id", scalar(x->action_id));
      add(v, "completed_lessons", scalar(x->completed_lessons));
      add(v, "fee_paid", scalar(x->fee_paid));
      add(v, "unprovided_released", scalar(x->unprovided_released));
    } else if (auto *x = std::get_if<StudyPauseChange>(&q.data)) {
      add(v, "action_id", scalar(x->action_id));
      add(v, "paused", scalar(x->paused));
      add(v, "paused_ticks", scalar(x->paused_ticks));
      add(v, "completed_lessons", scalar(x->completed_lessons));
      add(v, "reserved", scalar(x->reserved));
    } else if (auto *x = std::get_if<BargainPrepared>(&q.data))
      return {true, growth::encode_bargain_prepared(*x), {}};
    else if (auto *x = std::get_if<RestTerms>(&q.data)) {
      add(v, "estate_id", scalar(x->estate_id));
      add(v, "mouths", string_array(x->mouths));
      add(v, "food_item_id", scalar(x->food_item_id));
      add(v, "ration_per_person", scalar(x->ration_per_person));
    } else if (auto *x = std::get_if<RestCancellation>(&q.data))
      add(v, "action_id", scalar(x->action_id));
    else if (auto *x = std::get_if<GrowthChallengeInstance>(&q.data)) {
      add(v, "action_id", scalar(q.command.action_id));
      if (q.challenge_before) {
        auto before = growth::encode_challenge(*q.challenge_before);
        add(v, "challenge_before", field(before, "challenge", ""));
      }
      auto instance = growth::encode_challenge(*x);
      add(v, "challenge_next", field(instance, "challenge", ""));
    }
    return {true, std::move(v), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
static Outcome<GrowthState> decode_growth_seed_impl(
    const BoundaryValue &source, bool regional_life, bool cards,
    bool teaching_requests, bool study_teaching_requests, bool innate_learning,
    bool fixed_service, bool complete = false, bool exact_periods = false,
    bool exact_command_cuts = false, bool exact_rest_mouths = false,
    bool capability_chain = false, GrowthContributionWireVersion wire = GrowthContributionWireVersion::original) {
  try {
    using namespace boundary;
    if (wire != GrowthContributionWireVersion::original && wire != GrowthContributionWireVersion::trade_grants)
      unsupported("GrowthContributionWireVersionUnsupported", "growth");
    if (wire == GrowthContributionWireVersion::trade_grants && !capability_chain)
      unsupported("GrowthTradeGrantPhysicalSelectionRequired", "growth");
    if (capability_chain && !exact_rest_mouths)
      boundary::unsupported("GrowthCapabilityChainPhysicalSelectionRequired",
                            "growth");
    if (exact_rest_mouths && !exact_command_cuts)
      unsupported("GrowthExactRestMouthPhysicalSelectionRequired", "growth");
    if (exact_command_cuts && !exact_periods)
      unsupported("GrowthExactCommandCutsPhysicalSelectionRequired", "growth");
    if (exact_periods && (!fixed_service || !complete))
      unsupported("GrowthExactPeriodsPhysicalSelectionRequired", "growth");
    if (fixed_service && !complete &&
        (!regional_life || cards || teaching_requests ||
         study_teaching_requests || innate_learning))
      unsupported("UnsupportedGrowthFixedSchema", "growth");
    if (!fixed_service && source.find("fixed_service_boundaries"))
      unsupported("GrowthFixedServiceCapabilityRequired",
                  "fixed_service_boundaries");
    if (fixed_service) {
      auto unique = validate_succession_json_unique(source);
      if (!unique.ok)
        return {false, {}, unique.error};
    }
    GrowthState state;
    if (cards) {
      auto decoded =
          decode_growth_cards(field(source, "growth_cards", "state"));
      if (!decoded.ok)
        return {false, {}, decoded.error};
      state.cards = std::move(decoded.value);
    } else if (source.find("growth_cards"))
      fail("CardsCapabilityRequired", "growth_cards");
    for (const auto &row : object(field(source, "facts", ""), "facts")) {
      const auto &value = row.second;
      auto kind = get_string(value, "kind", row.first);
      if (kind == "life_activity") {
        if (!regional_life)
          fail("RegionalLifeUnavailable", row.first);
        if (fixed_service) {
          fields(value,
                 {"kind", "view", "context", "terms_kind", "terms", "cost_paid",
                  "wage_paid", "field_order", "view_field_order",
                  "selected_trade_settlement", "trade_terminal_time"},
                 {}, row.first);
          if (order(value) !=
              FieldOrder{"kind", "view", "context", "terms_kind", "terms",
                         "cost_paid", "wage_paid", "field_order",
                         "view_field_order", "selected_trade_settlement",
                         "trade_terminal_time"})
            fail("GrowthFixedActivityFieldOrderMismatch", row.first);
        } else
          fields(value,
                 {"kind", "view", "context", "terms", "cost_paid", "wage_paid"},
                 {}, row.first);
        LifeActivityRecord record;
        record.field_order =
            fixed_service
                ? strings(field(value, "field_order", row.first), row.first)
                : order(value);
        const auto &raw = field(value, "view", row.first);
        record.view_field_order =
            fixed_service ? strings(field(value, "view_field_order", row.first),
                                    row.first)
                          : order(raw);
        auto &v = record.view;
        if (fixed_service) {
          const auto tag = get_int(value, "terms_kind", row.first);
          if (tag < 0 || tag > 2)
            fail("InvalidLifeActivityTermsKind", row.first);
          if (tag == 2 || (!complete && tag != 0))
            unsupported("UnsupportedGrowthFixedTerms", row.first);
          if (!field(value, "selected_trade_settlement", row.first)
                   .as<std::monostate>())
            unsupported("UnsupportedGrowthFixedTradeSource", row.first);
        }
        fields(raw, {"fact_id",         "action_id",
                     "actor_id",        "provider_actor_id",
                     "activity",        "kind",
                     "phase",           "start_tick",
                     "settlement_tick", "completed_tick",
                     "success",         "code",
                     "item_id",         "quantity",
                     "amount",          "payer_wallet_id",
                     "payee_wallet_id", "from_inventory_id",
                     "to_inventory_id", "participant_actor_ids"},
               {}, row.first);
#define LIFE_STR(name) v.name = get_string(raw, #name, row.first)
        LIFE_STR(fact_id);
        LIFE_STR(action_id);
        LIFE_STR(actor_id);
        LIFE_STR(provider_actor_id);
        LIFE_STR(activity);
        LIFE_STR(code);
        LIFE_STR(item_id);
        LIFE_STR(payer_wallet_id);
        LIFE_STR(payee_wallet_id);
        LIFE_STR(from_inventory_id);
        LIFE_STR(to_inventory_id);
#undef LIFE_STR
#define LIFE_INT(name) v.name = get_int(raw, #name, row.first)
        LIFE_INT(start_tick);
        LIFE_INT(settlement_tick);
        LIFE_INT(completed_tick);
        LIFE_INT(quantity);
        LIFE_INT(amount);
#undef LIFE_INT
        const auto tag = get_int(raw, "kind", row.first),
                   phase = get_int(raw, "phase", row.first);
        if (tag < 0 || tag > 1 || phase < 0 || phase > 2)
          fail("InvalidLifeActivityTag", row.first);
        v.kind = static_cast<LifeActivityKind>(tag);
        v.phase = static_cast<LifeActivityPhase>(phase);
        if (fixed_service && !complete &&
            (v.kind != LifeActivityKind::service || v.activity != "rest"))
          unsupported("UnsupportedGrowthFixedActivity", row.first);
        v.success = get_bool(raw, "success", row.first);
        v.participant_actor_ids =
            strings(field(raw, "participant_actor_ids", row.first), row.first);
        record.context =
            decode_context(field(value, "context", row.first), row.first);
        record.cost_paid = get_int(value, "cost_paid", row.first);
        record.wage_paid = get_int(value, "wage_paid", row.first);
        const auto &terms = field(value, "terms", row.first);
        record.terms =
            v.kind == LifeActivityKind::service
                ? std::variant<ServiceTerms, TradeTerms>{decode_service(
                      terms, row.first)}
                : std::variant<ServiceTerms, TradeTerms>{
                      decode_trade(terms, row.first)};
        if (fixed_service) {
          if (get_int(value, "terms_kind", row.first) !=
              static_cast<Integer>(record.terms.index()))
            fail("LifeActivityTermsKindMismatch", row.first);
          const auto& terminal=field(value,"trade_terminal_time",row.first);
          if (!terminal.as<std::monostate>()) {
            auto decoded=growth_trade_terminal_codec::decode(terminal);
            if (!decoded.ok)return {false,{},decoded.error};
            record.trade_terminal_time=std::move(decoded.value);
          }
        }
        auto terminal_shape=growth_trade_terminal_codec::validate_activity(
            record,fixed_service&&complete);
        if (!terminal_shape.ok)return {false,{},terminal_shape.error};
        if (!state.life_activities.insert(row.first, std::move(record)))
          fail("DuplicateLifeActivity", row.first);
      } else if (kind == "training") {
        if (capability_chain)
          fields(value,
                 {"kind", "phase", "action_kind", "actor_id", "start_tick",
                  "completed_tick", "success", "training", "context",
                  "capability_application"},
                 {}, row.first);
        else
          fields(value,
                 {"kind", "phase", "action_kind", "actor_id", "start_tick",
                  "completed_tick", "success", "training", "context"},
                 {}, row.first);
        TrainingFact f;
        f.field_order = order(value);
        f.phase = get_string(value, "phase", row.first);
        f.action_kind = get_string(value, "action_kind", row.first);
        f.actor_id = get_string(value, "actor_id", row.first);
        f.start_tick = get_int(value, "start_tick", row.first);
        f.completed_tick = get_int(value, "completed_tick", row.first);
        f.success = get_bool(value, "success", row.first);
        f.training = decode_training(field(value, "training", row.first),
                                     row.first, teaching_requests);
        auto &context = field(value, "context", row.first);
        if (!object(context, row.first).empty())
          f.context = decode_context(context, row.first);
        if (capability_chain) {
          if (f.field_order.empty() ||
              f.field_order.back() != "capability_application")
            fail("GrowthCapabilityTailOrderMismatch", row.first);
          f.field_order.pop_back();
          const auto &value =
              field(row.second, "capability_application", row.first);
          if (!value.as<std::monostate>()) {
            auto decoded = decode_growth_capability_application(value, wire);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            f.capability_application = std::move(decoded.value);
          }
          if (bool(f.capability_application) !=
              (f.phase == "completed" && f.success &&
               f.training.mastery_after != f.training.mastery_before))
            fail("TrainingCapabilityApplicationMismatch", row.first);
        }
        state.facts.insert(row.first, std::move(f));
      } else if (kind == "skill_study") {
        if (complete)
          fields(value,
                 {"kind", "phase", "actor_id", "start_tick", "completed_tick",
                  "success", "terms", "lessons", "code",
                  "study_service_binding", "study_service_terminal"},
                 {"etiquette", "challenge", "pauses", "xp_settlement_before"},
                 row.first);
        else
          fields(value,
                 {"kind", "phase", "actor_id", "start_tick", "completed_tick",
                  "success", "terms", "lessons", "code"},
                 {"etiquette", "challenge", "pauses", "xp_settlement_before"},
                 row.first);
        StudyFact f;
        f.field_order = order(value);
        f.phase = get_string(value, "phase", row.first);
        f.actor_id = get_string(value, "actor_id", row.first);
        f.code = get_string(value, "code", row.first);
        f.start_tick = get_int(value, "start_tick", row.first);
        f.completed_tick = get_int(value, "completed_tick", row.first);
        f.success = get_bool(value, "success", row.first);
        if (complete) {
          if (f.field_order.size() < 2 ||
              f.field_order[f.field_order.size() - 2] !=
                  "study_service_binding" ||
              f.field_order.back() != "study_service_terminal")
            fail("StudyServiceTailOrderMismatch", row.first);
          f.field_order.resize(f.field_order.size() - 2);
          const auto &binding =
              field(value, "study_service_binding", row.first);
          if (!binding.as<std::monostate>()) {
            auto decoded = decode_study_service_binding_v2(binding);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            f.study_service_binding = std::move(decoded.value);
          }
          const auto &terminal =
              field(value, "study_service_terminal", row.first);
          if (!terminal.as<std::monostate>()) {
            auto decoded = decode_study_service_terminal_v2(terminal);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            f.study_service_terminal = std::move(decoded.value);
          }
        }
        f.terms = decode_study(field(value, "terms", row.first), row.first,
                               study_teaching_requests, innate_learning);
        for (const auto &item :
             array(field(value, "lessons", row.first), row.first)) {
          fields(item, {"fatigue_before", "injury", "xp"}, {"segment"},
                 row.first);
          StudyLesson lesson{get_int(item, "fatigue_before", row.first),
                             get_int(item, "injury", row.first),
                             get_int(item, "xp", row.first), order(item),
                             std::nullopt};
          if (auto *segment = item.find("segment"))
            lesson.segment =
                decode_segment(*segment, row.first, exact_periods,
                               exact_command_cuts, capability_chain, wire);
          f.lessons.push_back(std::move(lesson));
        }
        if (auto *pauses = value.find("pauses")) {
          f.pauses.emplace();
          for (const auto &item : array(*pauses, row.first)) {
            fields(item,
                   {"start_tick", "end_tick", "after_lessons",
                    "student_fatigue", "teacher_fatigue", "student_xp"},
                   {"credited_xp"}, row.first);
            f.pauses->push_back(
                StudyPause{get_int(item, "start_tick", row.first),
                           get_int(item, "end_tick", row.first),
                           get_int(item, "after_lessons", row.first),
                           get_int(item, "student_fatigue", row.first),
                           get_int(item, "teacher_fatigue", row.first),
                           get_int(item, "student_xp", row.first), order(item),
                           std::nullopt});
            if (item.find("credited_xp"))
              f.pauses->back().credited_xp =
                  get_int(item, "credited_xp", row.first);
          }
        }
        if (auto *before = value.find("xp_settlement_before")) {
          if (!before->as<Integer>())
            fail("InvalidStudyGrowth", row.first);
          f.xp_settlement_before = integer(*before, row.first);
        }
        if (f.terms.challenge)
          f.challenge = growth::decode_challenge(
              field(value, f.terms.challenge->field, row.first),
              *f.terms.challenge, row.first);
        else if (value.find("etiquette") || value.find("challenge"))
          fail("UnexpectedStudyChallenge", row.first);
        state.facts.insert(row.first, std::move(f));
      } else if (kind == "trade_bargain")
        state.facts.insert(row.first,
                           growth::decode_bargain_fact(value, row.first));
      else if (kind == "long_rest") {
        if (exact_periods)
          fields(value,
                 {"kind", "actor_id", "estate_id", "start_tick", "duration",
                  "elapsed", "phase", "completed_tick", "food_item_id",
                  "ration_per_person", "consumed", "last_mouths", "stop_reason",
                  "actual_periods"},
                 {}, row.first);
        else
          fields(value,
                 {"kind", "actor_id", "estate_id", "start_tick", "duration",
                  "elapsed", "phase", "completed_tick", "food_item_id",
                  "ration_per_person", "consumed", "last_mouths",
                  "stop_reason"},
                 {}, row.first);
        RestFact f;
        f.field_order = order(value);
        if (exact_periods) {
          if (f.field_order.empty() || f.field_order.back() != "actual_periods")
            fail("LongRestPeriodTailOrderMismatch", row.first);
          f.field_order.pop_back();
          for (const auto &period :
               array(field(value, "actual_periods", row.first), row.first)) {
            auto decoded =
                decode_long_rest_actual_period(period, exact_rest_mouths);
            if (!decoded.ok)
              return {false, {}, decoded.error};
            f.actual_periods.push_back(std::move(decoded.value));
          }
        }
#define HM_STR(name) f.name = get_string(value, #name, row.first)
        HM_STR(actor_id);
        HM_STR(estate_id);
        HM_STR(phase);
        HM_STR(food_item_id);
        HM_STR(stop_reason);
#undef HM_STR
#define HM_INT(name) f.name = get_int(value, #name, row.first)
        HM_INT(start_tick);
        HM_INT(duration);
        HM_INT(elapsed);
        HM_INT(completed_tick);
        HM_INT(ration_per_person);
        HM_INT(consumed);
#undef HM_INT
        f.last_mouths =
            strings(field(value, "last_mouths", row.first), row.first);
        state.facts.insert(row.first, std::move(f));
      }
    }
    for (const auto &row : object(field(source, "actions", ""), "actions")) {
      auto kind = get_string(row.second, "kind", row.first);
      if (!growth_action_kind(kind))
        continue;
      const auto &params = field(row.second, "params", row.first);
      GrowthAction action;
      action.field_order = order(params);
      action.request.kind = kind;
      action.quote.command.kind = kind;
      action.quote.duration = get_int(row.second, "remaining_ticks", row.first);
      if (kind == "long_rest") {
        auto command = decode_growth_command(kind, params);
        if (!command.ok)
          return {false, {}, command.error};
        action.request = command.value;
        auto *value = state.facts.find(row.first);
        auto *fact = value ? std::get_if<RestFact>(value) : nullptr;
        if (!fact)
          fail("InvalidRestFact", row.first);
        action.quote.duration = command.value.duration;
        action.quote.data = RestTerms{fact->estate_id,
                                      fact->food_item_id,
                                      fact->duration,
                                      fact->ration_per_person,
                                      {}};
      } else if (kind == "study_skill" || kind == "teach_study") {
        if (kind == "teach_study" && !study_teaching_requests)
          fail("StudyTeachingCapabilityRequired", row.first);
        fields(params, {"study"}, {}, row.first);
        auto terms = decode_study(field(params, "study", row.first), row.first,
                                  study_teaching_requests, innate_learning);
        action.request.skill_id = terms.skill_id;
        action.request.ticks = terms.duration;
        action.request.context =
            kind == "teach_study" && terms.study_teaching_agreement
                ? terms.study_teaching_agreement->request_context
                : terms.context;
        if (kind == "teach_study" && !terms.study_teaching_agreement)
          fail("StudyTeachingAgreementRequired", row.first);
        action.request.challenge_mode =
            terms.challenge ? terms.challenge->mode : "automatic";
        action.quote.context = action.request.context;
        action.quote.duration = terms.duration;
        action.quote.cost = terms.fee;
        action.quote.target_level = growth_skill_level(terms.xp_before) + 1;
        action.quote.data = std::move(terms);
      } else if (kind == "learn_skill" || kind == "practice_skill" ||
                 kind == "teach_skill") {
        fields(params, {"training"}, {"context"}, row.first);
        auto terms = decode_training(field(params, "training", row.first),
                                     row.first, teaching_requests);
        action.request.skill_id = terms.skill_id;
        action.request.ticks = terms.duration;
        if (auto *context = params.find("context")) {
          action.request.context = decode_context(*context, row.first);
          action.quote.context = action.request.context;
        }
        action.quote.duration = terms.duration;
        action.quote.cost = terms.fee;
        action.quote.data = std::move(terms);
      } else {
        auto input = params;
        auto &fields = std::get<BoundaryValue::Dict>(input.data);
        fields.erase(std::remove_if(fields.begin(), fields.end(),
                                    [&](const auto &x) {
                                      return x.first == (kind == "service"
                                                             ? "terms"
                                                             : "receipt");
                                    }),
                     fields.end());
        auto decoded = decode_growth_command(kind, input);
        if (!decoded.ok)
          return {false, {}, decoded.error};
        action.request = std::move(decoded.value);
        action.quote.context = action.request.context;
        if (kind == "service") {
          auto terms =
              decode_service(field(params, "terms", row.first), row.first);
          action.quote.duration = terms.duration;
          action.quote.cost = terms.cost;
          action.quote.wage = terms.wage;
          action.quote.data = std::move(terms);
        } else {
          auto terms =
              decode_trade(field(params, "receipt", row.first), row.first);
          action.quote.duration = 1;
          action.quote.context = terms.context;
          action.quote.data = std::move(terms);
        }
      }
      action.quote.command = action.request;
      state.actions.insert(row.first, std::move(action));
    }
    if (fixed_service)
      for (const auto &row :
           object(field(source, "fixed_service_boundaries", "growth"),
                  "fixed_service_boundaries")) {
        auto decoded = complete
                           ? decode_growth_complete_fixed_service_boundary(
                                 row.second, exact_periods, wire)
                           : decode_growth_fixed_service_boundary(row.second);
        if (!decoded.ok)
          return {false, {}, decoded.error};
        if (row.first != decoded.value.boundary_id)
          fail("GrowthFixedBoundaryKeyMismatch", row.first);
        if (!state.fixed_service_boundaries.insert(row.first,
                                                   std::move(decoded.value)))
          fail("DuplicateGrowthFixedBoundary", row.first);
      }
    return {true, std::move(state), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
static Status emit_growth_action_row_fields(ProjectionWriter &w,
                                            const GrowthAction &a) {
  if (const auto *t = std::get_if<StudyTerms>(&a.quote.data))
    w.object("study", [&](auto &x) { return emit_study_fields(x, *t); });
  else if (const auto *t = std::get_if<TrainingTerms>(&a.quote.data)) {
    w.object("training", [&](auto &x) { return emit_training_fields(x, *t); });
    if (a.quote.context)
      w.object("context", [&](auto &x) {
        return emit_interaction_context_fields(x, *a.quote.context);
      });
  } else {
    auto status = emit_growth_command_fields(w, a.request);
    if (!status.ok)
      return status;
    if (const auto *t = std::get_if<ServiceTerms>(&a.quote.data))
      w.object("terms", [&](auto &x) { return emit_service_fields(x, *t); });
    else if (const auto *t = std::get_if<TradeTerms>(&a.quote.data))
      w.object("receipt",
               [&](auto &x) { return emit_growth_trade_terms_fields(x, *t); });
  }
  return w.order_fields(a.field_order);
}
static Status emit_growth_fact_row_fields(ProjectionWriter &w,
                                          const GrowthFact &payload,
                                          bool complete, bool exact_periods,
                                          bool exact_command_cuts,
                                          bool exact_rest_mouths,
                                          bool capability_chain, GrowthContributionWireVersion wire) {
  return std::visit(
      [&](const auto &f) -> Status {
        using T = std::decay_t<decltype(f)>;
        if constexpr (std::is_same_v<T, TrainingFact>) {
          w.text("kind", "training");
          w.field("phase", f.phase);
          w.field("action_kind", f.action_kind);
          w.field("actor_id", f.actor_id);
          w.field("start_tick", f.start_tick);
          w.field("completed_tick", f.completed_tick);
          w.field("success", f.success);
          w.object("training", [&](auto &x) {
            return emit_training_fields(x, f.training);
          });
          w.object("context", [&](auto &x) {
            return f.context ? emit_interaction_context_fields(x, *f.context)
                             : x.status();
          });
        } else if constexpr (std::is_same_v<T, StudyFact>) {
          w.text("kind", "skill_study");
          w.field("phase", f.phase);
          w.field("actor_id", f.actor_id);
          w.field("start_tick", f.start_tick);
          w.field("completed_tick", f.completed_tick);
          w.field("success", f.success);
          w.object("terms",
                   [&](auto &x) { return emit_study_fields(x, f.terms); });
          w.sequence("lessons", f.lessons, [&](auto &x, const auto &lesson) {
            return x.object("", lesson.field_order, [&](auto &v) {
              v.field("fatigue_before", lesson.fatigue_before);
              v.field("injury", lesson.injury);
              v.field("xp", lesson.xp);
              if (lesson.segment)
                v.object("segment", [&](auto &segment) {
                  return emit_segment_fields(segment, *lesson.segment,
                                             exact_periods, exact_command_cuts,
                                             capability_chain, wire);
                });
              return v.status();
            });
          });
          w.field("code", f.code);
          if (f.challenge)
            w.object(f.challenge->frozen.field, [&](auto &x) {
              return growth::emit_growth_challenge_fields(x, *f.challenge);
            });
          if (f.pauses)
            w.sequence("pauses", *f.pauses, [](auto &x, const auto &pause) {
              return x.object("", pause.field_order, [&](auto &v) {
                v.field("start_tick", pause.start_tick);
                v.field("end_tick", pause.end_tick);
                v.field("after_lessons", pause.after_lessons);
                v.field("student_fatigue", pause.student_fatigue);
                v.field("teacher_fatigue", pause.teacher_fatigue);
                v.field("student_xp", pause.student_xp);
                if (pause.credited_xp)
                  v.field("credited_xp", *pause.credited_xp);
                return v.status();
              });
            });
          if (f.xp_settlement_before)
            w.field("xp_settlement_before", *f.xp_settlement_before);
        } else if constexpr (std::is_same_v<T, RestFact>) {
          w.text("kind", "long_rest");
#define F(x) w.field(#x, f.x)
          F(actor_id);
          F(estate_id);
          F(start_tick);
          F(duration);
          F(elapsed);
          F(phase);
          F(completed_tick);
          F(food_item_id);
          F(ration_per_person);
          F(consumed);
#undef F
          w.sequence("last_mouths", f.last_mouths,
                     [](auto &x, const auto &id) { return x.field("", id); });
          w.field("stop_reason", f.stop_reason);
        } else
          return growth::emit_growth_bargain_fact_fields(w, f);
        w.order_fields(f.field_order);
        if constexpr (std::is_same_v<T, TrainingFact>) {
          if (capability_chain) {
            if (bool(f.capability_application) !=
                (f.phase == "completed" && f.success &&
                 f.training.mastery_after != f.training.mastery_before))
              return w.reject("TrainingCapabilityApplicationMismatch");
            if (f.capability_application)
              emit_growth_capability_application(w, "capability_application",
                                                 *f.capability_application, wire);
            else
              w.null("capability_application");
          } else if (f.capability_application)
            return w.reject("GrowthCapabilityPhysicalSelectionRequired");
        }
        if constexpr (std::is_same_v<T, RestFact>) {
          if (exact_periods)
            w.sequence("actual_periods", f.actual_periods,
                       [&](auto &x, const auto &period) {
                         return emit_long_rest_actual_period(x, "", period,
                                                             exact_rest_mouths);
                       });
        }
        if constexpr (std::is_same_v<T, StudyFact>) {
          if (complete) {
            if (f.study_service_binding)
              emit_study_service_binding_v2(w, "study_service_binding",
                                            *f.study_service_binding);
            else
              w.null("study_service_binding");
            if (f.study_service_terminal)
              emit_study_service_terminal_v2(w, "study_service_terminal",
                                             *f.study_service_terminal);
            else
              w.null("study_service_terminal");
          }
        }
        return w.status();
      },
      payload);
}
static Status emit_growth_life_row_fields(ProjectionWriter &w,
                                          const LifeActivityRecord &record,
                                          bool fixed_service) {
  const auto &f = record.view;
  w.text("kind", "life_activity");
  w.object("view", record.view_field_order, [&](auto &v) {
#define F(x) v.field(#x, f.x)
    F(fact_id);
    F(action_id);
    F(actor_id);
    F(provider_actor_id);
    F(activity);
    v.field("kind", static_cast<Integer>(f.kind));
    v.field("phase", static_cast<Integer>(f.phase));
    F(start_tick);
    F(settlement_tick);
    F(completed_tick);
    F(success);
    F(code);
    F(item_id);
    F(quantity);
    F(amount);
    F(payer_wallet_id);
    F(payee_wallet_id);
    F(from_inventory_id);
    F(to_inventory_id);
#undef F
    return v.sequence("participant_actor_ids", f.participant_actor_ids,
                      [](auto &x, const auto &id) { return x.field("", id); });
  });
  w.object("context", [&](auto &x) {
    return emit_interaction_context_fields(x, record.context);
  });
  if (fixed_service)
    w.field("terms_kind", static_cast<Integer>(record.terms.index()));
  w.object("terms", [&](auto &x) {
    return std::holds_alternative<ServiceTerms>(record.terms)
               ? emit_service_fields(x, std::get<ServiceTerms>(record.terms))
               : emit_growth_trade_terms_fields(
                     x, std::get<TradeTerms>(record.terms));
  });
  w.field("cost_paid", record.cost_paid);
  w.field("wage_paid", record.wage_paid);
  if (fixed_service) {
    w.sequence("field_order", record.field_order,
               [](auto &x, const auto &id) { return x.field("", id); });
    w.sequence("view_field_order", record.view_field_order,
               [](auto &x, const auto &id) { return x.field("", id); });
    w.null("selected_trade_settlement");
    if(record.trade_terminal_time)
      return growth_trade_terminal_codec::emit(w,"trade_terminal_time",
                                              *record.trade_terminal_time);
    w.null("trade_terminal_time");
    return w.status();
  }
  return w.order_fields(record.field_order);
}
static Status emit_growth_state_fields_impl(
    ProjectionWriter &out, const GrowthState &state, bool regional_life,
    bool cards, bool teaching_requests, bool study_teaching_requests,
    bool innate_learning, bool fixed_service, bool complete, bool exact_periods,
    bool exact_command_cuts, bool exact_rest_mouths, bool capability_chain, GrowthContributionWireVersion wire) {
  using namespace boundary;
  if (wire != GrowthContributionWireVersion::original && wire != GrowthContributionWireVersion::trade_grants)
    unsupported("GrowthContributionWireVersionUnsupported", "growth");
  if (wire == GrowthContributionWireVersion::trade_grants && !capability_chain)
    unsupported("GrowthTradeGrantPhysicalSelectionRequired", "growth");

  if (capability_chain && !exact_rest_mouths)
    boundary::unsupported("GrowthCapabilityChainPhysicalSelectionRequired",
                          "growth");
  if (exact_rest_mouths && !exact_command_cuts)
    unsupported("GrowthExactRestMouthPhysicalSelectionRequired", "growth");
  if (exact_command_cuts && !exact_periods)
    unsupported("GrowthExactCommandCutsPhysicalSelectionRequired", "growth");
  if (exact_periods && (!fixed_service || !complete))
    unsupported("GrowthExactPeriodsPhysicalSelectionRequired", "growth");
  if (fixed_service && !complete &&
      (!regional_life || cards || teaching_requests ||
       study_teaching_requests || innate_learning))
    unsupported("UnsupportedGrowthFixedSchema", "growth");
  if (!fixed_service && !state.fixed_service_boundaries.rows().empty())
    unsupported("GrowthFixedServiceCapabilityRequired",
                "fixed_service_boundaries");
  for (const auto &row : state.life_activities.rows()) {
    const auto &r = row.second;
    if (r.selected_trade_settlement)
      unsupported("UnsupportedGrowthFixedTradeSource", row.first);
    auto terminal_shape=growth_trade_terminal_codec::validate_activity(
        r,fixed_service&&complete);
    if(!terminal_shape.ok)return terminal_shape;
    if (fixed_service && !complete &&
        (r.view.kind != LifeActivityKind::service ||
         r.view.activity != "rest" ||
         !std::holds_alternative<ServiceTerms>(r.terms)))
      unsupported("UnsupportedGrowthFixedActivity", row.first);
    if (fixed_service && (static_cast<Integer>(r.view.phase) < 0 ||
                          static_cast<Integer>(r.view.phase) > 2))
      fail("InvalidLifeActivityTag", row.first);
  }
  // 开关只准入真实先天课程来源；未选profile在永久事实和活动条款两侧均拒，旧输出字节不变。
  auto check_innate = [&](const StudyTerms &terms, const StableId &id) {
    const bool policy = terms.settlement_policy &&
                        *terms.settlement_policy == "innate_per_lesson_v1";
    if (!innate_learning && (terms.innate_source || policy))
      fail("InnateStudyCapabilityRequired", id);
    if (bool(terms.innate_source) != policy)
      fail("InnateStudyPolicySourceMismatch", id);
  };
  for (const auto &row : state.actions.rows())
    if (const auto *terms = std::get_if<StudyTerms>(&row.second.quote.data))
      check_innate(*terms, row.first);
  for (const auto &row : state.facts.rows())
    if (const auto *fact = std::get_if<StudyFact>(&row.second))
      check_innate(fact->terms, row.first);
  if (!study_teaching_requests) {
    for (const auto &row : state.actions.rows())
      if (row.second.request.kind == "teach_study" ||
          (std::get_if<StudyTerms>(&row.second.quote.data) &&
           std::get<StudyTerms>(row.second.quote.data)
               .study_teaching_agreement))
        fail("StudyTeachingCapabilityRequired", row.first);
    for (const auto &row : state.facts.rows())
      if (const auto *fact = std::get_if<StudyFact>(&row.second);
          fact && fact->terms.study_teaching_agreement)
        fail("StudyTeachingCapabilityRequired", row.first);
  }
  if (!teaching_requests) {
    for (const auto &row : state.actions.rows())
      if (const auto *terms =
              std::get_if<TrainingTerms>(&row.second.quote.data);
          terms && terms->teaching_agreement)
        fail("TeachingRequestsCapabilityRequired", row.first);
    for (const auto &row : state.facts.rows())
      if (const auto *fact = std::get_if<TrainingFact>(&row.second);
          fact && fact->training.teaching_agreement)
        fail("TeachingRequestsCapabilityRequired", row.first);
  }
  if (cards != state.cards.has_value())
    fail(cards ? "MissingGrowthCards" : "CardsCapabilityRequired",
         "growth_cards");
  if (!regional_life && !state.life_activities.rows().empty())
    fail("RegionalLifeUnavailable", "growth");
  if (!complete)
    for (const auto &row : state.facts.rows())
      if (const auto *f = std::get_if<StudyFact>(&row.second);
          f && (f->study_service_binding || f->study_service_terminal))
        unsupported("CompleteStudyServiceCodecRequired", row.first);

  if (!exact_periods)
    for (const auto &row : state.facts.rows())
      if (const auto *rest = std::get_if<RestFact>(&row.second);
          rest && !rest->actual_periods.empty())
        unsupported("GrowthExactPeriodsPhysicalSelectionRequired", row.first);
  out.object("action_params", [&](auto &params) {
    for (const auto &row : state.actions.rows())
      params.object(row.first, [&](auto &w) {
        return emit_growth_action_row_fields(w, row.second);
      });
    return params.status();
  });
  out.object("facts", [&](auto &facts) {
    for (const auto &row : state.facts.rows())
      facts.object(row.first, [&](auto &w) {
        return emit_growth_fact_row_fields(w, row.second, complete,
                                           exact_periods, exact_command_cuts,
                                           exact_rest_mouths, capability_chain, wire);
      });
    for (const auto &row : state.life_activities.rows())
      facts.object(row.first, [&](auto &w) {
        return emit_growth_life_row_fields(w, row.second, fixed_service);
      });
    return facts.status();
  });
  if (cards)
    out.object("growth_cards", [&](auto &w) {
      return emit_growth_cards_fields(w, *state.cards);
    });
  if (fixed_service)
    out.object("fixed_service_boundaries", [&](auto &w) {
      for (const auto &row : state.fixed_service_boundaries.rows()) {
        if (row.first != row.second.boundary_id)
          return w.reject("GrowthFixedBoundaryKeyMismatch", row.first);
        auto status =
            complete ? emit_growth_complete_fixed_service_boundary_projection(
                           w, row.first, row.second, exact_periods, wire)
                     : emit_growth_fixed_service_boundary_projection(
                           w, row.first, row.second);
        if (!status.ok)
          return status;
      }
      return w.status();
    });
  return out.status();
}
Status emit_growth_state_projection(
    ProjectionWriter &sink, std::string_view name, const GrowthState &state,
    bool regional_life, bool cards, bool teaching_requests,
    bool study_teaching_requests, bool innate_learning, bool fixed_service,
    bool complete, bool exact_periods, bool exact_command_cuts,
    bool exact_rest_mouths, bool capability_chain, GrowthContributionWireVersion wire) {
  try {
    return sink.object(name, [&](auto &out) {
      return emit_growth_state_fields_impl(
          out, state, regional_life, cards, teaching_requests,
          study_teaching_requests, innate_learning, fixed_service, complete,
          exact_periods, exact_command_cuts, exact_rest_mouths,
          capability_chain, wire);
    });
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_growth_action_payload_fields(ProjectionWriter &out,
                                         const GrowthState &state,
                                         const ActionCoreView &action) {
  const auto *row = state.actions.find(action.id);
  if (!row || row->request.kind != action.kind)
    return out.reject("GrowthActionPayloadRequired", action.id);
  return emit_growth_action_row_fields(out, *row);
}
Status emit_growth_fact_payload_fields(
    ProjectionWriter &out, const GrowthState &state, const FactRecord &head,
    bool fixed_service, bool complete, bool exact_periods,
    bool exact_command_cuts, bool exact_rest_mouths, bool capability_chain, GrowthContributionWireVersion wire) {
  if (wire != GrowthContributionWireVersion::original && wire != GrowthContributionWireVersion::trade_grants)
    return out.reject("GrowthContributionWireVersionUnsupported", "growth");
  if (wire == GrowthContributionWireVersion::trade_grants && !capability_chain)
    return out.reject("GrowthTradeGrantPhysicalSelectionRequired", "growth");
  if (capability_chain && !exact_rest_mouths)
    return out.reject("GrowthCapabilityChainPhysicalSelectionRequired",
                      "growth");
  if (exact_rest_mouths && !exact_command_cuts)
    return out.reject("GrowthExactRestMouthPhysicalSelectionRequired");
  if (exact_command_cuts && !exact_periods)
    return out.reject("GrowthExactCommandCutsPhysicalSelectionRequired");
  if (exact_periods && (!fixed_service || !complete))
    return out.reject("GrowthExactPeriodsPhysicalSelectionRequired", head.id);
  const auto *fact = state.facts.find(head.id);
  const auto *life = state.life_activities.find(head.id);
  if (bool(fact) == bool(life))
    return out.reject("GrowthFactPayloadRequired", head.id);
  if (life) {
    if (head.kind != "life_activity" || head.domain != "growth" ||
        life->view.fact_id != head.id)
      return out.reject("GrowthFactHeaderMismatch", head.id);
    return emit_growth_life_row_fields(out, *life, fixed_service);
  }
  const bool match = std::visit(
      [&](const auto &row) {
        using T = std::decay_t<decltype(row)>;
        if constexpr (std::is_same_v<T, TrainingFact>)
          return head.kind == "training" && head.domain == "cultivation";
        else if constexpr (std::is_same_v<T, StudyFact>)
          return head.kind == "skill_study" && head.domain == "cultivation";
        else if constexpr (std::is_same_v<T, RestFact>)
          return head.kind == "long_rest" && head.domain == "services";
        else
          return head.kind == "trade_bargain" && head.domain == "economy";
      },
      *fact);
  if (!match)
    return out.reject("GrowthFactHeaderMismatch", head.id);
  return emit_growth_fact_row_fields(out, *fact, complete, exact_periods,
                                     exact_command_cuts, exact_rest_mouths,
                                     capability_chain, wire);
}
Status emit_growth_additional_state_fields(
    ProjectionWriter &out, const GrowthState &state, bool cards,
    bool fixed_service, bool complete, bool exact_periods,
    bool exact_command_cuts, bool exact_rest_mouths, bool capability_chain, GrowthContributionWireVersion wire) {
  if (wire != GrowthContributionWireVersion::original && wire != GrowthContributionWireVersion::trade_grants)
    return out.reject("GrowthContributionWireVersionUnsupported", "growth");
  if (wire == GrowthContributionWireVersion::trade_grants && !capability_chain)
    return out.reject("GrowthTradeGrantPhysicalSelectionRequired", "growth");
  if (capability_chain && !exact_rest_mouths)
    return out.reject("GrowthCapabilityChainPhysicalSelectionRequired",
                      "growth");
  if (exact_rest_mouths && !exact_command_cuts)
    return out.reject("GrowthExactRestMouthPhysicalSelectionRequired");
  if (exact_command_cuts && !exact_periods)
    return out.reject("GrowthExactCommandCutsPhysicalSelectionRequired");
  if (exact_periods && (!fixed_service || !complete))
    return out.reject("GrowthExactPeriodsPhysicalSelectionRequired");
  if (cards != state.cards.has_value())
    return out.reject(cards ? "MissingGrowthCards" : "CardsCapabilityRequired",
                      "growth_cards");
  if (cards)
    out.object("growth_cards", [&](auto &w) {
      return emit_growth_cards_fields(w, *state.cards);
    });
  if (!fixed_service && !state.fixed_service_boundaries.empty())
    return out.reject("GrowthFixedServiceCapabilityRequired",
                      "fixed_service_boundaries");
  if (fixed_service)
    out.object("fixed_service_boundaries", [&](auto &w) {
      for (const auto &row : state.fixed_service_boundaries.rows()) {
        if (row.first != row.second.boundary_id)
          return w.reject("GrowthFixedBoundaryKeyMismatch", row.first);
        auto status =
            complete ? emit_growth_complete_fixed_service_boundary_projection(
                           w, row.first, row.second, exact_periods, wire)
                     : emit_growth_fixed_service_boundary_projection(
                           w, row.first, row.second);
        if (!status.ok)
          return status;
      }
      return w.status();
    });
  return out.status();
}
static BoundaryValue encode_growth_state_impl(
    const GrowthState &state, bool regional_life, bool cards,
    bool teaching_requests, bool study_teaching_requests, bool innate_learning,
    bool fixed_service, bool complete = false, bool exact_periods = false,
    bool exact_command_cuts = false, bool exact_rest_mouths = false,
    bool capability_chain = false, GrowthContributionWireVersion wire = GrowthContributionWireVersion::original) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status = emit_growth_state_projection(
      out, "", state, regional_life, cards, teaching_requests,
      study_teaching_requests, innate_learning, fixed_service, complete,
      exact_periods, exact_command_cuts, exact_rest_mouths, capability_chain, wire);
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}

// 旧入口仍精确选原布局；selected只通过独立G36入口开启，禁止从字段存在反推版本。
Outcome<GrowthState> decode_growth_seed(const BoundaryValue &source,
                                        bool regional_life, bool cards,
                                        bool teaching_requests,
                                        bool study_teaching_requests,
                                        bool innate_learning) {
  return decode_growth_seed_impl(source, regional_life, cards,
                                 teaching_requests, study_teaching_requests,
                                 innate_learning, false);
}
BoundaryValue encode_growth_state(const GrowthState &state, bool regional_life,
                                  bool cards, bool teaching_requests,
                                  bool study_teaching_requests,
                                  bool innate_learning) {
  return encode_growth_state_impl(state, regional_life, cards,
                                  teaching_requests, study_teaching_requests,
                                  innate_learning, false);
}
Outcome<GrowthState> decode_growth_fixed_service_seed(
    const BoundaryValue &source, bool regional_life, bool cards,
    bool teaching_requests, bool study_teaching_requests,
    bool innate_learning) {
  try {
    return decode_growth_seed_impl(source, regional_life, cards,
                                   teaching_requests, study_teaching_requests,
                                   innate_learning, true);
  } catch (const std::bad_alloc &) {
    RuleError e;
    e.original = {"OutOfMemory", "growth_fixed_state"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  } catch (const std::length_error &) {
    RuleError e;
    e.original = {"SizeExhausted", "growth_fixed_state"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  }
}
Outcome<BoundaryValue>
encode_growth_fixed_service_state(const GrowthState &state, bool regional_life,
                                  bool cards, bool teaching_requests,
                                  bool study_teaching_requests,
                                  bool innate_learning) {
  try {
    return {true,
            encode_growth_state_impl(state, regional_life, cards,
                                     teaching_requests, study_teaching_requests,
                                     innate_learning, true),
            {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  } catch (const std::bad_alloc &) {
    RuleError e;
    e.original = {"OutOfMemory", "growth_fixed_state"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  } catch (const std::length_error &) {
    RuleError e;
    e.original = {"SizeExhausted", "growth_fixed_state"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  }
}
// 同一翻译单元禁止声明/定义布尔参数数目漂移成另一个重载，避免只在最终链接才暴露。
static_assert(std::is_same_v<decltype(&encode_growth_state),
                             BoundaryValue (*)(const GrowthState &, bool, bool,
                                               bool, bool, bool)>);
// 完成回执只保存实际业务结果；课程挑战的私有实例留在所属事实，有限view另由目录投影。
Status emit_growth_effect_fields(ProjectionWriter &out, const GrowthEffect &e) {
  if (e.kind.rfind("trade_bargain_", 0) == 0) {
    out.field("quote_id", e.quote_id);
    return e.kind == "trade_bargain_open" ? out.field("reopened", e.reopened)
                                          : out.field("price_applied", false);
  }
  if (e.kind == "rest_cancel" || e.kind == "recovery_cancel") {
    out.field("action_id", e.action_id);
    return out.field("elapsed", e.elapsed);
  }
  if (e.kind == "long_rest") {
    out.field("ok", true);
    out.field("completed", true);
    out.field("success", e.success);
    return out.field("code", e.code);
  }
  if (e.kind == "study_pause" || e.kind == "study_resume") {
    out.field("action_id", e.action_id);
    out.field("paused", e.paused);
    out.field("paused_ticks", e.paused_ticks);
    out.field("completed_lessons", e.completed_lessons);
    out.field("reserved", e.reserved);
    return out.order_fields(e.field_order);
  }
  if (growth_busy_command(e.kind) && e.kind != "study_cancel") {
    if (!e.public_challenge)
      return out.reject("MissingGrowthPublicChallenge", "growth_effect");
    out.field("action_id", e.action_id);
    out.object("challenge", [&](auto &child) {
      return growth::emit_growth_public_challenge_fields(child,
                                                         *e.public_challenge);
    });
    if (e.kind == "study_challenge_choose" ||
        e.kind == "study_challenge_exit") {
      out.field("challenge_kind", e.public_challenge->kind);
      out.field("price_applied", false);
    }
    return out.status();
  }
  out.field("ok", true);
  if (e.kind == "equip_weapon" || e.kind == "equip_armor" ||
      e.kind == "configure_heart")
    return out.status();
  out.field("success", e.success);
  if (e.kind == "service" || e.kind == "trade")
    return out.order_fields(e.field_order);
  if (e.already_completed) {
    out.field("already_completed", true);
    out.field("code", e.code);
    return out.order_fields(e.field_order);
  }
  out.field("code", e.code);
  out.field("skill_id", e.skill_id);
  if (e.kind == "study_skill" || e.kind == "teach_study" ||
      e.kind == "study_cancel") {
    out.field("xp_gained", e.xp_gained);
    out.field("fee_paid", e.fee_paid);
    out.field("unprovided_released", e.unprovided_released);
    out.field("completed_lessons", e.completed_lessons);
    out.text("payment_policy", "reserve_then_pay_per_lesson");
    if (e.challenge && e.challenge->result) {
      if (e.challenge->frozen.field == "etiquette")
        out.object("etiquette", [&](auto &child) {
          return growth::emit_growth_challenge_result_fields(
              child, *e.challenge->result, e.challenge->frozen.kind);
        });
      else
        out.object("challenge", [&](auto &child) {
          child.field("kind", e.challenge->frozen.kind);
          child.object("result", [&](auto &result) {
            return growth::emit_growth_challenge_result_fields(
                result, *e.challenge->result, e.challenge->frozen.kind);
          });
          return child.field("price_applied", false);
        });
    }
    if (!e.action_id.empty())
      out.field("action_id", e.action_id);
  } else
    out.field("mastery", e.mastery);
  return out.order_fields(e.field_order);
}
BoundaryValue encode_growth_effect(const GrowthEffect &e) {
  BoundaryValue value;
  BoundaryProjectionWriter writer(value);
  auto status = writer.object(
      "", [&](auto &out) { return emit_growth_effect_fields(out, e); });
  if (status.ok)
    status = writer.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}
Outcome<GrowthEffect> decode_growth_effect(const StableId &kind,
                                           const BoundaryValue &v) {
  try {
    using namespace boundary;
    GrowthEffect e;
    e.kind = kind;
    e.field_order = order(v);
    if (kind.rfind("trade_bargain_", 0) == 0) {
      e.quote_id = get_string(v, "quote_id", kind);
      if (kind == "trade_bargain_open")
        e.reopened = get_bool(v, "reopened", kind);
      return {true, std::move(e), {}};
    }
    if (kind == "recovery_cancel") {
      fields(v, {"action_id", "elapsed"}, {}, kind);
      if (object(v, kind).size() != 2)
        fail("InvalidRecoveryCancellation", kind);
      e.action_id = get_string(v, "action_id", kind);
      e.elapsed = get_int(v, "elapsed", kind);
      if (e.action_id.empty() || e.elapsed < 0 || e.elapsed > 720)
        fail("InvalidRecoveryCancellation", kind);
      return {true, std::move(e), {}};
    }
    if (kind == "rest_cancel") {
      e.action_id = get_string(v, "action_id", kind);
      e.elapsed = get_int(v, "elapsed", kind);
      return {true, std::move(e), {}};
    }
    if (kind == "study_pause" || kind == "study_resume") {
      fields(v,
             {"action_id", "paused", "paused_ticks", "completed_lessons",
              "reserved"},
             {}, kind);
      e.action_id = get_string(v, "action_id", kind);
      e.paused = get_bool(v, "paused", kind);
      e.paused_ticks = get_int(v, "paused_ticks", kind);
      e.completed_lessons = get_int(v, "completed_lessons", kind);
      e.reserved = get_int(v, "reserved", kind);
      return {true, std::move(e), {}};
    }
    if (growth_busy_command(kind) && kind != "study_cancel") {
      e.action_id = get_string(v, "action_id", kind);
      auto challenge_kind = v.find("challenge_kind")
                                ? get_string(v, "challenge_kind", kind)
                                : "etiquette";
      e.public_challenge = growth::decode_public_challenge(
          field(v, "challenge", kind), challenge_kind, kind);
      return {true, std::move(e), {}};
    }
    if (!get_bool(v, "ok", kind))
      fail("InvalidGrowthReceipt", kind);
    if (auto *x = v.find("success"))
      e.success = boolean(*x, kind);
    if (auto *x = v.find("already_completed"))
      e.already_completed = boolean(*x, kind);
    if (auto *x = v.find("code"))
      e.code = string(*x, kind);
    if (auto *x = v.find("skill_id"))
      e.skill_id = string(*x, kind);
    if (auto *x = v.find("action_id"))
      e.action_id = string(*x, kind);
#define HM_READ(name)                                                          \
  if (auto *x = v.find(#name))                                                 \
  e.name = integer(*x, kind)
    HM_READ(mastery);
    HM_READ(xp_gained);
    HM_READ(fee_paid);
    HM_READ(unprovided_released);
    HM_READ(completed_lessons);
#undef HM_READ
    if (auto *result = v.find("etiquette")) {
      GrowthChallengeInstance i;
      i.frozen.field = "etiquette";
      i.frozen.kind = "etiquette";
      i.result = growth::decode_challenge_result(*result, "etiquette", kind);
      e.challenge = std::move(i);
    }
    if (auto *result = v.find("challenge")) {
      GrowthChallengeInstance i;
      i.frozen.field = "challenge";
      i.frozen.kind = get_string(*result, "kind", kind);
      i.result = growth::decode_challenge_result(field(*result, "result", kind),
                                                 i.frozen.kind, kind);
      e.challenge = std::move(i);
    }
    return {true, std::move(e), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<GrowthState>
decode_growth_exact_command_cut_seed(const BoundaryValue &source,
                                     const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts)
    return {false,
            {},
            common_failure("GrowthExactCommandCutsCatalogRequired").error};
  return decode_growth_seed_impl(
      source, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true);
}
Status emit_growth_exact_command_cut_state_projection(
    ProjectionWriter &out, std::string_view name, const GrowthState &state,
    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts)
    return common_failure("GrowthExactCommandCutsCatalogRequired");
  return emit_growth_state_projection(
      out, name, state, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true);
}
Outcome<BoundaryValue>
encode_growth_exact_command_cut_state(const GrowthState &state,
                                      const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_exact_command_cut_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<GrowthState>
decode_growth_exact_period_seed(const BoundaryValue &source,
                                const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources || !catalog.complete_exact_period_sources)
    return {
        false, {}, common_failure("GrowthExactPeriodsCatalogRequired").error};
  return decode_growth_seed_impl(source, catalog.regional_life,
                                 catalog.cards.enabled,
                                 catalog.teaching_requests.enabled,
                                 catalog.study_teaching_requests.enabled,
                                 catalog.innate_learning, true, true, true);
}
Status emit_growth_exact_period_state_projection(ProjectionWriter &out,
                                                 std::string_view name,
                                                 const GrowthState &state,
                                                 const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources || !catalog.complete_exact_period_sources)
    return common_failure("GrowthExactPeriodsCatalogRequired");
  return emit_growth_state_projection(
      out, name, state, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true);
}
Outcome<BoundaryValue>
encode_growth_exact_period_state(const GrowthState &state,
                                 const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_exact_period_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<GrowthState>
decode_growth_complete_fixed_seed(const BoundaryValue &source,
                                  const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources)
    return {false, {}, common_failure("CompleteGrowthCatalogRequired").error};
  return decode_growth_seed_impl(source, catalog.regional_life,
                                 catalog.cards.enabled,
                                 catalog.teaching_requests.enabled,
                                 catalog.study_teaching_requests.enabled,
                                 catalog.innate_learning, true, true);
}
Status emit_growth_complete_fixed_state_projection(
    ProjectionWriter &out, std::string_view name, const GrowthState &state,
    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources)
    return common_failure("CompleteGrowthCatalogRequired");
  return emit_growth_state_projection(out, name, state, catalog.regional_life,
                                      catalog.cards.enabled,
                                      catalog.teaching_requests.enabled,
                                      catalog.study_teaching_requests.enabled,
                                      catalog.innate_learning, true, true);
}
Outcome<BoundaryValue>
encode_growth_complete_fixed_state(const GrowthState &state,
                                   const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_complete_fixed_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}

Outcome<GrowthState>
decode_growth_exact_rest_mouth_seed(const BoundaryValue &source,
                                    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources)
    return {
        false, {}, common_failure("GrowthExactRestMouthCatalogRequired").error};
  return decode_growth_seed_impl(
      source, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true);
}
Status emit_growth_exact_rest_mouth_state_projection(
    ProjectionWriter &out, std::string_view name, const GrowthState &state,
    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources)
    return common_failure("GrowthExactRestMouthCatalogRequired");
  return emit_growth_state_projection(
      out, name, state, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true);
}
Outcome<BoundaryValue>
encode_growth_exact_rest_mouth_state(const GrowthState &state,
                                     const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_exact_rest_mouth_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}

} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthState>
decode_growth_capability_chain_seed(const BoundaryValue &source,
                                    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources ||
      !catalog.complete_capability_application_chain)
    return {false,
            {},
            common_failure("GrowthCapabilityChainCatalogRequired").error};
  return decode_growth_seed_impl(
      source, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true, true);
}
Status emit_growth_capability_chain_state_projection(
    ProjectionWriter &out, std::string_view name, const GrowthState &state,
    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources ||
      !catalog.complete_capability_application_chain)
    return common_failure("GrowthCapabilityChainCatalogRequired");
  return emit_growth_state_projection(
      out, name, state, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true, true);
}
Outcome<BoundaryValue>
encode_growth_capability_chain_state(const GrowthState &state,
                                     const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_capability_chain_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
// 显式 physical base+192，不在旧 Growth 表追加 grant。
Outcome<GrowthState>
decode_growth_trade_grant_seed(const BoundaryValue &source,
                                    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources ||
      !catalog.complete_capability_application_chain)
    return {false,
            {},
            common_failure("GrowthCapabilityChainCatalogRequired").error};
  return decode_growth_seed_impl(
      source, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true, true, GrowthContributionWireVersion::trade_grants);
}
Status emit_growth_trade_grant_state_projection(
    ProjectionWriter &out, std::string_view name, const GrowthState &state,
    const GrowthCatalog &catalog) {
  if (!catalog.complete_study_sources ||
      !catalog.complete_exact_period_sources ||
      !catalog.complete_exact_command_cuts ||
      !catalog.complete_exact_rest_mouth_sources ||
      !catalog.complete_capability_application_chain)
    return common_failure("GrowthCapabilityChainCatalogRequired");
  return emit_growth_state_projection(
      out, name, state, catalog.regional_life, catalog.cards.enabled,
      catalog.teaching_requests.enabled,
      catalog.study_teaching_requests.enabled, catalog.innate_learning, true,
      true, true, true, true, true, GrowthContributionWireVersion::trade_grants);
}
Outcome<BoundaryValue>
encode_growth_trade_grant_state(const GrowthState &state,
                                     const GrowthCatalog &catalog) {
  try {
    BoundaryValue value;
    BoundaryProjectionWriter out(value);
    auto status =
        emit_growth_trade_grant_state_projection(out, "", state, catalog);
    if (status.ok)
      status = out.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
}
