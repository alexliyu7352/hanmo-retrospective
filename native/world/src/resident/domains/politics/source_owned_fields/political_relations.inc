// P3 原六值型的唯一拥有字段；由中央 CheckpointValueFields 原命名空间包含。
// 只遍历实际成员，不派生关系、不制造事实或使用权。
template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalFactionPair &value,
                  Visitor &&visitor) {
  visitor(value.first);
  visitor(value.second);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalInitialRelation &value,
                  Visitor &&visitor) {
  visitor(value.factions);
  visitor(value.relation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalRelationsCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.initial_relations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalBattleRelationChange &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.factions);
  visitor(value.battle_start);
  visitor(value.occurred_at);
  visitor(value.before);
  visitor(value.alliance_breach);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalAllianceBreach &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.relation_change);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalRelationsState &value,
                  Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.initialization);
  visitor(value.initialized_at);
  visitor(value.battle_changes);
  visitor(value.alliance_breaches);
}
