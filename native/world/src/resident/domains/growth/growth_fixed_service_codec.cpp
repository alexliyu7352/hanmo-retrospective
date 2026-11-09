// FSL§2逐字段typed访客；JSON和binary共享声明序，不保存动态extras或制造历史。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_fixed_service_codec.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "growth_result_ref_fields.hpp"
#include "growth_skill_qualification_codec_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>
namespace hanmo::resident {
namespace {
// 首错按原类别传播；未实现能力不降成可忽略的普通业务失败。
struct CodecFailure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw CodecFailure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (CodecFailure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    RuleError e;
    e.original = {"OutOfMemory", "growth_fixed_codec"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  } catch (const std::length_error &) {
    RuleError e;
    e.original = {"SizeExhausted", "growth_fixed_codec"};
    e.kind = ErrorKind::internal;
    return {false, {}, std::move(e)};
  }
}
template <class C> Status unsupported(C &c, const char *code) {
  if (c.status().ok) {
    RuleError e;
    e.original = {code, "growth_fixed_codec"};
    e.kind = ErrorKind::unsupported_capability;
    throw CodecFailure{std::move(e)};
  }
  return c.status();
}
// 所有嵌套对象共用字节/节点预算；只在本次冷调用中持有BoundaryValue。
struct JsonBudget {
  std::size_t bytes = 0, nodes = 0;
};
class JsonCodec {
public:
  // Wire order/error policy remains here; the sole original archive owns all
  // text/default/vector/optional construction under the actual decode scope.
  JsonCodec(const BoundaryValue &value, JsonBudget &budget,
            std::size_t depth = 0, bool scalar = false)
      : owned_(std::in_place, value, scalar), archive_(&*owned_),
        input_(&value), budget_(budget), depth_(depth), scalar_(scalar) {
    inspect();
  }
  JsonCodec(BoundaryValue &value, JsonBudget &budget, std::size_t depth = 0,
            bool scalar = false)
      : owned_(std::in_place, value, scalar), archive_(&*owned_),
        budget_(budget), depth_(depth), scalar_(scalar) {
    inspect();
  }
  bool reading() const {
    return archive_->reading();
  }
  Status status() const {
    return result_;
  }
  Status reject(std::string_view code,
                std::string_view path = "growth_fixed_codec") {
    if (result_.ok)
      result_ = archive_->reject(code, path);
    return result_;
  }
  Status finish() {
    if (result_.ok && input_ && !scalar_ &&
        cursor_ != input_->as<BoundaryValue::Dict>()->size())
      reject("UnknownGrowthFixedField");
    if (result_.ok)
      absorb(archive_->finish());
    return result_;
  }
  template <class T> Status prepare_default_values(std::uint64_t count = 1) {
    if (result_.ok)
      absorb(archive_->template prepare_default_values<T>(count));
    return result_;
  }
  template <class T> Status field(std::string_view name, T &value) {
    if (!result_.ok)
      return result_;
    if (reading()) {
      const auto *n = read(name);
      if (!n)
        return result_;
      const auto *typed = n->as<T>();
      if (!typed)
        return reject("GrowthFixedScalarTypeMismatch", name);
      if constexpr (std::is_same_v<T, StableId>) {
        if (!charge(typed->size()))
          return result_;
      } else if (!charge(sizeof(T)))
        return result_;
    } else {
      if constexpr (std::is_same_v<T, StableId>) {
        if (!charge(value.size()))
          return result_;
      } else if (!charge(sizeof(T)))
        return result_;
      if (!write_field(name))
        return result_;
    }
    absorb(archive_->field(name, value));
    return result_;
  }
  template <class F> Status object(std::string_view name, F fn) {
    if (!result_.ok)
      return result_;
    const auto *node = reading() ? read(name) : nullptr;
    if (!result_.ok || (!reading() && !write_field(name)))
      return result_;
    absorb(archive_->object(name, [&](auto &original) {
      JsonCodec child(original, node, budget_, depth_ + 1, false);
      if (child.status().ok)
        child.absorb(fn(child));
      return child.finish();
    }));
    return result_;
  }
  template <class T, class A, class F>
  Status sequence(std::string_view name, std::vector<T, A> &values, F fn) {
    if (!result_.ok)
      return result_;
    constexpr bool scalar = is_scalar<T>();
    const BoundaryValue::Array *rows = nullptr;
    if (reading()) {
      const auto *node = read(name);
      if (!node)
        return result_;
      rows = node->as<BoundaryValue::Array>();
      if (!rows)
        return reject("GrowthFixedArrayRequired", name);
      if (rows->size() > CodecVisitor::max_items)
        return reject("CheckpointCollectionTooLarge", name);
    } else {
      if (values.size() > CodecVisitor::max_items)
        return reject("CheckpointCollectionTooLarge", name);
      if (!write_field(name))
        return result_;
    }
    std::size_t index = 0;
    absorb(archive_->sequence(
        name, values,
        [&](auto &original, auto &value) {
          const auto *node = rows ? &(*rows)[index] : nullptr;
          ++index;
          JsonCodec child(original, node, budget_, depth_ + 1, scalar);
          if (child.status().ok)
            child.absorb(fn(child, value));
          return child.finish();
        },
        scalar));
    return result_;
  }
  template <class T, class F>
  Status optional(std::string_view name, std::optional<T> &value, F fn) {
    if (!result_.ok)
      return result_;
    constexpr bool scalar = is_scalar<T>();
    const auto *node = reading() ? read(name) : nullptr;
    if (!result_.ok || (!reading() && !write_field(name)))
      return result_;
    absorb(archive_->optional(
        name, value,
        [&](auto &original, auto &target) {
          JsonCodec child(original, node, budget_, depth_ + 1, scalar);
          if (child.status().ok)
            child.absorb(fn(child, target));
          return child.finish();
        },
        scalar));
    return result_;
  }

private:
  std::optional<ActorFoundationJsonVisitor> owned_;
  ActorFoundationJsonVisitor *archive_;
  const BoundaryValue *input_ = nullptr;
  JsonBudget &budget_;
  std::size_t depth_, cursor_ = 0;
  bool scalar_;
  Status result_{true, {}, {}};
  JsonCodec(ActorFoundationJsonVisitor &archive, const BoundaryValue *input,
            JsonBudget &budget, std::size_t depth, bool scalar)
      : archive_(&archive), input_(input), budget_(budget), depth_(depth),
        scalar_(scalar) {
    inspect();
  }
  template <class T> static constexpr bool is_scalar() {
    return std::is_same_v<T, StableId> || std::is_same_v<T, Integer>;
  }
  bool charge(std::size_t bytes) {
    if (!result_.ok)
      return false;
    if (++budget_.nodes > 2000000u ||
        bytes > CodecVisitor::max_bytes - budget_.bytes) {
      reject("CheckpointTooLarge");
      return false;
    }
    budget_.bytes += bytes;
    return true;
  }
  void absorb(Status s) {
    if (result_.ok && !s.ok)
      result_ = std::move(s);
  }
  void inspect() {
    if (depth_ >= CodecVisitor::max_depth) {
      reject("CheckpointTooDeep");
      return;
    }
    if (!charge(1))
      return;
    if (input_ && !scalar_) {
      const auto *d = input_->as<BoundaryValue::Dict>();
      if (!d) {
        reject("GrowthFixedObjectRequired");
        return;
      }
      if (d->size() > CodecVisitor::max_items) {
        reject("CheckpointCollectionTooLarge");
        return;
      }
      for (std::size_t n = 0; n < d->size(); ++n) {
        const auto &p = (*d)[n];
        if (!charge(p.first.size()))
          return;
        for (std::size_t prior = 0; prior < n; ++prior)
          if ((*d)[prior].first == p.first) {
            reject("DuplicateGrowthFixedField", p.first);
            return;
          }
      }
    }
  }
  const BoundaryValue *read(std::string_view name) {
    if (!result_.ok)
      return nullptr;
    if (scalar_)
      return input_;
    const auto &d = *input_->as<BoundaryValue::Dict>();
    if (cursor_ >= d.size()) {
      reject("MissingGrowthFixedField", name);
      return nullptr;
    }
    if (d[cursor_].first != name) {
      reject("GrowthFixedFieldOrderMismatch", name);
      return nullptr;
    }
    return &d[cursor_++].second;
  }
  bool write_field(std::string_view name) {
    return scalar_ || charge(name.size());
  }
};
// 有限tag先拒非法整数，再由每个真实分支拒未实现能力。
template <class C>
Status fixed_service_finite(C &c, const char *name, Integer &v, Integer max) {
  c.field(name, v);
  if (c.status().ok && (v < 0 || v > max))
    return c.reject("InvalidGrowthFixedTag", name);
  return c.status();
}
template <class C>
Status fixed_service_ref(C &c, const char *name, StableId &v) {
  c.field(name, v);
  if (c.status().ok && v.empty())
    return c.reject("MissingGrowthFixedReference", name);
  return c.status();
}
template <class C>
Status fixed_service_positive(C &c, const char *name, Integer &v) {
  c.field(name, v);
  if (c.status().ok && v <= 0)
    return c.reject("InvalidGrowthFixedPositive", name);
  return c.status();
}
template <class C>
Status strings(C &c, const char *name, std::vector<StableId> &v) {
  std::size_t ordinal = 0;
  return c.sequence(name, v, [&](auto &x, auto &s) {
    fixed_service_ref(x, "value", s);
    if (x.status().ok)
      for (std::size_t prior = 0; prior < ordinal; ++prior)
        if (v[prior] == s)
          return x.reject("DuplicateGrowthFixedReference", name);
    ++ordinal;
    return x.status();
  });
}
template <class C, class T>
Status empty_rows(C &c, const char *name, std::vector<T> &v) {
  return c.sequence(name, v, [](auto &x, auto &) {
    return unsupported(x, "UnsupportedGrowthFixedInput");
  });
}
template <class C, class T>
Status empty_optional(C &c, const char *name, std::optional<T> &v) {
  return c.optional(name, v, [](auto &x, auto &) {
    return unsupported(x, "UnsupportedGrowthFixedInput");
  });
}
// Moment仅调用公开的同版精确数值编码，内部状态不得转double或自行约分。
template <class C> Status fixed_service_moment(C &c, WorldMoment &v) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    (void)0;
  EncodedWorldMoment e;
  if (!c.reading())
    e = take(encode_world_moment(v));
  c.field("half_day_tick", e.half_day_tick);
  c.object("sub_half_day", [&](auto &x) {
    x.field("numerator", e.sub_half_day.numerator);
    return x.field("denominator", e.sub_half_day.denominator);
  });
  if (c.status().ok && c.reading())
    v = take(decode_world_moment(e));
  return c.status();
}
template <class C> Status fixed_service_source(C &c, SourceRef &v) {
  fixed_service_ref(c, "domain", v.domain);
  fixed_service_ref(c, "kind", v.kind);
  return fixed_service_ref(c, "id", v.id);
}
template <class C> Status rule(C &c, ActorFoundationRuleRef &v) {
  fixed_service_ref(c, "id", v.id);
  fixed_service_positive(c, "version", v.version);
  return fixed_service_ref(c, "catalog_digest", v.catalog_digest);
}
// CatalogIdentity沿原结构完整顺序；模块名不可重复，版本和摘要不可省。
template <class C> Status catalog(C &c, CatalogIdentity &v) {
  fixed_service_ref(c, "content_id", v.content_id);
  fixed_service_ref(c, "content_version", v.content_version);
  fixed_service_ref(c, "content_digest", v.content_digest);
  fixed_service_ref(c, "ruleset_id", v.ruleset_id);
  fixed_service_ref(c, "schema_sha256", v.schema_sha256);
  c.optional("pack_set_digest", v.pack_set_digest,
             [](auto &x, auto &s) { return fixed_service_ref(x, "value", s); });
  std::size_t ordinal = 0;
  c.sequence("module_versions", v.module_versions, [&](auto &x, auto &p) {
    fixed_service_ref(x, "name", p.first);
    fixed_service_ref(x, "version", p.second);
    if (x.status().ok)
      for (std::size_t prior = 0; prior < ordinal; ++prior)
        if (v.module_versions[prior].first == p.first)
          return x.reject("DuplicateGrowthFixedModule");
    ++ordinal;
    return x.status();
  });
  return fixed_service_ref(c, "artifact_sha256", v.artifact_sha256);
}
template <class C> Status catalog_ref(C &c, LifeCatalogFieldRef &v) {
  c.object("catalog_identity",
           [&](auto &x) { return catalog(x, v.catalog_identity); });
  fixed_service_ref(c, "scenario_id", v.scenario_id);
  fixed_service_ref(c, "entity_id", v.entity_id);
  return fixed_service_ref(c, "field_selector", v.field_selector);
}
template <class C> Status origin(C &c, ScenarioOriginRef &v) {
  fixed_service_ref(c, "catalog_digest", v.catalog_digest);
  fixed_service_ref(c, "scenario_id", v.scenario_id);
  fixed_service_ref(c, "row_key", v.row_key);
  c.field("initial_tick", v.initial_tick);
  if (c.status().ok && v.initial_tick < 0)
    return c.reject("InvalidGrowthFixedOriginTick");
  return c.status();
}
// FSL原ref槽的有限局部联合，不把Catalog/初态伪装成Common事实。
template <class C> Status original(C &c, LifeOriginalRef &v) {
  Integer tag = static_cast<Integer>(v.index());
  fixed_service_finite(c, "tag", tag, 3);
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    switch (tag) {
    case 0:
      if (!prepare_checkpoint_default_values<SourceRef>(c).ok)
        return c.status();
      v = SourceRef{};
      break;
    case 1:
      if (!prepare_checkpoint_default_values<LifeOwnedFieldRef>(c).ok)
        return c.status();
      v = LifeOwnedFieldRef{};
      break;
    case 2:
      if (!prepare_checkpoint_default_values<LifeCatalogFieldRef>(c).ok)
        return c.status();
      v = LifeCatalogFieldRef{};
      break;
    case 3:
      if (!prepare_checkpoint_default_values<ScenarioOriginRef>(c).ok)
        return c.status();
      v = ScenarioOriginRef{};
      break;
    }
  }
  switch (tag) {
  case 0:
    return fixed_service_source(c, std::get<SourceRef>(v));
  case 1: {
    auto &r = std::get<LifeOwnedFieldRef>(v);
    c.object("owner_ref",
             [&](auto &x) { return fixed_service_source(x, r.owner_ref); });
    return fixed_service_ref(c, "field_selector", r.field_selector);
  }
  case 2:
    return catalog_ref(c, std::get<LifeCatalogFieldRef>(v));
  default:
    return origin(c, std::get<ScenarioOriginRef>(v));
  }
}
// 原Condition head、身体和能力result保持K3全部槽，不能存替代镜像。
template <class C> Status head(C &c, ActorConditionHead &v) {
  auto status = actor_condition_codec::head(c, v);
  if (status.ok &&
      (v.actor_id.empty() || v.condition_revision < 0 ||
       v.initial_condition_seed_ref.seed_key.empty() ||
       (v.latest_receipt_ref && v.latest_receipt_ref->result_id.empty())))
    return c.reject("InvalidGrowthFixedConditionHead");
  return status;
}
template <class C>
Status capability(C &c, FrozenFacilityCapabilityInputs &v, bool complete) {
  fixed_service_ref(c, "actor_id", v.actor_id);
  c.object("purpose_ref", [&](auto &x) { return rule(x, v.purpose_ref); });
  c.object("captured_at",
           [&](auto &x) { return fixed_service_moment(x, v.captured_at); });
  c.object("condition_head_snapshot",
           [&](auto &x) { return head(x, v.condition_head_snapshot); });
  c.object("body_scalars", [&](auto &x) {
    auto &b = v.body_scalars;
    x.field("alive", b.alive);
    x.field("hp", b.hp);
    x.field("fatigue", b.fatigue);
    x.field("injury", b.injury);
    return strings(x, "field_order", b.field_order);
  });
  empty_rows(c, "active_condition_inputs", v.active_condition_inputs);
  c.sequence("selected_rule_refs", v.selected_rule_refs,
             [](auto &x, auto &r) { return rule(x, r); });
  c.object("result", [&](auto &x) {
    auto &r = v.result;
    x.field("allowed", r.allowed);
    x.field("efficiency_bp", r.efficiency_bp);
    x.sequence(
        "limiting_condition_refs", r.limiting_condition_refs,
        [&](auto &y, auto &s) {
          Integer tag = static_cast<Integer>(s.index());
          if (complete)
            y.field("tag", tag);
          else if (!y.reading() && tag != 0)
            return y.reject("CompleteCapabilitySourceCodecRequired");
          else
            tag = 0;
          if (tag < 0 || tag > 1)
            return y.reject("CapabilityConditionSourceTagInvalid");
          if (y.reading())
            s = tag == 0
                    ? CharacterConditionSourceRef{InitialConditionSeedRef{}}
                    : CharacterConditionSourceRef{ConditionReceiptRefV2{}};
          return tag == 0 ? fixed_service_ref(
                                y, "seed_key",
                                std::get<InitialConditionSeedRef>(s).seed_key)
                          : fixed_service_ref(
                                y, "result_id",
                                std::get<ConditionReceiptRefV2>(s).result_id);
        });
    x.object("rule_ref", [&](auto &y) { return rule(y, r.rule_ref); });
    return x.object("observed_at", [&](auto &y) {
      return fixed_service_moment(y, r.observed_at);
    });
  });
  return strings(c, "field_order", v.field_order);
}
// Location严格保原Common选中Settlement的JSON字符串与binary整数标签，不造新近似类型。
Status location(JsonCodec &c, Location &v) {
  StableId tag = "Settlement";
  if (!c.reading() && v.index() != 0)
    return unsupported(c, "UnsupportedGrowthFixedLocation");
  c.field("kind", tag);
  if (c.status().ok && tag != "Settlement") {
    if (tag == "Wilderness" || tag == "Following" || tag == "InArmy" ||
        tag == "OnRoute")
      return unsupported(c, "UnsupportedGrowthFixedLocation");
    return c.reject("InvalidLocationKind");
  }
  if (!c.status().ok)
    return c.status();
  if (c.reading())
    v = SettlementLocation{};
  auto &s = std::get<SettlementLocation>(v);
  fixed_service_ref(c, "settlement_id", s.settlement_id);
  return fixed_service_ref(c, "place_id", s.place_id);
}
Status location(ActorFoundationProjectionArchive &c, Location &v) {
  if (v.index() != 0)
    return unsupported(c, "UnsupportedGrowthFixedLocation");
  StableId tag = "Settlement";
  c.field("kind", tag);
  auto &s = std::get<SettlementLocation>(v);
  fixed_service_ref(c, "settlement_id", s.settlement_id);
  return fixed_service_ref(c, "place_id", s.place_id);
}
Status location(CodecVisitor &c, Location &v) {
  Integer tag = static_cast<Integer>(v.index());
  fixed_service_finite(c, "kind", tag, 4);
  if (c.status().ok && tag != 0)
    return unsupported(c, "UnsupportedGrowthFixedLocation");
  if (!c.status().ok)
    return c.status();
  if (c.reading())
    v = SettlementLocation{};
  auto &s = std::get<SettlementLocation>(v);
  fixed_service_ref(c, "settlement_id", s.settlement_id);
  return fixed_service_ref(c, "place_id", s.place_id);
}
// 每个actor完整十三槽；未读技能、属性、年龄留原空槽而非抓全人快照。
template <class C> Status actor(C &c, LifeFixedActorInputs &v, bool complete) {
  fixed_service_ref(c, "actor_id", v.actor_id);
  fixed_service_ref(c, "role", v.role);
  c.sequence("capability_frames", v.capability_frames,
             [&](auto &x, auto &r) { return capability(x, r, complete); });
  fixed_service_ref(c, "origin_actor_key", v.origin_actor_key);
  c.field("origin_schema", v.origin_schema);
  if (c.status().ok && v.origin_schema != 2)
    return unsupported(c, "UnsupportedGrowthFixedOriginSchema");
  if (complete)
    c.sequence("skill_qualification_views", v.skill_qualification_views,
               [](auto &x, auto &view) {
                 return growth_skill_qualification_codec::fields(x, view);
               });
  else
    empty_rows(c, "skill_qualification_views", v.skill_qualification_views);
  empty_rows(c, "effective_attribute_inputs", v.effective_attribute_inputs);
  empty_optional(c, "age_view", v.age_view);
  c.object("location_snapshot",
           [&](auto &x) { return location(x, v.location_snapshot); });
  c.sequence("actual_location_source_refs", v.actual_location_source_refs,
             [](auto &x, auto &r) { return original(x, r); });
  c.sequence("participation_source_refs", v.participation_source_refs,
             [](auto &x, auto &r) { return original(x, r); });
  c.sequence("occupation_source_refs", v.occupation_source_refs,
             [](auto &x, auto &r) { return original(x, r); });
  return strings(c, "field_order", v.field_order);
}
// 原quiet字段完整成对存读；这里只保存真实captured
// source，业务reader仍须反验原冲突/访问。
template <class C> Status quiet(C &c, LifeQuietServiceInputs &v) {
  fixed_service_ref(c, "place_ref", v.place_ref);
  c.object("quiet_rule_ref",
           [&](auto &x) { return rule(x, v.quiet_rule_ref); });
  c.sequence("site_access_source_refs", v.site_access_source_refs,
             [](auto &x, auto &r) { return original(x, r); });
  c.sequence("actual_conflict_source_refs", v.actual_conflict_source_refs,
             [](auto &x, auto &r) { return original(x, r); });
  c.object("observed_at",
           [&](auto &x) { return fixed_service_moment(x, v.observed_at); });
  c.field("is_quiet", v.is_quiet);
  return strings(c, "field_order", v.field_order);
}
// LifeActivity首片只Service0；恢复、quiet与trade未接时非空明确Unsupported。
template <class C> Status specific(C &c, LifeFixedSpecificInputs &v) {
  fixed_service_finite(c, "tag", v.kind, 7);
  if (c.status().ok && v.kind > 1)
    return unsupported(c, "UnsupportedGrowthFixedSpecific");
  if (!c.status().ok)
    return c.status();
  if (v.kind == 0) {
    auto &g = v.growth_fact;
    fixed_service_ref(c, "terms_action_ref", g.terms_action_ref);
    c.optional("teaching_or_study_acceptance_ref",
               g.teaching_or_study_acceptance_ref,
               [](auto &x, auto &r) { return original(x, r); });
    c.object("original_training_or_study_rule_ref", [&](auto &x) {
      return catalog_ref(x, g.original_training_or_study_rule_ref);
    });
    c.optional("original_quiet_or_rest_basis", g.original_quiet_or_rest_basis,
               [](auto &x, auto &r) { return quiet(x, r); });
    c.sequence("actual_learning_or_rest_period_refs",
               g.actual_learning_or_rest_period_refs,
               [](auto &x, auto &r) { return fixed_service_source(x, r); });
    return strings(c, "field_order", g.field_order);
  }
  auto &x = v.life_activity;
  fixed_service_ref(c, "activity_id", x.activity_id);
  fixed_service_finite(c, "terms_kind", x.terms_kind, 2);
  c.optional("service_role_ref", x.service_role_ref,
             [](auto &y, auto &r) { return catalog_ref(y, r); });
  c.optional("activity_ref", x.activity_ref,
             [](auto &y, auto &r) { return catalog_ref(y, r); });
  c.optional("recovery_service_ref", x.recovery_service_ref,
             [](auto &y, auto &r) { return fixed_service_source(y, r); });
  c.optional("original_quiet_or_rest_basis", x.original_quiet_or_rest_basis,
             [](auto &y, auto &r) { return quiet(y, r); });
  c.optional("trade_settlement_ref", x.trade_settlement_ref,
             [](auto &y, auto &r) { return fixed_service_source(y, r); });
  strings(c, "field_order", x.field_order);
  if (!c.status().ok)
    return c.status();
  if (x.terms_kind == 0) {
    if (!x.service_role_ref || !x.activity_ref || x.trade_settlement_ref)
      return c.reject("InvalidGrowthFixedServiceRule");
  } else if (x.service_role_ref || x.activity_ref || x.recovery_service_ref ||
             !x.trade_settlement_ref)
    return c.reject("InvalidGrowthFixedTradeRule");
  return c.status();
}
// 仅真实Service初次支付与原wage预约来源；其它用途尚无本片reader。
template <class C> Status resource(C &c, LifeResourceBasisRef &v) {
  Integer tag = static_cast<Integer>(v.purpose);
  fixed_service_finite(c, "purpose", tag, 5);
  if (!c.status().ok)
    return c.status();
  if (c.reading())
    v.purpose = static_cast<LifeResourceBasisRef::Purpose>(tag);
  c.object("original_source_ref",
           [&](auto &x) { return original(x, v.original_source_ref); });
  return fixed_service_positive(c, "source_schema", v.source_schema);
}
template <class C>
Status inputs(C &c, LifeFixedServiceInputs &v, bool complete) {
  c.sequence("actors", v.actors,
             [&](auto &x, auto &a) { return actor(x, a, complete); });
  c.sequence("locked_rule_refs", v.locked_rule_refs,
             [](auto &x, auto &r) { return catalog_ref(x, r); });
  c.sequence("accepted_permission_refs", v.accepted_permission_refs,
             [](auto &x, auto &r) {
               fixed_service_ref(x, "permission_kind", r.permission_kind);
               x.object("original_ref",
                        [&](auto &y) { return original(y, r.original_ref); });
               return fixed_service_positive(x, "source_schema",
                                             r.source_schema);
             });
  c.sequence("facility_use_refs", v.facility_use_refs, [](auto &x, auto &r) {
    x.object("original_facility_use_ref", [&](auto &y) {
      return fixed_service_source(y, r.original_facility_use_ref);
    });
    fixed_service_positive(x, "source_schema", r.source_schema);
    return strings(x, "actual_claim_ids", r.actual_claim_ids);
  });
  c.sequence("resource_basis_refs", v.resource_basis_refs,
             [](auto &x, auto &r) { return resource(x, r); });
  c.object("specific", [&](auto &x) { return specific(x, v.specific); });
  return strings(c, "field_order", v.field_order);
}
// ActualCommand绑定真实结果与原控制字段；NPC route尚无真实reader。
template <class C>
Status command_binding_fields(C &x, LifeActualCommandBinding &v) {
  fixed_service_ref(x, "command_kind", v.command_kind);
  fixed_service_positive(x, "request_seq", v.request_seq);
  return fixed_service_ref(x, "result_id", v.result_id);
}
template <class C>
Status actor_route_fields(C &x, LifeActorRoute &v, const StableId &actor_id,
                          bool exact_periods) {
  Integer tag = static_cast<Integer>(v.index());
  fixed_service_finite(x, "tag", tag, 1);
  if (!x.status().ok)
    return x.status();
  if (x.reading()) {
    if (tag == 0) {
      if (!prepare_checkpoint_default_values<LifePlayerActorRoute>(x).ok)
        return x.status();
      v = LifePlayerActorRoute{};
    } else {
      if (!prepare_checkpoint_default_values<LifeNpcActorRoute>(x).ok)
        return x.status();
      v = LifeNpcActorRoute{};
    }
  }
  if (tag == 0)
    return x.object("original_control_source_ref", [&](auto &y) {
      auto &source =
          std::get<LifePlayerActorRoute>(v).original_control_source_ref;
      if (exact_periods)
        return original(y, source);
      if (y.reading()) {
        if (!prepare_checkpoint_default_values<LifeCatalogFieldRef>(y).ok)
          return y.status();
        source = LifeCatalogFieldRef{};
      }
      auto *old = std::get_if<LifeCatalogFieldRef>(&source);
      if (!old)
        return y.reject("GrowthDynamicControlPhysicalSelectionRequired");
      return catalog_ref(y, *old);
    });
  auto &r = std::get<LifeNpcActorRoute>(v);
  fixed_service_ref(x, "subject_actor_id", r.subject_actor_id);
  x.field("registration_epoch", r.registration_epoch);
  fixed_service_positive(x, "decision_seq", r.decision_seq);
  x.object("policy_ref", [&](auto &y) {
    Integer policy_tag = static_cast<Integer>(r.policy_ref.index());
    if (exact_periods)
      fixed_service_finite(y, "tag", policy_tag, 1);
    else if (!y.reading() && policy_tag != 0)
      return y.reject("GrowthNpcPolicyPhysicalSelectionRequired");
    else
      policy_tag = 0;
    if (!y.status().ok)
      return y.status();
    if (y.reading()) {
      if (policy_tag == 0) {
        if (!prepare_checkpoint_default_values<ActorFoundationRuleRef>(y).ok)
          return y.status();
        r.policy_ref = ActorFoundationRuleRef{};
      } else {
        if (!prepare_checkpoint_default_values<LifeCatalogFieldRef>(y).ok)
          return y.status();
        r.policy_ref = LifeCatalogFieldRef{};
      }
    }
    return policy_tag == 0
               ? rule(y, std::get<ActorFoundationRuleRef>(r.policy_ref))
               : catalog_ref(y, std::get<LifeCatalogFieldRef>(r.policy_ref));
  });
  if (x.status().ok &&
      (r.subject_actor_id != actor_id || r.registration_epoch < 0))
    return x.reject("InvalidGrowthNpcActorRoute");
  return x.status();
}
template <class C>
Status command(C &c, LifeActualCommand &v, bool exact_periods) {
  fixed_service_ref(c, "actor_id", v.actor_id);
  c.object("command_binding", [&](auto &x) {
    return command_binding_fields(x, v.command_binding);
  });
  c.object("route_basis", [&](auto &x) {
    return actor_route_fields(x, v.route_basis, v.actor_id, exact_periods);
  });
  return c.object("accepted_business_ref", [&](auto &x) {
    return original(x, v.accepted_business_ref);
  });
}
template <class C> Status outcome(C &c, LifeFixedOutcomeRef &v) {
  Integer tag = static_cast<Integer>(v.kind);
  fixed_service_finite(c, "tag", tag, 10);
  if (c.status().ok && tag > 3)
    return unsupported(c, "UnsupportedGrowthFixedOutcome");
  if (!c.status().ok)
    return c.status();
  if (c.reading())
    v.kind = static_cast<LifeFixedOutcomeRef::Kind>(tag);
  fixed_service_ref(c, tag == 3 ? "activity_id" : "action_id", v.id);
  if (c.status().ok && (v.period_ordinal != 0 || !v.secondary_ref.empty()))
    return c.reject("UnexpectedGrowthFixedOutcomePayload");
  return c.status();
}
// 真终局保disposition和全部结果/未清责任槽；首片只FixedCompletionGate。
// FSL§2十支输入变化的完整局部载荷；外来源必须由业务冷validator再核中央所选kind/schema。
template <class C> Status contribution_ref(C &c, LifeGrowthContributionRef &v, GrowthContributionWireVersion wire) {
  return growth_result_ref_codec::fields(c, v, wire);
}
template <class C> Status input_change(C &c, LifeFixedInputChange &value, GrowthContributionWireVersion wire) {
  Integer tag = static_cast<Integer>(value.index());
  fixed_service_finite(c, "tag", tag, 9);
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    switch (tag) {
    case 0:
      value = LifeConditionInputChange{};
      break;
    case 1:
      value = LifeDeathInputChange{};
      break;
    case 2:
      value = LifeGrowthInputChange{};
      break;
    case 3:
      value = LifeActualPositionInputChange{};
      break;
    case 4:
      value = LifeOccupationInputChange{};
      break;
    case 5:
      value = LifeFacilityInputChange{};
      break;
    case 6:
      value = LifeAcceptedTermsInputChange{};
      break;
    case 7:
      value = LifeResourceInputChange{};
      break;
    case 8:
      value = LifeKnowledgeInputChange{};
      break;
    case 9:
      value = LifeScheduledInputChange{};
      break;
    }
  }
  return std::visit(
      [&](auto &change) -> Status {
        using T = std::decay_t<decltype(change)>;
        if constexpr (std::is_same_v<T, LifeConditionInputChange>)
          return c.object("condition_receipt_ref", [&](auto &x) {
            return fixed_service_ref(x, "result_id",
                                     change.condition_receipt_ref.result_id);
          });
        else if constexpr (std::is_same_v<T, LifeDeathInputChange>)
          return c.object("death_ref", [&](auto &x) {
            fixed_service_ref(x, "domain", change.death_ref.domain);
            fixed_service_ref(x, "kind", change.death_ref.kind);
            fixed_service_ref(x, "id", change.death_ref.id);
            if (change.death_ref.domain != "family_life" ||
                change.death_ref.kind != "family_death")
              return x.reject("InvalidGrowthFixedDeathRef");
            return x.status();
          });
        else if constexpr (std::is_same_v<T, LifeGrowthInputChange>)
          return c.object("original_result_ref", [&](auto &x) {
            return contribution_ref(x, change.original_result_ref, wire);
          });
        else if constexpr (std::is_same_v<T, LifeScheduledInputChange>) {
          c.object("schedule_ref",
                   [&](auto &x) { return original(x, change.schedule_ref); });
          fixed_service_positive(c, "period_ordinal", change.period_ordinal);
          Integer boundary = static_cast<Integer>(change.boundary);
          fixed_service_finite(c, "boundary", boundary, 1);
          if (c.reading() && c.status().ok)
            change.boundary =
                static_cast<LifeScheduledInputChange::Boundary>(boundary);
          return c.status();
        } else {
          c.object("original_source_ref", [&](auto &x) {
            return fixed_service_source(x, change.original_source_ref);
          });
          return fixed_service_positive(c, "source_schema",
                                        change.source_schema);
        }
      },
      value);
}
template <class C>
Status actual_input_change(C &c, LifeActualInputChange &value, GrowthContributionWireVersion wire) {
  c.object("change", [&](auto &x) { return input_change(x, value.change, wire); });
  return c.object("transition_rule_ref",
                  [&](auto &x) { return rule(x, value.transition_rule_ref); });
}
template <class C>
Status terminal(C &c, LifeOriginalTerminalApplication &v, bool complete,
                bool exact_periods, GrowthContributionWireVersion wire) {
  Integer disposition = static_cast<Integer>(v.disposition);
  fixed_service_finite(c, "disposition", disposition, 1);
  if (c.reading() && c.status().ok)
    v.disposition =
        static_cast<LifeOriginalTerminalApplication::Disposition>(disposition);
  c.object("trigger", [&](auto &x) {
    Integer tag = static_cast<Integer>(v.trigger.index());
    fixed_service_finite(x, "tag", tag, 2);
    if (!x.status().ok)
      return x.status();
    if (x.reading()) {
      if (tag == 0)
        v.trigger = LifeOriginalResultTrigger{};
      else if (tag == 1)
        v.trigger = LifeFixedCompletionGate{};
      else
        v.trigger = LifeActualStopTrigger{};
    }
    if (tag == 0)
      return x.object("outcome", [&](auto &y) {
        return outcome(y,
                       std::get<LifeOriginalResultTrigger>(v.trigger).outcome);
      });
    if (tag == 1) {
      auto &t = std::get<LifeFixedCompletionGate>(v.trigger);
      fixed_service_ref(x, "action_ref", t.action_ref);
      x.object("original_required_rule_ref", [&](auto &y) {
        return catalog_ref(y, t.original_required_rule_ref);
      });
      return x.optional(
          "last_actual_period_ref", t.last_actual_period_ref,
          [](auto &y, auto &r) { return fixed_service_source(y, r); });
    }
    auto &t = std::get<LifeActualStopTrigger>(v.trigger);
    x.object("original_stop_rule_ref",
             [&](auto &y) { return rule(y, t.original_stop_rule_ref); });
    return x.object("source", [&](auto &y) {
      Integer kind = static_cast<Integer>(t.source.index());
      fixed_service_finite(y, "tag", kind, 1);
      if (y.status().ok && kind != 0 && !complete)
        return unsupported(y, "GrowthFixedInputChangeSourceRequired");
      if (!y.status().ok)
        return y.status();
      if (y.reading())
        t.source = kind == 0 ? decltype(t.source){LifeActualCommand{}}
                             : decltype(t.source){LifeFixedInputChange{}};
      return kind == 0
                 ? command(y, std::get<LifeActualCommand>(t.source),
                           exact_periods)
                 : input_change(y, std::get<LifeFixedInputChange>(t.source), wire);
    });
  });
  c.sequence("actual_result_refs", v.actual_result_refs,
             [](auto &x, auto &r) { return outcome(x, r); });
  c.sequence("remaining_responsibility_refs", v.remaining_responsibility_refs,
             [](auto &x, auto &r) { return original(x, r); });
  return strings(c, "field_order", v.field_order);
}
template <class C>
Status cause(C &c, LifeFixedBoundaryCause &v, bool complete,
             bool exact_periods, GrowthContributionWireVersion wire) {
  Integer tag = static_cast<Integer>(v.index());
  fixed_service_finite(c, "tag", tag, 3);
  if (c.status().ok && tag == 2 && !complete)
    return unsupported(c, "GrowthFixedInputChangeSourceRequired");
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    if (tag == 0)
      v = LifeActualCommand{};
    else if (tag == 1)
      v = LifeAcceptedExecution{};
    else if (tag == 2)
      v = LifeActualInputChange{};
    else
      v = LifeOriginalTerminalApplication{};
  }
  if (tag == 0)
    return command(c, std::get<LifeActualCommand>(v), exact_periods);
  if (tag == 2)
    return actual_input_change(c, std::get<LifeActualInputChange>(v), wire);
  if (tag == 3)
    return terminal(c, std::get<LifeOriginalTerminalApplication>(v), complete,
                    exact_periods, wire);
  auto &r = std::get<LifeAcceptedExecution>(v);
  c.object("accepted_source_ref", [&](auto &x) {
    return fixed_service_source(x, r.accepted_source_ref);
  });
  fixed_service_positive(c, "source_schema", r.source_schema);
  c.object("execution_rule_ref",
           [&](auto &x) { return rule(x, r.execution_rule_ref); });
  return fixed_service_ref(c, "actual_initiator_actor_id",
                           r.actual_initiator_actor_id);
}
// 完整原边界固定顺序；跨owner来源存在性和历史完整性仍由真实reader核验。
template <class C>
Status fixed_service_boundary_fields(C &c, LifeFixedServiceBoundary &v,
                                     bool complete = false,
                                     bool exact_periods = false, GrowthContributionWireVersion wire = GrowthContributionWireVersion::original) {
  fixed_service_ref(c, "boundary_id", v.boundary_id);
  c.field("source_schema", v.source_schema);
  if (c.status().ok && v.source_schema != 1)
    return unsupported(c, "UnsupportedGrowthFixedBoundarySchema");
  fixed_service_ref(c, "action_ref", v.action_ref);
  fixed_service_ref(c, "action_kind", v.action_kind);
  if (c.status().ok && v.action_kind != "service" && v.action_kind != "trade" &&
      v.action_kind != "trade_basket" && v.action_kind != "study_skill" &&
      v.action_kind != "teach_study" && v.action_kind != "learn_skill" &&
      v.action_kind != "teach_skill" && v.action_kind != "practice_skill" &&
      v.action_kind != "long_rest")
    return unsupported(c, "UnsupportedGrowthFixedActionKind");
  c.object("owner_ref", [&](auto &x) {
    fixed_service_finite(x, "tag", v.owner_ref.kind, 7);
    if (x.status().ok && v.owner_ref.kind > 1)
      return unsupported(x, "UnsupportedGrowthFixedOwner");
    if (!x.status().ok)
      return x.status();
    if (v.owner_ref.kind == 0) {
      fixed_service_ref(x, "action_id", v.owner_ref.action_id);
      StableId tag =
          v.owner_ref.fact_kind == GrowthFixedFactKind::training ? "Training"
          : v.owner_ref.fact_kind == GrowthFixedFactKind::study  ? "Study"
          : v.owner_ref.fact_kind == GrowthFixedFactKind::rest   ? "Rest"
                                                                 : "";
      x.field("fact_kind", tag);
      if (tag != "Training" && tag != "Study" && tag != "Rest")
        return x.reject("InvalidGrowthFixedFactKind");
      if (x.reading())
        v.owner_ref.fact_kind = tag == "Training"
                                    ? GrowthFixedFactKind::training
                                : tag == "Study" ? GrowthFixedFactKind::study
                                                 : GrowthFixedFactKind::rest;
      return x.status();
    }
    fixed_service_ref(x, "life_activity_id", v.owner_ref.life_activity_id);
    return fixed_service_finite(x, "terms_kind", v.owner_ref.terms_kind, 2);
  });
  c.field("boundary_ordinal", v.boundary_ordinal);
  c.optional("previous_boundary_ref", v.previous_boundary_ref,
             [](auto &x, auto &r) { return fixed_service_ref(x, "value", r); });
  Integer kind = static_cast<Integer>(v.kind);
  fixed_service_finite(c, "kind", kind, 5);
  if (c.reading() && c.status().ok)
    v.kind = static_cast<LifeFixedBoundaryKind>(kind);
  c.object("occurred_at",
           [&](auto &x) { return fixed_service_moment(x, v.occurred_at); });
  c.object("cause",
           [&](auto &x) { return cause(x, v.cause, complete, exact_periods, wire); });
  c.object("input_snapshot",
           [&](auto &x) { return inputs(x, v.input_snapshot, complete); });
  c.optional("original_outcome_ref", v.original_outcome_ref,
             [](auto &x, auto &r) { return outcome(x, r); });
  strings(c, "field_order", v.field_order);
  if (!c.status().ok)
    return c.status();
  if (v.boundary_ordinal < 0 ||
      (v.boundary_ordinal == 0) != !v.previous_boundary_ref)
    return c.reject("InvalidGrowthFixedBoundaryOrdinal");
  if (v.owner_ref.kind != v.input_snapshot.specific.kind ||
      (v.owner_ref.kind == 0
           ? v.owner_ref.action_id !=
                 v.input_snapshot.specific.growth_fact.terms_action_ref
           : v.owner_ref.life_activity_id !=
                 v.input_snapshot.specific.life_activity.activity_id))
    return c.reject("GrowthFixedOwnerInputMismatch");
  if ((kind == 0 && v.cause.index() > 1) ||
      (kind >= 4 && v.cause.index() != 3) ||
      (kind > 0 && kind < 4 && v.cause.index() > 2))
    return c.reject("GrowthFixedCauseKindMismatch");
  if (kind < 4 && v.original_outcome_ref)
    return c.reject("UnexpectedGrowthFixedOutcome");
  if (kind >= 4) {
    auto &t = std::get<LifeOriginalTerminalApplication>(v.cause);
    if ((kind == 4) !=
        (t.disposition ==
         LifeOriginalTerminalApplication::Disposition::completed))
      return c.reject("GrowthFixedTerminalDispositionMismatch");
    if (const auto *gate = std::get_if<LifeFixedCompletionGate>(&t.trigger);
        gate && gate->action_ref != v.action_ref)
      return c.reject("GrowthFixedGateActionMismatch");
    if (v.original_outcome_ref) {
      bool found = false;
      for (const auto &r : t.actual_result_refs)
        if (r.kind == v.original_outcome_ref->kind &&
            r.id == v.original_outcome_ref->id)
          found = true;
      if (!found)
        return c.reject("GrowthFixedOutcomeMismatch");
    }
  }
  return c.status();
}
// Decode owns a private candidate. The original record swap carries both
// copy leases while its explicit Fraction operations may throw; failure must
// discard the enclosing candidate, never continue with a partial prior image.
void publish(LifeFixedServiceBoundary &a, LifeFixedServiceBoundary &b) {
  swap(a, b);
}

} // namespace
// 单边界入口统一guard，失败不返回半候选；JSON保持完整原字段布局。
Status emit_growth_fixed_service_boundary_projection(
    ProjectionWriter &sink, std::string_view name,
    const LifeFixedServiceBoundary &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value,
      [&](auto &c, auto &v) { return fixed_service_boundary_fields(c, v); });
}
Outcome<BoundaryValue>
encode_growth_fixed_service_boundary(const LifeFixedServiceBoundary &v) {
  return guarded<BoundaryValue>([&] {
    BoundaryValue output;
    BoundaryProjectionWriter sink(output);
    take(emit_growth_fixed_service_boundary_projection(sink, "", v));
    take(sink.finish());
    return output;
  });
}
Outcome<LifeFixedServiceBoundary>
decode_growth_fixed_service_boundary(const BoundaryValue &v) {
  return guarded<LifeFixedServiceBoundary>([&] {
    JsonBudget budget;
    JsonCodec c(v, budget);
    take(c.prepare_default_values<LifeFixedServiceBoundary>());
    LifeFixedServiceBoundary output;
    take(fixed_service_boundary_fields(c, output));
    take(c.finish());
    return output;
  });
}
// binary不加新schema外壳，原Growth段直接嵌入同一声明顺序。
Status visit_growth_fixed_service_boundary(CodecVisitor &c,
                                           LifeFixedServiceBoundary &v) {
  return guarded<Done>([&] {
    if (!c.reading()) {
      take(fixed_service_boundary_fields(c, v));
      return Done{};
    }
    LifeFixedServiceBoundary candidate;
    take(fixed_service_boundary_fields(c, candidate));
    publish(v, candidate);
    return Done{};
  });
}
Outcome<CodecVisitor::Buffer> encode_growth_fixed_service_boundary_checkpoint(
    const LifeFixedServiceBoundary &v) {
  return guarded<CodecVisitor::Buffer>([&] {
    CodecVisitor::Buffer output;
    CodecVisitor c(output);
    take(visit_growth_fixed_service_boundary(
        c, const_cast<LifeFixedServiceBoundary &>(v)));
    take(c.finish());
    return output;
  });
}
Outcome<LifeFixedServiceBoundary>
decode_growth_fixed_service_boundary_checkpoint(const CodecVisitor::Buffer &v) {
  return guarded<LifeFixedServiceBoundary>([&] {
    LifeFixedServiceBoundary output;
    CodecVisitor c(v);
    take(visit_growth_fixed_service_boundary(c, output));
    take(c.finish());
    return output;
  });
}
Status emit_growth_complete_fixed_service_boundary_projection(
    ProjectionWriter &sink, std::string_view name,
    const LifeFixedServiceBoundary &value, bool exact_periods, GrowthContributionWireVersion wire) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [&](auto &c, auto &v) {
        return fixed_service_boundary_fields(c, v, true, exact_periods, wire);
      });
}
Outcome<BoundaryValue> encode_growth_complete_fixed_service_boundary(
    const LifeFixedServiceBoundary &value, bool exact_periods, GrowthContributionWireVersion wire) {
  return guarded<BoundaryValue>([&] {
    BoundaryValue output;
    BoundaryProjectionWriter sink(output);
    take(emit_growth_complete_fixed_service_boundary_projection(sink, "", value,
                                                                exact_periods, wire));
    take(sink.finish());
    return output;
  });
}
Outcome<LifeFixedServiceBoundary>
decode_growth_complete_fixed_service_boundary(const BoundaryValue &value,
                                              bool exact_periods, GrowthContributionWireVersion wire) {
  return guarded<LifeFixedServiceBoundary>([&] {
    JsonBudget budget;
    JsonCodec c(value, budget);
    take(c.prepare_default_values<LifeFixedServiceBoundary>());
    LifeFixedServiceBoundary output;
    take(fixed_service_boundary_fields(c, output, true, exact_periods, wire));
    take(c.finish());
    return output;
  });
}
Status visit_growth_complete_fixed_service_boundary(
    CodecVisitor &c, LifeFixedServiceBoundary &value, bool exact_periods, GrowthContributionWireVersion wire) {
  return guarded<Done>([&] {
    if (!c.reading()) {
      take(fixed_service_boundary_fields(c, value, true, exact_periods, wire));
      return Done{};
    }
    LifeFixedServiceBoundary candidate;
    take(fixed_service_boundary_fields(c, candidate, true, exact_periods, wire));
    publish(value, candidate);
    return Done{};
  });
}
// FH care 共用原命令三字段与原 Actor route，不另抄一份字段表。
Status visit_life_actual_command_binding(CodecVisitor &c,
                                         LifeActualCommandBinding &value) {
  return command_binding_fields(c, value);
}
Status
emit_life_actual_command_binding_fields(ProjectionWriter &w,
                                        const LifeActualCommandBinding &value) {
  ActorFoundationProjectionArchive c(w);
  return command_binding_fields(c,
                                const_cast<LifeActualCommandBinding &>(value));
}
Outcome<LifeActualCommandBinding>
decode_life_actual_command_binding(const BoundaryValue &input) {
  return guarded<LifeActualCommandBinding>([&] {
    JsonBudget budget;
    JsonCodec c(input, budget);
    take(c.prepare_default_values<LifeActualCommandBinding>());
    LifeActualCommandBinding value;
    take(command_binding_fields(c, value));
    take(c.finish());
    return value;
  });
}
Status visit_life_actor_route(CodecVisitor &c, LifeActorRoute &value,
                              const StableId &actor) {
  return actor_route_fields(c, value, actor, true);
}
Status emit_life_actor_route_fields(ProjectionWriter &w,
                                    const LifeActorRoute &value,
                                    const StableId &actor) {
  ActorFoundationProjectionArchive c(w);
  return actor_route_fields(c, const_cast<LifeActorRoute &>(value), actor,
                            true);
}
Outcome<LifeActorRoute> decode_life_actor_route(const BoundaryValue &input,
                                                const StableId &actor) {
  return guarded<LifeActorRoute>([&] {
    JsonBudget budget;
    JsonCodec c(input, budget);
    take(c.prepare_default_values<LifeActorRoute>());
    LifeActorRoute value;
    take(actor_route_fields(c, value, actor, true));
    take(c.finish());
    return value;
  });
}

Outcome<LifeActorRoute>
decode_life_actor_route(const BoundaryValue &input, const StableId &actor,
                        ScopedCheckpointJsonDecode &scope) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    scope.failure("CheckpointJsonDecodeScopeMismatch", {"life_actor_route"},
                  ErrorKind::internal);
  return decode_life_actor_route(input, actor);
}
Outcome<LifeActualCommandBinding>
decode_life_actual_command_binding(const BoundaryValue &input,
                                   ScopedCheckpointJsonDecode &scope) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    scope.failure("CheckpointJsonDecodeScopeMismatch",
                  {"life_actual_command_binding"}, ErrorKind::internal);
  return decode_life_actual_command_binding(input);
}

} // namespace hanmo::resident
