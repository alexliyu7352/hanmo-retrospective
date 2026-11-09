// PC14 原预算与作者政策的唯一普通拥有字段入口。
// 在中央原命名空间包含；pragma once 防原全根入口与本窄入口重复定义。
#pragma once
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetLimit &value,
                  Visitor &&visitor) {
  visitor(value.unit);
  visitor(value.cap);
  visitor(value.civil_floor);
  visitor(value.garrison_floor);
  visitor(value.container_refs);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetCategoryLimit &value,
                  Visitor &&visitor) {
  visitor(value.purpose);
  visitor(value.unit);
  visitor(value.cap);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::BudgetAuthorizationSource &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.budget_id);
  visitor(value.policy_id);
  visitor(value.principal_org_id);
  visitor(value.issuer_actor_id);
  visitor(value.budget_revision);
  visitor(value.policy_revision);
  visitor(value.period);
  visitor(value.scope);
  visitor(value.container_refs);
  visitor(value.purposes);
  visitor(value.total_caps);
  visitor(value.category_caps);
  visitor(value.parent_budget_ref);
  visitor(value.authority_basis_ref);
  visitor(value.approval_source);
  visitor(value.approved_at);
  visitor(value.decision);
  visitor(value.closed);
}

template <class Visitor>
inline void visit(const ::hanmo::resident::PoliticalBudgetState &value,
                  Visitor &&visitor) {
  visitor(value.authorization_history);
  visitor(value.schema_version);
}

// 原作者政策的真实拥有字段；不投影支用余额或创建第二权源。
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetPolicyScope &value,
                  Visitor &&visitor) {
  visitor(value.kind);
  visitor(value.id);
}

// 原作者政策的真实拥有字段；不投影支用余额或创建第二权源。
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetPolicyLimit &value,
                  Visitor &&visitor) {
  visitor(value.unit);
  visitor(value.cap);
}

// 原作者政策的真实拥有字段；不投影支用余额或创建第二权源。
template <class Visitor>
inline void
visit(const ::hanmo::resident::PublicBudgetPolicyPurposeLimit &value,
      Visitor &&visitor) {
  visitor(value.purpose);
  visitor(value.unit);
  visitor(value.cap);
}

// 原作者政策的真实拥有字段；不投影支用余额或创建第二权源。
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetPolicy &value,
                  Visitor &&visitor) {
  visitor(value.policy_id);
  visitor(value.revision);
  visitor(value.purposes);
  visitor(value.scope);
  visitor(value.period);
  visitor(value.containers);
  visitor(value.total_caps);
  visitor(value.purpose_caps);
}

// 原作者政策的真实拥有字段；不投影支用余额或创建第二权源。
template <class Visitor>
inline void visit(const ::hanmo::resident::PublicBudgetPolicyCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.policies);
}
