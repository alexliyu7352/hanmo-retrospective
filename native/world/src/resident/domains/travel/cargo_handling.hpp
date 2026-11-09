#pragma once
#include "spatial_carrier.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct CargoHandlingLaborCoverage {
    MomentSpan span;Integer eligible_workers=0;std::vector<SourceRef> actual_coverage_sources;
    std::optional<SourceRef> ineligibility_source;
};
struct CargoHandlingLaborAdoption { std::vector<SourceRef> labor_binding_refs,participation_source_refs; };
struct CargoHandlingLaborChange { SourceRef actual_cause;WorldMoment at;bool resume=false; };
class CargoHandlingLaborRead {
public:
    virtual ~CargoHandlingLaborRead()=default;
    virtual Outcome<std::optional<CargoHandlingLaborChange>> read_pending_handling_labor_change(const CargoHandlingTerms&,const CargoHandlingExecution&,const WorldMoment&,const ReadStamp&)const=0;
    virtual Outcome<CargoHandlingLaborCoverage> read_handling_labor_coverage(const CargoHandlingTerms&,const CargoHandlingExecution&,const MomentSpan&,const ReadStamp&)const=0;
    virtual Status validate_handling_labor_history(const CargoHandlingTerms&,const CargoHandlingExecution&,const WorkUnits& actual_credited,const TimeScaleRef&,const WorldMoment&)const=0;
    virtual Outcome<std::vector<SourceRef>> read_handling_labor_release_sources(const CargoHandlingExecution&)const=0;
};
class CargoHandlingLaborWriter:public CargoHandlingLaborRead {
public:
    virtual Outcome<CargoHandlingLaborAdoption> reserve_handling_labor_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingExecution&,const ReadStamp&)=0;
    // 原P02只持真实半开参与/资格覆盖；不复制本域credited、不重复工资/供养。
    virtual Outcome<CargoHandlingLaborCoverage> settle_handling_labor_span_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingExecution&,const MomentSpan&,const ReadStamp&)=0;
    virtual Outcome<std::vector<SourceRef>> release_handling_labor_in_candidate(CommonStore&,const CargoHandlingExecution&,const SourceRef& actual_cause,const WorldMoment&)=0;
};
struct CargoHandlingStartSources { std::vector<SourceRef> actual_cost_sources; };
struct CargoHandlingTransferWait { SourceRef actual_waiting_source; };
struct CargoHandlingTransferReady {};
using CargoHandlingTransferDisposition=std::variant<CargoHandlingTransferReady,CargoHandlingTransferWait>;
struct CargoHandlingBusinessTransfer {
    std::vector<SourceRef> actual_transfer_sources,actual_resource_effects,actual_lot_flows,manifest_changes,original_business_results;
};
// 只注册原业务有限source kind；完整批次/权源/预约/位置和实际flow必须由原owner反读。
struct CargoHandlingTermination {CargoHandlingResultStatus status=CargoHandlingResultStatus::interrupted;SourceRef actual_cause;WorldMoment at;};
class CargoHandlingBusinessRead {
public:
    virtual ~CargoHandlingBusinessRead()=default;
    virtual Outcome<std::optional<CargoHandlingTermination>> read_pending_handling_termination(const CargoHandlingTerms&,const CargoHandlingExecution&,const WorldMoment&,const ReadStamp&)const=0;
    virtual Outcome<CargoHandlingTerms> prepare_actual_handling_terms(const SourceRef& business,Integer batch,CargoHandlingOperation,const WorldMoment&,const ReadStamp&)const=0;
    virtual Status validate_original_handling_terms(const CargoHandlingTerms&,const CargoHandlingRule&)const=0;
    virtual Status validate_actual_handling_start(const CargoHandlingTerms&,const CargoHandlingRule&,const ReadStamp&)const=0;
    virtual Outcome<CargoHandlingTransferDisposition> read_actual_transfer_disposition(const CargoHandlingTerms&,const CargoHandlingWorkResult&,const WorldMoment&,const ReadStamp&)const=0;
    virtual Status validate_actual_handling_result(const CargoHandlingTerms&,const CargoHandlingExecution&,const CargoHandlingResult&)const=0;
    // Pure original occurrence history through the supplied actual M: at
    // accepted_at it returns precisely the actual start callback sources.
    virtual Outcome<std::vector<SourceRef>> read_original_handling_costs(const CargoHandlingTerms&,const CargoHandlingExecution&,const WorldMoment&)const=0;
    virtual Outcome<WorldMoment> read_actual_handling_cause_moment(const SourceRef&)const=0;
};
class CargoHandlingBusinessWriter:public CargoHandlingBusinessRead {
public:
    virtual Outcome<CargoHandlingStartSources> adopt_handling_start_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingExecution&,const ReadStamp&)=0;
    // Common物理腿只在此原owner回调一次，S1自身不TransferLots。
    virtual Outcome<CargoHandlingBusinessTransfer> complete_actual_handling_transfer_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingExecution&,const CargoHandlingWorkResult&,const SourceRef& pending_result,const WorldMoment&,const ReadStamp&)=0;
    virtual Status interrupt_actual_handling_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingExecution&,CargoHandlingResultStatus,const SourceRef& actual_reason,const WorldMoment&,const ReadStamp&)=0;
};
class CargoHandlingFacilityRead {
public:virtual ~CargoHandlingFacilityRead()=default;
    virtual Status validate_handling_facility(const CargoHandlingTerms&,const CargoHandlingRule&,const MomentSpan&,const ReadStamp&)const=0;
    virtual Outcome<std::vector<SourceRef>> read_handling_facility_use_sources(const CargoHandlingExecution&)const=0;
    virtual Outcome<std::vector<SourceRef>> read_handling_claim_release_sources(const CargoHandlingExecution&)const=0;
};
class CargoHandlingFacilityWriter:public CargoHandlingFacilityRead {
public:
    virtual Outcome<std::optional<SourceRef>> adopt_handling_claim_in_candidate(CommonStore&,const CargoHandlingTerms&,const CargoHandlingRule&,const StableId& execution,const WorldMoment&,const ReadStamp&)=0;
    virtual Outcome<std::vector<SourceRef>> release_handling_claim_in_candidate(CommonStore&,const CargoHandlingExecution&,const SourceRef&,const WorldMoment&)=0;
};
struct CargoHandlingReadContext {
    const TravelCatalog& catalog;const TimeScaleRef& scale;const CargoHandlingBusinessRead& business;
    const CargoHandlingLaborRead& labor;const CargoHandlingFacilityRead& facility;
    const CurrentReadStampRead& stamps;ExactArithmeticBudget budget;
};
struct CargoHandlingWriteContext {
    const TravelCatalog& catalog;const TimeScaleRef& scale;CargoHandlingBusinessWriter& business;
    CargoHandlingLaborWriter& labor;CargoHandlingFacilityWriter& facility;
    const CurrentReadStampRead& stamps;std::function<Status(const StableId&)> release_action;
    ExactArithmeticBudget budget;
};
ActionTimeRegistration cargo_handling_action_time_registration();
class NativeCargoHandlingActionTimeRead final:public ActionTimeOriginalSourceRead {
public:
    NativeCargoHandlingActionTimeRead(const CommonRead& c,const CargoHandlingState& s,const ActionElapsedStore& e,const CargoHandlingReadContext& x):common_(c),state_(s),elapsed_(e),context_(x){}
    Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
    Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
    Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
    Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
    Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
    Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
    Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
    Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
    Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
    Status validate_original_gate_result(const SharedWorkGateResult&)const override;
private:const CommonRead& common_;const CargoHandlingState& state_;const ActionElapsedStore& elapsed_;CargoHandlingReadContext context_;
};
Status validate_cargo_handling_rule(const CargoHandlingRule&);
Outcome<OrderedTable<CargoHandlingRule>> decode_cargo_handling_rules(const BoundaryValue&);
Outcome<BoundaryValue> export_cargo_handling_rules(const OrderedTable<CargoHandlingRule>&);
Status emit_cargo_handling_rule_projection(ProjectionWriter&,std::string_view,const CargoHandlingRule&);
Outcome<CargoHandlingTerms> prepare_cargo_handling(const CargoHandlingReadContext&,const SourceRef&,Integer batch,CargoHandlingOperation,const WorldMoment&);
Outcome<StableId> start_cargo_handling_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const SourceRef&,Integer batch,CargoHandlingOperation,const WorldMoment&);
Status advance_cargo_handling_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const StableId& execution,const MomentSpan&);
Outcome<std::optional<WorldMoment>> predict_cargo_handling_service_due(const CargoHandlingState&,const ActionElapsedStore&,const StableId&,const TimeScaleRef&,ExactArithmeticBudget budget={});
Outcome<StableId> complete_cargo_handling_service_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const StableId&,const WorldMoment&);
Outcome<std::optional<StableId>> complete_cargo_handling_transfer_in_candidate(CommonStore&,CargoHandlingState&,const CargoHandlingWriteContext&,const StableId&,const WorldMoment&);
Status change_cargo_handling_service_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const StableId&,bool resume,const SourceRef& actual_cause,const WorldMoment&);
Outcome<StableId> terminate_cargo_handling_in_candidate(CommonStore&,CargoHandlingState&,ActionElapsedStore&,const CargoHandlingWriteContext&,const StableId&,CargoHandlingResultStatus,const SourceRef& actual_reason,const WorldMoment&);
Outcome<std::optional<CargoHandlingResult>> read_cargo_handling_result(const CargoHandlingState&,const StableId& execution_or_result);
Outcome<std::optional<CargoHandlingWorkResult>> read_cargo_handling_work_result(const CargoHandlingState&,const StableId& execution_or_result);
Status validate_cargo_handling_sources(const CommonStore&,const CargoHandlingState&,const ActionElapsedStore&,const CargoHandlingReadContext&,const WorldMoment&);
struct CommonResourceCodecContext;
Outcome<CargoHandlingState> decode_cargo_handling_payloads(const BoundaryValue&,const CommonResourceCodecContext&);
Outcome<BoundaryValue> export_cargo_handling_payloads(const CargoHandlingState&,const CommonResourceCodecContext&);
Status emit_cargo_handling_projection(ProjectionWriter&,const CargoHandlingState&,const CommonResourceCodecContext&);
Status visit_cargo_handling_payloads(CodecVisitor&,CargoHandlingState&);
Status visit_cargo_handling_rule(CodecVisitor&,CargoHandlingRule&);
Outcome<std::uint64_t> read_cargo_handling_domain_revision(const CommonStore&,const CargoHandlingState&,const DomainReadKey&);

}
