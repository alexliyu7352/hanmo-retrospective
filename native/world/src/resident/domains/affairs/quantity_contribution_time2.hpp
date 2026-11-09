#pragma once
#include "affairs_model.hpp"
namespace hanmo::resident {
// ProcurementExact is a finite S3 purpose. Treaty aid and custody/instance deliveries have their own consumers.
Status apply_quantity_contribution_v2(const CommonStore&,AffairsState&,const TaskState&,const StableId& delivery_id);
Status reconcile_quantity_contributions_v2(const CommonStore&,AffairsState&,const TaskState&);
}
