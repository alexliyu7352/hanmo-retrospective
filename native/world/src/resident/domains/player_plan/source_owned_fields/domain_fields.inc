// 职责：集中玩家计划中的有限拥有字段访问；仅借用原字段，不改变顺序或失败行为。
// 由中央登记头在 source_owned_fields
// 命名空间内包含；依赖与具名助手已先行可见。

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlan &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.actor_id);
  visitor(value.created_tick);
  visitor(value.steps);
  visitor(value.stop_reason);
  visitor(value.budget);
  visitor(value.approved_scope_revision);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanBudget &value,
                  Visitor &&visitor) {
  visitor(value.max_additional_ticks);
  visitor(value.max_gross_spend);
  visitor(value.used_ticks);
  visitor(value.gross_spend);
  visitor(value.approved_tick);
  visitor(value.exact_used_duration);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanContinue &value,
                  Visitor &&visitor) {
  visitor(value.action_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanCourse &value,
                  Visitor &&visitor) {
  visitor(value.context);
  visitor(value.skill_id);
  visitor(value.ticks);
  visitor(value.challenge_mode);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanMove &value,
                  Visitor &&visitor) {
  visitor(value.place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanRest &value,
                  Visitor &&visitor) {
  visitor(value.estate_id);
  visitor(value.duration);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanSource &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.source_id);
  visitor(value.observed_tick);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanState &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.current);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanStep &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.data);
  visitor(value.source);
  visitor(value.authorized);
  visitor(value.terms);
  visitor(value.phase);
  visitor(value.action_id);
  visitor(value.start_result_id);
  visitor(value.completion_result_id);
  visitor(value.started_tick);
  visitor(value.completed_tick);
  visitor(value.stop_reason);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanTaskReminder &value,
                  Visitor &&visitor) {
  visitor(value.task_id);
  visitor(value.place_id);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanTerms &value,
                  Visitor &&visitor) {
  visitor(value.provider_actor_id);
  visitor(value.role_id);
  visitor(value.place_id);
  visitor(value.payer_wallet_id);
  visitor(value.recipient_wallet_id);
  visitor(value.mode);
  visitor(value.skill_id);
  visitor(value.qualification_key);
  visitor(value.challenge_kind);
  visitor(value.challenge_id);
  visitor(value.challenge_mode);
  visitor(value.settlement_policy);
  visitor(value.challenge_source_sha256);
  visitor(value.duration);
  visitor(value.cost);
  visitor(value.wage);
  visitor(value.hp_gain);
  visitor(value.fatigue_delta);
  visitor(value.xp_limit);
  visitor(value.target_level);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanTravel &value,
                  Visitor &&visitor) {
  visitor(value.request);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PlayerPlanWork &value,
                  Visitor &&visitor) {
  visitor(value.context);
}
