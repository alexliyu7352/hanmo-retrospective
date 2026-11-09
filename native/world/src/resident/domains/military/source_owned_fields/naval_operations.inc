// 职责：集中军旅与战斗中的水军航行、海战和维修访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalAssetState &value,
                  Visitor &&visitor) {
  visitor(value.vessels);
  visitor(value.fleets);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBattleBinding &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.unit_id);
  visitor(value.fleet_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBattleRecord &value,
                  Visitor &&visitor) {
  visitor(value.frame);
  visitor(value.ai_orders);
  visitor(value.pending_orders);
  visitor(value.rng_seed);
  visitor(value.rng_state);
  visitor(value.created_tick);
  visitor(value.last_tick);
  visitor(value.completed_tick);
  visitor(value.contact_source_id);
  visitor(value.settlement_source_id);
  visitor(value.pending_plan_source);
  visitor(value.round_source_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBattleState &value,
                  Visitor &&visitor) {
  visitor(value.battles);
  visitor(value.contacts);
  visitor(value.plans);
  visitor(value.rounds);
  visitor(value.captures);
  visitor(value.declines);
  visitor(value.continuations);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCaptureClaim &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.captor_unit_id);
  visitor(value.captor_army_id);
  visitor(value.kind);
  visitor(value.round);
  visitor(value.survivor_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCaptureFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.round_source_id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.vessel_id);
  visitor(value.from_fleet_id);
  visitor(value.to_fleet_id);
  visitor(value.captor_unit_id);
  visitor(value.authority_source);
  visitor(value.tick);
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.anchor);
  visitor(value.before_assignments);
  visitor(value.after_assignments);
  visitor(value.from_before_assignments);
  visitor(value.from_after_assignments);
  visitor(value.interrupted_action_ids);
  visitor(value.released_vessel_ids);
  visitor(value.prisoner_ids);
  visitor(value.original_prisoner_ids);
  visitor(value.casualty_source_id);
  visitor(value.cargo);
  visitor(value.emptied_inventory_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCapturedCargo &value,
                  Visitor &&visitor) {
  visitor(value.source_inventory_id);
  visitor(value.lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.from_owner);
  visitor(value.to_owner);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalClaimDeclineFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.round_source_id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.vessel_id);
  visitor(value.captor_unit_id);
  visitor(value.army_id);
  visitor(value.formation_source);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCombatEvent &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.unit_id);
  visitor(value.target_unit_id);
  visitor(value.code);
  visitor(value.casualties);
  visitor(value.hull_damage);
  visitor(value.self_damage);
  visitor(value.hit_chance);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalContactApproach &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.action_id);
  visitor(value.plan_id);
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.start);
  visitor(value.route_index);
  visitor(value.budget);
  visitor(value.travel_credit);
  visitor(value.stationary);
  visitor(value.moving);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalContactFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.first_fleet_id);
  visitor(value.second_fleet_id);
  visitor(value.battle_id);
  visitor(value.tick);
  visitor(value.time_num);
  visitor(value.time_den);
  visitor(value.first_anchor);
  visitor(value.second_anchor);
  visitor(value.first_motion);
  visitor(value.second_motion);
  visitor(value.observed_player_id);
  visitor(value.first_approach);
  visitor(value.second_approach);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalContinueFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.round_source_id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.before);
  visitor(value.after);
  visitor(value.ai_orders);
  visitor(value.capture_source_ids);
  visitor(value.decline_source_ids);
  visitor(value.unavailable_claim_unit_ids);
  visitor(value.interrupted_action_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalFleetRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.army_id);
  visitor(value.commander_actor_id);
  visitor(value.source_id);
  visitor(value.office_id);
  visitor(value.appointment_source);
  visitor(value.formation_source);
  visitor(value.formation_berth_id);
  visitor(value.request_key);
  visitor(value.release_source_id);
  visitor(value.release_request_key);
  visitor(value.release_berth_id);
  visitor(value.release_actor_id);
  visitor(value.release_authority_kind);
  visitor(value.release_authority_id);
  visitor(value.release_authority_source);
  visitor(value.status);
  visitor(value.created_tick);
  visitor(value.released_tick);
  visitor(value.assignments);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalGunneryCaptain &value,
                  Visitor &&visitor) {
  visitor(value.unit_id);
  visitor(value.vessel_id);
  visitor(value.fleet_id);
  visitor(value.army_id);
  visitor(value.captain_actor_id);
  visitor(value.authored_actor_id);
  visitor(value.experience);
  visitor(value.level);
  visitor(value.causal_fact_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalGunneryPreparation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.contact_source_id);
  visitor(value.tick);
  visitor(value.captains);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalGunneryState &value,
                  Visitor &&visitor) {
  visitor(value.preparations);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalHullChange &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.before);
  visitor(value.after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalNavigationState &value,
                  Visitor &&visitor) {
  visitor(value.voyages);
  visitor(value.steps);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalNavigationStepFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.voyage_source_id);
  visitor(value.fleet_id);
  visitor(value.army_id);
  visitor(value.contact_source_id);
  visitor(value.tick);
  visitor(value.movement);
  visitor(value.uncut);
  visitor(value.vessels);
  visitor(value.interrupted);
  visitor(value.movement_field_order);
  visitor(value.uncut_field_order);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalNavigationVesselStep &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.previous_step_id);
  visitor(value.completed_before);
  visitor(value.completed_after);
  visitor(value.integrity_before);
  visitor(value.integrity_after);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalPlanFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.request_key);
  visitor(value.actor_id);
  visitor(value.army_id);
  visitor(value.scope);
  visitor(value.office_id);
  visitor(value.authority_source);
  visitor(value.tick);
  visitor(value.round);
  visitor(value.orders);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairChargeFact &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairChargeState &value,
                  Visitor &&visitor) {
  visitor(value.charges);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairChargeTerms &value,
                  Visitor &&visitor) {
  visitor(value.service_id);
  visitor(value.service_version);
  visitor(value.rule_id);
  visitor(value.berth_place_id);
  visitor(value.garrison_id);
  visitor(value.occupation_source_id);
  visitor(value.army_id);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.payer_owner);
  visitor(value.payee_owner);
  visitor(value.amount);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.request_key);
  visitor(value.tick);
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairResult &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.request_key);
  visitor(value.reason);
  visitor(value.interruption_source_id);
  visitor(value.tick);
  visitor(value.success);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairState &value,
                  Visitor &&visitor) {
  visitor(value.starts);
  visitor(value.results);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairTerms &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.fleet_id);
  visitor(value.army_id);
  visitor(value.vessel_id);
  visitor(value.unit_id);
  visitor(value.captain_actor_id);
  visitor(value.formation_source_id);
  visitor(value.command_source_id);
  visitor(value.rule_id);
  visitor(value.inventory_id);
  visitor(value.owner);
  visitor(value.anchor);
  visitor(value.duration);
  visitor(value.integrity_before);
  visitor(value.integrity_after);
  visitor(value.crew_ids);
  visitor(value.participants);
  visitor(value.materials);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRoundFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.battle_id);
  visitor(value.round);
  visitor(value.start_tick);
  visitor(value.tick);
  visitor(value.before);
  visitor(value.after);
  visitor(value.orders);
  visitor(value.rng_seed);
  visitor(value.rng_before);
  visitor(value.rng_after);
  visitor(value.casualty_source_id);
  visitor(value.supply_source_ids);
  visitor(value.hull_changes);
  visitor(value.events);
  visitor(value.claims);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalTacticalFrame &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_id);
  visitor(value.battlefield_id);
  visitor(value.controller_actor_id);
  visitor(value.phase);
  visitor(value.round);
  visitor(value.controller_side);
  visitor(value.units);
  visitor(value.bindings);
  visitor(value.controlled_unit_ids);
  visitor(value.plan_submitted);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalTacticalOrder &value,
                  Visitor &&visitor) {
  visitor(value.action);
  visitor(value.target_unit_id);
  visitor(value.destination);
  visitor(value.heading);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalVesselAssignment &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.unit_id);
  visitor(value.passenger_ids);
  visitor(value.prisoner_ids);
  visitor(value.extra_inventory_ids);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalVesselRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.hull_id);
  visitor(value.cargo_inventory_id);
  visitor(value.berth_id);
  visitor(value.fleet_id);
  visitor(value.origin_source);
  visitor(value.owner);
  visitor(value.integrity);
  visitor(value.afloat_location);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalVoyageFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.journey_id);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.fleet_id);
  visitor(value.army_id);
  visitor(value.formation_source_id);
  visitor(value.command_source_id);
  visitor(value.payer_wallet_id);
  visitor(value.map_id);
  visitor(value.origin);
  visitor(value.tick);
  visitor(value.action_start_tick);
  visitor(value.quoted_half_days);
  visitor(value.budget);
  visitor(value.initial_credit);
  visitor(value.vessels);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalVoyageVessel &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
  visitor(value.unit_id);
  visitor(value.captain_actor_id);
  visitor(value.owner);
  visitor(value.integrity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBattleCommandReceipt &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.request_key);
  visitor(value.receipt_id);
  visitor(value.battle_id);
  visitor(value.round_source_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCommandReceipt &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.request_key);
  visitor(value.receipt_id);
  visitor(value.fleet_id);
  visitor(value.army_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairReceipt &value,
                  Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.action_id);
  visitor(value.actor_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.completed);
  visitor(value.success);
  visitor(value.replayed);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalVesselDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.hull_id);
  visitor(value.cargo_inventory_id);
  visitor(value.berth_id);
  visitor(value.owner);
  visitor(value.integrity);
  visitor(value.name_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalTacticalNavigationPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.quarter_turn_cost);
  visitor(value.upstream_cost);
  visitor(value.currents);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalTacticalCurrent &value,
                  Visitor &&visitor) {
  visitor(value.position);
  visitor(value.heading);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairService &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.version);
  visitor(value.rule_id);
  visitor(value.berth_place_id);
  visitor(value.garrison_id);
  visitor(value.organization_id);
  visitor(value.wallet_id);
  visitor(value.fee);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairRule &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.hull_id);
  visitor(value.duration);
  visitor(value.integrity_gain);
  visitor(value.minimum_crew);
  visitor(value.afloat_integrity_limit);
  visitor(value.materials);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.rules);
  visitor(value.npc_plans);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairNpcPlan &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.rule_id);
  visitor(value.repair_below);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRepairChargePolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.services);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalRamDefinition &value,
                  Visitor &&visitor) {
  visitor(value.hull_id);
  visitor(value.target_damage);
  visitor(value.self_damage);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalNavigationPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.wear_interval);
  visitor(value.wear_damage);
  visitor(value.long_voyage_min_half_days);
  visitor(value.long_voyage_min_integrity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalHullDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.max_integrity);
  visitor(value.minimum_crew);
  visitor(value.person_capacity);
  visitor(value.cargo_capacity);
  visitor(value.minimum_naval_level);
  visitor(value.movement);
  visitor(value.travel_budget);
  visitor(value.waters);
  visitor(value.name_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalGunneryWeapon &value,
                  Visitor &&visitor) {
  visitor(value.hull_id);
  visitor(value.minimum_range);
  visitor(value.maximum_range);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalGunneryPolicy &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.weapons);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalFleetPlan &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.order_id);
  visitor(value.vessel_ids);
  visitor(value.station);
  visitor(value.departure_food);
  visitor(value.departure_money);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCommanderLossRule &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.exposure_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCombatRules &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.ram);
  visitor(value.commander_losses);
  visitor(value.tactical_navigation);
  visitor(value.gunnery);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalCatalog &value,
                  Visitor &&visitor) {
  visitor(static_cast<const NavalAssetCatalog &>(value));
  visitor(value.combat_policy);
  visitor(value.fleet_plans);
  visitor(value.repair_policy);
  visitor(value.repair_charge_policy);
  visitor(value.navigation_policy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBerthDefinition &value,
                  Visitor &&visitor) {
  visitor(value.place_id);
  visitor(value.water_kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalBattlePolicy &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.battlefield_id);
  visitor(value.max_rounds);
  visitor(value.allow_withdrawal);
  visitor(value.ai_surrender_morale);
  visitor(value.combat);
  visitor(value.ai_maneuver_policy);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NavalAssetCatalog &value,
                  Visitor &&visitor) {
  visitor(value.profile);
  visitor(value.world_map_id);
  visitor(value.hulls);
  visitor(value.berths);
  visitor(value.vessels);
}

// 原 S2 binding 地址的两部分；只遍历拥有字符串，不验证业务存在。
template <class Visitor>
inline void visit(const ::hanmo::resident::SeaEmbarkCareBindingRef &value,
                  Visitor &&visitor) {
  visitor(value.transfer_action_id);
  visitor(value.binding_id);
}

// 原实船键只有一份字符串；不保存第二船体或局部运动状态。
template <class Visitor>
inline void visit(const ::hanmo::resident::VesselRef &value,
                  Visitor &&visitor) {
  visitor(value.vessel_id);
}
