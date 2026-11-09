// 职责：集中地方经济中的财政、预算与既有义务访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ArrearRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.obligation_id);
  visitor(value.due_tick);
  visitor(value.original_due);
  visitor(value.paid_amount);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalDistrictProgress &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.registration_fact_id);
  visitor(value.next_due_tick);
  visitor(value.tax_base_bp);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalIncomeBucket &value,
                  Visitor &&visitor) {
  visitor(value.key);
  visitor(value.remainder);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalIncomeBucketKey &value,
                  Visitor &&visitor) {
  visitor(value.district_id);
  visitor(value.source_id);
  visitor(value.organization_id);
  visitor(value.public_account_id);
  visitor(value.kind);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalIncomeRemainder &value,
                  Visitor &&visitor) {
  visitor(value.coarse);
  visitor(value.fine);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalPaymentReceipt &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.assessment_id);
  visitor(value.tick);
  visitor(value.cash_paid);
  visitor(value.grain_paid);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalPeriodAssessment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.district_id);
  visitor(value.registration_fact_id);
  visitor(value.period_start_tick);
  visitor(value.due_tick);
  visitor(value.tax_base_bp);
  visitor(value.policy);
  visitor(value.source_assessment_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalSettlementState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_step_tick);
  visitor(value.districts);
  visitor(value.remainders);
  visitor(value.periods);
  visitor(value.assessments);
  visitor(value.payments);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalSourceAssessment &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.period_id);
  visitor(value.source_id);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
  visitor(value.grain_item_id);
  visitor(value.owner);
  visitor(value.produced_grain);
  visitor(value.civilian_grain_reserve);
  visitor(value.cash_remainder_before);
  visitor(value.grain_remainder_before);
  visitor(value.cash_remainder_after);
  visitor(value.grain_remainder_after);
  visitor(value.production_receipt_ids);
  visitor(value.cash);
  visitor(value.grain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalSourceRemainders &value,
                  Visitor &&visitor) {
  visitor(value.cash);
  visitor(value.grain);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalTaxpayerContinuityState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_step_tick);
  visitor(value.suspended_periods);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalTaxpayerSuspendedPeriod &value,
                  Visitor &&visitor) {
  visitor(value.period_id);
  visitor(value.district_id);
  visitor(value.policy_source_fact_id);
  visitor(value.due_tick);
  visitor(value.blocked_source_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::MembershipTerminalFractionReceipt &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.wage_binding_id);
  visitor(value.member_episode_id);
  visitor(value.obligation_id);
  visitor(value.departure_fact_id);
  visitor(value.arrear_id);
  visitor(value.tick);
  visitor(value.original_numerator);
  visitor(value.denominator);
  visitor(value.compensation_numerator);
  visitor(value.integer_due);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MembershipWageBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.member_episode_id);
  visitor(value.obligation_id);
  visitor(value.offer_id);
  visitor(value.decision_id);
  visitor(value.terms_id);
  visitor(value.organization_id);
  visitor(value.actor_id);
  visitor(value.payer_wallet_id);
  visitor(value.payee_wallet_id);
  visitor(value.grant_id);
  visitor(value.office_id);
  visitor(value.issuer_actor_id);
  visitor(value.authority_epoch);
  visitor(value.joined_tick);
  visitor(value.first_due_tick);
  visitor(value.credited_periods);
  visitor(value.last_credited_due_tick);
  visitor(value.frozen_numerator);
  visitor(value.frozen_denominator);
  visitor(value.period_ticks);
  visitor(value.amount_limit);
  visitor(value.terminal_fraction_policy_id);
  visitor(value.ended_tick);
  visitor(value.terminal_fact_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationObservation &value,
                  Visitor &&visitor) {
  visitor(value.obligation_id);
  visitor(value.principal_id);
  visitor(value.beneficiary_id);
  visitor(value.source_fact_id);
  visitor(value.due_tick);
  visitor(value.due_amount);
  visitor(value.paid_amount);
  visitor(value.outstanding_amount);
  visitor(value.arrear_id);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalLegacyCarryClaim &value,
      Visitor &&visitor) {
  visitor(value.key);
  visitor(value.legacy_period_id);
  visitor(value.legacy_assessment_id);
  visitor(value.legacy_policy_fact_id);
  visitor(value.claim_period_id);
  visitor(value.remainder);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalPeriodAssessment &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.legacy_period_id);
  visitor(value.district_id);
  visitor(value.period_start_tick);
  visitor(value.due_tick);
  visitor(value.tax_base_bp);
  visitor(value.policy);
  visitor(value.registration_fact_id);
  visitor(value.occupation_fact_id);
  visitor(value.occupation_rule);
  visitor(value.occupation_tick);
  visitor(value.integration_until_tick);
  visitor(value.income_basis_points);
  visitor(value.source_assessment_ids);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalPeriodSuspension &value,
      Visitor &&visitor) {
  visitor(value.period);
  visitor(value.sources);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalSettlementState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.initialized_tick);
  visitor(value.last_step_tick);
  visitor(value.buckets);
  visitor(value.legacy_carry_claims);
  visitor(value.periods);
  visitor(value.suspended_periods);
  visitor(value.assessments);
  visitor(value.payments);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalSourceAssessment &value,
      Visitor &&visitor) {
  visitor(value.assessment);
  visitor(value.cash_fine_before);
  visitor(value.grain_fine_before);
  visitor(value.cash_fine_after);
  visitor(value.grain_fine_after);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OccupationFiscalTaxSourcePause &value,
      Visitor &&visitor) {
  visitor(value.source_id);
  visitor(value.original_taxpayer);
  visitor(value.wallet);
  visitor(value.inventory);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RecurringObligation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.principal);
  visitor(value.beneficiary);
  visitor(value.kind);
  visitor(value.source_wallet_or_inventory);
  visitor(value.destination);
  visitor(value.rate_profile);
  visitor(value.authority_ref);
  visitor(value.item_id);
  visitor(value.period_ticks);
  visitor(value.next_due_tick);
  visitor(value.last_receipt_id);
  visitor(value.active);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationRate &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.numerator);
  visitor(value.denominator);
  visitor(value.per_capita);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalTaxSource &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.taxpayer);
  visitor(value.wallet_id);
  visitor(value.inventory_id);
  visitor(value.grain_item_id);
  visitor(value.production_unit_ids);
  visitor(value.base_cash_due);
  visitor(value.grain_share_bp);
  visitor(value.civilian_grain_reserve);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalSettlementCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.districts);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalNeedPolicy &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.district_id);
  visitor(value.source_id);
  visitor(value.semantic_key);
  visitor(value.unit);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalNeedCatalog &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.policies);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::FiscalDistrictProfile &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.site_id);
  visitor(value.period_ticks);
  visitor(value.first_due_tick);
  visitor(value.initial_tax_base_bp);
  visitor(value.normal_recovery_bp);
  visitor(value.sources);
}

