// Real submit actor -> inspected physical transfer -> actual Common seal -> bind.
#pragma once
#include "repair_service_physical.hpp"
#include "resident/domains/craft/repair_contract.hpp"
#include <memory>
namespace hanmo::resident {
struct RepairItemCommandContext {
 const RepairPhysicalReadContext&physical;const InteractionAccess&interaction_access;
 const CurrentReadStampRead&stamps;ReadStampAuthority&stamp_authority;
 const DomainReadVersionSource&versions;
 // Required only when returning an actually started Repair; no fake Job is
 // allocated for an accepted, not-yet-started service.
 const RepairWriteContext*repair_write=nullptr;ActionElapsedStore*elapsed=nullptr;
};
class PreparedRepairItemCommand {
 struct Impl;std::unique_ptr<Impl>impl_;explicit PreparedRepairItemCommand(std::unique_ptr<Impl>);
 friend struct RepairItemCommandAccess;
public:PreparedRepairItemCommand();~PreparedRepairItemCommand();
 PreparedRepairItemCommand(PreparedRepairItemCommand&&)noexcept;
 PreparedRepairItemCommand&operator=(PreparedRepairItemCommand&&)noexcept;
 PreparedRepairItemCommand(const PreparedRepairItemCommand&)=delete;
 PreparedRepairItemCommand&operator=(const PreparedRepairItemCommand&)=delete;
};
class PendingRepairItemCommand {
 struct Impl;std::unique_ptr<Impl>impl_;explicit PendingRepairItemCommand(std::unique_ptr<Impl>);
 friend struct RepairItemCommandAccess;
public:PendingRepairItemCommand();~PendingRepairItemCommand();
 PendingRepairItemCommand(PendingRepairItemCommand&&)noexcept;
 PendingRepairItemCommand&operator=(PendingRepairItemCommand&&)noexcept;
 PendingRepairItemCommand(const PendingRepairItemCommand&)=delete;
 PendingRepairItemCommand&operator=(const PendingRepairItemCommand&)=delete;
};
Outcome<PreparedRepairItemCommand>prepare_repair_item_command(const RepairItemCommandContext&,const StableId&actual_command_actor,const RepairServiceCommandRequest&);
Outcome<PendingRepairItemCommand>apply_repair_item_command_in_candidate(CommonStore&,ItemInstancesState&,const RepairItemCommandContext&,PreparedRepairItemCommand&&);
Outcome<const RepairServiceCommandReceipt*>read_pending_repair_item_command_receipt(const PendingRepairItemCommand&);
Status bind_repair_item_command_in_candidate(CommonStore&,ItemInstancesState&,const RepairItemCommandContext&,PendingRepairItemCommand&&,const MomentOwnerSource&actual_command_result);
}
