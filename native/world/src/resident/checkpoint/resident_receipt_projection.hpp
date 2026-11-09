// 原缓存回执的无树输出面；只借已冻结typed结果，业务门仍由同中央writer执行。
#pragma once
#include "projection_writer.hpp"
namespace hanmo::resident {
struct CommitResult;
Status emit_resident_receipt(ProjectionWriter&,const StableId& name,const CommitResult&);
}
