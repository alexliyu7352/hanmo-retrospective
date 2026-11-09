// 原player提交结果永久绑定，不由当前控制人或首条保留缓存猜当时签字人。
#pragma once
#include "political_dismissal_codec.hpp"
namespace hanmo::resident {
// 可信原writer前后恰新增一条无结果来源，原旧行（含全部绑定）逐值不变。
Outcome<StableId> political_dismissal_new_result_source(const PoliticalDismissalSourceState& before,
 const PoliticalDismissalSourceState& after,const StableId& actor,const StableId& kind,Integer tick);
Status bind_political_dismissal_result(const CommonStore&,PoliticalDismissalSourceState&,
 const StableId& source,const StableId& actor,Integer request,const CommitResult&,const PoliticalDismissalResultMatch&);
Status validate_political_dismissal_results(const CommonStore&,const PoliticalDismissalSourceState&,
 const PoliticalDismissalResultMatch&);
}
