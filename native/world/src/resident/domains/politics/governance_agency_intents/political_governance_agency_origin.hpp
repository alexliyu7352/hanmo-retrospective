// 显式agency2的永久NPC来源只存作者意图和实际签字人，不复制公职或原批准。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
enum class PoliticalGovernanceAgencyAuthoredStep {offer=1,answer=2,issue=3};
struct PoliticalGovernanceAgencyAuthoredOrigin {
 StableId intent_id,director_policy_id,actor_id;
 PoliticalGovernanceAgencyAuthoredStep step=PoliticalGovernanceAgencyAuthoredStep::offer;
};
}
