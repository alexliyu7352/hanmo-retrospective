// 职责：集中个人行动中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalBattleLifecycleEvent1 &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalBattleLifecycle1 &value,
                  Visitor &&visitor) {
  visitor(value.started);
  visitor(value.result_applied_at);
  visitor(value.closed);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalBattle &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.controller_id);
  visitor(value.battlefield_id);
  visitor(value.objective_id);
  visitor(value.source);
  visitor(value.created_tick);
  visitor(value.completed_tick);
  visitor(value.round);
  visitor(value.phase);
  visitor(value.outcome);
  visitor(value.result_id);
  visitor(value.units);
  visitor(value.ai_plans);
  visitor(value.last_events);
  visitor(value.applied);
  visitor(value.field_order);
  visitor(value.lifecycle);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalEvent &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.target_id);
  visitor(value.action);
  visitor(value.skill_id);
  visitor(value.code);
  visitor(value.status);
  visitor(value.damage);
  visitor(value.expires_round);
  visitor(value.amount);
  visitor(value.hit_bp);
  visitor(value.roll);
  visitor(value.position);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalFact &value,
                  Visitor &&visitor) {
  visitor(value.battle_id);
  visitor(value.result_id);
  visitor(value.outcome);
  visitor(value.tick);
  visitor(value.participants);
  visitor(value.defeated);
  visitor(value.withdrawn);
  visitor(value.source);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalHitLevels &value,
                  Visitor &&visitor) {
  visitor(value.martial);
  visitor(value.archery);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalPlan &value,
                  Visitor &&visitor) {
  visitor(value.destination);
  visitor(value.heading);
  visitor(value.action);
  visitor(value.target_id);
  visitor(value.skill_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalPoint &value,
                  Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.encounter_id);
  visitor(value.context);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalState &value,
                  Visitor &&visitor) {
  visitor(value.battles);
  visitor(value.facts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalUnit &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.side);
  visitor(value.heading);
  visitor(value.radius);
  visitor(value.hp);
  visitor(value.initial_hp);
  visitor(value.stamina);
  visitor(value.martial);
  visitor(value.recovery_until);
  visitor(value.position);
  visitor(value.weapon_id);
  visitor(value.heart_id);
  visitor(value.learned);
  visitor(value.statuses);
  visitor(value.protected_actor);
  visitor(value.skill_levels);
  visitor(value.armor_id);
  visitor(value.armor_defense);
  visitor(value.withdrawn_round);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalReceipt &value,
                  Visitor &&visitor) {
  visitor(value.battle_id);
  visitor(value.round);
  visitor(value.outcome);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateLivelihoodPolicy &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.target_quantity);
  visitor(value.cash_reserve);
  visitor(value.work_fatigue_limit);
  visitor(value.known_service_role_ids);
  visitor(value.return_place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalWeapon &value,
                  Visitor &&visitor) {
  visitor(value.category);
  visitor(value.minimum_range);
  visitor(value.maximum_range);
  visitor(value.arc_degrees);
  visitor(value.damage);
  visitor(value.stamina_cost);
  visitor(value.recovery_rounds);
  visitor(value.move_distance);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalTerms &value,
                  Visitor &&visitor) {
  visitor(value.battlefield_id);
  visitor(value.objective_id);
  visitor(value.fee);
  visitor(value.wallet_id);
  visitor(value.field_order);
  visitor(value.cargo_policy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalSpawn &value,
                  Visitor &&visitor) {
  visitor(value.side);
  visitor(value.heading);
  visitor(value.position);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalSkillRequirement &value,
                  Visitor &&visitor) {
  visitor(value.skill_id);
  visitor(value.level);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalSkill &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.description_key);
  visitor(value.kind);
  visitor(value.status);
  visitor(value.weapons);
  visitor(value.practice_ticks);
  visitor(value.stamina_cost);
  visitor(value.power_bp);
  visitor(value.defense_bp);
  visitor(value.move_bonus);
  visitor(value.status_rounds);
  visitor(value.required_skill_level);
  visitor(value.requires_stationary);
  visitor(value.maximum_range_bonus);
  visitor(value.hit_bonus_bp);
  visitor(value.defend_recovery_bonus);
  visitor(value.counter_damage_bp);
  visitor(value.guard_facing);
  visitor(value.area);
  visitor(value.field_order);
  visitor(value.range_order);
  visitor(value.counter_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalObstacle &value,
                  Visitor &&visitor) {
  visitor(value.x);
  visitor(value.y);
  visitor(value.width);
  visitor(value.height);
  visitor(value.blocks_movement);
  visitor(value.blocks_sight);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalObjective &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.kind);
  visitor(value.max_rounds);
  visitor(value.injury_on_defeat);
  visitor(value.allow_withdrawal);
  visitor(value.ai_model);
  visitor(value.escort_model);
  visitor(value.stamina_model);
  visitor(value.hit_model);
  visitor(value.initial_stamina);
  visitor(value.facing_damage);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalFacing &value,
                  Visitor &&visitor) {
  visitor(value.side_bp);
  visitor(value.back_bp);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalExit &value,
                  Visitor &&visitor) {
  visitor(value.side);
  visitor(value.radius);
  visitor(value.position);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalCatalog &value,
                  Visitor &&visitor) {
  visitor(value.enabled);
  visitor(value.weapons);
  visitor(value.armor_defenses);
  visitor(value.skills);
  visitor(value.objectives);
  visitor(value.arenas);
  visitor(value.sparring);
  visitor(value.road_conflicts);
  visitor(value.simultaneous_outcome_objectives);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalArena &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.name_key);
  visitor(value.visual_tag);
  visitor(value.width);
  visitor(value.height);
  visitor(value.obstacles);
  visitor(value.spawns);
  visitor(value.exits);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PersonalArea &value,
                  Visitor &&visitor) {
  visitor(value.shape);
  visitor(value.max_targets);
  visitor(value.half_width);
  visitor(value.arc_degrees);
  visitor(value.field_order);
}
