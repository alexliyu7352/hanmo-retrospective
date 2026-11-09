// 雇用边界只接受具名政策或本人决定，不收工资、钱包、到岗与第三人同意的setter。
#pragma once
#include "estate_employment.hpp"
namespace hanmo::resident {
Outcome<EstateEmploymentCommand> decode_estate_employment_command(const StableId& kind,const BoundaryValue& params);
StableId estate_employment_command_kind(const EstateEmploymentCommand&);
BoundaryValue encode_estate_employment_command(const EstateEmploymentCommand&);
BoundaryValue encode_estate_employment_quote(const EstateEmploymentQuote&);
BoundaryValue encode_estate_employment_receipt(const EstateEmploymentReceipt&);
// 完整本域已验后，用旧EstateReceipt外形核原具名合同/本人决定；不接受自动到期或服务事实作命令回执。
Status validate_estate_employment_receipt(const EstateEmploymentState&,const StableId& kind,const StableId& actor,Integer tick,const EstateReceipt&);
// 精确缓存键、原命令历史与全局ID分配窗口共同证明本人；同tick旧事实不可换入新回执。
Outcome<StableId> estate_employment_receipt_actor(const CommonState&,const StableId& cache_key,const StableId& kind,const StableId& result_id,Integer tick,const StableId& fact_id);
}
