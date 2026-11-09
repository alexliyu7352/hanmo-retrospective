// 转移前与历史验证共用唯一转换，保本域真实主体和容器角色。
#pragma once
#include "politics_takeover_state.hpp"
#include "resident/domains/local_economy/takeover/responsibility_source.hpp"
#include "resident/domains/tasks/task_public_account_dependencies.hpp"
namespace hanmo::resident {
struct OccupationAssetManifest;
PoliticalResponsibilityRef political_economic_responsibility_ref(const EconomicResponsibilityRef&);
Outcome<EconomicResponsibilityRef> economic_political_responsibility_ref(const PoliticalResponsibilityRef&);
Outcome<PoliticalOriginalResponsibility> political_original_economic_source(const EconomicResponsibilitySource&,const OccupationAssetManifest&);
Outcome<PoliticalOriginalResponsibility> political_original_task_source(const TaskPublicAccountDependency&,const OccupationAssetManifest&);
void canonicalize_political_original_responsibility(PoliticalOriginalResponsibility&);
}
