// 中央最终执行合同引用军事owner的提议和决定；不重复定义人物同意或资产状态。
#pragma once
#include "siege_integration_state.hpp"
#include "military_integration_asset_state.hpp"
namespace hanmo::resident {
// 只有提议与语义key可由调用者指定，实际接受来源和资产来自当前候选。
struct SiegeIntegrationApply { StableId proposal_id,proposal_key; };
struct SiegeIntegrationExecutionQuote { StableId proposal_id,proposal_key;MilitaryIntegrationQuote assets; };
} // actor身份由统一入口提供；关闭UI、报价和政策配置都不是接受命令。
