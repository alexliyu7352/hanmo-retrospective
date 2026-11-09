// 原物理5的有限家庭A/B尾，不向它伪补物理6 accepted_command。
#pragma once
#include "resident/domains/director/director_subject2.hpp"
namespace hanmo::resident {
// 只作 typed 路由分类；实际本人许可仍由原 apply/真实 command owner 核验。
bool is_family_director_post_proposal5(
    const SubjectDecisionProposal2 &) noexcept;
// 只在原 adoption 已真插入之后：反核同根借行、原A/B binding和真实B头。
// 通过仅准进入中央原 finalizer；该尾仍须反读真实B命令并完成P10/S0，不授收知。
Status validate_family_director_post_adoption_tail5(
    const CommonStore &, const DirectorState &,
    const SubjectDecisionProposal2 &, const DirectorAdoptionResult2 &,
    const WorldMoment &);
// 只纳同LC01的3个实际旁支A和原B，不把任意Actor命令赋予新5尾。
bool is_family_director_personal_proposal5(
    const SubjectDecisionProposal2 &) noexcept;
// A须原outcome=A、真实新A头及binding；终局A另核原proposal在其前。
// 家庭资格/身体/本人意愿/command-result仍由中央同候选full来源尾反验。
Status validate_family_director_personal_adoption_tail5(
    const CommonStore &, const DirectorState &,
    const SubjectDecisionProposal2 &, const DirectorAdoptionResult2 &,
    const WorldMoment &);
} // namespace hanmo::resident
