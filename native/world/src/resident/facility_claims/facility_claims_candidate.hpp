// 仅原WorldBundle未发布候选阶段可调用；任何失败必须丢整个候选，不是独立可重试事务。
#pragma once
#include "facility_claims.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 不接任意可变FacilityClaimsState，唯一根必须是当前world.facility_claims；持久JSON接口不暴露此口。
// 原业务source已实际写完后采用；只撤重复全域复制，完整语义校验和最终publish门仍保留。
Status adopt_facility_claim_mutation_in_candidate(WorldBundle&,const FacilityClaimsCatalog&,const FacilitySlotRef&,
    const FacilityClaimMutationReceipt&,Integer expected_claim_revision,const WorldMoment&,
    const FacilityClaimSourceRead&,ExactArithmeticBudget={});
}
