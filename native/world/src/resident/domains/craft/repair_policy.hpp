#pragma once
#include "repair_model.hpp"
#include "item_service.hpp"
namespace hanmo::resident {
Status validate_repair_policy_catalog(const RepairPolicyCatalog&,const CommonCatalog&);
Outcome<const RepairPolicy*> read_repair_policy(const RepairPolicyCatalog&,const RepairPolicyRef&);
struct RepairConditionTarget {Integer durability=0,max_durability=0;};
Outcome<RepairConditionTarget> read_repair_condition_target(const RepairPolicy&,const ItemInstance&,Integer adopted_maximum);
class ProjectionWriter;class CodecVisitor;
Outcome<RepairPolicyCatalog> decode_repair_policy_catalog(const BoundaryValue&,const CommonCatalog&);
Outcome<BoundaryValue> encode_repair_policy_catalog(const RepairPolicyCatalog&);
Status emit_repair_policy_catalog(ProjectionWriter&,const StableId&,const RepairPolicyCatalog&);
}
