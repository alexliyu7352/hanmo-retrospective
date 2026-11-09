// Original catalog entry point. Its typed allocation graph and shared-copy
// owner semantics now reside in the sole world retention traversal.
#include "catalog_retention.hpp"
#include "world_retention.hpp"
#include "json_decoded_world_seed.hpp"
namespace hanmo::resident {
Status retain_prepared_catalog(const std::shared_ptr<const PreparedCatalog>& value,CheckpointRetentionCensus& census){if(has_json_decoded_catalog_owner(value))return retain_json_decoded_catalog_owner(value,census);return retain_prepared_catalog_owned_graph(value,census);}
}
