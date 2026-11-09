// The original Task Work physical section is selected explicitly. It never
// supplies the missing Task base, service approval or an alternative Work owner.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct PreparedCatalog;struct WorldBundle;struct TaskState;class ProjectionWriter;
Outcome<Integer> selected_native_task_work_version(const CatalogIdentity&);
Status append_native_task_work_module(const BoundaryValue& scenario,CatalogIdentity&);
Status validate_native_task_work_catalog(const PreparedCatalog&);
Status initialize_native_task_work_root(const PreparedCatalog&,TaskState&);
Status validate_native_task_work_layout(const PreparedCatalog&,const TaskState&);
Status validate_native_task_work_world(const WorldBundle&);
// The same original owner emits this complete section without a BoundaryValue
// intermediate. The complete World composition must call this selected port.
Status emit_native_task_work_section(ProjectionWriter&,const WorldBundle&);
}
