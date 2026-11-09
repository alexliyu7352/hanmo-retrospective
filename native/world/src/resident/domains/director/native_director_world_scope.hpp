// 同一 Complete 世界内的 Director 实际时刻拥有域；只组合原读口与原命令写者。
#pragma once
#include "director_native_lease.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
#include "resident/kernel/native_complete_world_registry.hpp"
#include <memory>
namespace hanmo::resident {
struct FamilyDirectorPostProposalContext5;
struct PublicSupplyInspectionReadContext;
// 只由中央已发布的同世界拥有图填入；shared_ptr 可别名借原子图中的具名对象，
// 必须保活真实对象，不能把工厂局部 context 交成无所有者的裸指针。
struct NativeDirectorWorldScopeDependencies {
  const NativeCompleteWorldCoreSources &core;
  std::shared_ptr<const PoliticalExactReadLease> authority_owner;
  std::shared_ptr<const NativeTaskWorkSources> work;
  std::shared_ptr<const NativeHouseholdServiceLease> household;
  std::shared_ptr<const DirectorPoliticalContext> politics;
  std::shared_ptr<const DirectorDiplomaticContext> diplomacy;
  std::shared_ptr<const EconomicSupplyPolicyRead> issued_supply;
  std::shared_ptr<const TaskResponsibilityDirectorContext> task_responsibility;
  std::shared_ptr<const FamilyDirectorPostProposalContext5> family_post;
  std::shared_ptr<const DirectorInstitutionalSourceRead2> institutions;
  DirectorConversationResponse conversation_response;
  // 选择由中央原编译目录给出；未选不构造渠道，已选必须提供原来源与写端。
  bool propagation_selected = false;
  std::shared_ptr<const PropagationReadContext> propagation;
  // 同一原世界公开告示来源的别名拥有指针，不能引用工厂临时 context。
  std::shared_ptr<const PublicSupplyInspectionReadContext>
      public_supply_inspection;
};
struct NativeDirectorWorldScopeWriteDependencies {
  NativeDirectorWorldScopeDependencies read;
  std::shared_ptr<DirectorHouseholdRoutineWrite2> household_routine;
  std::shared_ptr<DirectorInstitutionalWrite2> institutions;
  std::shared_ptr<PropagationChannelWrite> propagation;
};
// 每次实际收集/复评重新取得；读端绝不构造写端，写端只借当前未发布候选。
// 外层仍保活 core，返回 scope 保活其 P10 快照、真实前序 lease 与全部局部
// context。
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_director_world_read_scope(const WorldBundle &,
                                      NativeDirectorWorldScopeDependencies,
                                      const WorldMoment &);
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_director_world_write_scope(
    WorldBundle &, NativeDirectorWorldScopeWriteDependencies,
    const WorldMoment &);
} // namespace hanmo::resident
