// 已有治理业务读口贯穿整次同步preview；资金与受领仍由原命令在完整候选中执行。
#include "political_governance_director_world.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
// 新模板无自身writer，保原原生目录、政治Task及公共赈济的实际启用条件。
Outcome<std::optional<DecisionProposal>> propose_political_governance_director_world(const WorldBundle& w,const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){
 if(!w.catalog->director||!w.catalog->director->governance_intents||!policy.governance_intent_id||!w.governance_tasks_enabled()||!w.catalog->travel)return {false,{},common_failure("GovernanceIntentWorldProfileRequired",known.actor_id,ErrorKind::internal).error};const auto* intent=w.catalog->director->governance_intents->intents.find(*policy.governance_intent_id);if(!intent)return {false,{},common_failure("GovernanceIntentSourceMismatch",*policy.governance_intent_id,ErrorKind::internal).error};if(known.actor_id!=intent->issuer_actor_id&&known.actor_id!=intent->executor_actor_id)return {true,std::nullopt,{}};
 auto context=w.governance_read();return propose_director_governance_commission(context,*w.catalog->travel,known,policy,*intent,base,w.catalog->director->candidates_per_subject);
}
// 末位已有完整原治理/Task/赈济先验；不用计划状态倒推任何新的政治权限。
Status validate_political_governance_director_world(const WorldBundle& w){const bool enabled=w.catalog->director&&w.catalog->director->governance_intents;auto valid=validate_political_governance_intent_modules(enabled,w.catalog->identity.module_versions);if(!valid.ok)return valid;if(!w.catalog->director)return common_success();if(!enabled)return validate_political_governance_intent_director(*w.catalog->director,w.director,nullptr);if(!w.governance_tasks_enabled())return common_failure("GovernanceIntentWorldProfileRequired");auto context=w.governance_read();return validate_political_governance_intent_director(*w.catalog->director,w.director,&context);}
}
