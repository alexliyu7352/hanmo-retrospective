// B1/B5 actual due executions belong to FamilyBirthState, after its original
// seven roots. A schedule borrows the accepted arrangement; it allocates no ID.
#pragma once
#include "birth_arrangement_source.hpp"
#include "death_ref.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct ScheduledBirthSource {
 StableId arrangement_id;Integer terms_version=0,due_tick=0;WorldMoment occurrence;
 SourceRef source_ref;ActorFoundationRuleRef selected_policy_ref;
};
struct BirthResponsibilityDisposition {
 enum class Kind{no_outstanding_obligation,pending,closed};Kind kind=Kind::no_outstanding_obligation;
 std::vector<SourceRef> original_refs;
};
struct BirthCancellationResult {
 StableId result_id,arrangement_id;Integer terms_version=0;
 StableId reason="delivery_actor_dead";DeathRef death_ref;WorldMoment effective_at;
 std::vector<SourceRef> original_responsibility_refs;
 BirthResponsibilityDisposition closed_or_pending_disposition;
};
struct BirthCareDisposition {
 enum class Kind{accepted_care,care_need_opened};Kind kind=Kind::accepted_care;
 SourceRef care_assignment_ref;StableId care_need_ref;
};
struct BornBirthDueResult {
 SourceRef birth_fact_ref;StableId child_actor_id;SourceRef location_result_ref;
 std::optional<StableId> transport_binding_ref;BirthCareDisposition care;
 SourceRef population_transition_ref,provision_binding_ref;
 std::optional<SourceRef> carrier_load_result_ref;
};
struct BirthDueExecution {ScheduledBirthSource source;std::variant<BirthCancellationResult,BornBirthDueResult> result;};
struct BirthAlreadyTerminal {SourceRef existing_birth_or_cancel_result_ref;};
using BirthAtLocationOutcome=std::variant<BirthAlreadyTerminal,BirthCancellationResult,BornBirthDueResult>;
class ProjectionWriter;struct FamilyBirthState;struct BirthArrangementAccess;
// The Born source owner must re-read original placement, population, provision,
// care and load results. Merely decoding their refs grants no permission.
class BirthDueBornSourceRead {
public:virtual ~BirthDueBornSourceRead()=default;
 virtual bool bound_to(const CommonStore&,const FamilyBirthState&)const noexcept=0;
 virtual Status validate_actual_born_result(const ScheduledBirthSource&,const BornBirthDueResult&)const=0;
};
Outcome<ScheduledBirthSource> read_scheduled_birth_source(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const StableId& accepted_id);
Status validate_birth_due_executions(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&);
// Same outer world candidate owns Common and Birth. Only the real unused
// reservation is released. The death branch performs no RNG/Actor allocation.
Outcome<BirthAtLocationOutcome> cancel_scheduled_birth_after_actual_death_in_candidate(CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,FamilyBirthState&,const BirthArrangementAccess&,const ScheduledBirthSource&,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
struct BornCharacterCreationContext;struct BornCharacterCreated;
// 原 Birth4 创建及全部真实跨域结果完成后的唯一正向采用点。调用方在最后
// 一次前驱写入后重取 birth_due 印章，原创建/位置 token 仍交外层 capture。
// 不分配第二出生头、不生成缺失的供养/照护/载荷源；失败撤整个 World 候选。
Outcome<BirthAtLocationOutcome> adopt_born_birth_due_in_candidate(
 BornCharacterCreationContext&,const ScheduledBirthSource&,const BornCharacterCreated&,
 const BornBirthDueResult&,const ReadStamp&);
// Stable install/restore/publish only. The private due batch can contain other
// due arrangements while it consumes them in original accepted-source order.
Status validate_birth_due_completed_through(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&);
class ScheduledBirthDueVisitor {public:virtual ~ScheduledBirthDueVisitor()=default;virtual Status due(const ScheduledBirthSource&)=0;};
Status visit_scheduled_births_due_at(const CommonStore&,const InteractionCatalog&,const FamilyCatalog&,const BirthArrangementCatalog&,const FamilyBirthState&,const BirthArrangementAccess&,const WorldMoment&,ScheduledBirthDueVisitor&);
Status visit_birth_due_execution(CodecVisitor&,BirthDueExecution&);
Status emit_birth_due_execution_fields(ProjectionWriter&,const BirthDueExecution&);
Outcome<BirthDueExecution> decode_birth_due_execution(const BoundaryValue&);
}
