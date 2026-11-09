// 生前自愿接续的真实关系依据；本人许可、稳定可玩处境与资产交割由原各自程序另核。
#pragma once
#include "family_model.hpp"
#include "biological_legacy_basis.hpp"
#include "family_apprenticeship.hpp"
#include "collateral_control_state.hpp"
namespace hanmo::resident {
enum class VoluntaryRelationKind { adoption, biological_child, accepted_disciple, recognized_collateral };
struct VoluntaryRelationSource {
    WorldIdentity source_identity;
    VoluntaryRelationKind kind=VoluntaryRelationKind::adoption;
    StableId predecessor_actor_id,successor_actor_id,relation_fact_id,before_fact_id,skill_id;
    Integer observed_tick=0,established_tick=0,predecessor_birth_tick=0,successor_birth_tick=0;
    // Installed原亲代不是Common事实；保明确标签，不能填一个伪relation_fact_id。
    std::optional<BiologicalChildOrigin> biological;
};
// 同一完整已验Common/Creation/Birth/Lineage/Life和所选关系账，真实军史死亡读；不反入World。
struct VoluntaryRelationContext {
    const CommonStore& common;
    const CommonCatalog& catalog;
    const FamilyState& family;
    const FamilyLineageCatalog* lineage=nullptr;
    const FamilyBirthState* births=nullptr;
    const FamilyApprenticeshipState* apprenticeships=nullptr;
    const CollateralControlState* collateral=nullptr;
    FamilyDeathOriginRead death_origin;
};
// 调用方明确给关系种类和原ID；亲生分支exact_relation必须为空，由真实Installed/Birth给原源。
// 不按姓名/同姓扫描候选，不读取任何Legacy死后同意；返回本身不授控制、财产或关系公开。
Outcome<VoluntaryRelationSource> read_current_voluntary_relation(const VoluntaryRelationContext&,VoluntaryRelationKind,const StableId& predecessor,const StableId& successor,const StableId& exact_relation={});
// 仅原family:/family_succession_consent或family_voluntary_succession/family实际头。
// 调用方须先核自己的原payload.tick；后来的退出/死亡保留此前合法关系与生前资格。
Outcome<VoluntaryRelationSource> read_voluntary_relation_before_fact(const VoluntaryRelationContext&,VoluntaryRelationKind,const StableId& predecessor,const StableId& successor,const StableId& exact_relation,Integer tick,const StableId& actual_fact);
}
