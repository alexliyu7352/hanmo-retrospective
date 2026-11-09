// The existing occupancy catalog identity comparison, shared without copies.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
inline bool same_original_catalog_identity(const CatalogIdentity& a,const CatalogIdentity& b){
    return a.content_id==b.content_id&&a.content_version==b.content_version&&
        a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&
        a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&
        a.artifact_sha256==b.artifact_sha256&&a.module_versions==b.module_versions;
}
}
