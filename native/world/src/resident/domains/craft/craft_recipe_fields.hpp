#pragma once
#include "craft_recipe.hpp"
#include "craft_quality.hpp"
namespace hanmo::resident::craft_fields {
template<class C>void fields(C&c,SourceRef&v){c("domain",v.domain);c("kind",v.kind);c("id",v.id);}
template<class C>void fields(C&c,SubjectRef&v){c("kind",v.kind);c("id",v.id);}
template<class C>void fields(C&c,CraftRecipeRef&v){c("recipe_id",v.recipe_id);c("recipe_version",v.recipe_version);}
template<class C>void fields(C&c,CraftStageMaterial&v){c("material_index",v.material_index);c("quantity",v.quantity);}
template<class C>void fields(C&c,CraftStageSpec&v){c("ordinal",v.ordinal);c("stage_kind",v.stage_kind);c("trigger",v.trigger);c("required_work_ticks",v.required_work_ticks);c("inputs",v.inputs);c("handler_id",v.handler_id);}
template<class C>void fields(C&c,CraftMaterialOption&v){c("definition_id",v.definition_id);c("minimum_quality",v.minimum_quality);c("require_properties",v.require_properties);c("required_fit_tag_ids",v.required_fit_tag_ids);}
template<class C>void fields(C&c,CraftMaterialRequirement&v){c("quantity",v.quantity);c("base_material",v.base_material);c("alternatives",v.alternatives);}
template<class C>void fields(C&c,CraftMaterialFitRule&v){c("alternative_indices",v.alternative_indices);c("material_fit",v.material_fit);}
template<class C>void fields(C&c,CraftOutputDefinition&v){c("definition_id",v.definition_id);c("quantity",v.quantity);c("unique_instance",v.unique_instance);c("max_durability",v.max_durability);c("repair_policy_id",v.repair_policy_id);c("property_policy_id",v.property_policy_id);c("property_policy_version",v.property_policy_version);c("fit_tag_ids",v.fit_tag_ids);}
template<class C>void fields(C&c,CraftRecipeVariant&v){c("variant_id",v.variant_id);c("output",v.output);c("required_output_capabilities",v.required_output_capabilities);}
template<class C>void fields(C&c,CraftRecipeDef&v){c("recipe_id",v.recipe_id);c("recipe_version",v.recipe_version);c("required_skill_id",v.required_skill_id);c("minimum_skill_level",v.minimum_skill_level);c("minimum_equipment_level",v.minimum_equipment_level);c("learning_policy_ref",v.learning_policy_ref);c("workstation_slot_kind",v.workstation_slot_kind);c("allowed_purposes",v.allowed_purposes);c("normal_work_ticks",v.normal_work_ticks);c("materials",v.materials);c("material_fit_rules",v.material_fit_rules);c("stages",v.stages);c("variants",v.variants);c("minimum_quality",v.minimum_quality);c("maximum_quality",v.maximum_quality);c("pace_policy_ref",v.pace_policy_ref);c("automatic_quality_policy_ref",v.automatic_quality_policy_ref);c("fee_policy_ref",v.fee_policy_ref);c("cancellation_policy_ref",v.cancellation_policy_ref);c("result_owner_policy_ref",v.result_owner_policy_ref);}
template<class C>void fields(C&c,CraftRecipeCatalog&v){c("recipes",v.recipes);}
template<class C>void fields(C&c,CraftQualityRange&v){c("minimum",v.minimum);c("maximum",v.maximum);}
template<class C>void fields(C&c,CraftBrief&v){c("brief_id",v.brief_id);c("requester_ref",v.requester_ref);c("maker_ref",v.maker_ref);c("purpose_kind",v.purpose_kind);c("target_item_definition_id",v.target_item_definition_id);c("recipe_ref",v.recipe_ref);c("variant_id",v.variant_id);c("required_quality_range",v.required_quality_range);c("masterwork_eligibility_ref",v.masterwork_eligibility_ref);c("commercial_terms_ref",v.commercial_terms_ref);c("design_revision",v.design_revision);}
template<class C>void fields(C&c,CraftQualityInputs&v){c("skill_level",v.skill_level);c("equipment_level",v.equipment_level);c("material_fit",v.material_fit);c("recipe_minimum",v.recipe_minimum);c("recipe_maximum",v.recipe_maximum);c("process_policy_ref",v.process_policy_ref);c("rush_policy_ref",v.rush_policy_ref);}
template<class C>void fields(C&c,CraftQualityValue&v){c("performance",v.performance);c("rush_penalty",v.rush_penalty);c("quality_score",v.quality_score);}
}
