// S9唯一实例/批次性质；Common唯一物量、物权与保管，输出必须来自真实producer结果。
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "item_condition_model.hpp"
#include "repair_model.hpp"
#include "resident/domains/commerce/repair_service/repair_service_model.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
enum class ItemInstanceState { live,destroyed };
struct ItemInstance { StableId id,item_definition_id,creation_fact_id; SubjectRef maker_ref; Integer quality_score=0,durability=0,max_durability=0; StableId repair_policy_id; ItemInstanceState state=ItemInstanceState::live; Integer instance_version=1; std::vector<StableId> repair_fact_refs; };
struct LotProperties { StableId properties_id,definition_id; Integer schema_version=1; std::optional<Integer> quality_score; std::vector<StableId> fit_tag_ids; Integer produced_tick=0; std::optional<Integer> expires_at_tick; SourceRef origin_receipt_ref; std::optional<SubjectRef> maker_ref; StableId property_policy_id; Integer property_policy_version=1; };
enum class ItemOutputKind { resource_harvest,production_output,craft_output,pharmacy_output,ritual_material_output,arcane_reward_output,cultural_carrier };
enum class ItemOutputRole { finished,recoverable,waste };
struct ItemOutputKey { SourceRef source_ref; Integer output_ordinal=0,source_schema=1; };
struct ItemOutputAdoption { ItemOutputKey key; ItemOutputKind source_kind=ItemOutputKind::production_output; StableId definition_id; Integer original_quantity=0; std::optional<StableId> instance_id; std::optional<LotPropertiesRef> properties_ref; StableId original_lot_id; };
struct ItemTerminationResult { StableId id,instance_id; SourceRef cause_ref; Integer cause_schema=0,usage_ordinal=0,effective_tick=0,instance_version_before=0,instance_version_after=0; StableId source_inventory_id,source_lot_id; };
struct ItemInstancesState { OrderedTable<ItemInstance> instances; std::vector<ItemOutputAdoption> output_adoptions; std::vector<ItemTerminationResult> terminations; std::vector<ItemConditionResultView> condition_results; RepairJobsState repair_jobs; RepairServiceContractsState repair_service_contracts; };
struct LotPropertiesState { OrderedTable<LotProperties> properties; std::vector<ItemOutputAdoption> output_adoptions; };
struct ItemPropertyPolicy { StableId id,definition_id; Integer version=1; bool nonperishable=false,splittable=true; std::optional<Integer> shelf_life_ticks; std::vector<StableId> allowed_fit_tag_ids; };
struct ItemServiceCatalog { OrderedTable<ItemPropertyPolicy> property_policies; };
// Full locked author gate, including unused policies. It does not grant use.
Status validate_item_service_catalog(const ItemServiceCatalog&,const CommonCatalog&);
// 此返回值只能由静态已登记原owner reader从已应用投入/真实结果构造；不作为命令输入。
struct ItemOutputSourceView { ItemOutputKey key; ItemOutputKind source_kind=ItemOutputKind::production_output; ItemOutputRole output_role=ItemOutputRole::finished; StableId definition_id; Integer quantity=0,produced_tick=0; std::optional<Integer> quality_score; std::vector<StableId> fit_tag_ids; std::optional<Integer> expires_at_tick; std::optional<SubjectRef> maker_ref; StableId property_policy_id; Integer property_policy_version=0; bool unique_instance=false; Integer max_durability=0; StableId repair_policy_id; };
struct ItemTerminationSourceView { SourceRef source_ref; Integer source_schema=0,usage_ordinal=0,effective_tick=0; StableId instance_id; };
class ItemOutputSourceRead {
public:
 virtual ~ItemOutputSourceRead()=default;
 virtual Outcome<ItemOutputSourceView> read_item_output(const ItemOutputKey&,ItemOutputKind) const=0;
 virtual Outcome<ItemTerminationSourceView> read_item_termination(const SourceRef&,Integer schema,Integer ordinal,const StableId& instance_id) const=0;
};
// Original creation reads and their direct edges. These do not prove today's
// legal title, possession or condition; no Common source address is invented.
Outcome<const ItemOutputAdoption*> read_item_instance_original_output(const CommonStore&,const ItemInstancesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const StableId& instance_id,const CurrentMomentRead* actual_clock=nullptr);
Outcome<const ItemOutputAdoption*> read_lot_properties_original_output(const CommonStore&,const LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const LotPropertiesRef&,const CurrentMomentRead* actual_clock=nullptr);
Outcome<const ItemTerminationResult*> read_item_original_termination(const CommonStore&,const ItemInstancesState&,const ItemOutputSourceRead&,const StableId& termination_id,const CurrentMomentRead* actual_clock=nullptr);
class ItemOutputSourcePinSink {
public:
 virtual ~ItemOutputSourcePinSink()=default;
 virtual Status original_output(const ItemOutputKey&,ItemOutputKind)=0;
};
class LotPropertiesSourcePinSink:public ItemOutputSourcePinSink {
public:
 virtual Status property_policy(const StableId& policy_id,Integer policy_version)=0;
};
Status visit_item_instance_original_output_pins(const CommonStore&,const ItemInstancesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const StableId& instance_id,ItemOutputSourcePinSink&,const CurrentMomentRead* actual_clock=nullptr);
Status visit_lot_properties_original_pins(const CommonStore&,const LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const LotPropertiesRef&,LotPropertiesSourcePinSink&,const CurrentMomentRead* actual_clock=nullptr);
struct ItemMaterialUse { StableId definition_id; std::optional<Integer> minimum_quality; std::vector<StableId> required_fit_tag_ids; bool require_properties=false,allow_expired=false; };
struct ItemMaterialization { std::optional<StableId> instance_id; std::optional<LotPropertiesRef> properties_ref; StableId original_lot_id; };
// 实际输出与S9引用只同候选物化一次；ID只从Common原分配器取，原货流从capture读。
Outcome<ItemMaterialization> materialize_item_output_in_candidate(CommonStore&,ItemInstancesState&,LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const ItemOutputKey&,ItemOutputKind,const StableId& inventory_id,const Owner& owner,const CurrentMomentRead* actual_clock=nullptr);
Status check_item_material_use(const ItemInstancesState&,const LotPropertiesState&,const ItemServiceCatalog&,const LotView&,const ItemMaterialUse&,Integer at_tick);
class ItemConditionOriginalRead;class CurrentMomentRead;
struct RepairReadContext;
Status validate_item_service(const CommonStore&,const ItemInstancesState&,const LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,Integer selected_version=1,const ItemConditionOriginalRead* conditions=nullptr,const CurrentMomentRead* clock=nullptr,const RepairReadContext* repair=nullptr);
// 只有真实终结source已验证的同栈授权才准consume唯一实例；它不持久也不改变Owner/钱。
struct ItemConsumptionScope { SourceRef source_ref; Integer source_schema=0,usage_ordinal=0; StableId instance_id; };
class ItemResourceAccess final:public SelectedLotSemanticRead {
public:
 ItemResourceAccess(const ItemInstancesState&,const LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const ItemConsumptionScope* =nullptr,const CurrentMomentRead* actual_clock=nullptr);
 Status validate_live_lot(const CommonRead&,const StableId&,const LotView&) const override;
 Status validate_selected_use(const CommonRead&,const StableId&,const LotView&,Integer,SelectedLotUse) const override;
 Status validate_materialized_lot(const CommonRead&,const StableId&,const LotView&) const override;
private:
 const ItemInstancesState& instances_;const LotPropertiesState& properties_;const ItemServiceCatalog& catalog_;const ItemOutputSourceRead& sources_;const ItemConsumptionScope* consumption_;const CurrentMomentRead* actual_clock_;
};
Outcome<ItemTerminationResult> consume_unique_item_in_candidate(CommonStore&,ItemInstancesState&,const LotPropertiesState&,const ItemServiceCatalog&,const ItemOutputSourceRead&,const ItemConsumptionScope&,const CargoSelection&,const Owner&,const ReservationRef&);
}
