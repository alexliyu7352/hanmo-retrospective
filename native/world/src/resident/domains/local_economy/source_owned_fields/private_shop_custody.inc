// 显式托管唯一源的完整拥有字段，不借序列化长度估算。
template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopHouseholdCustodyPolicy &value,
      Visitor &&visitor) {
  visitor(value.policy_ref);
  visitor(value.premises_id);
  visitor(value.inventory_selection);
  visitor(value.handling_rule_id);
  visitor(value.handling_rule_version);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopHouseholdCustodyCatalog &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.catalog_digest);
  visitor(value.policies);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopHouseholdCustodyAcceptedSource &value,
      Visitor &&visitor) {
  visitor(value.id);
  visitor(value.operation_id);
  visitor(value.proprietor_actor_id);
  visitor(value.original_shop_epoch);
  visitor(value.explicit_policy_ref);
  visitor(value.accepted_at);
  visitor(value.accepted_service);
}
template <class Visitor>
inline void
visit(const ::hanmo::resident::PrivateShopHouseholdCustodyState &value,
      Visitor &&visitor) {
  visitor(value.version);
  visitor(value.accepted);
}
