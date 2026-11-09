#include "injury_capability_map.hpp"
#include "injury_grade_effects_v3.hpp"
// 原冷作者输入的严格适配；从锁定initial取基属性/技能，不接受活世界或任意seed
// setter。
#include "authored_origin.hpp"
#include "resident/checkpoint/checkpoint_catalog_rows.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_codec.hpp"
#include <string_view>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
const V *find(const V &value, std::string_view name) {
  if (const auto *rows = value.as<V::Dict>())
    for (const auto &row : *rows)
      if (row.first == name)
        return &row.second;
  return nullptr;
}
std::string child(std::string_view path, std::string_view name) {
  return checkpoint_decode_boundary_join({path, "/", name});
}
void copy_identity(CatalogIdentity &destination,
                   const CatalogIdentity &source) {
  if (!checkpoint_boundary_decode_active()) {
    destination = source;
    return;
  }


  checkpoint_decode_boundary_array(std::size_t{0}, 1);
  destination = source;
}
void copy_seed(InitialConditionSeed &destination,
               const InitialConditionSeed &source) {
  if (!checkpoint_boundary_decode_active()) {
    destination = source;
    return;
  }


  checkpoint_decode_boundary_array(std::size_t{0}, 1);
  destination = source;
}

// 每层都核重复/未知/缺字段，直接构造的BoundaryValue也不能绕JSON parser。
void shape(const V &v, std::initializer_list<const char *> required,
           std::initializer_list<const char *> optional,
           std::string_view path) {
  boundary::order(v);
  boundary::fields(v, required, optional, path);
}
// 作者引用只有省略摘要时才绑定已验内容锁；显式摘要不改写，空或异锁拒绝。
// 这是冷作者输入，不改变运行 DTO 或存档 decoder 的完整摘要要求。
StableId authored_digest(const V &v, std::string_view p,
                         const CatalogIdentity &identity) {
  if (identity.content_digest.empty())
    boundary::fail("AuthoredCatalogIdentityRequired", p);
  if (find(v, "catalog_digest")) {
    auto value = boundary::get_string(v, "catalog_digest", p);
    if (value.empty() || value != identity.content_digest)
      boundary::fail("AuthoredCatalogDigestMismatch",
                     child(p, "catalog_digest"));
    return value;
  }
  return checkpoint_decode_boundary_string(identity.content_digest);
}
// 锁内规则引用最终仍保 id/version/digest 全部值，不把符号留进世界。
ActorFoundationRuleRef authored_rule(const V &v, std::string_view p,
                                     const CatalogIdentity &identity) {
  shape(v, {"id", "version"}, {"catalog_digest"}, p);
  return {boundary::get_string(v, "id", p), boundary::get_int(v, "version", p),
          authored_digest(v, p, identity)};
}
// 原 ScenarioOrigin 完整复合键仍由原验证核同情景/同人物/初始时点。
ScenarioOriginRef authored_origin(const V &v, std::string_view p,
                                  const CatalogIdentity &identity) {
  shape(v, {"scenario_id", "row_key", "initial_tick"}, {"catalog_digest"}, p);
  return {authored_digest(v, p, identity),
          boundary::get_string(v, "scenario_id", p),
          boundary::get_string(v, "row_key", p),
          boundary::get_int(v, "initial_tick", p)};
}
// 未接入领域种子保真实空；不能默默丢弃作者需要的业务。
void empty(const V &v, std::string_view p) {
  if (!boundary::array(v, p).empty())
    boundary::unsupported("UnsupportedAuthoredActorFoundationKind", p);
}
// 初态只接受完整声明整岁；没有真正Birth reader就不伪支持ExactBirth。
AuthoredActorAgeBasis authored_age(const V &v, std::string_view p,
                                   const CatalogIdentity &identity) {
  shape(v, {"kind", "years", "as_of_tick", "calendar_year_ordinal", "source"},
        {}, p);
  if (boundary::get_string(v, "kind", p) != "DeclaredAgeAtStart")
    boundary::unsupported("UnsupportedAuthoredAgeBasis", p);
  AuthoredActorAgeBasis a;
  a.years = boundary::get_int(v, "years", p);
  a.as_of_tick = boundary::get_int(v, "as_of_tick", p);
  a.calendar_year_ordinal = boundary::get_int(v, "calendar_year_ordinal", p);
  a.source = authored_origin(boundary::field(v, "source", p),
                             child(p, "source"), identity);
  return a;
}
// 初始身体完整源只在这里读取一次，非空episodes待真正provider。
InitialConditionSeed authored_seed(const V &v, std::string_view p,
                                   const CatalogIdentity &identity) {
  shape(v,
        {"seed_key", "actor_id", "source", "hp", "fatigue", "injury",
         "episodes", "activity_limit_profile"},
        {}, p);
  InitialConditionSeed s;
  s.seed_key = boundary::get_string(v, "seed_key", p);
  s.actor_id = boundary::get_string(v, "actor_id", p);
  s.source = authored_origin(boundary::field(v, "source", p),
                             child(p, "source"), identity);
  s.hp = boundary::get_int(v, "hp", p);
  s.fatigue = boundary::get_int(v, "fatigue", p);
  s.injury = boundary::get_int(v, "injury", p);
  empty(boundary::field(v, "episodes", p), child(p, "episodes"));
  s.activity_limit_profile =
      authored_rule(boundary::field(v, "activity_limit_profile", p),
                    child(p, "activity_limit_profile"), identity);
  return s;
}
// 基属性从原已锁initial同一人物行取得，不从Common.current猜回去。
ActorAttributes attributes(const V &v, std::string_view p) {
  shape(v, {"administration", "charisma", "intellect", "leadership", "martial"},
        {}, p);
  ActorAttributes a;
  for (auto item :
       {std::pair<const char *, Integer *>{"administration", &a.administration},
        {"charisma", &a.charisma},
        {"intellect", &a.intellect},
        {"leadership", &a.leadership},
        {"martial", &a.martial}}) {
    *item.second = boundary::get_int(v, item.first, p);
    if (*item.second < 1 || *item.second > 100)
      boundary::fail("InvalidAuthoredBaseAttribute", child(p, item.first));
  }
  return a;
}
// 原ten/sixteen完整技能联合，不把缺扩展块当六个0。
ActorSkills skills(const V &v, std::string_view p) {
  const bool extended = find(v, "naval") != nullptr;
  boundary::order(v);
  if (extended)
    shape(v,
          {"archery", "cavalry", "commerce", "debate", "infantry", "martial",
           "negotiation", "rites", "tactics", "transport", "naval", "espionage",
           "fortification", "agriculture", "medicine", "forging"},
          {}, p);
  else
    shape(v,
          {"archery", "cavalry", "commerce", "debate", "infantry", "martial",
           "negotiation", "rites", "tactics", "transport"},
          {}, p);
  ActorSkills s;
  for (auto item : {std::pair<const char *, Integer *>{"archery", &s.archery},
                    {"cavalry", &s.cavalry},
                    {"commerce", &s.commerce},
                    {"debate", &s.debate},
                    {"infantry", &s.infantry},
                    {"martial", &s.martial},
                    {"negotiation", &s.negotiation},
                    {"rites", &s.rites},
                    {"tactics", &s.tactics},
                    {"transport", &s.transport}}) {
    *item.second = boundary::get_int(v, item.first, p);
    if (*item.second < 0 || *item.second > 1200)
      boundary::fail("InvalidAuthoredSkill", child(p, item.first));
  }
  if (extended) {
    s.extended.emplace();
    auto &x = *s.extended;
    for (auto item : {std::pair<const char *, Integer *>{"naval", &x.naval},
                      {"espionage", &x.espionage},
                      {"fortification", &x.fortification},
                      {"agriculture", &x.agriculture},
                      {"medicine", &x.medicine},
                      {"forging", &x.forging}}) {
      *item.second = boundary::get_int(v, item.first, p);
      if (*item.second < 0 || *item.second > 1200)
        boundary::fail("InvalidAuthoredSkill", child(p, item.first));
    }
  }
  return s;
}
} // namespace
Outcome<ActorFoundationCatalog> decode_authored_actor_foundation_catalog(
    const BoundaryValue &selected, const BoundaryValue &initial,
    const CatalogIdentity &identity, const StableId &scenario_id,
    std::shared_ptr<const GrowthCatalog> growth) {
  try {
    const auto rule = [&](const V &value, std::string_view path) {
      return authored_rule(value, path, identity);
    };
    const auto age = [&](const V &value, std::string_view path) {
      return authored_age(value, path, identity);
    };
    const auto seed = [&](const V &value, std::string_view path) {
      return authored_seed(value, path, identity);
    };
    constexpr std::string_view path = "actor_foundation";
    shape(selected,
          {"construction_profile_ref", "provenance_policy_ref", "rest_rule_ref",
           "rest_purpose_ref", "provider_purpose_ref",
           "initial_condition_seeds", "actor_states"},
          {"travel_rule_ref", "work_or_study_rule_ref",
           "study_capability_rule_ref", "normal_work_efficiency_rule",
           "capability_purpose_bindings", "original_military_wound_rule",
           "injury_capability_map", "original_battle_injury_rules", "military_death_rule_ref",
           "natural_age_death_rule_ref", "sea_death_rule_ref"},
          path);
    ActorFoundationCatalog c;
    copy_identity(c.identity, identity);
    c.scenario_id = checkpoint_decode_boundary_string(scenario_id);
    c.original_growth = std::move(growth);
    c.construction_profile =
        rule(boundary::field(selected, "construction_profile_ref", path),
             child(path, "construction_profile_ref"));
    c.provenance_policy =
        rule(boundary::field(selected, "provenance_policy_ref", path),
             child(path, "provenance_policy_ref"));
    c.rest_rule = rule(boundary::field(selected, "rest_rule_ref", path),
                       child(path, "rest_rule_ref"));
    c.rest_purpose = rule(boundary::field(selected, "rest_purpose_ref", path),
                          child(path, "rest_purpose_ref"));
    c.provider_purpose =
        rule(boundary::field(selected, "provider_purpose_ref", path),
             child(path, "provider_purpose_ref"));
    if (const auto *travel = find(selected, "travel_rule_ref"))
      c.travel_rule = rule(*travel, child(path, "travel_rule_ref"));
    if (const auto *work = find(selected, "work_or_study_rule_ref"))
      c.work_or_study_rule = rule(*work, child(path, "work_or_study_rule_ref"));
    if (const auto *study = find(selected, "study_capability_rule_ref"))
      c.study_capability_rule =
          rule(*study, child(path, "study_capability_rule_ref"));
    for (auto item :
         {std::pair<const char *, std::optional<ActorFoundationRuleRef> *>{
              "military_death_rule_ref", &c.military_death_rule},
          {"natural_age_death_rule_ref", &c.natural_age_death_rule},
          {"sea_death_rule_ref", &c.sea_death_rule}})
      if (const auto *source = find(selected, item.first))
        *item.second = rule(*source, child(path, item.first));
    if (const auto *source = find(selected, "original_military_wound_rule")) {
      shape(*source,
            {"rule_ref", "injury_rule_ref", "result_choice_ref",
             "original_casualty_rule_ref", "selected_result"},
            {}, child(path, "original_military_wound_rule"));
      const auto selected_result =
          boundary::get_string(*source, "selected_result", path);
      if (selected_result != "NoAdditionalEffect" &&
          selected_result != "Applied")
        boundary::unsupported("SelectedMilitaryInjuryEffectAdapterRequired",
                              path);
      OriginalMilitaryWoundRule binding;
      binding.selected_applied = selected_result == "Applied";
      binding.rule_ref = rule(boundary::field(*source, "rule_ref", path), path);
      binding.injury_rule_ref =
          rule(boundary::field(*source, "injury_rule_ref", path), path);
      binding.result_choice_ref =
          rule(boundary::field(*source, "result_choice_ref", path), path);
      binding.original_casualty_rule_ref =
          boundary::get_string(*source, "original_casualty_rule_ref", path);
      c.original_military_wound_rule = std::move(binding);
    }

    if (const auto *source = find(selected, "original_battle_injury_rules")) {
      const auto &rows = boundary::array(*source, "original_battle_injury_rules");
      if (rows.empty() || rows.size() > 2)
        boundary::fail("BattleInjurySelectionCount", path);
      checkpoint_decode_boundary_array(sizeof(OriginalBattleInjuryRuleV1), rows.size());
      for (const auto &row : rows) {
        shape(row, {"cause_adapter", "rule_ref", "injury_rule_ref",
                    "result_choice_ref", "selected_result"}, {}, path);
        OriginalBattleInjuryRuleV1 binding;
        const auto adapter = boundary::get_string(row, "cause_adapter", path);
        if (adapter == "field_segment_wounded")
          binding.cause_adapter = InjuryCapabilityMapEntry::CauseAdapter::field_segment_wounded;
        else if (adapter == "personal_defeat")
          binding.cause_adapter = InjuryCapabilityMapEntry::CauseAdapter::personal_defeat;
        else boundary::unsupported("BattleInjuryAdapterRequired", path);
        const auto result = boundary::get_string(row, "selected_result", path);
        if (result != "Applied" && result != "NoAdditionalEffect")
          boundary::unsupported("BattleInjuryExplicitResultRequired", path);
        binding.selected_applied = result == "Applied";
        binding.rule_ref = rule(boundary::field(row, "rule_ref", path), path);
        binding.injury_rule_ref = rule(boundary::field(row, "injury_rule_ref", path), path);
        binding.result_choice_ref = rule(boundary::field(row, "result_choice_ref", path), path);
        c.original_battle_injury_rules.push_back(std::move(binding));
      }
    }

    if (const auto *source = find(selected, "injury_capability_map")) {
      auto decoded = decode_authored_injury_capability_map(*source, c.identity);
      if (!decoded.ok)
        return {false, {}, std::move(decoded.error)};
      c.injury_capability_map = std::move(decoded.value);
      if (c.injury_capability_map->version == 3) {
        if (find(selected, "original_battle_injury_rules"))
          boundary::fail("OriginalInjuryGradeDuplicateAuthorChoice", path);
        auto binding = install_original_injury_grade_bindings_v3(c);
        if (!binding.ok)
          return {false, {}, binding.error};
      }
    }
    auto injury_map_valid = validate_injury_capability_map(c);
    if (!injury_map_valid.ok)
      return {false, {}, std::move(injury_map_valid.error)};

    if (const auto *source = find(selected, "normal_work_efficiency_rule")) {
      shape(*source,
            {"rule_ref", "fatigue_cutoffs", "fatigue_caps", "injury_caps"}, {},
            child(path, "normal_work_efficiency_rule"));
      CharacterNormalWorkEfficiencyRule work;
      work.rule_ref = rule(boundary::field(*source, "rule_ref", path),
                           child(path, "normal_work_efficiency_rule/rule_ref"));
      auto values = [&](const char *field, auto &target) {
        const auto &rows =
            boundary::array(boundary::field(*source, field, path), path);
        if (rows.size() != target.size())
          boundary::fail("CharacterWorkRuleParameterCount", field);
        for (std::size_t n = 0; n < rows.size(); ++n) {
          const auto *value = rows[n].as<Integer>();
          if (!value)
            boundary::fail("CharacterWorkRuleParameterType", field);
          target[n] = *value;
        }
      };
      values("fatigue_cutoffs", work.fatigue_cutoffs);
      values("fatigue_caps", work.fatigue_caps);
      values("injury_caps", work.injury_caps);
      c.normal_work_efficiency_rule = std::move(work);
    }
    if (const auto *bindings = find(selected, "capability_purpose_bindings")) {
      const auto &inputs = boundary::array(
          *bindings, child(path, "capability_purpose_bindings"));
      CheckpointCatalogRows<CharacterPurposeBinding> purposes(inputs.size(),
                                                              path);
      for (const auto &source : inputs) {
        shape(source, {"purpose_ref", "rule_ref", "reducer"}, {}, path);
        CharacterPurposeBinding value;
        value.purpose_ref =
            rule(boundary::field(source, "purpose_ref", path), path);
        value.rule_ref = rule(boundary::field(source, "rule_ref", path), path);
        auto kind = boundary::get_string(source, "reducer", path);
        if (kind != "normal_work" && kind != "expression")
          boundary::unsupported("CharacterPurposeReducerUnsupported", kind);
        value.reducer = kind == "normal_work"
                            ? CharacterPurposeBinding::Reducer::normal_work
                            : CharacterPurposeBinding::Reducer::expression;
        const auto &key = value.purpose_ref.id;
        auto copied_key = checkpoint_decode_boundary_string(key);
        if (!purposes.insert(copied_key, std::move(value)))
          boundary::fail("DuplicateCharacterPurposeBinding", copied_key);
      }
      purposes.publish(c.capability_purpose_bindings);
    }
    const auto &seed_inputs = boundary::array(
        boundary::field(selected, "initial_condition_seeds", path), path);
    CheckpointCatalogRows<InitialConditionSeed> seeds(seed_inputs.size(), path);
    for (const auto &row : seed_inputs) {
      auto s = seed(row, child(path, "initial_condition_seeds"));
      auto key = checkpoint_decode_boundary_string(s.seed_key);
      if (!seeds.insert(key, std::move(s)))
        boundary::fail("DuplicateInitialConditionSeed", key);
    }
    const auto &original_inputs = boundary::array(
        boundary::field(initial, "actors", "initial"), "initial/actors");
    CheckpointCatalogRows<const V *> original(original_inputs.size(), path);
    for (const auto &row : original_inputs) {
      auto id = boundary::get_string(row, "actor_id", "initial/actors");
      if (!original.insert(id, &row))
        boundary::fail("DuplicateInitialActor", id);
    }
    CheckpointVector<const StableId *> used;
    used.reserve(seeds.size());
    const auto &actor_inputs =
        boundary::array(boundary::field(selected, "actor_states", path),
                        child(path, "actor_states"));
    CheckpointCatalogRows<AuthoredActorFoundationDefinition> actors(
        actor_inputs.size(), path);
    for (const auto &row : actor_inputs) {
      shape(row,
            {"actor_id", "identity_definition_ref", "alive", "hp", "fatigue",
             "injury", "age_basis", "condition_seed_refs", "placement",
             "service_seed_refs", "office_seed_refs",
             "property_business_seed_refs", "initial_knowledge_refs",
             "appearance_source_ref"},
            {"activity_seed_ref", "custody_seed_ref"},
            child(path, "actor_states"));
      AuthoredActorFoundationDefinition d;
      d.actor_id = boundary::get_string(row, "actor_id", path);
      const auto *old = original.find(d.actor_id);
      if (!old)
        boundary::fail("MissingOriginalAuthoredActor", d.actor_id);
      d.identity_definition_ref =
          rule(boundary::field(row, "identity_definition_ref", path), path);
      d.alive = boundary::get_bool(row, "alive", path);
      d.available = boundary::get_bool(**old, "available", d.actor_id);
      d.age_basis = age(boundary::field(row, "age_basis", path), path);
      d.scenario_origin_ref = {
          checkpoint_decode_boundary_string(identity.content_digest),
          checkpoint_decode_boundary_string(scenario_id),
          checkpoint_decode_boundary_string(d.actor_id), 0};
      const auto &refs = boundary::array(
          boundary::field(row, "condition_seed_refs", path), path);
      if (refs.size() != 1)
        boundary::unsupported("UnsupportedAuthoredConditionSeedCount",
                              d.actor_id);
      shape(refs.front(), {"seed_key"}, {}, path);
      const auto seed_key =
          boundary::get_string(refs.front(), "seed_key", path);
      const auto *s = seeds.find(seed_key);
      if (!s)
        boundary::fail("MissingOrDuplicateAuthoredConditionSeed", d.actor_id);
      for (const auto *prior : used)
        if (*prior == seed_key)
          boundary::fail("MissingOrDuplicateAuthoredConditionSeed", d.actor_id);
      used.push_back(&s->seed_key);
      copy_seed(d.condition, *s);
      if (d.condition.actor_id != d.actor_id ||
          d.condition.hp != boundary::get_int(row, "hp", path) ||
          d.condition.fatigue != boundary::get_int(row, "fatigue", path) ||
          d.condition.injury != boundary::get_int(row, "injury", path))
        boundary::fail("AuthoredConditionSeedValueMismatch", d.actor_id);
      const auto &place = boundary::field(row, "placement", path);
      shape(place, {"kind", "place_ref"}, {}, path);
      if (boundary::get_string(place, "kind", path) != "AtPlace")
        boundary::unsupported("UnsupportedAuthoredStartPlacement", d.actor_id);
      d.placement.place_ref = boundary::get_string(place, "place_ref", path);
      if (d.placement.place_ref !=
          boundary::get_string(**old, "place_id", d.actor_id))
        boundary::fail("AuthoredPlacementSourceMismatch", d.actor_id);
      for (const auto *field :
           {"service_seed_refs", "office_seed_refs",
            "property_business_seed_refs", "initial_knowledge_refs"})
        empty(boundary::field(row, field, path), child(path, field));
      for (const auto *field : {"activity_seed_ref", "custody_seed_ref"})
        if (find(row, field))
          boundary::unsupported("UnsupportedAuthoredDomainSeed",
                                child(path, field));
      d.appearance_source_ref =
          rule(boundary::field(row, "appearance_source_ref", path), path);
      d.attributes =
          attributes(boundary::field(**old, "attributes", d.actor_id),
                     child(d.actor_id, "attributes"));
      d.skills = skills(boundary::field(**old, "skills", d.actor_id),
                        child(d.actor_id, "skills"));
      auto key = checkpoint_decode_boundary_string(d.actor_id);
      if (!actors.insert(key, std::move(d)))
        boundary::fail("DuplicateAuthoredActorFoundation", key);
    }
    if (used.size() != seeds.size() || actors.size() != original.size())
      boundary::fail("IncompleteAuthoredActorFoundation", path);
    actors.publish(c.actors);
    // 在未发布的局部Actor上调用同一严格规则门；没有活世界可被这里修改。
    for (const auto &row : c.actors.rows()) {
      ActorRecord candidate;
      candidate.id = checkpoint_decode_boundary_string(row.first);
      auto valid =
          initialize_authored_actor_foundation_actor(candidate, c, row.first);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    return {true, std::move(c), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace hanmo::resident
