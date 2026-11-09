// 实际照护Action只占照护者本人；子女不是劳动/支付参与者，预付私粮到完成时才消耗。
#pragma once
#include "family_birth_creation.hpp"
namespace hanmo::resident {
struct ChildCareQuote { WorldIdentity identity;StableId accepted_id,child_actor_id,caregiver_actor_id,place_id;Integer duration=0,period_index=0,food_per_period=0; };
struct ChildCareEffect { StableId fact_id,child_actor_id,code;bool success=false,already_completed=false; };
Outcome<ChildCareQuote> preview_child_care(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const StableId& caregiver,const StableId& accepted_id);
// CommonAction必须已由原调度器建立并占用本人。本域失败由原整个命令候选回滚该开工。
Status start_child_care_action(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&,const ChildCareQuote&);
// 跟原family_complete一样，只在clock_commit之后完成；外层随后释放同一CommonAction。
Outcome<ChildCareEffect> finish_child_care_action(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&);
// 本人可以明确中止本次照护；未消费原粮仍属于原资助计划，不伪造已喂养或删除儿童。
Outcome<ChildCareEffect> cancel_child_care_action(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& actor,const ActionCoreView&);
// 整个World候选或外层Common保存点专用；调用方必须备份全部typed写集，任一失败整体丢弃/回滚。
// 不自行开启或提交保存点；不得把失败的候选继续用于后续命令。
// 本入口写Common和FamilyBirth，其他领域只读。
Status start_child_care_action_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&,const ChildCareQuote&);
Outcome<ChildCareEffect> finish_child_care_action_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&);
Outcome<ChildCareEffect> cancel_child_care_action_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const StableId& actor,const ActionCoreView&);
// 当前只支持实际已登记死亡使原照护中断；离场等来源未接时明确拒绝，不造通用失败事实。
Outcome<ChildCareEffect> interrupt_child_care_action(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&);
Outcome<ChildCareEffect> interrupt_child_care_action_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ActionCoreView&);
}
