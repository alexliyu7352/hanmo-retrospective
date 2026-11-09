// 职责：集中商业经营中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdRetailSettlement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms);
  visitor(value.settled_at);
  visitor(value.market_version_after);
  visitor(value.cost_of_goods);
  visitor(value.lots);
  visitor(value.consumed_lots);
  visitor(value.payer_balance_before);
  visitor(value.payer_balance_after);
  visitor(value.payee_balance_before);
  visitor(value.payee_balance_after);
  visitor(value.payment_ref);
  visitor(value.transfer_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdRetailState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.settlements);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdRetailTerms &value,
                  Visitor &&visitor) {
  visitor(value.purchase_id);
  visitor(value.household_group_id);
  visitor(value.executor);
  visitor(value.seller_ref);
  visitor(value.item_id);
  visitor(value.buyer_wallet_id);
  visitor(value.seller_wallet_id);
  visitor(value.source_inventory_id);
  visitor(value.custody_inventory_id);
  visitor(value.destination_inventory_id);
  visitor(value.place_id);
  visitor(value.buyer);
  visitor(value.seller);
  visitor(value.quantity);
  visitor(value.total);
  visitor(value.market_revision);
  visitor(value.market_stock);
  visitor(value.base_price);
  visitor(value.midpoint);
  visitor(value.unit_price);
  visitor(value.quoted_at);
  visitor(value.selected_cargo);
  visitor(value.accepted_terms_ref);
  visitor(value.custody_terms_ref);
  visitor(value.terms_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopCapitalFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.tick);
  visitor(value.amount);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopClientQualification &value,
      Visitor &&visitor) {
  visitor(value.original_client);
  visitor(value.delivery_beneficiary);
  visitor(value.task_id);
  visitor(value.settlement_id);
  visitor(value.template_id);
  visitor(value.payment_fact_id);
  visitor(value.completed_tick);
  visitor(value.objective_fact_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopClosingLot &value,
                  Visitor &&visitor) {
  visitor(value.original_lot_id);
  visitor(value.destination_lot_id);
  visitor(value.item_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopClosingRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.from_wallet_id);
  visitor(value.to_wallet_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.tick);
  visitor(value.returned_cash);
  visitor(value.returned_lots);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopCommandResultBinding &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.source_kind);
  visitor(value.submitter_actor_id);
  visitor(value.result_id);
  visitor(value.result_kind);
  visitor(value.source_tick);
  visitor(value.from_tick);
  visitor(value.result_tick);
  visitor(value.allocation_begin);
  visitor(value.request_seq);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopCommandResultsState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.bindings);
  visitor(value.control_bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopDailyRentState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.last_step_tick);
  visitor(value.assessments);
  visitor(value.payments);
  visitor(value.resumptions);
  visitor(value.player_results);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopDayResult &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.operation_id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.item_id);
  visitor(value.reason);
  visitor(value.tick);
  visitor(value.market_observed_tick);
  visitor(value.market_revision);
  visitor(value.market_stock);
  visitor(value.base_price);
  visitor(value.midpoint);
  visitor(value.quantity);
  visitor(value.income);
  visitor(value.cost_of_goods);
  visitor(value.rent_paid);
  visitor(value.wage_paid);
  visitor(value.net_income);
  visitor(value.completed);
  visitor(value.consumed_lots);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopEstateCancellationRequest &value,
      Visitor &&visitor) {
  visitor(value.shop_id);
  visitor(value.order_id);
  visitor(value.original_proprietor_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.mandate_fact_id);
  visitor(value.death_notice_binding_fact_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopEstateClosingRecord &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.closing_fact_id);
  visitor(value.shop_id);
  visitor(value.proprietor_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.mandate_fact_id);
  visitor(value.acceptance_fact_id);
  visitor(value.death_fact_id);
  visitor(value.death_cause_fact_id);
  visitor(value.death_notice_binding_fact_id);
  visitor(value.death_notice_handoff_fact_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopExecutorAcceptance &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.mandate_fact_id);
  visitor(value.executor_actor_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopExecutorMandate &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.shop_id);
  visitor(value.proprietor_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.shop_wallet_id);
  visitor(value.shop_inventory_id);
  visitor(value.return_wallet_id);
  visitor(value.return_inventory_id);
  visitor(value.acceptance_fact_id);
  visitor(value.appointed_tick);
  visitor(value.requested_by_executor);
  visitor(value.agreement);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopExecutorRevocation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.mandate_fact_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopLiquidationResultBinding &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.command_kind);
  visitor(value.result_id);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopLiquidationState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.mandates);
  visitor(value.acceptances);
  visitor(value.revocations);
  visitor(value.closings);
  visitor(value.result_bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopMarkupFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.tick);
  visitor(value.before_basis_points);
  visitor(value.after_basis_points);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopMeetingSource &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.provider_actor_id);
  visitor(value.session_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.place_id);
  visitor(value.role_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOpeningTerms &value,
                  Visitor &&visitor) {
  visitor(value.premises_id);
  visitor(value.place_id);
  visitor(value.proprietor_actor_id);
  visitor(value.lessor_actor_id);
  visitor(value.payer_wallet_id);
  visitor(value.lessor_wallet_id);
  visitor(value.market_shop_id);
  visitor(value.opening_fee);
  visitor(value.initial_capital);
  visitor(value.inventory_capacity);
  visitor(value.daily_rent);
  visitor(value.rent_rate_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOperation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.item_id);
  visitor(value.rent_arrear_id);
  visitor(value.rent_payment_id);
  visitor(value.result_fact_id);
  visitor(value.staff_contract_id);
  visitor(value.start_tick);
  visitor(value.markup_basis_points);
  visitor(value.attendance_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.order_id);
  visitor(value.task_id);
  visitor(value.acceptance_fact_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderLotTransfer &value,
                  Visitor &&visitor) {
  visitor(value.original_lot_id);
  visitor(value.destination_lot_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.policy_id);
  visitor(value.status);
  visitor(value.task_id);
  visitor(value.acceptance_fact_id);
  visitor(value.binding_fact_id);
  visitor(value.terms);
  visitor(value.client_declaration);
  visitor(value.issued_tick);
  visitor(value.valid_through_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderSettlement &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.order_id);
  visitor(value.task_id);
  visitor(value.task_settlement_id);
  visitor(value.receipt_fact_id);
  visitor(value.outcome);
  visitor(value.tick);
  visitor(value.income);
  visitor(value.cost_of_goods);
  visitor(value.delivered_lots);
  visitor(value.consumed_lots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderState &value,
                  Visitor &&visitor) {
  visitor(value.orders);
  visitor(value.bindings);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderTerms &value,
                  Visitor &&visitor) {
  visitor(value.order_id);
  visitor(value.shop_id);
  visitor(value.proprietor_actor_id);
  visitor(value.original_client);
  visitor(value.task_template_id);
  visitor(value.item_id);
  visitor(value.stock_inventory_id);
  visitor(value.shop_wallet_id);
  visitor(value.client_inventory_id);
  visitor(value.client_wallet_id);
  visitor(value.quantity);
  visitor(value.contract_price);
  visitor(value.accepted_tick);
  visitor(value.deadline_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopPermissionRequest &value,
                  Visitor &&visitor) {
  visitor(value.actor_id);
  visitor(value.premises_id);
  visitor(value.place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopPurchaseFact &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.trade_fact_id);
  visitor(value.item_id);
  visitor(value.market_shop_id);
  visitor(value.from_inventory_id);
  visitor(value.to_inventory_id);
  visitor(value.tick);
  visitor(value.quantity);
  visitor(value.purchase_cash);
  visitor(value.purchased_lots);
  visitor(value.stocked_lots);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopQualification &value,
                  Visitor &&visitor) {
  visitor(value.minimum_commerce_xp);
  visitor(value.minimum_commerce_level);
  visitor(value.commerce_study_source_ids);
  visitor(value.original_clients);
  visitor(value.permit_fact_id);
  visitor(value.clearance_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.status);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
  visitor(value.rent_obligation_id);
  visitor(value.terms);
  visitor(value.meeting);
  visitor(value.qualification);
  visitor(value.opened_tick);
  visitor(value.markup_basis_points);
  visitor(value.opened_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRentAssessment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.arrear_id);
  visitor(value.trigger);
  visitor(value.resume_fact_id);
  visitor(value.automatic_payment_fact_id);
  visitor(value.tick);
  visitor(value.period_index);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRentPayment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.assessment_fact_id);
  visitor(value.local_payment_id);
  visitor(value.funding_kind);
  visitor(value.actor_id);
  visitor(value.payer_wallet_id);
  visitor(value.mandate_fact_id);
  visitor(value.death_notice_binding_fact_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRentPlayerResult &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.command_kind);
  visitor(value.result_id);
  visitor(value.request_seq);
  visitor(value.revision);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRentResumption &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.shop_id);
  visitor(value.actor_id);
  visitor(value.assessment_fact_id);
  visitor(value.tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopRetailLot &value,
                  Visitor &&visitor) {
  visitor(value.purchase_fact_id);
  visitor(value.lot_id);
  visitor(value.quantity);
  visitor(value.cost);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffContract &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.status);
  visitor(value.terminal_fact_id);
  visitor(value.obligation_id);
  visitor(value.terms);
  visitor(value.agreement);
  visitor(value.accepted_tick);
  visitor(value.through_tick);
  visitor(value.accepted_by_worker);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffService &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.operation_id);
  visitor(value.arrear_id);
  visitor(value.payment_id);
  visitor(value.tick);
  visitor(value.attendance_index);
  visitor(value.amount);
  visitor(value.remainder_before);
  visitor(value.remainder_after);
  visitor(value.closing_fraction);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffState &value,
                  Visitor &&visitor) {
  visitor(value.contracts);
  visitor(value.decisions);
  visitor(value.services);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffTermination &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.contract_id);
  visitor(value.actor_id);
  visitor(value.decision);
  visitor(value.tick);
  visitor(value.cause_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffTerms &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.shop_id);
  visitor(value.employer_actor_id);
  visitor(value.worker_actor_id);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.rate_profile);
  visitor(value.wage_numerator);
  visitor(value.wage_denominator);
  visitor(value.service_ticks);
  visitor(value.service_item_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_step_tick);
  visitor(value.staff);
  visitor(value.orders);
  visitor(value.shops);
  visitor(value.closings);
  visitor(value.order_settlements);
  visitor(value.operations);
  visitor(value.operating_results);
  visitor(value.capital);
  visitor(value.markups);
  visitor(value.purchases);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStockLot &value,
                  Visitor &&visitor) {
  visitor(value.lot_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateTransportFundingState &value,
                  Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.terms);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceAcceptedResult &value,
                  Visitor &&visitor) {
  visitor(value.commercial_contract);
  visitor(value.repair_acceptance);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceCommandReceipt &value,
                  Visitor &&visitor) {
  visitor(value.command_kind);
  visitor(value.actor_id);
  visitor(value.actual_source);
  visitor(value.occurred_at);
  visitor(value.accepted_result);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SelfTransportFundingTerms &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.principal_actor_id);
  visitor(value.wallet_id);
  visitor(value.journey_or_plan_ref);
  visitor(value.currency_id);
  visitor(value.gross_cap);
  visitor(value.valid_from);
  visitor(value.until);
  visitor(value.allowed_destination_scope);
  visitor(value.continuity);
  visitor(value.acceptance_ref);
  visitor(value.revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::VerifiedTradeLot &value,
                  Visitor &&visitor) {
  visitor(value.original_lot_id);
  visitor(value.destination_lot_id);
  visitor(value.quantity);
}

