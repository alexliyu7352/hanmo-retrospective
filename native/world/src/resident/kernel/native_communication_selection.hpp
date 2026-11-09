// Exact physical communication selection. Legacy three-table standalone
// codecs are preserved separately; they never impersonate the new /1 design.
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline Outcome<Integer> selected_native_communication_links_version(
    const CatalogIdentity& identity) {
  Integer version=0;
  bool found=false;
  for(const auto& row:identity.module_versions)if(row.first=="communication_links") {
    if(found || row.second!="2")
      return {false,{},common_failure("NativeCommunicationLinksPhysicalSelectionRequired",row.first,ErrorKind::unsupported_capability).error};
    found=true;version=2;
  }
  return {true,version,{}};
}
} // namespace hanmo::resident
