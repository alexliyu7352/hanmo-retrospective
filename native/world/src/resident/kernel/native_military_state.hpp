// Finite selected physical owners and their authored rule catalogs. Selection is authored, never inferred from
// current actions or nonempty historical rows. Business source validation stays
// with the original military owners.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;
struct NativeMilitaryStateVersions {
    Integer dynamic_military=0;
    Integer military_duty_catalog=0;
    Integer military_action_time=0;
    Integer military_authority_time=0;
    Integer military_action_resources=0;
    Integer military_return_detachments=0;
    Integer military_action_conditions=0;
    Integer military_training_completion_time=0;
    Integer military_formation_reservations=0;
};
Outcome<NativeMilitaryStateVersions> selected_native_military_state_versions(const CatalogIdentity&);
Status append_native_military_state_modules(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_military_state_catalog(const BoundaryValue&,PreparedCatalog&);
Status validate_native_military_state_catalog(const PreparedCatalog&);
Status validate_native_military_state_layout(const WorldBundle&);
class CommonStore;struct MilitaryState;struct MilitaryActionTimeState;
Status validate_native_military_training_completion_headers(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState*,Integer selected_version);
}
