// 职责：集中庄园经营中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateAppoint &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.manager_actor_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateAssign &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.resident_actor_id);
  visitor(value.duty);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateBuild &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.facility);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateBuildAction &value,
                  Visitor &&visitor) {
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateBuildTerms &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.facility);
  visitor(value.from_level);
  visitor(value.to_level);
  visitor(value.duration);
  visitor(value.money_cost);
  visitor(value.material_item_id);
  visitor(value.material_quantity);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.inventory_id);
  visitor(value.owner_actor_id);
  visitor(value.place_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateCashTerms &value,
                  Visitor &&visitor) {
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateConstructionFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.phase);
  visitor(value.actor_id);
  visitor(value.start_tick);
  visitor(value.completed_tick);
  visitor(value.success);
  visitor(value.terms);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateDetachment &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.actor_ids);
  visitor(value.source_id);
  visitor(value.returning);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentContract &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.status);
  visitor(value.acceptance_fact_id);
  visitor(value.terminal_fact_id);
  visitor(value.obligation_id);
  visitor(value.terms);
  visitor(value.offered_at);
  visitor(value.offered_tick);
  visitor(value.accepted_tick);
  visitor(value.through_tick);
  visitor(value.offer_from_provider);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentDecisionFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.meeting);
  visitor(value.terminal_arrear_id);
  visitor(value.provider_declared);
  visitor(value.death_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentMeeting &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.node_kind);
  visitor(value.place_id);
  visitor(value.speaker_actor_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentService &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.production_fact_id);
  visitor(value.arrear_id);
  visitor(value.tick);
  visitor(value.remainder_before);
  visitor(value.remainder_after);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_step_tick);
  visitor(value.contracts);
  visitor(value.decisions);
  visitor(value.services);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentTerms &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.estate_id);
  visitor(value.employer_actor_id);
  visitor(value.worker_actor_id);
  visitor(value.job);
  visitor(value.rate_profile);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.wage_numerator);
  visitor(value.wage_denominator);
  visitor(value.service_ticks);
  visitor(value.offer_expires_tick);
  visitor(value.reservation_cap);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateFacilities &value,
                  Visitor &&visitor) {
  visitor(value.farm);
  visitor(value.granary);
  visitor(value.palisade);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateFund &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateManagementFact &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.estate_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.command);
  visitor(value.cash);
  visitor(value.transfer);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateMusterSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.estate_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateProductionFact &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.tick);
  visitor(value.owner_actor_id);
  visitor(value.manager_actor_id);
  visitor(value.workers);
  visitor(value.guards);
  visitor(value.fed);
  visitor(value.unfed);
  visitor(value.farm_level);
  visitor(value.palisade_level);
  visitor(value.consumed);
  visitor(value.produced);
  visitor(value.lost_output);
  visitor(value.defense);
  visitor(value.reasons);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.owner_actor_id);
  visitor(value.manager_actor_id);
  visitor(value.resident_actor_ids);
  visitor(value.duties);
  visitor(value.facilities);
  visitor(value.detached);
  visitor(value.last_tick);
  visitor(value.last_production_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateRestAdoptionBasis &value,
                  Visitor &&visitor) {
  visitor(value.source_schema);
  visitor(value.estate_id);
  visitor(value.initial_origin);
  visitor(value.adopted_at);
  visitor(value.common_fact_count);
  visitor(value.last_common_fact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateState &value,
                  Visitor &&visitor) {
  visitor(value.estates);
  visitor(value.facts);
  visitor(value.actions);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateTransfer &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.source_id);
  visitor(value.deposit);
  visitor(value.source_id_present);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateTransferTerms &value,
                  Visitor &&visitor) {
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.owners);
  visitor(value.new_owner);
  visitor(value.source_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ItemQuantity &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicEstateTransferSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.estate_id);
  visitor(value.actor_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.source_owner);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateReceipt &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.fact_id);
  visitor(value.actor_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.estate_id);
  visitor(value.job);
  visitor(value.rate_profile);
  visitor(value.offer_dialogue_id);
  visitor(value.offer_node_id);
  visitor(value.worker_dialogue_id);
  visitor(value.worker_node_id);
  visitor(value.service_ticks);
  visitor(value.offer_valid_ticks);
  visitor(value.provider_offer_dialogue_id);
  visitor(value.provider_offer_node_id);
  visitor(value.provider_reply_dialogue_id);
  visitor(value.provider_reply_node_id);
  visitor(value.provider_reply_decision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateEmploymentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateDefinition &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.food_item_id);
  visitor(value.inventory_id);
  visitor(value.manager_actor_id);
  visitor(value.output_item_id);
  visitor(value.owner_actor_id);
  visitor(value.place_id);
  visitor(value.wallet_id);
  visitor(value.granary_capacity_per_level);
  visitor(value.output_per_worker);
  visitor(value.palisade_defense_per_guard);
  visitor(value.ration_per_person);
  visitor(value.base_capacity);
  visitor(value.resident_actor_ids);
  visitor(value.initial_facilities);
  visitor(value.initial_duties);
  visitor(value.construction);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateConstruction &value,
                  Visitor &&visitor) {
  visitor(value.build_ticks);
  visitor(value.material_cost);
  visitor(value.max_level);
  visitor(value.money_cost);
  visitor(value.material_item_id);
  visitor(value.payee_wallet_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EstateCatalog &value,
                  Visitor &&visitor) {
  visitor(value.estates);
}

template <class Visitor>
inline void visit(const OriginalEstateDetachmentReleaseView &value,
                  Visitor &&visitor) {
  visit_military_return_detachment_release_payload(value, visitor);
}
