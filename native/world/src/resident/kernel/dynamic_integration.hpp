// 动态领域中央接缝只组合真实命令与阶段，不拥有另一份领域规则。
#pragma once
#include "world_bundle.hpp"
namespace hanmo::resident {
bool dynamic_command_kind(const StableId&);
Outcome<CommandPayload> decode_dynamic_command(const StableId&,const BoundaryValue&);
Status preview_dynamic_command(const WorldBundle&,const StableId&,const CommandPayload&);
Outcome<OrganizationEffects> apply_dynamic_command(WorldBundle&,const StableId&,const CommandPayload&);
Status bootstrap_dynamic_world(WorldBundle&);
Status reconcile_dynamic_director_registry(WorldBundle&);
DynamicStepInput capture_dynamic_step(const WorldBundle&,Integer to_tick);
Status validate_dynamic_provision_owners(const WorldBundle&,Integer tick);
Status finish_dynamic_boundary(WorldBundle&,const DynamicStepInput&);
Status apply_director_proposal(WorldBundle&,const DecisionProposal&,Integer tick);
Status run_dynamic_decisions(WorldBundle&,Integer tick);
bool director_routes_actor(const WorldBundle&,const StableId&);
} // 领域错误保留原类型；普通拒绝由中央typed写集原子回退。
