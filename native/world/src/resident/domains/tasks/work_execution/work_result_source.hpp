// S3 原工作身份与成果引用的唯一轻型合同。只用于寻址，不持工作进度、费用或权限。
#pragma once
#include "resident/model/commitment_ref.hpp"
namespace hanmo::resident {
enum class WorkKind {patrol,agronomy_supervision,industry_supervision,construction_supervision};
struct WorkProgramAuthorizationScope {SourceRef program_ref;StableId target_line_id;TermsRevisionRef terms_revision;};
struct WorkAuthorizationRef {SourceRef program_ref;StableId target_line_id;TermsRevisionRef terms_revision;StableId accepted_task_id;};
struct LegacyAppeaseWork {StableId work_completion_fact_id;};
struct PatrolResult {StableId fact_id;};
struct SupervisedResult {StableId fact_id;};
using WorkResultSource=std::variant<LegacyAppeaseWork,PatrolResult,SupervisedResult>;
bool same_work_result_source(const WorkResultSource&,const WorkResultSource&);
}
