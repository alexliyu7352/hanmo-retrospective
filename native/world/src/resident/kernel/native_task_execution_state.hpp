// Explicit selection of the original Task execution appendix. This helper is
// not another Task owner and cannot satisfy missing Life, S0 or waiting sources.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;struct TaskState;class CommonStore;class ProjectionWriter;
// Zero means not selected. No retained row can select or upgrade a profile.
Outcome<Integer> decode_native_task_execution_selection(const BoundaryValue& scenario);
Outcome<Integer> selected_native_task_execution_version(const CatalogIdentity&);
Status append_native_task_execution_module(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_task_execution_catalog(const BoundaryValue&,PreparedCatalog&);
Status validate_native_task_execution_catalog(const PreparedCatalog&);
Status initialize_native_task_execution_root(const CommonStore&,const PreparedCatalog&,TaskState&);
Status validate_native_task_execution_layout(const CommonStore&,const PreparedCatalog&,const TaskState&);
Status validate_native_task_execution_layout(const WorldBundle&);
// Common owns kind/domain/order; the original Task owner emits its actual row.
Status emit_native_task_execution_fact_payloads_fields(ProjectionWriter&,const CommonStore&,const PreparedCatalog&,const TaskState&);
}
