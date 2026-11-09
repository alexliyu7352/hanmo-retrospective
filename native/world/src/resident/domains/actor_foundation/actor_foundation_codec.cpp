#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "frozen_condition_codec_fields.hpp"
#include "initial_condition_seed_codec_fields.hpp"
#include "injury_effect_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/growth/growth_capability_application_codec.hpp"
#include <type_traits>
// 成对固定字段访客；只接本首片已实现分支，缺字段、未知tag与非空未接表均拒绝。
#include "actor_foundation_codec.hpp"
#include "actor_foundation_json_visitor.hpp"
#include "actor_foundation_scoped_codec.hpp"
#include "condition_head_codec_fields.hpp"
#include "military_condition.hpp"
#include "resident/domains/family/family_life_exact_codec_detail.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
namespace {
// 字符串数组沿原真实顺序；JSON与binary各用相同typed访问。
template <class C>
Status strings(C &c, const std::string &name, std::vector<StableId> &rows) {
  return c.sequence(name, rows, [](C &v, StableId &value) {
    return v.field("value", value);
  });
}
// 原OrderedTable的索引不存档，JSON按原键顺序，binary复用原表访客。
template <class T, class F>
Status table(CodecVisitor &c, const std::string &name, OrderedTable<T> &value,
             F visit) {
  return common_checkpoint::table(c, name, value, visit);
}
// 新JSON段沿原有序字典，不把表额外包成可变运行JSON。
template <class T, class F>
Status table(ActorFoundationJsonVisitor &c, const std::string &name,
             OrderedTable<T> &value, F visit) {
  return c.table(name, value, visit);
}

template <class T, class F>
Status table(ActorFoundationProjectionArchive &c, const StableId &name,
             OrderedTable<T> &rows, F visit) {
  return c.table(name, rows, visit);
}
// 具名规则完整身份。
template <class C> Status rule(C &c, ActorFoundationRuleRef &r) {
  c.field("id", r.id);
  c.field("version", r.version);
  return c.field("catalog_digest", r.catalog_digest);
}
template <class C> Status source(C &c, SourceRef &r) {
  c.field("domain", r.domain);
  c.field("kind", r.kind);
  return c.field("id", r.id);
}
// 原冷源复合键不压成字符串。
template <class C> Status origin(C &c, ScenarioOriginRef &r) {
  c.field("catalog_digest", r.catalog_digest);
  c.field("scenario_id", r.scenario_id);
  c.field("row_key", r.row_key);
  return c.field("initial_tick", r.initial_tick);
}
// 只读引用保所属源种类。
template <class C> Status seed_ref(C &c, InitialConditionSeedRef &r) {
  return c.field("seed_key", r.seed_key);
}
// 条件结果引用不冒Common fact头。
template <class C> Status receipt_ref(C &c, ConditionReceiptRefV2 &r) {
  return c.field("result_id", r.result_id);
}
template <class C>
Status capability_condition_ref(C &c, CharacterConditionSourceRef &r,
                                bool complete) {
  Integer tag = static_cast<Integer>(r.index());
  if (complete)
    c.field("tag", tag);
  else if (!c.reading() && tag != 0)
    return c.reject("CompleteCapabilitySourceCodecRequired");
  else
    tag = 0;
  if (tag < 0 || tag > 1)
    return c.reject("CapabilityConditionSourceTagInvalid");
  if (c.reading())
    r = tag == 0 ? CharacterConditionSourceRef{InitialConditionSeedRef{}}
                 : CharacterConditionSourceRef{ConditionReceiptRefV2{}};
  return tag == 0 ? seed_ref(c, std::get<InitialConditionSeedRef>(r))
                  : receipt_ref(c, std::get<ConditionReceiptRefV2>(r));
}
// 未接业务根保存在且空；非空两向均不能伪成功。
template <class C>
Status empty_rows(C &c, const StableId &name,
                  UnimplementedActorFoundationRows &rows) {
  return c.sequence(name, rows, [](C &v, auto &) {
    return v.reject("UnsupportedActorFoundationRecord");
  });
}
// 顶层原OrderedTable的空形状也保真，未知非空record没有空成功路径。
template <class C, class T>
Status empty_table(C &c, const StableId &name, OrderedTable<T> &rows) {
  return table(c, name, rows, [](C &v, auto &) {
    return v.reject("UnsupportedActorFoundationRecord");
  });
}
// 规范精确Moment复用K2数值叶，拒绝非正规分数。
template <class C> Status moment(C &c, WorldMoment &m) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    (void)0;
  EncodedWorldMoment e;
  if (!c.reading()) {
    auto x = encode_world_moment(m);
    if (!x.ok)
      return {false, {}, x.error};
    e = std::move(x.value);
  }
  c.field("half_day_tick", e.half_day_tick);
  c.object("sub_half_day", [&](C &v) {
    v.field("numerator", e.sub_half_day.numerator);
    return v.field("denominator", e.sub_half_day.denominator);
  });
  if (c.reading() && c.status().ok) {
    auto x = decode_world_moment(e);
    if (!x.ok)
      return {false, {}, x.error};
    m = std::move(x.value);
  }
  return c.status();
}
// 原At联合保显式精度，不从legacy补0。
template <class C> Status at(C &c, ActorFoundationAt &a) {
  Integer tag = static_cast<Integer>(a.kind);
  c.field("tag", tag);
  if (tag != 1)
    return c.reject("UnsupportedActorFoundationTimePrecision");
  if (c.reading()) {
    a.kind = ActorFoundationAt::Kind::exact_moment;
    a.legacy_tick = 0;
  } else if (a.legacy_tick != 0)
    return c.reject("InactiveActorFoundationTimePayload");
  return c.object("exact_moment",
                  [&](C &v) { return moment(v, a.exact_moment); });
}
// 完整目录身份保原字段与模块有序列表。
template <class C> Status catalog_identity(C &c, CatalogIdentity &x) {
  c.field("content_id", x.content_id);
  c.field("content_version", x.content_version);
  c.field("content_digest", x.content_digest);
  c.field("ruleset_id", x.ruleset_id);
  c.field("schema_sha256", x.schema_sha256);
  c.optional("pack_set_digest", x.pack_set_digest,
             [](C &v, auto &z) { return v.field("value", z); });
  c.sequence("module_versions", x.module_versions, [](C &v, auto &z) {
    v.field("name", z.first);
    return v.field("version", z.second);
  });
  return c.field("artifact_sha256", x.artifact_sha256);
}
// 首片声明整岁完整四字段；未知生日源不吞掉。
template <class C> Status age(C &c, AuthoredActorAgeBasis &a) {
  Integer tag = static_cast<Integer>(a.kind);
  c.field("tag", tag);
  if (tag != 1)
    return c.reject("UnsupportedAuthoredAgeBasis");
  if (c.reading()) {
    a.kind = AuthoredActorAgeBasis::Kind::declared_age_at_start;
    a.birth_tick = 0;
    a.birth_source_ref.clear();
  } else if (a.birth_tick != 0 || !a.birth_source_ref.empty())
    return c.reject("InactiveAuthoredAgePayload");
  c.field("years", a.years);
  c.field("as_of_tick", a.as_of_tick);
  c.field("calendar_year_ordinal", a.calendar_year_ordinal);
  return c.object("source", [&](C &v) { return origin(v, a.source); });
}
// 当前只接真实场所；其他placement必须有相应owner后才支持。
template <class C> Status placement(C &c, AuthoredStartPlacement &p) {
  Integer tag = static_cast<Integer>(p.kind);
  c.field("tag", tag);
  if (tag != 0)
    return c.reject("UnsupportedAuthoredStartPlacement");
  if (c.reading())
    p.kind = AuthoredStartPlacement::Kind::at_place;
  return c.field("place_ref", p.place_ref);
}
// 初始条件的完整原槽顺序。
template <class C> Status seed(C &c, InitialConditionSeed &s) {
  c.field("seed_key", s.seed_key);
  c.field("actor_id", s.actor_id);
  c.object("source", [&](C &v) {
    if (v.reading())
      s.source = ScenarioOriginRef{};
    auto *original = std::get_if<ScenarioOriginRef>(&s.source);
    return original
               ? origin(v, *original)
               : v.reject("DynamicInitialConditionSeedRequiresStartContext3");
  });
  c.field("hp", s.hp);
  c.field("fatigue", s.fatigue);
  c.field("injury", s.injury);
  empty_rows(c, "episodes", s.episodes);
  return c.object("activity_limit_profile",
                  [&](C &v) { return rule(v, s.activity_limit_profile); });
}
// 初态绑定引用只允许本片真实已有源，其余字段空值也不省。
template <class C> Status binding(C &c, ActorDomainBinding &b) {
  c.field("actor_id", b.actor_id);
  c.object("scenario_origin_ref",
           [&](C &v) { return origin(v, b.scenario_origin_ref); });
  c.object("identity_definition_ref",
           [&](C &v) { return rule(v, b.identity_definition_ref); });
  c.object("age_basis_ref",
           [&](C &v) { return v.field("actor_id", b.age_basis_ref.actor_id); });
  c.sequence("condition_seed_refs", b.condition_seed_refs, seed_ref<C>);
  c.object("placement_ref",
           [&](C &v) { return placement(v, b.placement_ref); });
  c.optional("activity_ref", b.activity_ref, [](C &v, auto &) {
    return v.reject("UnsupportedAuthoredActivitySeed");
  });
  c.optional("custody_ref", b.custody_ref, [](C &v, auto &) {
    return v.reject("UnsupportedAuthoredCustodySeed");
  });
  for (auto item : {std::pair<const char *, std::vector<StableId> *>{
                        "service_refs", &b.service_refs},
                    {"office_refs", &b.office_refs},
                    {"property_business_refs", &b.property_business_refs},
                    {"initial_knowledge_refs", &b.initial_knowledge_refs}})
    c.sequence(item.first, *item.second, [](C &v, auto &) {
      return v.reject("UnsupportedAuthoredDomainSeed");
    });
  return c.object("appearance_source_ref",
                  [&](C &v) { return rule(v, b.appearance_source_ref); });
}
// Authored origin完整十字段，来源kind不可偷换为动态登记。
template <class C> Status character_origin(C &c, CharacterOriginRecordV2 &o) {
  c.field("actor_id", o.actor_id);
  Integer kind = static_cast<Integer>(o.origin_kind);
  c.field("origin_kind", kind);
  if (kind != 0)
    return c.reject("UnsupportedCharacterOriginKind");
  if (c.reading()) {
    o.origin_kind = CharacterOriginRecordV2::Kind::authored;
    o.source = AuthoredCharacterSource{};
    o.initial_capability_basis = AuthoredInitialCapabilityBasis{};
  } else if (o.source.index() != 0 || o.initial_capability_basis.index() != 0)
    return c.reject("ActorOriginProfileMismatch");
  c.optional("origin_fact_ref", o.origin_fact_ref, [](C &v, auto &) {
    return v.reject("UnsupportedAuthoredCreationFact");
  });
  c.object("created_at", [&](C &v) { return at(v, o.created_at); });
  c.object("age_basis_ref", [&](C &v) {
    Integer tag = static_cast<Integer>(o.age_basis_ref.kind);
    v.field("tag", tag);
    if (tag != 0)
      return v.reject("UnsupportedCharacterAgeBasisRef");
    if (v.reading())
      o.age_basis_ref.kind = CharacterAgeBasisRef::Kind::actor_start;
    return v.field("actor_id", o.age_basis_ref.actor_start.actor_id);
  });
  c.optional("actor_start_context_ref", o.actor_start_context_ref, origin<C>);
  c.object("source", [&](C &v) {
    Integer tag = 0;
    v.field("tag", tag);
    if (tag)
      return v.reject("UnsupportedCharacterOriginSource");
    v.object("actor_start_context_ref", [&](C &w) {
      return origin(
          w,
          std::get<AuthoredCharacterSource>(o.source).actor_start_context_ref);
    });
    v.field("authored_actor_id",
            std::get<AuthoredCharacterSource>(o.source).authored_actor_id);
    return v.object("prototype_ref", [&](C &w) {
      return rule(w, std::get<AuthoredCharacterSource>(o.source).prototype_ref);
    });
  });
  c.object("initial_capability_basis", [&](C &v) {
    Integer tag = 0;
    v.field("tag", tag);
    if (tag)
      return v.reject("UnsupportedInitialCapabilityBasis");
    v.object("catalog_identity", [&](C &w) {
      return catalog_identity(w, std::get<AuthoredInitialCapabilityBasis>(
                                     o.initial_capability_basis)
                                     .catalog_identity);
    });
    return v.object("prototype_ref", [&](C &w) {
      return rule(w, std::get<AuthoredInitialCapabilityBasis>(
                         o.initial_capability_basis)
                         .prototype_ref);
    });
  });
  c.object("initial_condition_seed_ref",
           [&](C &v) { return seed_ref(v, o.initial_condition_seed_ref); });
  return c.object("provenance_policy_ref",
                  [&](C &v) { return rule(v, o.provenance_policy_ref); });
}
// 当前head不保存身体镜像。
template <class C> Status head(C &c, ActorConditionHead &h) {
  return actor_condition_codec::head(c, h);
}
// ConditionReceipt/2的完整字段序；未选结果分支不能靠空payload通过。
template <class C>
Status receipt(C &c, ConditionReceiptV2 &r, ConditionWireProfile profile) {
  const bool injury_sources = profile == ConditionWireProfile::injury_source_chain;
  const bool full = profile != ConditionWireProfile::legacy_rest_only;
  // Explicit write rejection precedes the old finite-union dispatch below.
  if (!c.reading() && !injury_sources &&
      (std::holds_alternative<GatherFatigueConditionCauseV1>(r.cause) ||
       std::holds_alternative<GatherFatigueConditionBusinessKeyV1>(r.business_key) ||
       std::holds_alternative<CharacterInjurySourceRefV1>(r.cause) ||
       std::holds_alternative<CharacterInjuryConditionBusinessKeyV1>(r.business_key)))
    return c.reject("ConditionInjurySourcePhysical8Required");
  const bool chain =
      profile == ConditionWireProfile::capability_application_chain ||
      profile == ConditionWireProfile::trade_grant_capability_chain ||
      profile == ConditionWireProfile::injury_source_chain ||
      profile == ConditionWireProfile::purpose_capacity_transition ||
      profile == ConditionWireProfile::named_custody_availability ||
      profile == ConditionWireProfile::injury_effect_sources;
  if (!full) {
    if (!c.reading() &&
        (!std::holds_alternative<RestConditionBusinessKey>(r.business_key) ||
         !std::holds_alternative<OriginalRestProvidedCause>(r.cause)))
      return c.reject("LegacyRestConditionUnionDenied");
    if (c.reading()) {
      r.business_key = RestConditionBusinessKey{};
      r.cause = OriginalRestProvidedCause{};
    }
  }
  c.field("result_id", r.result_id);
  c.field("actor_id", r.actor_id);
  c.object("business_key", [&](C &v) {
    Integer tag =
        std::holds_alternative<GatherFatigueConditionBusinessKeyV1>(r.business_key) ? 8
        : std::holds_alternative<CharacterInjuryConditionBusinessKeyV1>(r.business_key) ? 7
        : std::holds_alternative<RestConditionBusinessKey>(r.business_key)     ? 2
        : std::holds_alternative<TravelConditionBusinessKey>(r.business_key) ? 0
        : std::holds_alternative<WorkOrStudyConditionBusinessKey>(
              r.business_key)
            ? 1
        : std::holds_alternative<ProvisionShortfallConditionBusinessKey>(
              r.business_key)
            ? 6
            : std::get<LifeTerminationConditionBusinessKey>(r.business_key)
                  .cause_tag;
    if (full)
      v.field("tag", tag);
    else
      tag = 2;
    if ((tag != 0 && tag != 1 && tag != 2 && tag != 4 && tag != 6 &&
         tag != 12 && tag != 13 && tag != 7 && tag != 8) ||
        (tag == 6 && !chain) || ((tag == 7 || tag == 8) && !injury_sources))
      return v.reject("UnsupportedConditionBusinessKey");
    if (v.reading())
      r.business_key =
          tag == 8 ? CharacterConditionBusinessKey{GatherFatigueConditionBusinessKeyV1{}}
          : tag == 7 ? CharacterConditionBusinessKey{CharacterInjuryConditionBusinessKeyV1{}}
          : tag == 0 ? CharacterConditionBusinessKey{TravelConditionBusinessKey{}}
          : tag == 1
              ? CharacterConditionBusinessKey{WorkOrStudyConditionBusinessKey{}}
          : tag == 2 ? CharacterConditionBusinessKey{RestConditionBusinessKey{}}
          : tag == 6
              ? CharacterConditionBusinessKey{ProvisionShortfallConditionBusinessKey{}}
              : CharacterConditionBusinessKey{
                    LifeTerminationConditionBusinessKey{tag}};
    if (tag == 8) {
      auto &x = std::get<GatherFatigueConditionBusinessKeyV1>(r.business_key);
      v.field("receipt_id", x.receipt_id); v.field("actor_id", x.actor_id);
      return x.receipt_id.empty() || x.actor_id.empty() ? v.reject("GatherConditionKeyRequired") : v.status();
    }
    if (tag == 7) {
      auto &x = std::get<CharacterInjuryConditionBusinessKeyV1>(r.business_key);
      v.object("source_ref", [&](C &w) {
        w.field("result_id", x.source_ref.result_id);
        return x.source_ref.result_id.empty() ? w.reject("CharacterInjurySourceRefRequired") : w.status();
      });
      v.field("actor_id", x.actor_id);
      return x.actor_id.empty() ? v.reject("CharacterInjuryActorRequired") : v.status();
    }
    if (tag == 0) {
      auto &x = std::get<TravelConditionBusinessKey>(r.business_key);
      v.field("unit_ref", x.unit_ref);
      v.field("actor_id", x.actor_id);
      return v.field("exertion_source_id", x.exertion_source_id);
    }
    if (tag == 1) {
      auto &x = std::get<WorkOrStudyConditionBusinessKey>(r.business_key);
      v.field("action_id", x.action_id);
      v.field("actor_id", x.actor_id);
      return v.field("ordinal", x.ordinal);
    }
    if (tag == 2) {
      auto &x = std::get<RestConditionBusinessKey>(r.business_key);
      v.field("action_id", x.action_id);
      v.field("actor_id", x.actor_id);
      return v.field("completion_ordinal", x.completion_ordinal);
    }
    if (tag == 6) {
      auto &x =
          std::get<ProvisionShortfallConditionBusinessKey>(r.business_key);
      v.object("original_supply_ref",
               [&](C &w) { return source(w, x.original_supply_ref); });
      v.field("source_schema", x.source_schema);
      v.field("source_ordinal", x.source_ordinal);
      v.field("actor_id", x.actor_id);
      Integer purpose = static_cast<Integer>(x.purpose);
      v.field("purpose", purpose);
      if (purpose != 0 || x.original_supply_ref.domain != "military" ||
          x.original_supply_ref.kind != "military_supply" ||
          x.original_supply_ref.id.empty() ||
          (x.source_schema != 2 && !(injury_sources && x.source_schema == 1)) ||
          x.source_ordinal < 0 || x.actor_id.empty())
        return v.reject("InvalidSupplyShortfallBusinessKey");
      if (v.reading())
        x.purpose = ProvisionShortfallConditionBusinessKey::Purpose::
            military_supply_shortage;
      return v.status();
    }
    auto &x = std::get<LifeTerminationConditionBusinessKey>(r.business_key);
    v.field("source_id", x.source_id);
    v.field("actor_id", x.actor_id);
    return v.field("source_ordinal", x.source_ordinal);
  });
  c.object("cause", [&](C &v) {
    Integer tag =
        std::holds_alternative<GatherFatigueConditionCauseV1>(r.cause) ? 8
        : std::holds_alternative<CharacterInjurySourceRefV1>(r.cause)        ? 7
        : std::holds_alternative<OriginalRestProvidedCause>(r.cause)          ? 2
        : std::holds_alternative<TravelExertionConditionCause>(r.cause)     ? 0
        : std::holds_alternative<WorkOrStudyConditionCause>(r.cause)        ? 1
        : std::holds_alternative<MilitaryCasualtyConditionCause>(r.cause)   ? 4
        : std::holds_alternative<ProvisionShortfallConditionCause>(r.cause) ? 6
        : std::get<LifeTerminationConditionCause>(r.cause).source.index() == 0
            ? 4
        : std::get<LifeTerminationConditionCause>(r.cause).source.index() == 1
            ? 12
            : 13;
    v.field("tag", tag);
    if ((!full && tag != 2) ||
        (tag != 0 && tag != 1 && tag != 2 && tag != 4 && tag != 6 &&
         tag != 12 && tag != 13 && tag != 7 && tag != 8) ||
        (tag == 6 && !chain) || ((tag == 7 || tag == 8) && !injury_sources))
      return v.reject("UnsupportedConditionCause");
    if (v.reading())
      r.cause =
          tag == 8 ? CharacterConditionCause{GatherFatigueConditionCauseV1{}}
          : tag == 7   ? CharacterConditionCause{CharacterInjurySourceRefV1{}}
          : tag == 0   ? CharacterConditionCause{TravelExertionConditionCause{}}
          : tag == 1 ? CharacterConditionCause{WorkOrStudyConditionCause{}}
          : tag == 2 ? CharacterConditionCause{OriginalRestProvidedCause{}}
          : tag == 4 ? CharacterConditionCause{MilitaryCasualtyConditionCause{}}
          : tag == 6
              ? CharacterConditionCause{ProvisionShortfallConditionCause{}}
              : CharacterConditionCause{LifeTerminationConditionCause{}};
    if (tag == 8) {
      auto &x = std::get<GatherFatigueConditionCauseV1>(r.cause);
      v.field("receipt_id", x.receipt_id);
      return x.receipt_id.empty() ? v.reject("GatherConditionSourceRequired") : v.status();
    }
    if (tag == 7) {
      auto &x = std::get<CharacterInjurySourceRefV1>(r.cause);
      v.field("result_id", x.result_id);
      return x.result_id.empty() ? v.reject("CharacterInjurySourceRefRequired") : v.status();
    }
    if (tag == 0) {
      auto &x = std::get<TravelExertionConditionCause>(r.cause);
      v.field("exertion_source_id", x.exertion_source_id);
      return v.field("actor_id", x.actor_id);
    }
    if (tag == 1) {
      auto &x = std::get<WorkOrStudyConditionCause>(r.cause);
      Integer kind = static_cast<Integer>(x.kind);
      v.field("result_kind", kind);
      if (kind < 0 || kind > 3 || (kind == 3 && !chain))
        return v.reject("InvalidWorkStudyConditionKind");
      if (v.reading())
        x.kind = static_cast<WorkOrStudyConditionCause::Kind>(kind);
      v.field("action_id", x.action_id);
      v.field("actor_id", x.actor_id);
      v.field("ordinal", x.ordinal);
      return x.action_id.empty() || x.actor_id.empty() || x.ordinal < 1
                 ? v.reject("InvalidWorkStudyConditionSource")
                 : v.status();
    }
    if (tag == 2) {
      auto &x = std::get<OriginalRestProvidedCause>(r.cause);
      v.field("action_id", x.action_id);
      v.field("rest_kind", x.rest_kind);
      v.field("provider_actor_id", x.provider_actor_id);
      v.field("site_id", x.site_id);
      v.field("provided_half_days", x.provided_half_days);
      return v.field("service_fulfillment_ref", x.service_fulfillment_ref);
    }
    if (tag == 4) {
      if (!v.reading() &&
          std::holds_alternative<LifeTerminationConditionCause>(r.cause))
        return v.reject("ExplicitMilitaryConditionDispositionRequired");
      auto &m = std::get<MilitaryCasualtyConditionCause>(r.cause);
      Integer disposition = static_cast<Integer>(m.disposition);
      v.field("disposition", disposition);
      if (disposition < 0 || disposition > 1)
        return v.reject("InvalidMilitaryConditionDisposition");
      if (v.reading())
        m.disposition =
            static_cast<MilitaryCasualtyConditionCause::Disposition>(
                disposition);
      v.field("casualty_fact_id", m.casualty_fact_id);
      v.field("battle_id", m.battle_id);
      v.field("actor_id", m.actor_id);
      v.object("occurrence_at",
               [&](C &w) { return moment(w, m.occurrence_at); });
      v.field("source_schema", m.source_schema);
      v.field("source_ordinal", m.source_ordinal);
      if (m.casualty_fact_id.empty() || m.battle_id.empty() ||
          m.actor_id.empty() ||
          (m.source_schema != 2 &&
           !(injury_sources && m.source_schema == 1 &&
             m.disposition == MilitaryCasualtyConditionCause::Disposition::dead)) ||
          m.source_ordinal < 0)
        return v.reject("InvalidMilitaryConditionSource");
      return v.status();
    }
    if (tag == 6) {
      auto &x = std::get<ProvisionShortfallConditionCause>(r.cause);
      v.object("original_supply_ref",
               [&](C &w) { return source(w, x.original_supply_ref); });
      v.field("source_schema", x.source_schema);
      v.field("source_ordinal", x.source_ordinal);
      v.field("actor_id", x.actor_id);
      return x.original_supply_ref.domain != "military" ||
                     x.original_supply_ref.kind != "military_supply" ||
                     x.original_supply_ref.id.empty() ||
                     (x.source_schema != 2 &&
                      !(injury_sources && x.source_schema == 1)) ||
                     x.source_ordinal < 0 || x.actor_id.empty()
                 ? v.reject("InvalidSupplyShortfallConditionCause")
                 : v.status();
    }
    auto &x = std::get<LifeTerminationConditionCause>(r.cause);
    v.object("source",
             [&](C &w) { return family_life4_codec::cause(w, x.source); });
    v.field("source_schema", x.source_schema);
    v.field("source_ordinal", x.source_ordinal);
    if (x.source.index() != std::size_t(tag == 4    ? 0
                                        : tag == 12 ? 1
                                                    : 2) ||
        x.source_schema < 1 || x.source_ordinal < 0)
      return v.reject("InvalidLifeTerminationConditionCause");
    return v.status();
  });
  c.object("rule_ref", [&](C &v) { return rule(v, r.rule_ref); });
  c.object("effective_at", [&](C &v) { return at(v, r.effective_at); });
  c.field("condition_revision_before", r.condition_revision_before);
  c.field("condition_revision_after", r.condition_revision_after);
  c.sequence("scalar_changes", r.scalar_changes,
             [](C &v, ConditionScalarChange &x) {
               Integer tag = static_cast<Integer>(x.field);
               v.field("field", tag);
               if (tag < 0 || tag > 3)
                 return v.reject("InvalidConditionScalarField");
               if (v.reading())
                 x.field = static_cast<ConditionScalarChange::Field>(tag);
               v.field("before", x.before);
               return v.field("after", x.after);
             });
  c.object("selected_injury_result", [&](C &v) {
    return injury_effect_codec::selected(
        v, r.selected_injury_result, full,
        profile == ConditionWireProfile::injury_effect_sources || injury_sources);
  });
  for (auto item : {std::pair<const char *, std::vector<StableId> *>{
                        "created_effect_keys", &r.created_effect_keys},
                    {"recovery_resolution_refs", &r.recovery_resolution_refs}})
    c.sequence(item.first, *item.second, [&](C &v, StableId &id) {
      if (profile != ConditionWireProfile::injury_effect_sources && !injury_sources)
        return v.reject("UnsupportedConditionEffect");
      v.field("value", id);
      return id.empty() ? v.reject("ConditionEffectSourceRequired")
                        : v.status();
    });
  c.optional("life_transition_ref", r.life_transition_ref,
             [&](C &v, StableId &id) {
               if (!full)
                 return v.reject("UnsupportedRestLifeTransition");
               v.field("value", id);
               return id.empty() ? v.reject("InvalidConditionLifeTransition")
                                 : v.status();
             });
  c.sequence(
      "capacity_transition_refs", r.capacity_transition_refs,
      [&](C &v, StableId &id) {
        if (profile != ConditionWireProfile::purpose_capacity_transition &&
            profile != ConditionWireProfile::named_custody_availability &&
            profile != ConditionWireProfile::injury_effect_sources &&
            profile != ConditionWireProfile::injury_source_chain)
          return v.reject("ConditionCapacityPhysical4Required");
        v.field("value", id);
        return id.empty() || id.size() > 256
                   ? v.reject("InvalidConditionCapacityTransitionRef")
                   : v.status();
      });
  if (chain) {
    c.optional("capability_application", r.capability_application,
               [&](auto &v, auto &x) {
                 return growth_capability_codec::application(v, x,
                     (profile == ConditionWireProfile::trade_grant_capability_chain || injury_sources)
                       ? GrowthContributionWireVersion::trade_grants
                       : GrowthContributionWireVersion::original);
               });
    if (c.status().ok && !r.capability_application)
      return c.reject("ConditionCapabilityApplicationRequired");
  } else if (r.capability_application)
    return c.reject("ConditionCapabilityApplicationPhysicalSelectionRequired");
  return c.status();
}
template <class C>
Status start_context(C &c, ActorStartContextStateV2 &s,
                     bool dynamic_seed = false) {
  c.sequence("scenario_origins", s.scenario_origins,
             [](C &v, ScenarioOriginRecord &r) {
               origin(v, r.ref);
               return v.object("construction_profile_ref", [&](C &w) {
                 return rule(w, r.construction_profile_ref);
               });
             });
  table(c, "actor_age_bases", s.actor_age_bases,
        [](C &v, ActorAgeBasisRecord &r) {
          v.field("actor_id", r.actor_id);
          v.object("scenario_origin_ref",
                   [&](C &w) { return origin(w, r.scenario_origin_ref); });
          return v.object("age_basis",
                          [&](C &w) { return age(w, r.age_basis); });
        });
  table(c, "initial_condition_seeds", s.initial_condition_seeds,
        [&](C &v, InitialConditionSeed &value) {
          return dynamic_seed ? initial_condition_seed_codec::seed3(v, value)
                              : seed(v, value);
        });
  table(c, "actor_domain_bindings", s.actor_domain_bindings, binding<C>);
  return table(
      c, "construction_receipts", s.construction_receipts,
      [](C &v, ActorConstructionReceipt &r) {
        v.object("scenario_origin_ref",
                 [&](C &w) { return origin(w, r.scenario_origin_ref); });
        v.object("construction_profile_ref",
                 [&](C &w) { return rule(w, r.construction_profile_ref); });
        strings(v, "constructed_actor_ids", r.constructed_actor_ids);
        v.sequence("initial_source_refs", r.initial_source_refs, seed_ref<C>);
        return v.field("initialization_result_ref",
                       r.initialization_result_ref);
      });
}
template <class C>
Status origin_records(
    C &c, CharacterOriginRecordsStateV1 &s,
    ActorOriginWireProfile profile = ActorOriginWireProfile::authored_only) {
  const bool full = profile == ActorOriginWireProfile::complete_origin2;
  table(c, "origins", s.origins, [&](auto &v, auto &o) {
    return full ? character_origin_codec::origin(v, o, profile)
                : character_origin(v, o);
  });
  table(c, "registration_sources", s.registration_sources,
        [&](auto &v, auto &r) {
          return full ? character_origin_codec::registration(v, r)
                      : v.reject("UnsupportedActorRegistrationSource");
        });
  return table(c, "allocation_sources", s.allocation_sources,
               [&](auto &v, auto &r) {
                 return full ? character_origin_codec::allocation(v, r)
                             : v.reject("UnsupportedActorAllocationSource");
               });
}
template <class C>
Status capacity_transition(C &c, CapacityTransitionV2 &value,
                           ConditionWireProfile profile) {
  c.field("transition_id", value.transition_id);
  c.field("actor_id", value.actor_id);
  c.object("purpose", [&](C &v) { return rule(v, value.purpose); });
  c.field("from", value.from);
  c.field("to", value.to);
  c.object("effective_at", [&](C &v) { return at(v, value.effective_at); });
  c.optional("expected_recovery_at", value.expected_recovery_at,
             [&](C &v, ActorFoundationAt &x) { return at(v, x); });
  c.object("cause_ref", [&](C &v) {
    Integer tag = static_cast<Integer>(value.cause_ref.index());
    if ((profile == ConditionWireProfile::purpose_capacity_transition ||
         profile == ConditionWireProfile::named_custody_availability ||
         profile == ConditionWireProfile::injury_effect_sources ||
         profile == ConditionWireProfile::injury_source_chain))
      v.field("tag", tag);
    else if (!v.reading() && tag != 0)
      return v.reject("ConditionCapacityPhysical4Required");
    else
      tag = 0;
    if (tag < 0 ||
        tag > (profile == ConditionWireProfile::named_custody_availability ? 3
                                                                           : 1))
      return v.reject("InvalidCapacityTransitionCauseTag");
    if (v.reading()) {
      if (tag == 0)
        value.cause_ref.emplace<MilitaryAvailabilityCauseV2>();
      else if (tag == 1)
        value.cause_ref.emplace<ConditionPurposeCapacityCause>();
      else if (tag == 2)
        value.cause_ref.emplace<NamedMilitaryCaptureAvailabilityCause>();
      else
        value.cause_ref.emplace<NamedMilitaryReleaseAvailabilityCause>();
    }
    if (tag == 2) {
      auto &original =
          std::get<NamedMilitaryCaptureAvailabilityCause>(value.cause_ref);
      v.object("term_source",
               [&](C &x) { return source(x, original.term_source); });
      v.field("source_schema", original.source_schema);
      return v.optional(
          "predecessor_transition_id", original.predecessor_transition_id,
          [&](C &x, StableId &id) { return x.field("value", id); });
    }
    if (tag == 3) {
      auto &original =
          std::get<NamedMilitaryReleaseAvailabilityCause>(value.cause_ref);
      v.object("release_source",
               [&](C &x) { return source(x, original.release_source); });
      v.field("source_schema", original.source_schema);
      return v.field("capture_transition_id", original.capture_transition_id);
    }
    if (tag == 1)
      return v.object("condition_receipt_ref", [&](C &x) {
        return receipt_ref(
            x, std::get<ConditionPurposeCapacityCause>(value.cause_ref)
                   .condition_receipt_ref);
      });
    auto &original = std::get<MilitaryAvailabilityCauseV2>(value.cause_ref);
    v.object("casualty_source",
             [&](C &x) { return source(x, original.casualty_source); });
    v.field("source_schema", original.source_schema);
    return v.field("source_ordinal", original.source_ordinal);
  });
  if (!c.status().ok)
    return c.status();
  if (value.transition_id.empty() || value.actor_id.empty() ||
      value.purpose.id.empty() || value.purpose.version < 1 ||
      value.purpose.catalog_digest.empty() || value.expected_recovery_at)
    return c.reject("InvalidCapacityTransition");
  if (const auto *original =
          std::get_if<MilitaryAvailabilityCauseV2>(&value.cause_ref)) {
    if (value.purpose.id != "original_military_availability" ||
        value.purpose.version != 1 || value.to ||
        original->casualty_source.domain != "military" ||
        original->casualty_source.kind != "military_casualties" ||
        original->casualty_source.id.empty() ||
        (original->source_schema != 2 &&
         !(profile == ConditionWireProfile::injury_source_chain &&
           original->source_schema == 1)) ||
        original->source_ordinal < 0)
      return c.reject("InvalidOriginalMilitaryCapacityTransition");
  } else if (const auto *original =
                 std::get_if<NamedMilitaryCaptureAvailabilityCause>(
                     &value.cause_ref)) {
    if (profile != ConditionWireProfile::named_custody_availability ||
        value.purpose.id != "original_named_custody_availability" ||
        value.purpose.version != 1 || value.to ||
        value.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        original->term_source.domain != "military" ||
        original->term_source.kind != "military_named_custody_term" ||
        original->term_source.id.empty() || original->source_schema != 1 ||
        (original->predecessor_transition_id &&
         original->predecessor_transition_id->empty()))
      return c.reject("InvalidNamedCustodyCaptureCapacityTransition");
  } else if (const auto *original =
                 std::get_if<NamedMilitaryReleaseAvailabilityCause>(
                     &value.cause_ref)) {
    if (profile != ConditionWireProfile::named_custody_availability ||
        value.purpose.id != "original_named_custody_availability" ||
        value.purpose.version != 1 || value.from ||
        value.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        original->release_source.domain != "military" ||
        original->release_source.kind != "military_named_custody_released" ||
        original->release_source.id.empty() || original->source_schema != 1 ||
        original->capture_transition_id.empty())
      return c.reject("InvalidNamedCustodyReleaseCapacityTransition");
  } else if (value.from == value.to ||
             std::get<ConditionPurposeCapacityCause>(value.cause_ref)
                 .condition_receipt_ref.result_id.empty())
    return c.reject("InvalidConditionPurposeCapacityTransition");
  return c.status();
}
template <class C>
Status condition_extensions(C &c, CharacterConditionExtensionsStateV1 &s,
                            ConditionWireProfile profile) {
  const bool effects = profile == ConditionWireProfile::injury_effect_sources ||
                       profile == ConditionWireProfile::injury_source_chain;
  table(c, "actor_condition_heads", s.actor_condition_heads, head<C>);
  if (effects)
    table(c, "episodes", s.episodes,
          [&](auto &v, auto &x) { return injury_effect_codec::episode(v, x, profile == ConditionWireProfile::injury_source_chain); });
  else
    empty_table(c, "episodes", s.episodes);
  table(c, "applied_receipts", s.applied_receipts,
        [&](C &v, ConditionReceiptV2 &r) { return receipt(v, r, profile); });
  if (effects) {
    table(c, "effect_contributions", s.effect_contributions,
          [](auto &v, auto &x) {
            return injury_effect_codec::contribution(v, x);
          });
    table(c, "recovery_resolutions", s.recovery_resolutions,
          [](auto &v, auto &x) { return injury_effect_codec::recovery(v, x); });
  } else {
    empty_table(c, "effect_contributions", s.effect_contributions);
    empty_table(c, "recovery_resolutions", s.recovery_resolutions);
  }
  table(c, "capacity_transitions", s.capacity_transitions,
        [&](C &v, CapacityTransitionV2 &r) {
          return profile != ConditionWireProfile::legacy_rest_only
                     ? capacity_transition(v, r, profile)
                     : v.reject("UnsupportedRestCapacityTransition");
        });
  empty_table(c, "combat_return_bindings", s.combat_return_bindings);
  return empty_table(c, "instant_limits", s.instant_limits);
}
// 完整冻结字段沿原明确入口读取；旧 rest 的非空分支继续原拒绝，业务来源另验。
template <class C>
Status capability_inputs(C &c, FrozenFacilityCapabilityInputs &f,
                         bool complete = false) {
  c.field("actor_id", f.actor_id);
  c.object("purpose_ref", [&](C &v) { return rule(v, f.purpose_ref); });
  c.object("captured_at", [&](C &v) { return moment(v, f.captured_at); });
  c.object("condition_head_snapshot",
           [&](C &v) { return head(v, f.condition_head_snapshot); });
  c.object("body_scalars", [&](C &v) {
    auto &b = f.body_scalars;
    v.field("alive", b.alive);
    v.field("hp", b.hp);
    v.field("fatigue", b.fatigue);
    v.field("injury", b.injury);
    return strings(v, "field_order", b.field_order);
  });
  c.sequence("active_condition_inputs", f.active_condition_inputs,
             [&](auto &v, auto &input) {
               if (!complete)
                 return v.reject("UnsupportedActorFoundationRecord");
               return frozen_condition_codec::input(v, input);
             });
  c.sequence("selected_rule_refs", f.selected_rule_refs, rule<C>);
  c.object("result", [&](C &v) {
    auto &r = f.result;
    v.field("allowed", r.allowed);
    v.field("efficiency_bp", r.efficiency_bp);
    v.sequence("limiting_condition_refs", r.limiting_condition_refs,
               [&](auto &x, auto &ref) {
                 return capability_condition_ref(x, ref, complete);
               });
    v.object("rule_ref", [&](C &w) { return rule(w, r.rule_ref); });
    return v.object("observed_at",
                    [&](C &w) { return moment(w, r.observed_at); });
  });
  return strings(c, "field_order", f.field_order);
}
} // namespace
Status visit_actor_start_context_v2(CodecVisitor &c,
                                    ActorStartContextStateV2 &s) {
  return start_context(c, s);
}
Status visit_character_origin_records_v1(CodecVisitor &c,
                                         CharacterOriginRecordsStateV1 &s,
                                         ActorOriginWireProfile profile) {
  return origin_records(c, s, profile);
}
Status
visit_character_condition_extensions_v1(CodecVisitor &c,
                                        CharacterConditionExtensionsStateV1 &s,
                                        ConditionWireProfile profile) {
  if (profile == ConditionWireProfile::capability_application_chain ||
      profile == ConditionWireProfile::trade_grant_capability_chain ||
      profile == ConditionWireProfile::injury_source_chain ||
      profile == ConditionWireProfile::purpose_capacity_transition ||
      profile == ConditionWireProfile::named_custody_availability ||
      profile == ConditionWireProfile::injury_effect_sources)
    return c.reject("ConditionCapabilityPhysical3Required");
  return condition_extensions(c, s, profile);
}
Status visit_frozen_rest_capability_inputs(CodecVisitor &c,
                                           FrozenFacilityCapabilityInputs &s) {
  return capability_inputs(c, s);
}
Status
emit_actor_start_context_v2_projection(ProjectionWriter &sink,
                                       std::string_view name,
                                       const ActorStartContextStateV2 &input) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, input,
      [&](auto &c, auto &value) { return start_context(c, value); });
}
Outcome<BoundaryValue>
encode_actor_start_context_v2(const ActorStartContextStateV2 &input) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto valid = emit_actor_start_context_v2_projection(sink, "", input);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<ActorStartContextStateV2>
decode_actor_start_context_v2(const BoundaryValue &input) {
  ActorStartContextStateV2 value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = start_context(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
Status emit_character_origin_records_v1_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterOriginRecordsStateV1 &input,
    ActorOriginWireProfile profile) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, input,
      [&](auto &c, auto &value) { return origin_records(c, value, profile); });
}
Outcome<BoundaryValue>
encode_character_origin_records_v1(const CharacterOriginRecordsStateV1 &input,
                                   ActorOriginWireProfile profile) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto valid =
      emit_character_origin_records_v1_projection(sink, "", input, profile);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<CharacterOriginRecordsStateV1>
