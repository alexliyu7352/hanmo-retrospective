// E22.4唯一结构状态。slot与原生产asset单位分离；真正S2/专业工作来源只借用，不复制军事工时。
#pragma once
#include "../economic_facility_sources.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
enum class ProductionFacilityKind { grain_mill,irrigation_work,craft_workshop };
struct FacilityBaseSlotCapacity { StableId slot_kind,capacity_unit_id;Integer units=0; };
struct ProductionFacilityCondition {
 StableId facility_ref;Integer condition_revision=1,integrity=0,max_integrity=0;
 std::vector<FacilityBaseSlotCapacity> base_capacity_by_slot;Integer base_asset_capacity_units=0;
 EconomicRuleRef damage_policy_ref,repair_policy_ref;SourceRef last_change_ref;
};
struct ProductionFacilityConditionSeed {StableId condition_ref,original_asset_ref;ProductionFacilityKind kind=ProductionFacilityKind::grain_mill;Integer initial_integrity=0,max_integrity=0,base_asset_capacity_units=0;std::vector<FacilityBaseSlotCapacity> base_capacity_by_slot;EconomicRuleRef damage_policy_ref,repair_policy_ref;FacilitySeedRowRef original_installation_seed_ref;};
struct FacilityAttackResultRef {SourceRef source;Integer source_schema=0;};
struct CompletedFacilityRepairRef {SourceRef source;Integer source_schema=0;};
using FacilityConditionEffectSource=std::variant<FacilityAttackResultRef,CompletedFacilityRepairRef>;
struct FacilitySlotCapacityChange {StableId slot_kind;Integer before=0,after=0;};
struct FacilityConditionChange {
 StableId change_id,facility_ref;FacilityConditionEffectSource source;WorldMoment occurred_at;
 Integer old_integrity=0,new_integrity=0,old_revision=0,new_revision=0;
 std::vector<FacilitySlotCapacityChange> capacity_changes;std::vector<FacilitySourceRef> interrupted_activity_results;std::vector<FacilityClaimReceiptRef> claim_release_results;
};
struct ProductionFacilityConditionsState {OrderedTable<ProductionFacilityCondition> conditions;OrderedTable<FacilityConditionChange> changes;};
struct ProductionFacilityConditionsCatalog {OrderedTable<ProductionFacilityConditionSeed> seeds;};
// 两个值仅从真实原source reader返回，不能由命令传损害/修复量。
struct AppliedFacilityStructureEffect {SourceRef source;Integer source_schema=0;StableId facility_ref;EconomicRuleRef policy_ref;Integer actual_units=0;WorldMoment occurred_at;};
class ProductionFacilityEffectRead {public:virtual ~ProductionFacilityEffectRead()=default;
 virtual Outcome<AppliedFacilityStructureEffect> read_applied_facility_attack(const FacilityAttackResultRef&)const=0;
 virtual Outcome<AppliedFacilityStructureEffect> read_completed_facility_repair(const CompletedFacilityRepairRef&)const=0;
};
// 实际变更原子栈先结旧段，再结构变化/全holder处置，最后装未来输入。三个钩子皆必需。
class ProductionFacilityMutationServices {public:virtual ~ProductionFacilityMutationServices()=default;
 virtual Status close_facility_production_inputs(const StableId&,const WorldMoment&,const SourceRef&)=0;
 virtual Status reconcile_facility_capacity_change(FacilityConditionChange&)=0;
 virtual Status reopen_facility_production_inputs(const StableId&,const WorldMoment&)=0;
};
struct ConditionBoundAssetCapacity {StableId asset_ref,condition_ref;Integer condition_revision=0,integrity=0,max_integrity=0,usable_asset_units=0;SourceRef condition_change_source_ref;};
// Borrow the unique original Installed extension selected by this exact locked
// seed. This identity edge does not replace the full Facility title/catalog gate.
Outcome<const PhysicalServiceFacilityExtension*> read_production_facility_condition_installation(const ProductionFacilityConditionSeed&,const EconomicFacilitySourcesState&);
Outcome<Integer> facility_condition_capacity(Integer base,Integer integrity,Integer maximum,ExactArithmeticBudget={});
Status initialize_production_facility_conditions_in_candidate(ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&);
Outcome<ProductionFacilityCondition> read_production_facility_condition_revision(const ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const StableId&,Integer);
Outcome<ConditionBoundAssetCapacity> read_condition_bound_asset_capacity(const ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const StableId& asset,const WorldMoment&,ExactArithmeticBudget={});
Outcome<StableId> apply_production_facility_effect_in_candidate(CommonStore&,ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const ProductionFacilityEffectRead&,ProductionFacilityMutationServices&,const StableId& condition_ref,Integer expected_revision,const FacilityConditionEffectSource&,ExactArithmeticBudget={});
Status validate_production_facility_conditions(const CommonStore&,const ProductionFacilityConditionsState&,const ProductionFacilityConditionsCatalog&,const EconomicFacilitySourcesState&,const ProductionFacilityEffectRead&,ExactArithmeticBudget={});
class ProjectionWriter;class CodecVisitor;
// P08's actual condition-change Common heads only. Installation/capacity/retention
// are owned inline originals, never fabricated Common payloads here.
class ProductionFacilityConditionFactHeaderSink {
public:virtual ~ProductionFacilityConditionFactHeaderSink()=default;
 virtual Status append(const FactRecord& actual_header,Integer original_schema)=0;
};
Outcome<Integer> read_production_facility_condition_fact_payload_schema(const CommonStore&,const ProductionFacilityConditionsState&,const EconomicFacilitySourcesState&,const FactRecord&);
Status visit_production_facility_condition_fact_headers(const CommonStore&,const ProductionFacilityConditionsState&,const EconomicFacilitySourcesState&,ProductionFacilityConditionFactHeaderSink&);
Status emit_production_facility_condition_fact_payload_fields(ProjectionWriter&,const CommonStore&,const ProductionFacilityConditionsState&,const EconomicFacilitySourcesState&,const FactRecord&);
BoundaryValue encode_production_facility_conditions(const ProductionFacilityConditionsState&);
Outcome<ProductionFacilityConditionsState> decode_production_facility_conditions(const BoundaryValue&);
Status emit_production_facility_conditions(ProjectionWriter&,const ProductionFacilityConditionsState&);
Status visit_production_facility_conditions(CodecVisitor&,ProductionFacilityConditionsState&);
BoundaryValue encode_production_facility_conditions_catalog(const ProductionFacilityConditionsCatalog&);
Outcome<ProductionFacilityConditionsCatalog> decode_production_facility_conditions_catalog(const BoundaryValue&);
Status emit_production_facility_conditions_catalog(ProjectionWriter&,const ProductionFacilityConditionsCatalog&);
Status visit_production_facility_conditions_catalog(CodecVisitor&,ProductionFacilityConditionsCatalog&);
}
