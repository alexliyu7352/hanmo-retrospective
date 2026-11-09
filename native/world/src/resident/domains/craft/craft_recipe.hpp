// E12/E16, C01-C02. Immutable authored professional inputs, never stock or XP.
#pragma once
#include "item_service.hpp"
#include "resident/kernel/work_execution.hpp"
namespace hanmo::resident {
struct CraftRecipeRef {StableId recipe_id;Integer recipe_version=0;};
enum class CraftPurpose {civilian_tool,weapon,gift};
enum class CraftMode {automatic,local};
enum class CraftStageTrigger {after_first_work_tick,at_work_progress,at_completion};
// These finite handlers only consume the frozen material rows and certify the
// original professional stage. They cannot run content scripts or modify actors.
enum class CraftStageKind {material_process,final_inspection};
struct CraftStageMaterial {Integer material_index=0,quantity=0;};
struct CraftStageSpec {
 Integer ordinal=0;CraftStageKind stage_kind=CraftStageKind::material_process;
 CraftStageTrigger trigger=CraftStageTrigger::at_work_progress;
 Integer required_work_ticks=0;std::vector<CraftStageMaterial> inputs;
 StableId handler_id;
};
struct CraftMaterialOption {
 StableId definition_id;std::optional<Integer> minimum_quality;
 bool require_properties=false;std::vector<StableId> required_fit_tag_ids;
};
struct CraftMaterialRequirement {
 Integer quantity=0;bool base_material=false;
 std::vector<CraftMaterialOption> alternatives;
};
struct CraftOutputDefinition {
 StableId definition_id;Integer quantity=0;bool unique_instance=false;
 Integer max_durability=0;StableId repair_policy_id;
 StableId property_policy_id;Integer property_policy_version=0;
 std::vector<StableId> fit_tag_ids;
};
struct CraftRecipeVariant {
 StableId variant_id;CraftOutputDefinition output;
 // Every listed capability must have a real compiled output consumer.
 std::vector<StableId> required_output_capabilities;
};
// 材料适配由作者对完整替代组合明示，不在运行期自选加总或最大值。
struct CraftMaterialFitRule {std::vector<Integer> alternative_indices;Integer material_fit=0;};
struct CraftRecipeDef {
 StableId recipe_id;Integer recipe_version=0;StableId required_skill_id;
 Integer minimum_skill_level=0,minimum_equipment_level=0;
 StableId learning_policy_ref,workstation_slot_kind;
 std::vector<CraftPurpose> allowed_purposes;
 Integer normal_work_ticks=0;std::vector<CraftMaterialRequirement> materials;
 std::vector<CraftMaterialFitRule> material_fit_rules;
 std::vector<CraftStageSpec> stages;std::vector<CraftRecipeVariant> variants;
 Integer minimum_quality=0,maximum_quality=0;
 StableId pace_policy_ref,automatic_quality_policy_ref;
 // Fees remain actual original accepted S5/S3 terms, not a second wallet.
 StableId fee_policy_ref,cancellation_policy_ref,result_owner_policy_ref;
};
struct CraftRecipeCatalog {OrderedTable<CraftRecipeDef> recipes;};
struct CraftQualityRange {Integer minimum=0,maximum=0;};
struct CraftBrief {
 StableId brief_id;SubjectRef requester_ref,maker_ref;CraftPurpose purpose_kind=CraftPurpose::civilian_tool;
 StableId target_item_definition_id;CraftRecipeRef recipe_ref;StableId variant_id;
 CraftQualityRange required_quality_range;
 std::optional<SourceRef> masterwork_eligibility_ref,commercial_terms_ref;
 Integer design_revision=0;
};
// Pure native shape/source-definition admission. Output consumer capability
// availability is separately verified by the actual compiled domain registry.
Status validate_craft_recipe_catalog(const CraftRecipeCatalog&,const CommonCatalog&,const ItemServiceCatalog&);
Outcome<const CraftRecipeDef*> read_craft_recipe(const CraftRecipeCatalog&,const CraftRecipeRef&);
Outcome<const CraftRecipeVariant*> read_craft_recipe_variant(const CraftRecipeDef&,const StableId&);
Outcome<Integer> read_craft_material_fit(const CraftRecipeDef&,const std::vector<Integer>& actual_alternative_indices);
Status validate_craft_brief_definition(const CraftBrief&,const CraftRecipeCatalog&);
Outcome<Integer> craft_required_normal_thirds(const CraftRecipeDef&);
// Reached ordinals are derived from K2's actual integral gate result; no time is
// advanced here and no caller-provided service quantity is credited.
Outcome<std::vector<Integer>> read_craft_reached_stage_ordinals(const CraftRecipeDef&,Integer next_stage_ordinal,const CraftWorkGateResult&);
class ProjectionWriter;class CodecVisitor;
Outcome<CraftRecipeCatalog> decode_craft_recipe_catalog(const BoundaryValue&,const CommonCatalog&,const ItemServiceCatalog&);
Outcome<BoundaryValue> encode_craft_recipe_catalog(const CraftRecipeCatalog&);
Status visit_craft_recipe_catalog(CodecVisitor&,CraftRecipeCatalog&);
Status emit_craft_recipe_catalog(ProjectionWriter&,const StableId& name,const CraftRecipeCatalog&);
}
