// Concrete projection of the one original production owner. It creates neither
// attendance nor a second capacity/supply balance.
#pragma once
#include "production_capacity_input.hpp"
#include "production_labor_input.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct NativeProductionInputContext {
 const OriginalProductionCapacityContext& capacity;
 const OriginalProductionLaborContext& labor;
 const TaskState* tasks=nullptr;
};
class NativeProductionInputSourceRead final:public ProductionInputSourceRead {
 const NativeProductionInputContext& x_;
public:
 explicit NativeProductionInputSourceRead(const NativeProductionInputContext& x):x_(x){}
 Outcome<ProductionInputRead> read_production_segment_inputs(const StableId&,const StableId&,const WorldMoment&,const WorldMoment&)const override;
 Outcome<ProductionInputRead> read_production_inputs_after_change(const StableId&,const StableId&,const WorldMoment&,const WorldMoment&,const MomentOwnerSource&)const override;
 Outcome<ProductionInputRead> read_validated_production_segment_source(const ProductionSegmentSource&)const override;
 Status validate_production_segment_source(const ProductionSegmentSource&)const override;
 Status validate_production_input_change(const StableId&,const WorldMoment&,const SourceRef&)const override;
 Outcome<std::optional<WorldMoment>> read_next_production_rule_boundary(const StableId&,const StableId&)const override;
};
}
