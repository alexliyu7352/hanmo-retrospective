#pragma once
#include "political_relations.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident::political_relations_fields {
using organization_lifecycle_fields::source;
using organization_lifecycle_fields::table;
// 只有本域实际变更行含可抛 WorldMoment 复制。沿原表一次预留/采用，
// 保留暂存行至迁移结束；失败丢弃整未发布候选，不扩大组织域有限类型表。
struct PoliticalBattleChangeDecodedTransfer {
  template <class Destination>
  Status
  operator()(Destination &destination,
             std::pair<StableId, PoliticalBattleRelationChange> &row) const {
    destination.emplace_back(std::move(row));
    return common_success();
  }
};
template <class F>
Status changes(CodecVisitor &c,
               OrderedTable<PoliticalBattleRelationChange> &rows, F f) {
  return common_checkpoint::table(c, "battle_changes", rows, f,
                                  PoliticalBattleChangeDecodedTransfer{});
}
template <class F>
Status changes(ActorFoundationJsonVisitor &c,
               OrderedTable<PoliticalBattleRelationChange> &rows, F f) {
  return c.table("battle_changes", rows, f,
                 PoliticalBattleChangeDecodedTransfer{});
}
template <class F>
Status changes(ActorFoundationProjectionArchive &c,
               OrderedTable<PoliticalBattleRelationChange> &rows, F f) {
  return c.table("battle_changes", rows, f);
}
template <class C> Status pair(C &c, PoliticalFactionPair &p) {
  c.field("first", p.first);
  return c.field("second", p.second);
}
template <class C>
Status relation(C &c, const char *name, PoliticalRelation &r) {
  static constexpr const char *tags[] = {"hostile", "neutral", "allied"};
  return organization_lifecycle_fields::enum_field(c, name, r, tags);
}
template <class C> Status initial(C &c, PoliticalInitialRelation &r) {
  c.object("factions", [&](auto &v) { return pair(v, r.factions); });
  return relation(c, "relation", r.relation);
}
template <class C> Status change(C &c, PoliticalBattleRelationChange &r) {
  c.field("id", r.id);
  c.field("battle_id", r.battle_id);
  c.object("factions", [&](auto &v) { return pair(v, r.factions); });
  c.object("battle_start", [&](auto &v) {
    v.object("source_ref",
             [&](auto &z) { return source(z, r.battle_start.source_ref); });
    return v.field("source_schema", r.battle_start.source_schema);
  });
  c.object("occurred_at",
           [&](auto &v) { return exact_fields::moment(v, r.occurred_at); });
  relation(c, "before", r.before);
  return c.optional("alliance_breach", r.alliance_breach,
                    [](auto &v, auto &ref) { return source(v, ref); });
}
template <class C> Status breach(C &c, PoliticalAllianceBreach &r) {
  c.field("id", r.id);
  return c.object("relation_change",
                  [&](auto &v) { return source(v, r.relation_change); });
}
template <class C> Status state(C &c, PoliticalRelationsState &r) {
  c.field("schema_version", r.schema_version);
  if (!c.status().ok)
    return c.status();
  if (r.schema_version != 1)
    return c.reject("UnsupportedPoliticalRelationsSchema");
  c.object("initialization",
           [&](auto &v) { return source(v, r.initialization); });
  c.object("initialized_at",
           [&](auto &v) { return exact_fields::moment(v, r.initialized_at); });
  changes(c, r.battle_changes, [](auto &v, auto &x) { return change(v, x); });
  table(c, "alliance_breaches", r.alliance_breaches,
        [](auto &v, auto &x) { return breach(v, x); });
  if (c.status().ok) {
    auto valid = validate_political_relations_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}
template <class C> Status catalog(C &c, PoliticalRelationsCatalog &r) {
  Integer schema = 1;
  c.field("schema_version", schema);
  if (!c.status().ok)
    return c.status();
  if (schema != 1)
    return c.reject("UnsupportedPoliticalRelationsCatalogSchema");
  c.sequence("initial_relations", r.initial_relations,
             [](auto &v, auto &x) { return initial(v, x); });
  if (c.status().ok) {
    auto valid = validate_political_relations_catalog_shape(r);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  return c.status();
}
} // namespace hanmo::resident::political_relations_fields
