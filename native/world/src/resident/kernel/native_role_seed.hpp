// The original native role seeder supplies the immutable authored role leaf.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct CommonCatalog;
Status bind_native_authored_role_seed(CommonCatalog&,const BoundaryValue& initial);
}