template <class Visitor>
inline void visit(const RepairItemProposalBuildView &value, Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_item_proposal_fields::fields(fields, value);
}

template <class Visitor>
inline void visit_payload(const RepairCustodyReceivedSource &value,
                          Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairCustodyReceivedSource &>(value));
}

template <class Visitor>
inline void visit_payload(const RepairItemReturnedSource &value,
                          Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairItemReturnedSource &>(value));
}

template <class Visitor>
inline void visit_payload(const RepairCustodyReceivedBuildView &value,
                          Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields, value);
}

template <class Visitor>
inline void visit_payload(const RepairItemReturnedBuildView &value,
                          Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields, value);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::HouseholdRetailCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.premises_ids);
  visitor(value.public_market_shop_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.premises_id);
  visitor(value.worker_actor_id);
  visitor(value.wage_rate_profile);
  visitor(value.employer_dialogue_id);
  visitor(value.employer_offer_node_id);
  visitor(value.worker_dialogue_id);
  visitor(value.worker_accept_node_id);
  visitor(value.service_ticks);
  visitor(value.service_item_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopStaffCatalog &value,
                  Visitor &&visitor) {
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopPremises &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.place_id);
  visitor(value.lessor_actor_id);
  visitor(value.lessor_wallet_id);
  visitor(value.lessor_role_id);
  visitor(value.agreement_dialogue_id);
  visitor(value.agreement_node_id);
  visitor(value.market_shop_id);
  visitor(value.opening_fee);
  visitor(value.initial_capital);
  visitor(value.inventory_capacity);
  visitor(value.daily_rent);
  visitor(value.rent_rate_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.premises_id);
  visitor(value.client_actor_id);
  visitor(value.client_role_id);
  visitor(value.dialogue_id);
  visitor(value.node_id);
  visitor(value.task_template_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.contract_price);
  visitor(value.accept_ticks);
  visitor(value.deadline_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOrderCatalog &value,
                  Visitor &&visitor) {
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopLiquidationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopExecutorPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.premises_id);
  visitor(value.proprietor_actor_id);
  visitor(value.executor_actor_id);
  visitor(value.proprietor_role_id);
  visitor(value.executor_role_id);
  visitor(value.proprietor_dialogue_id);
  visitor(value.proprietor_appointment_node_id);
  visitor(value.executor_dialogue_id);
  visitor(value.executor_acceptance_node_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopDailyRentCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.profile);
  visitor(value.arrears_policy);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopCommandResultsCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.premises);
  visitor(value.staff);
  visitor(value.orders);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceFeePolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceFeePolicy &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceCustodyPolicy &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceCustodyPolicy &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceRuleCatalog &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceRuleCatalog &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceFeeTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceFeeTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceReturnValues &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceReturnValues &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceReturnTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceReturnTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceCustodyTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceCustodyTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceContractTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceContractTerms &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::RepairServiceAcceptanceInspection &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceAcceptanceInspection &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceOfferSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceOfferSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceAssentSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceAssentSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceContractSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceContractSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceChargeSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceChargeSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServicePaymentSource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServicePaymentSource &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::RepairServiceTerminationSettlementSource &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceTerminationSettlementSource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceContractsState &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(
      fields, const_cast<RepairServiceContractsState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceQuotedTerms &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceQuotedTerms &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RepairServiceOfferValues &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  repair_service_fields::fields(fields,
                                const_cast<RepairServiceOfferValues &>(value));
}

