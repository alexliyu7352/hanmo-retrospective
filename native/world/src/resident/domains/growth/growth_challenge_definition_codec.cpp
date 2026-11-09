// 原作者三题解析/SHA唯一实现；Family新版本只增加真实作者版本，不复制定义快照。
#include "growth_challenge_definition_codec.hpp"
#include "growth_model.hpp"
#include <algorithm>
#include <picosha2.h>
#include <set>
namespace hanmo::resident {
namespace {
using namespace boundary;
void add(BoundaryValue &value, const StableId &key, BoundaryValue element) {
  std::get<BoundaryValue::Dict>(value.data)
      .emplace_back(key, std::move(element));
}
BoundaryValue scalar(const StableId &value) {
  return BoundaryValue{value};
}
// 摘要排序严格局限冷作者输入，保存的热目录只有固定字段和摘要字符串。
void sorted(BoundaryValue &v) {
  if (auto *rows = v.as<BoundaryValue::Dict>()) {
    std::sort(rows->begin(), rows->end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (auto &row : *rows)
      sorted(row.second);
  } else if (auto *rows = v.as<BoundaryValue::Array>())
    for (auto &row : *rows)
      sorted(row);
}
// 原三轮作者条件只能引用已经定义的节点/正确类别条件；不接收隐藏表达式或重复ID。
void validate_definition_links(const GrowthChallengeDefinition &d) {
  std::set<StableId> node_ids, condition_ids;
  if (d.conditions.size() > 18)
    fail("InvalidBargainDefinition", d.id);
  for (const auto &c : d.conditions)
    if (c.id.empty() || !condition_ids.insert(c.id).second)
      fail("InvalidBargainCondition", c.id);
  std::map<StableId, std::set<StableId>> earlier;
  for (const auto &node : d.nodes) {
    if (node.id.empty() || !node_ids.insert(node.id).second)
      fail(d.kind == "etiquette" ? "InvalidEtiquetteNode"
                                 : "InvalidBargainRound",
           node.id);
    std::set<StableId> options, kinds;
    for (const auto &option : node.options) {
      if (option.id.empty() || !options.insert(option.id).second)
        fail(d.kind == "etiquette" ? "InvalidEtiquetteOption"
                                   : "InvalidBargainOption",
             option.id);
      if (d.kind == "etiquette") {
        std::set<StableId> seen;
        for (const auto &required : option.requires_choices) {
          auto found = earlier.find(required.node_id);
          if (found == earlier.end() ||
              !found->second.count(required.option_id) ||
              !seen.insert(required.node_id).second)
            fail("InvalidEtiquetteCondition", required.node_id);
        }
      } else {
        if (!kinds.insert(option.kind).second)
          fail("InvalidBargainOption", option.id);
        auto check = [&](const std::optional<StableId> &id,
                         const StableId &kind) {
          if (!id)
            return;
          auto found = std::find_if(
              d.conditions.begin(), d.conditions.end(),
              [&](const auto &c) { return c.id == *id && c.kind == kind; });
          if (found == d.conditions.end())
            fail("InvalidBargainConditionReference", *id);
        };
        check(option.supply_condition_id, "supply");
        check(option.terms_condition_id, "terms");
      }
    }
    earlier.emplace(node.id, std::move(options));
  }
}
GrowthChallengeDefinition decode_definition(const BoundaryValue &v,
                                            const StableId &kind,
                                            const CommonCatalog &common,
                                            bool family2) {
  GrowthChallengeDefinition d;
  d.field_order = order(v);
  d.kind = kind;
  d.id = get_string(v, "id", kind);
  d.difficulty = get_int(v, "difficulty", d.id);
  if (d.difficulty < 0 || d.difficulty > 3)
    fail("InvalidChallengeDefinition", d.id);
  if (kind == "etiquette") {
    if (family2) {
      fields(v, {"id", "definition_version", "difficulty", "nodes"}, {}, d.id);
      d.definition_version = get_int(v, "definition_version", d.id);
      if (d.definition_version < 1)
        fail("InvalidFamilyEtiquetteDefinitionVersion", d.id);
    } else
      fields(v, {"id", "difficulty", "nodes"}, {}, d.id);
  } else
    fields(v, {"id", "difficulty", "conditions", "rounds"}, {}, d.id);
  BoundaryValue selected{BoundaryValue::Dict{}};
  auto select = [&](const StableId &key) {
    auto *text = common.texts.find(key);
    if (!text || text->empty() || text->size() > 4096)
      fail("MissingChallengeText", key);
    if (!selected.find(key))
      add(selected, key, scalar(*text));
  };
  if (kind == "bargain")
    for (const auto &entry : array(field(v, "conditions", d.id), d.id)) {
      fields(entry, {"id", "kind", "text_key", "matched"}, {}, d.id);
      GrowthChallengeCondition c;
      c.field_order = order(entry);
      c.id = get_string(entry, "id", d.id);
      c.kind = get_string(entry, "kind", d.id);
      c.text_key = get_string(entry, "text_key", d.id);
      c.matched = get_bool(entry, "matched", d.id);
      if (c.kind != "supply" && c.kind != "terms")
        fail("InvalidBargainCondition", c.id);
      select(c.text_key);
      d.conditions.push_back(std::move(c));
    }
  const auto &nodes =
      array(field(v, kind == "etiquette" ? "nodes" : "rounds", d.id), d.id);
  if (nodes.size() != 3)
    fail("InvalidChallengeDefinition", d.id);
  for (const auto &row : nodes) {
    GrowthChallengeNode n;
    n.field_order = order(row);
    n.id = get_string(row, "id", d.id);
    if (kind == "etiquette") {
      fields(row, {"id", "topic_key", "stance_key", "options"}, {}, d.id);
      n.topic_key = get_string(row, "topic_key", d.id);
      n.stance_key = get_string(row, "stance_key", d.id);
      select(n.topic_key);
      select(n.stance_key);
    } else {
      fields(row, {"id", "prompt_key", "options"}, {}, d.id);
      n.prompt_key = get_string(row, "prompt_key", d.id);
      select(n.prompt_key);
    }
    const auto &options = array(field(row, "options", d.id), d.id);
    if (options.size() != 3)
      fail("InvalidChallengeNode", n.id);
    for (const auto &entry : options) {
      GrowthChallengeOption o;
      o.field_order = order(entry);
      o.id = get_string(entry, "id", d.id);
      o.text_key = get_string(entry, "text_key", d.id);
      select(o.text_key);
      if (kind == "etiquette") {
        fields(entry, {"id", "text_key", "points", "requires_choices"}, {},
               d.id);
        o.points = get_int(entry, "points", d.id);
        if (o.points < 0 || o.points > 25)
          fail("InvalidEtiquetteOption", o.id);
        for (const auto &condition :
             array(field(entry, "requires_choices", d.id), d.id)) {
          fields(condition, {"node_id", "option_id"}, {}, d.id);
          o.requires_choices.push_back(
              {get_string(condition, "node_id", d.id),
               get_string(condition, "option_id", d.id), order(condition)});
        }
      } else {
        fields(entry,
               {"id", "kind", "method", "text_key", "supply_condition_id",
                "terms_condition_id"},
               {}, d.id);
        o.kind = get_string(entry, "kind", d.id);
        o.method = get_string(entry, "method", d.id);
        if ((o.kind == "quantity" && o.method != "small_quantity") ||
            (o.kind == "batch" && o.method != "combined_purchase") ||
            (o.kind == "payment" && o.method != "cash") ||
            (o.kind != "quantity" && o.kind != "batch" && o.kind != "payment"))
          fail("InvalidBargainOption", o.id);
        auto nullable = [&](const char *key) -> std::optional<StableId> {
          auto &value = field(entry, key, d.id);
          return value.as<std::monostate>()
                     ? std::nullopt
                     : std::optional<StableId>{string(value, d.id)};
        };
        o.supply_condition_id = nullable("supply_condition_id");
        o.terms_condition_id = nullable("terms_condition_id");
      }
      n.options.push_back(std::move(o));
    }
    d.nodes.push_back(std::move(n));
  }
  auto source = obj({{"definition", v}, {"texts", selected}});
  sorted(source);
  auto json = render_boundary_json(source);
  if (!json.ok)
    fail(json.error.original.code, d.id);
  validate_definition_links(d);
  d.source_sha256 = picosha2::hash256_hex_string(json.value);
  return d;
}
} // namespace
// 后置婚姻自治只保留不支持能力门；原 Study/Bargain 定义解码保持原接受域。
Status
validate_growth_family_etiquette_selection2(const CatalogIdentity &) {
  return common_failure("NOT_IMPLEMENTED", "director_family_autonomy",
                        ErrorKind::unsupported_capability);
}
// 原Study/Bargain旧入口不接受新version字段；原排序、题面、依赖与SHA逐字保持。
Outcome<GrowthChallengeDefinition>
decode_original_growth_challenge_definition(const BoundaryValue &value,
                                            const StableId &kind,
                                            const CommonCatalog &common) {
  try {
    if (kind != "etiquette" && kind != "bargain")
      return {false, {}, common_failure("InvalidChallengeKind", kind).error};
    return {true, decode_definition(value, kind, common, false), {}};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
// 新Family只复用原etiquette三题定义；SHA输入保留真实正版本及原公开文本。
Outcome<GrowthChallengeDefinition>
decode_family_etiquette_definition2(const BoundaryValue &value,
                                    const CommonCatalog &common,
                                    const CatalogIdentity &identity) {
  auto selected = validate_growth_family_etiquette_selection2(identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  try {
    return {true, decode_definition(value, "etiquette", common, true), {}};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
