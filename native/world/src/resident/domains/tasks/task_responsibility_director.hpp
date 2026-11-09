// 特殊已受领责任只提出原Travel/Talk/Envoy命令；权源、资源与同意仍由原领域持有。
#pragma once
#include "task_responsibility_continuation.hpp"
#include "task_authored_continuation.hpp"
namespace hanmo::resident {
struct InteractionState;struct InteractionCatalog;struct InteractionAccess;
// 同候选预览不提交事务；真正采用仍由中央原命令重报价，不能把提案当授权。
using TaskEnvoyContinuationPreview=std::function<Status(const StableId& actor,
    EnvoyTaskActionKind,const StableId& task_id,const StableId& provider,Integer expected_revision)>;
struct TaskResponsibilityDirectorContext {
    TaskResponsibilityContinuationRead sources;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
    const OrganizationRead& organization;
    TaskEnvoyContinuationPreview preview_envoy;
};
// v1与禁用保持原行为；显式v2在普通定义解析前分流本人已受领的两种真实侧账。
Outcome<std::optional<DecisionProposal>> propose_task_responsibility_continuation(
    const CommonStore&,const TaskState&,const CommonCatalog&,const TravelCatalog&,
    const KnownSubjectView&,const StableId& task_id,const DecisionProposal&,
    const TaskAuthoredContinuationCatalog*,const TaskResponsibilityDirectorContext*);
}