// native/world/src/resident/domains/commerce/private_shop_capability_model.hpp;
// original owning fields, no semantic admission.
template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopActualLaborSourceRef &value,
      Visitor &&visitor) {
  visitor(value.source_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopOperationAcceptance &value,
      Visitor &&visitor) {
  visitor(value.operation_ref);
  visitor(value.original_shop_epoch_ref);
  visitor(value.market_definition_ref);
  visitor(value.accepted_at);
  visitor(value.committed_service_window);
  visitor(value.actual_presence_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopLaborAppliedSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.operation_ref);
  visitor(value.work_period_ref);
  visitor(value.occurred_at);
  visitor(value.original_staff_service_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopOperationTerminalSource &value,
      Visitor &&visitor) {
  visitor(value.operation_ref);
  visitor(value.original_result_ref);
  visitor(value.occurred_at);
  visitor(value.actual_cause_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopOperationSourcesState &value,
      Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.accepted_operations);
  visitor(value.labor_applied_sources);
  visitor(value.terminal_sources);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopOperationProfile &value,
                  Visitor &&visitor) {
  visitor(value.operation_ref);
  visitor(value.operation_profile);
  visitor(value.migration_source);
  visitor(value.accepted_purpose_ref);
  visitor(value.acceptance_condition_source_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PrivateShopServiceApplied &value,
                  Visitor &&visitor) {
  visitor(value.work_period_ref);
  visitor(value.original_labor_receipt_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopParticipationStopped &value,
      Visitor &&visitor) {
  visitor(value.original_operation_pause_or_ineligibility_result_ref);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ShopOperationCapabilityBinding &value,
      Visitor &&visitor) {
  visitor(value.operation_ref);
  visitor(value.segment_source_ref);
  visitor(value.actor_ref);
  visitor(value.role);
  visitor(value.staff_contract_ref);
  visitor(value.purpose_ref);
  visitor(value.actual_span);
  visitor(value.condition_source_refs);
  visitor(value.capability_rule_id);
  visitor(value.capability_rule_version);
  visitor(value.allowed);
  visitor(value.efficiency_bp);
  visitor(value.activity_evidence);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopOperationCapabilitiesState &value,
      Visitor &&visitor) {
  visitor(value.schema_version);
  visitor(value.operation_profiles);
  visitor(value.period_capability_bindings);
}