// native/world/src/resident/domains/local_economy/fiscal/budget_execution_types.hpp:5;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetExecutionRef &value,
                  Visitor &&visitor) {
  visitor(value.budget_id);
  visitor(value.budget_revision);
  visitor(value.policy_revision);
  visitor(value.approval_fact_id);
}

// native/world/src/resident/domains/local_economy/fiscal/budget_execution_types.hpp:7;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetItemQuantity &value,
                  Visitor &&visitor) {
  visitor(value.item_id);
}

// native/world/src/resident/domains/local_economy/fiscal/budget_execution_types.hpp:6;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetMoneyQian &value,
                  Visitor &&visitor) {}

// native/world/src/resident/domains/local_economy/fiscal/budget_execution_types.hpp:9;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetPeriod &value,
                  Visitor &&visitor) {
  visitor(value.period_from_tick);
  visitor(value.period_until_tick);
}

// native/world/src/resident/domains/local_economy/material_aid/material_aid_model.hpp:28;
// actual owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::MaterialAidObligation &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.schedule_commitment_id);
  visitor(value.occurrence);
  visitor(value.established_at);
  visitor(value.due_tick);
  visitor(value.delivery_deadline_tick);
  visitor(value.beneficiary);
  visitor(value.delivery_site);
  visitor(value.frozen_items);
  visitor(value.frozen_money);
  visitor(value.resource_acceptance_ref);
  visitor(value.item_routes);
  visitor(value.money_route);
  visitor(value.original_money_obligation_id);
  visitor(value.original_money_arrear_id);
  visitor(value.issue_result_refs);
  visitor(value.receipt_result_refs);
  visitor(value.deadline_result_ref);
  visitor(value.state);
}

