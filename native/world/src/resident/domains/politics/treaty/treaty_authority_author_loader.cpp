#include "treaty_authority_author_loader.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include <new>
namespace hanmo::resident {
Outcome<TreatyAuthorityMappingCatalog>
load_treaty_authority_mapping_from_scenario(
    const BoundaryValue &scenario, const CatalogIdentity &identity,
    const PoliticalTreatyTermsCatalog &terms) {
  try {
    const BoundaryValue *selected = nullptr;
    for (const auto &row : boundary::object(scenario, "scenario")) {
      if (row.first != "treaty_authority_mapping") continue;
      if (selected)
        return {false, {}, common_failure("DuplicateTreatyAuthorityMappingAuthorBlock").error};
      selected = &row.second;
    }
    if (!selected)
      return {false, {}, common_failure("TreatyAuthorityMappingAuthorBlockRequired").error};
    if (terms.definitions.rows().empty())
      return {false, {}, common_failure("TreatyAuthorityMappingOriginalTermsRequired").error};
    // The original paired decoder enforces exact schema 1, every original
    // definition/party/term exactly once, kind equality, explicit command/scope
    // and original CatalogIdentity. It rejects unknown/missing block fields.
    return decode_treaty_authority_mapping(*selected, identity, terms);
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  } catch (const std::bad_alloc &) {
    return {false, {}, common_failure("TreatyAuthorityMappingResourceExhausted").error};
  }
}
Outcome<std::shared_ptr<const TreatyAuthorityMappingCatalog>>
make_treaty_authority_mapping_from_scenario(
    const BoundaryValue &scenario, const CatalogIdentity &identity,
    const PoliticalTreatyTermsCatalog &terms) {
  auto read = load_treaty_authority_mapping_from_scenario(scenario, identity, terms);
  if (!read.ok) return {false, {}, read.error};
  try {
    return {true, checkpoint_make_shared<const TreatyAuthorityMappingCatalog>(
                       std::move(read.value)), {}};
  } catch (const std::bad_alloc &) {
    return {false, {}, common_failure("TreatyAuthorityMappingResourceExhausted").error};
  }
}
} // namespace hanmo::resident