decode_character_origin_records_v1(const BoundaryValue &input,
                                   ActorOriginWireProfile profile) {
  CharacterOriginRecordsStateV1 value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = origin_records(visitor, value, profile);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
Status emit_character_condition_extensions_v1_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &input,
    ConditionWireProfile profile) {
  if (profile == ConditionWireProfile::capability_application_chain ||
      profile == ConditionWireProfile::trade_grant_capability_chain ||
      profile == ConditionWireProfile::injury_source_chain ||
      profile == ConditionWireProfile::purpose_capacity_transition ||
      profile == ConditionWireProfile::named_custody_availability ||
      profile == ConditionWireProfile::injury_effect_sources)
    return sink.reject("ConditionCapabilityPhysical3Required");
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, input, [&](auto &c, auto &value) {
        return condition_extensions(c, value, profile);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v1(
    const CharacterConditionExtensionsStateV1 &input,
    ConditionWireProfile profile) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto valid = emit_character_condition_extensions_v1_projection(
      sink, "", input, profile);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v1(const BoundaryValue &input,
                                         ConditionWireProfile profile) {
  if (profile == ConditionWireProfile::capability_application_chain ||
      profile == ConditionWireProfile::trade_grant_capability_chain ||
      profile == ConditionWireProfile::injury_source_chain ||
      profile == ConditionWireProfile::purpose_capacity_transition ||
      profile == ConditionWireProfile::named_custody_availability ||
      profile == ConditionWireProfile::injury_effect_sources)
    return {false,
            {},
            common_failure("ConditionCapabilityPhysical3Required").error};
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = condition_extensions(visitor, value, profile);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
Status emit_frozen_rest_capability_projection(
    ProjectionWriter &sink, std::string_view name,
    const FrozenFacilityCapabilityInputs &input) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, input,
      [&](auto &c, auto &value) { return capability_inputs(c, value); });
}
Outcome<BoundaryValue> encode_frozen_rest_capability_inputs(
    const FrozenFacilityCapabilityInputs &input) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto valid = emit_frozen_rest_capability_projection(sink, "", input);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_rest_capability_inputs(const BoundaryValue &input) {
  FrozenFacilityCapabilityInputs value;
  ActorFoundationJsonVisitor visitor(input);
  auto status = capability_inputs(visitor, value);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
Outcome<CodecVisitor::Buffer>
encode_actor_foundation_checkpoint(const AuthoredActorFoundationState &input,
                                   ConditionWireProfile profile) {
  auto &s = const_cast<AuthoredActorFoundationState &>(input);
  CodecVisitor::Buffer out;
  CodecVisitor c(out);
  c.object("actor_start_context/2", [&](CodecVisitor &v) {
    return visit_actor_start_context_v2(v, s.actor_start_context);
  });
  c.object("character_origin_records/1", [&](CodecVisitor &v) {
    return visit_character_origin_records_v1(v, s.character_origin_records);
  });
  c.object("character_condition_extensions/1", [&](CodecVisitor &v) {
    return visit_character_condition_extensions_v1(
        v, s.character_condition_extensions, profile);
  });
  auto status = c.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<AuthoredActorFoundationState>
decode_actor_foundation_checkpoint(const CodecVisitor::Buffer &bytes,
                                   ConditionWireProfile profile) {
  AuthoredActorFoundationState s;
  CodecVisitor c(bytes);
  c.object("actor_start_context/2", [&](CodecVisitor &v) {
    return visit_actor_start_context_v2(v, s.actor_start_context);
  });
  c.object("character_origin_records/1", [&](CodecVisitor &v) {
    return visit_character_origin_records_v1(v, s.character_origin_records);
  });
  c.object("character_condition_extensions/1", [&](CodecVisitor &v) {
    return visit_character_condition_extensions_v1(
        v, s.character_condition_extensions, profile);
  });
  auto status = c.finish();
  return {status.ok, std::move(s), status.error};
}
Status visit_frozen_character_capability_inputs(
    CodecVisitor &c, FrozenFacilityCapabilityInputs &value) {
  return capability_inputs(c, value, true);
}
Status emit_frozen_character_capability_fields(
    ProjectionWriter &w, const FrozenFacilityCapabilityInputs &value) {
  ActorFoundationProjectionArchive c(w);
  return capability_inputs(
      c, const_cast<FrozenFacilityCapabilityInputs &>(value), true);
}
Status emit_frozen_character_capability_projection(
    ProjectionWriter &sink, std::string_view name,
    const FrozenFacilityCapabilityInputs &input) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, input,
      [](auto &c, auto &value) { return capability_inputs(c, value, true); });
}
Outcome<BoundaryValue> encode_frozen_character_capability_inputs(
    const FrozenFacilityCapabilityInputs &input) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status = emit_frozen_character_capability_projection(sink, "", input);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_character_capability_inputs(const BoundaryValue &input) {
  FrozenFacilityCapabilityInputs value;
  ActorFoundationJsonVisitor c(input);
  auto status = capability_inputs(c, value, true);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
// 明确同job原冷读；先为临时完整值预留默认拥有成员，再借唯一原字段表。
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_character_capability_inputs(const BoundaryValue &input,
                                          ScopedCheckpointJsonDecode &scope) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    return {
        false,
        {},
        common_failure("CharacterCapabilitySameJsonCandidateRequired").error};
  scope.default_values<FrozenFacilityCapabilityInputs>();
  return decode_frozen_character_capability_inputs(input);
}
Status visit_actor_condition_head(CodecVisitor &c, ActorConditionHead &value) {
  return actor_condition_codec::head(c, value);
}
Status emit_actor_condition_head_projection(ProjectionWriter &sink,
                                            std::string_view name,
                                            const ActorConditionHead &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value,
      [](auto &c, auto &v) { return actor_condition_codec::head(c, v); });
}
Outcome<BoundaryValue>
encode_actor_condition_head(const ActorConditionHead &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status = emit_actor_condition_head_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<ActorConditionHead>
decode_actor_condition_head(const BoundaryValue &input) {
  ActorConditionHead value;
  ActorFoundationJsonVisitor c(input);
  auto status = actor_condition_codec::head(c, value);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_actor_start_context_v3(CodecVisitor &c,
                                    ActorStartContextStateV2 &value) {
  return start_context(c, value, true);
}
Status
emit_actor_start_context_v3_projection(ProjectionWriter &sink,
                                       std::string_view name,
                                       const ActorStartContextStateV2 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value,
      [](auto &c, auto &v) { return start_context(c, v, true); });
}
Outcome<BoundaryValue>
encode_actor_start_context_v3(const ActorStartContextStateV2 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status = emit_actor_start_context_v3_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<ActorStartContextStateV2>
decode_actor_start_context_v3(const BoundaryValue &input) {
  ActorStartContextStateV2 value;
  ActorFoundationJsonVisitor c(input);
  auto status = start_context(c, value, true);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_character_condition_extensions_v3(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(
      c, value, ConditionWireProfile::capability_application_chain);
}
Status emit_character_condition_extensions_v3_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::capability_application_chain);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v3(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v3_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v3(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::capability_application_chain);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_character_condition_extensions_v4(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(
      c, value, ConditionWireProfile::purpose_capacity_transition);
}
Status emit_character_condition_extensions_v4_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::purpose_capacity_transition);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v4(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v4_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v4(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::purpose_capacity_transition);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_character_condition_extensions_v5(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(c, value,
                              ConditionWireProfile::injury_effect_sources);
}
Status emit_character_condition_extensions_v5_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::injury_effect_sources);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v5(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v5_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v5(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::injury_effect_sources);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_character_condition_extensions_v6(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(c, value,
                              ConditionWireProfile::named_custody_availability);
}
Status emit_character_condition_extensions_v6_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::named_custody_availability);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v6(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v6_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v6(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::named_custody_availability);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

// 原 /3 字段结构的新有限来源格式，不混入其它物理后继。
namespace hanmo::resident {
Status visit_character_condition_extensions_v7(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(
      c, value, ConditionWireProfile::trade_grant_capability_chain);
}
Status emit_character_condition_extensions_v7_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::trade_grant_capability_chain);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v7(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v7_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v7(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::trade_grant_capability_chain);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status visit_character_condition_extensions_v8(
    CodecVisitor &c, CharacterConditionExtensionsStateV1 &value) {
  return condition_extensions(
      c, value, ConditionWireProfile::injury_source_chain);
}
Status emit_character_condition_extensions_v8_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterConditionExtensionsStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &x) {
        return condition_extensions(
            c, x, ConditionWireProfile::injury_source_chain);
      });
}
Outcome<BoundaryValue> encode_character_condition_extensions_v8(
    const CharacterConditionExtensionsStateV1 &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto status =
      emit_character_condition_extensions_v8_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(out), status.error};
}
Outcome<CharacterConditionExtensionsStateV1>
decode_character_condition_extensions_v8(const BoundaryValue &input) {
  CharacterConditionExtensionsStateV1 value;
  ActorFoundationJsonVisitor c(input);
  auto status = condition_extensions(
      c, value, ConditionWireProfile::injury_source_chain);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(value), status.error};
}
} // namespace hanmo::resident
