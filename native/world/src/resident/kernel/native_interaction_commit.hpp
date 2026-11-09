// 真实 Common seal 后绑定原 S3 零时事件；只借同候选原 owner，不提前捕获 P10。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CommitResult;
Status bind_native_personal_appointment_commit_in_candidate(
    WorldBundle &, const std::vector<StableId> &, const StableId &, Integer,
    const CommitResult &);
Status bind_native_visit_appointment_commit_in_candidate(
    WorldBundle &, const StableId &, const std::vector<StableId> &,
    const StableId &, Integer, const CommitResult &);
} // namespace hanmo::resident
