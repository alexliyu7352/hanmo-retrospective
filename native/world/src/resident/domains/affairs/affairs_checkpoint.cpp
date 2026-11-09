// 事务域检查点逐字段持久化责任和贡献键；恢复只重建索引，不重放交付。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "affairs_codec.hpp"
namespace hanmo::resident {
Status visit_affairs_checkpoint(CodecVisitor& codec,AffairsState& state,bool obligation_needs,bool fiscal_needs,bool cohort_wage_needs){if(codec.reading()){state.cohort_wage_needs=cohort_wage_needs;state.obligation_needs=obligation_needs;state.fiscal_needs=fiscal_needs;}else if(state.cohort_wage_needs!=cohort_wage_needs||state.obligation_needs!=obligation_needs||state.fiscal_needs!=fiscal_needs)return codec.reject("AffairsNeedVersionMismatch");return affairs_codec::state(codec,state);}
} // 完整候选再交由跨域校验器决定能否发布。
