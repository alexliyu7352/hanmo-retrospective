// 庄园领域唯一typed状态；私库与私钱仅通过共同recipe变更，不保存第二资产余额。
#pragma once
#include "../../storage/common_codec.hpp"
namespace hanmo::resident {
class PrivateShopWorkerCommitmentRead;struct DynamicStepInput;
struct EstateFacilities { Integer farm=0,granary=0,palisade=0;FieldOrder field_order{}; };
struct EstateRecord { StableId id,owner_actor_id,manager_actor_id;std::vector<StableId> resident_actor_ids;OrderedTable<StableId> duties;EstateFacilities facilities;OrderedTable<StableId> detached;Integer last_tick=0;StableId last_production_id;FieldOrder field_order{}; };
struct EstateConstruction { Integer build_ticks=0,material_cost=0,max_level=0,money_cost=0;StableId material_item_id,payee_wallet_id; };
struct EstateDefinition { StableId id,food_item_id,inventory_id,manager_actor_id,output_item_id,owner_actor_id,place_id,wallet_id;Integer granary_capacity_per_level=0,output_per_worker=0,palisade_defense_per_guard=0,ration_per_person=0,base_capacity=0;std::vector<StableId> resident_actor_ids;EstateFacilities initial_facilities;OrderedTable<StableId> initial_duties;EstateConstruction construction; };
struct EstateCatalog { OrderedTable<EstateDefinition> estates; };
struct ItemQuantity { StableId item_id;Integer quantity=0;FieldOrder field_order{}; };
struct EstateProductionFact { StableId estate_id;Integer tick=0;StableId owner_actor_id,manager_actor_id;std::vector<StableId> workers,guards,fed,unfed;Integer farm_level=0,palisade_level=0;ItemQuantity consumed,produced;Integer lost_output=0,defense=0;std::vector<StableId> reasons;FieldOrder field_order{}; };
struct EstateFund { StableId estate_id;Integer quantity=0;FieldOrder field_order{}; };
struct EstateAppoint { StableId estate_id,manager_actor_id;FieldOrder field_order{}; };
struct EstateAssign { StableId estate_id,resident_actor_id,duty;FieldOrder field_order{}; };
struct EstateTransfer { StableId estate_id,item_id;Integer quantity=0;StableId source_id;bool deposit=false,source_id_present=false;FieldOrder field_order{}; };
// 建造与借调沿用原命令参数；借调只引用已有真人和永久来源。
struct EstateBuild { StableId estate_id,facility;FieldOrder field_order{}; };
struct EstateDetachment { StableId estate_id;std::vector<StableId> actor_ids;StableId source_id;bool returning=false;FieldOrder field_order{}; };
using EstateCommand=std::variant<EstateFund,EstateAppoint,EstateAssign,EstateTransfer,EstateBuild,EstateDetachment>;
// 冻结建设条款由作者配置逐级计算，资产只保存引用。
struct EstateBuildTerms { StableId estate_id,facility;Integer from_level=0,to_level=0,duration=0,money_cost=0;StableId material_item_id;Integer material_quantity=0;StableId payer_wallet_id,payee_wallet_id,inventory_id,owner_actor_id,place_id;FieldOrder field_order{}; };
struct EstateBuildAction { EstateBuildTerms terms;FieldOrder field_order{}; };
struct EstateConstructionFact { StableId kind="estate_construction",phase="started",actor_id;Integer start_tick=0,completed_tick=-1;bool success=false;EstateBuildTerms terms;FieldOrder field_order{}; };
// 原永久外部来源的显式载荷，不用动态字典绕过公共所有权。
struct EstateMusterSource { StableId kind="estate_muster",estate_id;FieldOrder field_order{}; };
struct PublicEstateTransferSource { StableId kind="public_estate_transfer",estate_id,actor_id,item_id;Integer quantity=0;Owner source_owner;FieldOrder field_order{}; };
// 军事来源和军团成员由同候选适配器读取，空回调遇到相关请求明确失败。
struct EstateAccess { std::function<std::optional<StableId>(const StableId&)> military_muster_estate;std::function<StableId(const StableId&)> army_id;const PrivateShopWorkerCommitmentRead* private_shop_staff=nullptr;const DynamicStepInput* private_shop_phase=nullptr; };
struct EstateCashTerms { StableId from_wallet_id,to_wallet_id;Integer quantity=0;FieldOrder field_order{}; };
struct EstateTransferTerms { StableId from_inventory_id,to_inventory_id,item_id;Integer quantity=0;std::vector<Owner> owners;Owner new_owner;StableId source_id;FieldOrder field_order{}; };
struct EstateQuote { EstateCommand command;std::optional<EstateCashTerms> cash;std::optional<EstateTransferTerms> transfer;Integer duration=0;std::optional<EstateBuildTerms> terms;std::vector<StableId> participants,actor_ids; };
struct EstateManagementFact { StableId kind,estate_id,actor_id;Integer tick=0;EstateCommand command;std::optional<EstateCashTerms> cash;std::optional<EstateTransferTerms> transfer;FieldOrder field_order{}; };
using EstateFact=std::variant<EstateProductionFact,EstateManagementFact,EstateConstructionFact,EstateMusterSource,PublicEstateTransferSource>;
struct EstateState { OrderedTable<EstateRecord> estates;OrderedTable<EstateFact> facts;OrderedTable<EstateBuildAction> actions; };
struct EstateEffects { bool applied=false;std::vector<StableId> events; };
struct EstateReceipt { StableId estate_id,fact_id;std::optional<std::vector<StableId>> actor_ids; };
struct EstateView { EstateRecord estate;std::vector<StableId> available_workers,available_guards;Integer defense=0,capacity=0;std::optional<EstateProductionFact> last_production; };
struct EstateBuildResult { bool success=false,already_completed=false;StableId code; };
Outcome<EstateCatalog> decode_estate_catalog(const BoundaryValue& data,const CommonCatalog&);
Outcome<EstateState> decode_estate_seed(const BoundaryValue& state);
class ProjectionWriter;
Status emit_estate_state_fields(ProjectionWriter&,const EstateState&);
Status emit_estate_fact_fields(ProjectionWriter&,const EstateFact&);
Status emit_estate_action_fields(ProjectionWriter&,const EstateBuildAction&);
Status emit_estate_fact_payload_fields(ProjectionWriter&,const CommonStore&,const EstateState&,const FactRecord&);
Status emit_estate_action_payload_fields(ProjectionWriter&,const CommonStore&,const EstateState&,const ActionCoreView&);
Status emit_estate_build_terms_fields(ProjectionWriter&,const EstateBuildTerms&);
Status emit_estate_command_fields(ProjectionWriter&,const EstateCommand&);
Status emit_estate_quote_fields(ProjectionWriter&,const EstateQuote&);
BoundaryValue encode_estate_state(const EstateState&);
Status validate_estate(const CommonStore&,const EstateState&,const EstateCatalog&,const CommonCatalog&,const EstateAccess* access=nullptr);
Outcome<EstateEffects> step_estate(CommonStore&,EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,Integer finish_tick,const std::vector<StableId>& hold={},const EstateAccess* access=nullptr);
// 既有庄园页有限读取接口；观察者固定为唯一共同metadata中的当前玩家。
bool estate_available(const CommonStore&,const EstateState&,const EstateCatalog&,const StableId& estate_id);
Outcome<BoundaryValue> query_estate_page(const CommonStore&,const EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,const StableId& estate_id,const StableId& tab="people");
Outcome<BoundaryValue> query_estate_inputs(const CommonStore&,const EstateState&,const EstateCatalog&,const StableId& estate_id);
Outcome<EstateView> estate_view(const CommonStore&,const EstateState&,const EstateCatalog&,const ActivityRead&,const StableId& estate_id);
bool estate_construction_pending(const EstateState&,const StableId& estate_id);
Outcome<EstateQuote> preview_estate(const CommonStore&,const EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,const StableId& actor,const EstateCommand&,const EstateAccess* access=nullptr,const StableId& own_action={});
Outcome<EstateReceipt> apply_estate(CommonStore&,EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,const StableId& actor,const EstateQuote&,const EstateAccess* access=nullptr);
Outcome<EstateCommand> decode_estate_command(const StableId& kind,const BoundaryValue& params);
Outcome<EstateCommand> decode_estate_command(const StableId& kind,const BoundaryValue& params,ScopedCheckpointJsonDecode&);
StableId estate_command_kind(const EstateCommand&);
BoundaryValue encode_estate_command(const EstateCommand&);
Outcome<BoundaryValue> encode_estate_quote(const EstateQuote&);
// 同原字段writer，只发当前object字段，不构造中间树。
class ProjectionWriter;
Status emit_estate_receipt_fields(ProjectionWriter&,const EstateReceipt&);
BoundaryValue encode_estate_receipt(const EstateReceipt&);
Outcome<std::vector<StableId>> preview_estate_muster(const CommonStore&,const EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,const StableId& actor,const StableId& estate,const std::vector<StableId>& people);
Status begin_estate_build(CommonStore&,EstateState&,const EstateCatalog&,const CommonCatalog&,const ActivityRead&,const ActionCoreView&,const EstateQuote&,const EstateAccess* access=nullptr);
Outcome<EstateBuildResult> finish_estate_build(CommonStore&,EstateState&,const EstateCatalog&,const ActivityRead&,const ActionCoreView&);
Status validate_estate_action(const CommonStore&,const EstateState&,const EstateCatalog&,const ActionCoreView&,bool finishing=false);
Status validate_estate_construction_fact(const CommonStore&,const EstateState&,const EstateCatalog&,const StableId&,const EstateConstructionFact&);
Status erase_estate_action_payload(EstateState&,const StableId&);
EstateBuildTerms estate_build_terms(const EstateDefinition&,const EstateRecord&,const StableId& facility);
bool same_estate_build_terms(const EstateBuildTerms&,const EstateBuildTerms&);
bool estate_muster_source(const CommonStore&,const EstateState&,const StableId& source,const StableId& estate,const EstateAccess*);
Status validate_estate_transfer_source(const CommonStore&,const EstateState&,const CommonCatalog&,const StableId& estate,const StableId& actor,const EstateTransferTerms&);
EstateBuildTerms decode_estate_build_terms(const BoundaryValue&,const StableId& path);
BoundaryValue encode_estate_build_terms(const EstateBuildTerms&);
Status visit_estate_checkpoint(CodecVisitor&,EstateState&);
} // 所有写入留在唯一候选，中央只负责行动生命周期和有限跨域接线。