// native/world/src/resident/domains/local_economy/obligation_ref.hpp:5; actual
// owning declaration, no codec validation.
template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationRef &value,
                  Visitor &&visitor) {
  visitor(value.owner_domain);
  visitor(value.obligation_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.execution_ref);
  visitor(value.responsibility_ref);
  visitor(value.responsibility_source_schema);
  visitor(value.purpose);
  visitor(value.unit);
  visitor(value.period);
  visitor(value.payer_container_id);
  visitor(value.accepted_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetExecutionState &value,
                  Visitor &&visitor) {
  visitor(value.bindings);
  visitor(value.schema_version);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalRecurringObligationSource &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalRecurringObligationSource &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalServiceTerminationPayable &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalServiceTerminationPayable &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalOrderedEarnedCharge &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalOrderedEarnedCharge &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalShopOrderRefund &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalShopOrderRefund &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalShopCancellationFee &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalShopCancellationFee &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalCrewShareAccrual &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalCrewShareAccrual &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalCrewShareCorrection &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalCrewShareCorrection &>(value));
}

// 原赔偿应付款复用已存在的唯一来源字段算法。
template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalMaterialAidCompensationPayable &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalMaterialAidCompensationPayable &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalMaterialAidPayable &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalMaterialAidPayable &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::OriginalTransportExpensePayable &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalTransportExpensePayable &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalRepairEarnedCharge &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalRepairEarnedCharge &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalInvestmentPrincipal &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalInvestmentPrincipal &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalJointFundingReturn &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalJointFundingReturn &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalJointDistribution &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalJointDistribution &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicObligationSourceBinding &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<EconomicObligationSourceBinding &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::EconomicPayableSourceBinding &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<EconomicPayableSourceBinding &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicPayableAdjustmentBinding &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<EconomicPayableAdjustmentBinding &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicRecoveryObligationBinding &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<EconomicRecoveryObligationBinding &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ObligationResponsibilityEpochRef &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ObligationResponsibilityEpochRef &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalResponsibilitySource &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<OriginalResponsibilitySource &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ConsentedObligationNovation &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ConsentedObligationNovation &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ObligationExecutionWalletChange &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ObligationExecutionWalletChange &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationReceiverContinuity &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ObligationReceiverContinuity &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::ObligationOriginalPoliticalTakeover &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ObligationOriginalPoliticalTakeover &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ObligationResponsibilityEpoch &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<ObligationResponsibilityEpoch &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PaymentResponsibilityBinding &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<PaymentResponsibilityBinding &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::LegacyResponsibilityPrefix &value,
                  Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<LegacyResponsibilityPrefix &>(value));
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::EconomicObligationSourcesState &value,
      Visitor &&visitor) {
  RepairOwnedFieldCallback<Visitor> fields{visitor};
  obligation_source_owned_fields::fields(
      fields, const_cast<EconomicObligationSourcesState &>(value));
}

template <class Visitor>
inline void visit(const ::hanmo::resident::InvestmentContractRef &value,
                  Visitor &&visitor) {
  visitor(value.contract_id);
  visitor(value.terms_revision);
}
