// Original command transaction: prepare -> actual effects -> Common seal -> bind.
// Neither private intermediate object may be published or resumed in another world.
#pragma once
#include "repair_service_agreement.hpp"
#include "resident/domains/craft/repair_contract.hpp"
#include <memory>
namespace hanmo::resident {
struct RepairServiceCommandContext {
 const RepairServiceAgreementReadContext& agreements;
 const RepairServiceReadContext& commercial;const RepairReadContext& repair;
 const InteractionAccess& interaction_access;
 const CurrentReadStampRead& stamps;ReadStampAuthority& stamp_authority;
 const DomainReadVersionSource& versions;
};
class PreparedRepairServiceCommand {
 struct Impl;std::unique_ptr<Impl>impl_;explicit PreparedRepairServiceCommand(std::unique_ptr<Impl>);
 friend struct RepairServiceCommandAccess;
public:PreparedRepairServiceCommand();~PreparedRepairServiceCommand();
 PreparedRepairServiceCommand(PreparedRepairServiceCommand&&)noexcept;
 PreparedRepairServiceCommand&operator=(PreparedRepairServiceCommand&&)noexcept;
 PreparedRepairServiceCommand(const PreparedRepairServiceCommand&)=delete;
 PreparedRepairServiceCommand&operator=(const PreparedRepairServiceCommand&)=delete;
};
class PendingRepairServiceCommand {
 struct Impl;std::unique_ptr<Impl>impl_;explicit PendingRepairServiceCommand(std::unique_ptr<Impl>);
 friend struct RepairServiceCommandAccess;
public:PendingRepairServiceCommand();~PendingRepairServiceCommand();
 PendingRepairServiceCommand(PendingRepairServiceCommand&&)noexcept;
 PendingRepairServiceCommand&operator=(PendingRepairServiceCommand&&)noexcept;
 PendingRepairServiceCommand(const PendingRepairServiceCommand&)=delete;
 PendingRepairServiceCommand&operator=(const PendingRepairServiceCommand&)=delete;
};
// actual_command_actor comes only from the original selected command route.
// The final binder separately proves it against the actual Common seal and
// original Player/Director control; actor_id itself is not consent.
Outcome<PreparedRepairServiceCommand> prepare_repair_service_command(
 const RepairServiceCommandContext&,const StableId& actual_command_actor,const RepairServiceCommandRequest&);
Outcome<PendingRepairServiceCommand> apply_repair_service_command_in_candidate(
 CommonStore&,ItemInstancesState&,const RepairServiceCommandContext&,PreparedRepairServiceCommand&&);
Outcome<const RepairServiceCommandReceipt*> read_pending_repair_service_command_receipt(const PendingRepairServiceCommand&);
// Called after the real Common result and (for NPC) its real Director adoption.
// Fails closed if they do not bind this exact candidate and its actual effects.
Status bind_repair_service_command_in_candidate(CommonStore&,ItemInstancesState&,
 const RepairServiceCommandContext&,PendingRepairServiceCommand&&,const MomentOwnerSource& actual_command_result);
}
