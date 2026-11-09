#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_budget_policy_codec.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {
namespace {
using namespace organization_lifecycle_fields;
template <class C> Status budget_unit(C &c, BudgetUnit &u) {
  StableId kind;
  if (!c.reading())
    kind = std::holds_alternative<BudgetMoneyQian>(u) ? "money_qian"
                                                      : "item_quantity";
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind != "money_qian" && kind != "item_quantity")
    return c.reject("UnsupportedPublicBudgetPolicyUnit", kind);
  if (c.reading()) {
    if (kind == "money_qian")
      u = BudgetMoneyQian{};
    else
      u = BudgetItemQuantity{};
  }
  if (kind == "item_quantity")
    c.field("item_id", std::get<BudgetItemQuantity>(u).item_id);
  return c.status();
}
template <class C> Status purpose(C &c, PublicBudgetPolicyPurpose &p) {

  StableId tag;
  if (!c.reading())
    tag = public_budget_policy_purpose_name(p);
  c.field("purpose", tag);
  if (!c.status().ok)
    return c.status();
  if (tag != "aid" && tag != "work")
    return c.reject("UnsupportedPublicBudgetPolicyPurpose", tag);
  if (c.reading())
    p = tag == "aid" ? PublicBudgetPolicyPurpose::aid
                     : PublicBudgetPolicyPurpose::work;
  return c.status();
}
template <class C> Status fields(C &c, PublicBudgetPolicy &p) {
  c.field("policy_id", p.policy_id);
  c.field("revision", p.revision);
  c.sequence("purposes", p.purposes,
             [](auto &v, auto &x) { return purpose(v, x); });
  c.object("scope", [&](auto &v) {
    static constexpr const char *tags[] = {"organization", "jurisdiction"};
    enum_field(v, "kind", p.scope.kind, tags);
    return v.field("id", p.scope.id);
  });
  c.object("period", [&](auto &v) {
    v.object("from",
             [&](auto &x) { return exact_fields::moment(x, p.period.from); });
    return v.object(
        "until", [&](auto &x) { return exact_fields::moment(x, p.period.to); });
  });
  c.sequence("containers", p.containers, [](auto &v, auto &r) {
    StableId kind;
    if (!v.reading()) {
      if (r.kind == ResourceKey::Kind::wallet)
        kind = "wallet";
      else if (r.kind == ResourceKey::Kind::inventory)
        kind = "inventory";
      else
        return v.reject("UnsupportedPublicBudgetPolicyContainer");
    }
    v.field("kind", kind);
    if (!v.status().ok)
      return v.status();
    if (kind != "wallet" && kind != "inventory")
      return v.reject("UnsupportedPublicBudgetPolicyContainer", kind);
    if (v.reading())
      r.kind = kind == "wallet" ? ResourceKey::Kind::wallet
                                : ResourceKey::Kind::inventory;
    return v.field("id", r.id);
  });
  c.sequence("total_caps", p.total_caps, [](auto &v, auto &x) {
    v.object("unit", [&](auto &u) { return budget_unit(u, x.unit); });
    return v.field("cap", x.cap);
  });
  c.sequence("purpose_caps", p.purpose_caps, [](auto &v, auto &x) {
    purpose(v, x.purpose);
    v.object("unit", [&](auto &u) { return budget_unit(u, x.unit); });
    return v.field("cap", x.cap);
  });
  if (!c.status().ok)
    return c.status();
  const auto valid = validate_public_budget_policy_shape(p);
  return valid.ok ? c.status()
                  : c.reject(valid.error.original.code, p.policy_id);
}
template <class C> Status catalog(C &c, PublicBudgetPolicyCatalog &out) {
  Integer version = 1;
  c.field("schema_version", version);
  if (!c.status().ok)
    return c.status();
  if (version != 1)
    return c.reject("UnsupportedPublicBudgetPolicyVersion",
                    "budget_policy/observed_version/" +
                        std::to_string(version) + "/expected_version/1");
  c.sequence("policies", out.policies,
             [](auto &v, auto &p) { return fields(v, p); });
  if (!c.status().ok)
    return c.status();
  const auto valid = validate_public_budget_policy_catalog_shape(out);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace
Status visit_public_budget_policy_catalog(CodecVisitor &c,
                                          PublicBudgetPolicyCatalog &value) {
  return catalog(c, value);
}
Status emit_public_budget_policy_catalog_projection(
    ProjectionWriter &sink, std::string_view name,
    const PublicBudgetPolicyCatalog &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &v) { return catalog(c, v); });
}
Outcome<BoundaryValue>
encode_public_budget_policy_catalog(const PublicBudgetPolicyCatalog &value) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto valid = emit_public_budget_policy_catalog_projection(sink, "", value);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<PublicBudgetPolicyCatalog>
decode_public_budget_policy_catalog(const BoundaryValue &value,
                                    const CatalogIdentity &identity) {
  ActorFoundationJsonVisitor reader(value);
  auto valid = reader.prepare_default_values<PublicBudgetPolicyCatalog>();
  if (!valid.ok)
    return {false, {}, valid.error};
  PublicBudgetPolicyCatalog out;
  valid = catalog(reader, out);
  if (valid.ok)
    valid = reader.finish();
  if (valid.ok) {


    if (auto *owner = ScopedCheckpointJsonDecode::current())
      owner->ordinary(std::size_t{0});
    else if (auto *owner = ScopedCheckpointBoundaryDecode::current())
      owner->ordinary(std::size_t{0});
    else
      return {false,
              {},
              common_failure("PublicBudgetPolicyDecodeOwnerRequired").error};
    out.identity = CatalogIdentity(identity);
  }
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
