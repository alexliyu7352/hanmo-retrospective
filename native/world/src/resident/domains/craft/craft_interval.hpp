// One selected positive slice: prepare at the real from, consume at the real to.
// This token is candidate-local and has no persistence or independent clock.
#pragma once
#include "craft_job.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <memory>
namespace hanmo::resident {
class CraftIntervalCandidate:public CheckpointAllocatedObject {
 struct Impl;std::unique_ptr<Impl> impl_;
 explicit CraftIntervalCandidate(std::unique_ptr<Impl>);
 friend Outcome<std::unique_ptr<CraftIntervalCandidate>> begin_craft_interval_candidate(const CraftJobReadContext&,const MomentSpan&);
 friend Status consume_craft_interval_candidate_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,CraftIntervalCandidate&,const MomentSpan&);
 friend Status validate_craft_interval_candidate_consumed(const CraftIntervalCandidate&,const CommonStore&,const MomentSpan&);
public:
 ~CraftIntervalCandidate();
 CraftIntervalCandidate(const CraftIntervalCandidate&)=delete;
 CraftIntervalCandidate& operator=(const CraftIntervalCandidate&)=delete;
 CraftIntervalCandidate(CraftIntervalCandidate&&)=delete;
 CraftIntervalCandidate& operator=(CraftIntervalCandidate&&)=delete;
};
Outcome<std::unique_ptr<CraftIntervalCandidate>> begin_craft_interval_candidate(const CraftJobReadContext&,const MomentSpan&);
Status consume_craft_interval_candidate_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,CraftIntervalCandidate&,const MomentSpan&);
// S0 calls this once all to-writers have returned, before releasing the token.
Status validate_craft_interval_candidate_consumed(const CraftIntervalCandidate&,const CommonStore&,const MomentSpan&);
}
