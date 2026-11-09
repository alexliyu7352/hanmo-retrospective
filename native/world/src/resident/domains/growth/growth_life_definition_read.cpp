// 同一Growth作者行的实际id/version读取，缺版/错版/重复均不猜测默认值。
#include "growth_life_definition_read.hpp"
#include "growth_model.hpp"
namespace hanmo::resident {
namespace {
// 原定义地址只有两个字段；新字段重复也必须拒绝，不采用字典首项。
Status valid_ref(const StableId &id, Integer version) {
  if (id.empty() || id.size() > 256 || version < 1)
    return common_failure("GrowthLifeDefinitionRefRequired", id);
  return common_success();
}
} // namespace
// 后置自主生活环只保留明确不支持能力门；不改变原基础 Growth 接受域。
Status
validate_growth_life_definition_selection1(const CatalogIdentity &) {
  return common_failure("NOT_IMPLEMENTED", "director_life_loops",
                        ErrorKind::unsupported_capability);
}
Outcome<GrowthLifeDefinitionRef1>
decode_growth_life_definition_ref1(const BoundaryValue &value) {
  try {
    using namespace boundary;
    const auto &rows = object(value, "definition_ref");
    fields(value, {"id", "version"}, {}, "definition_ref");
    if (rows.size() != 2)
      return {false,
              {},
              common_failure("DuplicateGrowthLifeDefinitionField").error};
    GrowthLifeDefinitionRef1 ref{get_string(value, "id", "definition_ref"),
                                 get_int(value, "version", "definition_ref")};
    const auto valid = valid_ref(ref.id, ref.version);
    return {valid.ok, std::move(ref), valid.error};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
Status validate_growth_life_definition_rows1(const GrowthCatalog &catalog) {
  // 不建影子索引。每个地址仍以原role/课程或rest行作为唯一实际持有者。
  for (const auto &role : catalog.roles.rows()) {
    for (const auto &course : role.second.courses) {
      if (!course.definition_ref)
        continue;
      auto valid =
          valid_ref(course.definition_ref->id, course.definition_ref->version);
      if (!valid.ok)
        return valid;
      std::size_t matches = 0;
      for (const auto &other : catalog.roles.rows())
        for (const auto &row : other.second.courses)
          if (row.definition_ref &&
              row.definition_ref->id == course.definition_ref->id &&
              row.definition_ref->version == course.definition_ref->version)
            ++matches;
      if (matches != 1)
        return common_failure("DuplicateGrowthStudyDefinition",
                              course.definition_ref->id);
    }
    for (const auto &activity : role.second.activities.rows()) {
      if (!activity.second.definition_ref)
        continue;
      if (activity.first != "rest")
        return common_failure("GrowthLifeRestDefinitionRequired",
                              activity.first);
      auto valid = valid_ref(activity.second.definition_ref->id,
                             activity.second.definition_ref->version);
      if (!valid.ok)
        return valid;
      std::size_t matches = 0;
      for (const auto &other : catalog.roles.rows())
        for (const auto &row : other.second.activities.rows())
          if (row.second.definition_ref &&
              row.second.definition_ref->id ==
                  activity.second.definition_ref->id &&
              row.second.definition_ref->version ==
                  activity.second.definition_ref->version)
            ++matches;
      if (matches != 1)
        return common_failure("DuplicateGrowthRestDefinition",
                              activity.second.definition_ref->id);
    }
  }
  return common_success();
}
Outcome<GrowthStudyDefinitionRead1>
read_growth_study_definition1(const GrowthCatalog &catalog,
                              const CatalogIdentity &identity,
                              const StableId &id, Integer version) {
  auto valid = validate_growth_life_definition_selection1(identity);
  if (valid.ok)
    valid = valid_ref(id, version);
  if (valid.ok)
    valid = validate_growth_life_definition_rows1(catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &role : catalog.roles.rows())
    for (const auto &course : role.second.courses)
      if (course.definition_ref && course.definition_ref->id == id &&
          course.definition_ref->version == version)
        return {true, {&role.first, &course}, {}};
  return {false,
          {},
          common_failure("OriginalGrowthStudyDefinitionRequired", id).error};
}
Outcome<GrowthRestDefinitionRead1>
read_growth_rest_definition1(const GrowthCatalog &catalog,
                             const CatalogIdentity &identity,
                             const StableId &id, Integer version) {
  auto valid = validate_growth_life_definition_selection1(identity);
  if (valid.ok)
    valid = valid_ref(id, version);
  if (valid.ok)
    valid = validate_growth_life_definition_rows1(catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &role : catalog.roles.rows()) {
    const auto *rest = role.second.activities.find("rest");
    if (rest && rest->definition_ref && rest->definition_ref->id == id &&
        rest->definition_ref->version == version)
      return {true, {&role.first, rest}, {}};
  }
  return {false,
          {},
          common_failure("OriginalGrowthRestDefinitionRequired", id).error};
}
} // namespace hanmo::resident
