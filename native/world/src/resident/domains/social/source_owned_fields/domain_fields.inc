// 职责：集中社会关系中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::ActorKnownDeathReceiptRef &value,
                  Visitor &&visitor) {
  visitor(value.report_id);
  visitor(value.death);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::DynamicSocialState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.last_collected_tick);
  visitor(value.reports);
  visitor(value.pending_messages);
  visitor(value.known_reports);
  visitor(value.time2);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownReport &value,
                  Visitor &&visitor) {
  visitor(value.receiver);
  visitor(value.report_id);
  visitor(value.source_message_id);
  visitor(value.learned_tick);
  visitor(value.source_subject);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::KnownSocialPolicyBasis &value,
                  Visitor &&visitor) {
  visitor(value.policy_source);
  visitor(value.kind);
  visitor(value.share_news);
  visitor(value.interval_ticks);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MessageCreatedKey &value,
                  Visitor &&visitor) {
  visitor(value.message_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::MessageDeliveredKey &value,
                  Visitor &&visitor) {
  visitor(value.message_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::NoRecordedRelationBasis &value,
                  Visitor &&visitor) {
  visitor(value.from_actor_id);
  visitor(value.to_actor_id);
  visitor(value.original_zero_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PendingMessage &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.report_id);
  visitor(value.sender);
  visitor(value.receiver);
  visitor(value.channel);
  visitor(value.created_tick);
  visitor(value.due_tick);
  visitor(value.status);
  visitor(value.local_place_id);
  visitor(value.address_binding);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RecordedRelationBasis &value,
                  Visitor &&visitor) {
  visitor(value.from_actor_id);
  visitor(value.to_actor_id);
  visitor(value.record_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RelationProjection &value,
                  Visitor &&visitor) {
  visitor(value.from_actor_id);
  visitor(value.to_actor_id);
  visitor(value.basis);
  visitor(value.effective_trust);
  visitor(value.effective_affinity);
  visitor(value.current_contribution_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::RelationRecord &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.from_actor);
  visitor(value.to_actor);
  visitor(value.trust);
  visitor(value.affinity);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::Report &value, Visitor &&visitor) {
  visitor(value.id);
  visitor(value.fact_id);
  visitor(value.payload_kind);
  visitor(value.origin_subject);
  visitor(value.observation_tick);
  visitor(value.visibility_scope);
  visitor(value.valid_until_tick);
  visitor(value.reliability_kind);
  visitor(value.observation);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::ReportObservationKey &value,
                  Visitor &&visitor) {
  visitor(value.report_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialCollectionCursor &value,
                  Visitor &&visitor) {
  visitor(value.last_scanned_fact_id);
  visitor(value.through_moment);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SocialEvaluationContributionRef &value,
      Visitor &&visitor) {
  visitor(value.evaluation_id);
  visitor(value.revision);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SocialEventOriginalApplication &value,
      Visitor &&visitor) {
  visitor(value.original_period_opening);
  visitor(value.occurred_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialEvent &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.kind);
  visitor(value.tick);
  visitor(value.place_id);
  visitor(value.actor_id);
  visitor(value.target_actor_id);
  visitor(value.item_id);
  visitor(value.quantity);
  visitor(value.reason);
  visitor(value.fact_id);
  visitor(value.field_order);
  visitor(value.original_application);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialExactMoment &value,
                  Visitor &&visitor) {
  visitor(value.moment);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialExactSource &value,
                  Visitor &&visitor) {
  visitor(value.original_timed_source_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialKnowledgeKey &value,
                  Visitor &&visitor) {
  visitor(value.receiver);
  visitor(value.report_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialLegacyBoundary &value,
                  Visitor &&visitor) {
  visitor(value.tick);
  visitor(value.original_catalog_identity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialLegacyChannel &value,
                  Visitor &&visitor) {
  visitor(value.original_profile);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialMemory &value,
                  Visitor &&visitor) {
  visitor(value.learned_tick);
  visitor(value.source_actor_id);
  visitor(value.field_order);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialMessageExecutionBinding &value,
                  Visitor &&visitor) {
  visitor(value.message_id);
  visitor(value.source_binding);
  visitor(value.actual_delivery_ref);
  visitor(value.actual_dossier_access_ref);
  visitor(value.actual_knowledge_keys);
  visitor(value.channel_rule_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPeriodClosure &value,
                  Visitor &&visitor) {
  visitor(value.period_id);
  visitor(value.closed_at);
  visitor(value.delivered_message_ids);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPeriodOpening &value,
                  Visitor &&visitor) {
  visitor(value.period_id);
  visitor(value.opened_at);
  visitor(value.through_tick);
  visitor(value.due_message_ids);
  visitor(value.catalog_identity);
  visitor(value.provider_rule_id);
  visitor(value.provider_rule_version);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPolicySource &value,
                  Visitor &&visitor) {
  visitor(value.catalog_identity);
  visitor(value.lookup);
  visitor(value.policy_actor_key);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPropagationCooldown &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.report_id);
  visitor(value.recipient);
  visitor(value.policy_ref);
  visitor(value.last_actual_dispatch_tick);
  visitor(value.next_eligible_tick);
  visitor(value.source_decision_ref);
}

template <class Visitor>
inline void
visit_payload(const ::hanmo::resident::SocialPropagationAdoption &value,
              Visitor &&visitor) {
  visitor(value.adopted_at);
  visitor(value.original_prefix);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPropagationDecision &value,
                  Visitor &&visitor) {
  visitor(value.subject);
  visitor(value.registration_epoch);
  visitor(value.decision_seq);
  visitor(value.report_id);
  visitor(value.policy_ref);
  visitor(value.source_known_key);
  visitor(value.source_relation_basis);
  visitor(value.disposition_policy_basis);
  visitor(value.evaluated_at);
  visitor(value.considered_recipients);
  visitor(value.selected_recipients);
  visitor(value.outcome);
  visitor(value.actual_message_refs);
  visitor(value.next_review_tick);
  visitor(value.causal_source_refs);
  visitor(value.actual_adoption);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialS3AddressBinding &value,
                  Visitor &&visitor) {
  visitor(value.original_business_fact_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialS3BusinessLink &value,
                  Visitor &&visitor) {
  visitor(value.original_business_fact_ref);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialSourceTimeBinding &value,
                  Visitor &&visitor) {
  visitor(value.key);
  visitor(value.time_source);
  visitor(value.actual_source_ref);
  visitor(value.learned_after_common_fact);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialExchangeRelationCause &value,
                  Visitor &&visitor) {
  visitor(value.event_id);
  visitor(value.role);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialNewsRelationCause &value,
                  Visitor &&visitor) {
  visitor(value.news_event_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialTaskReportRelationCause &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.settlement_id);
  visitor(value.actual_terminal);
}

template <class Visitor>
inline void
visit(const ::hanmo::resident::SocialGrowthServiceRelationCause &value,
      Visitor &&visitor) {
  visitor(value.action_id);
  visitor(value.actual_terminal);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialRelationResult &value,
                  Visitor &&visitor) {
  visitor(value.result_id);
  visitor(value.relation_record_key);
  visitor(value.from_actor);
  visitor(value.to_actor);
  visitor(value.original_cause);
  visitor(value.trust_before);
  visitor(value.affinity_before);
  visitor(value.trust_delta);
  visitor(value.affinity_delta);
  visitor(value.trust_after);
  visitor(value.affinity_after);
  visitor(value.applied_at);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalSocialRelationSeed &value,
                  Visitor &&visitor) {
  visitor(value.from_actor);
  visitor(value.to_actor);
  visitor(value.trust);
  visitor(value.affinity);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::OriginalSocialRelationCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.relations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialPolicy &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.target_actor_id);
  visitor(value.food_item_id);
  visitor(value.interval_ticks);
  visitor(value.share_news);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialCatalog &value,
                  Visitor &&visitor) {
  visitor(value.policies);
  visitor(value.original_relations);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::SocialTime2State &value,
                  Visitor &&visitor) {
  visitor(value.source_time_bindings);
  visitor(value.message_execution_bindings);
  visitor(value.propagation_decisions);
  visitor(value.propagation_cooldowns);
  visitor(value.collection_cursor);
}
