#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_relations_fields.hpp"
namespace hanmo::resident {
Status visit_political_relations(CodecVisitor &c, PoliticalRelationsState &r) {
  return political_relations_fields::state(c, r);
}
Status emit_political_relations_projection(ProjectionWriter &w,
                                           std::string_view name,
                                           const PoliticalRelationsState &r) {
  auto valid = validate_political_relations_shape(r);
  if (!valid.ok)
    return valid;
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r,
      [](auto &c, auto &v) { return political_relations_fields::state(c, v); });
}
Outcome<BoundaryValue>
encode_political_relations(const PoliticalRelationsState &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto valid = emit_political_relations_projection(w, "", r);
  if (valid.ok)
    valid = w.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<PoliticalRelationsState>
decode_political_relations(const BoundaryValue &input) {
  PoliticalRelationsState out;
  ActorFoundationJsonVisitor r(input);
  auto valid = political_relations_fields::state(r, out);
  if (valid.ok)
    valid = r.finish();
  return {valid.ok, std::move(out), valid.error};
}
Status visit_political_relations_catalog(CodecVisitor &c,
                                         PoliticalRelationsCatalog &r) {
  return political_relations_fields::catalog(c, r);
}
Status emit_political_relations_catalog_projection(
    ProjectionWriter &w, std::string_view name,
    const PoliticalRelationsCatalog &r) {
  auto valid = validate_political_relations_catalog_shape(r);
  if (!valid.ok)
    return valid;
  return emit_projection_record<ActorFoundationProjectionArchive>(
      w, name, r, [](auto &c, auto &v) {
        return political_relations_fields::catalog(c, v);
      });
}
Outcome<BoundaryValue>
encode_political_relations_catalog(const PoliticalRelationsCatalog &r) {
  BoundaryValue out;
  BoundaryProjectionWriter w(out);
  auto valid = emit_political_relations_catalog_projection(w, "", r);
  if (valid.ok)
    valid = w.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<PoliticalRelationsCatalog>
decode_political_relations_catalog(const BoundaryValue &input,
                                   const CatalogIdentity &identity,
                                   const OrganizationCatalog &organizations) {
  PoliticalRelationsCatalog out;
  out.identity = identity;
  ActorFoundationJsonVisitor r(input);
  auto valid = political_relations_fields::catalog(r, out);
  if (valid.ok)
    valid = r.finish();
  if (valid.ok)
    valid = validate_political_relations_catalog(out, organizations);
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
