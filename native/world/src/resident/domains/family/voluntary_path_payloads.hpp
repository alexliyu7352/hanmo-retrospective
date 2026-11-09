// 独立生前尾只投影撤回事实；原接受和交割仍由Family唯一事实编码器导出。
#pragma once
#include "voluntary_path_commands.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status emit_voluntary_path_fact_payloads_fields(ProjectionWriter&,const CommonStore&,const VoluntaryPathState*);
Outcome<BoundaryValue> voluntary_path_fact_payloads(const CommonStore&,const VoluntaryPathState*);
Status validate_voluntary_path_json_payloads(const CommonStore&,const VoluntaryPathState*,const BoundaryValue& full_state);
BoundaryValue encode_voluntary_path_quote(const VoluntaryPathQuote&);
}
