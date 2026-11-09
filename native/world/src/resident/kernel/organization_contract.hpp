// 组织承接采购只指定责任方与真实执行人，后续仍沿唯一任务合同和个人行动履约。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 发令者保留任职权限；task只允许运行时accept_offer，执行人必须是愿意承担职责的真实成员。
struct OrganizationContractCommand { ActingAuthority authority;StableId executor_actor_id,payee_wallet_id;TaskCommand task; };
// 报价核验当前任职、已知机会与实际接洽位置，不占用发令者或分配合同ID。
Outcome<TaskQuote> preview_organization_contract(const WorldBundle&,const StableId& issuer_actor_id,const OrganizationContractCommand&);
// 同一世界候选中建立普通accept_offer行动及唯一claim/contract/escrow；失败由中央候选回滚。
Outcome<StableId> start_organization_contract(WorldBundle&,const StableId& issuer_actor_id,const OrganizationContractCommand&);
} // 行动ID归实际执行人，组织ID绝不写入TaskRecord.assignee。
