// I25.5/IC8/ST3 同一原终局只读 DTO；不拥有中央服务状态。
#pragma once
#include "service_actual_termination_contract.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
using ServiceAcceptedFeeTermsRef=std::variant<SourceRef,StudyFactTermsRef>;
using ServiceEarnedChargeRef=std::variant<SourceRef,StudyLessonCharge>;
// role 是原 owner 的角色身份；形状通过不代表该角色已获原服务承认。
struct ServiceVerifiedDeath {DeathRef death_ref;StableId actor_id,role;};
struct ServiceOwnerConfirmedUnavailability {OriginalServiceTerminalRef terminal_ref;};
using ServiceActualTerminationReason=std::variant<ServiceVerifiedDeath,ServiceOwnerConfirmedUnavailability>;
struct ServiceActualTerminationResult {
 StableId termination_id;AcceptedServiceScopeRef scope;OriginalServiceTerminalRef original_terminal_source;
 ServiceAcceptedFeeTermsRef accepted_fee_terms_ref;ServiceActualTerminationReason reason;
 ActorFoundationAt effective_game_time;std::vector<ServiceEarnedChargeRef> earned_charge_refs;
 std::vector<SourceRef> existing_payable_refs,custody_or_continuity_refs;
};
// 调用期拥有值的实际深复制预留先于其字符串/数组。原结果字段本身无新持久根。
struct OwnedServiceActualTerminationResult {
 CheckpointMemoryCharge memory;ServiceActualTerminationResult value;
 OwnedServiceActualTerminationResult()=default;
 OwnedServiceActualTerminationResult(const OwnedServiceActualTerminationResult&)=delete;
 OwnedServiceActualTerminationResult&operator=(const OwnedServiceActualTerminationResult&)=delete;
 OwnedServiceActualTerminationResult(OwnedServiceActualTerminationResult&&)=default;
 OwnedServiceActualTerminationResult&operator=(OwnedServiceActualTerminationResult&&)=delete;
};
// Result /4 removes the unadmitted advance slot. Its nested contracts are /3;
// scope/terminal schema 4 does not exist. Old result /1-/3 accept an empty legacy
// slot only; a nonempty original cash/allocation history requires its real owner.
inline constexpr Integer service_actual_termination_result_schema=4;
Outcome<Integer> service_actual_termination_result_contract_schema(Integer selected_result_schema);
Status validate_service_actual_termination_result_shape(const ServiceActualTerminationResult&,Integer selected_schema);
// 长度按原 action_id UTF-8 字节数；不分配 Common ID，不是 SourceRef。
Status validate_study_service_termination_id(const StableId& encoded,const StableId& action_id);
Outcome<ServiceActualTerminationResult> decode_service_actual_termination_result(const BoundaryValue&,Integer selected_schema);
Outcome<BoundaryValue> encode_service_actual_termination_result(const ServiceActualTerminationResult&,Integer selected_schema);
Status visit_service_actual_termination_result_checkpoint(CodecVisitor&,ServiceActualTerminationResult&,Integer selected_schema);
Status emit_service_actual_termination_result_fields(ProjectionWriter&,const ServiceActualTerminationResult&,Integer selected_schema);
}
