// 原job的allocator桥；一个真实block只在total收费一次，同时应用原子限额。
#pragma once
#include "checkpoint_job_budget.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
CheckpointJobResult<std::shared_ptr<CheckpointMemoryResource>> checkpoint_memory_resource(CheckpointJobBudget&,CheckpointJobUse);
// 仅原job工厂产生且受candidate子限额约束的资源能替代decoder预留。
// Identity only, with no candidate-subquota claim; used by immutable owned Bytes retention.
bool checkpoint_resource_shares_job(const std::shared_ptr<CheckpointMemoryResource>&,const CheckpointJobReservation&)noexcept;
bool checkpoint_resource_accounts_candidate(const std::shared_ptr<CheckpointMemoryResource>&,const CheckpointJobReservation&)noexcept;
// Reuse each original live reservation's subquota constraints. This does not
// charge total a second time or create another ledger/reservation for the block.
CheckpointJobStatus checkpoint_resource_add_constraints(const std::shared_ptr<CheckpointMemoryResource>&,CheckpointJobUse);
}
