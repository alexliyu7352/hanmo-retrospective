#pragma once
#include "director_subject2.hpp"
#include "resident/domains/social/propagation.hpp"
#include "resident/domains/social/social_calendar_phase.hpp"
namespace hanmo::resident {
struct DirectorExactReadBinding {
 const DirectorSubjectOwnerRead& original_subjects;
 bool propagation_selected=false;const PropagationReadContext* propagation=nullptr;
};
struct DirectorExactWriteBinding {
 const DirectorSubjectOwnerRead& original_subjects;DirectorSubjectExecutor& original_executor;
 bool propagation_selected=false;const PropagationReadContext* propagation=nullptr;PropagationChannelWrite* propagation_writer=nullptr;
};
class DirectorExactReadLease {public:virtual ~DirectorExactReadLease()=default;virtual DirectorExactReadBinding reads()const=0;};
class DirectorExactWriteLease {public:virtual ~DirectorExactWriteLease()=default;virtual DirectorExactWriteBinding writes()=0;};
struct DirectorExactWorldFactory {
 Outcome<std::shared_ptr<const DirectorExactReadLease>>(*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<DirectorExactWriteLease>>(*write)(WorldBundle&,const WorldMoment&)=nullptr;
};
DirectorExactWorldFactory native_director_exact_world_factory() noexcept;
// 接在原宏观Director review位置，不注册伪due事件或第二Director状态。
Status review_complete_director_in_candidate(WorldBundle&,const ExactPeriodContext&,DirectorExactWorldFactory);
Status validate_complete_director_sources(const WorldBundle&,DirectorExactWorldFactory);
// Same original validation for an already retained same-world native lease.
// This neither reacquires the factory nor creates a second source graph.
Status validate_complete_director_read_binding(const WorldBundle&,const WorldMoment&,const DirectorExactReadBinding&);
// New-world-only original registration and pending-wake adoption. Requires a
// pristine selected Subject2 state; never uses decode-time historical rebuild.
Status initialize_complete_director_in_candidate(WorldBundle&,DirectorExactWorldFactory);
// Called at a stable successful source-writing boundary, before the candidate
// can be saved or published. Records actual pending wakes; does not review or
// execute optional decisions. The restored/initial hot index must already exist.
Status collect_complete_director_wakes_in_candidate(WorldBundle&,DirectorExactWorldFactory);
// Decode-time derived-cache reconstruction only. Persisted registrations and
// pending wake_reasons are authoritative; this never adds historical pending
// events or uses current absence as evidence that they were consumed.
Status restore_complete_director_wake_cache_in_candidate(WorldBundle&,DirectorExactWorldFactory);
// 原exchange保实际writer；仅真实已安装传播route的actor停止同效share腿。
Outcome<SocialCalendarDispatch> read_complete_social_calendar_dispatch(const WorldBundle&,const StableId&actor,DirectorExactWorldFactory);
// Native calendar registration borrows the original Director factory from the
// same world's owning graph. It does not install a second propagation policy.
Outcome<SocialCalendarDispatch> read_native_social_calendar_dispatch(const WorldBundle&,const StableId&actor);
}
