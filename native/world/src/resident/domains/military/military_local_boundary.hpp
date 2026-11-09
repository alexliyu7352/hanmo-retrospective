#pragma once
#include "military_local_motion.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Source-local professional reads. They must not call the whole boundary
// validator recursively or use today's cursor to authenticate a frozen input.
class MilitaryLocalBoundaryOriginalRead {
public:
 virtual ~MilitaryLocalBoundaryOriginalRead()=default;
 virtual Integer selected_local_schema()const noexcept=0;
 virtual bool bound_to(const CommonStore&,const MilitaryLocalMotionState&)const noexcept=0;
 virtual Outcome<BodyMotionBoundaryInput3> read_actual_body_input(const StableId& session,Integer generation,
     const MilitaryLocalBodyRef&,const CommonActualFactPrefix&,const WorldMoment&)const=0;
 virtual Status validate_body_input(const StableId& session,Integer generation,
     const CommonActualFactPrefix&,const BodyMotionBoundaryInput3&)const=0;
 virtual Status validate_actual_body_service(const BodyMotionBoundaryInput3&,const MomentSpan&)const=0;
 // This is a complete source-local prediction from the actual current input
 // set, including terrain and all ties. It carries no head/id/cost outcome.
 // The core installs its own current Common prefix and private candidate seal.
 virtual Outcome<LocalMotionContact3> read_actual_contact_selection(const StableId& session,Integer generation,
     const CommonActualFactPrefix&,const WorldMoment& requested_upper)const=0;
 virtual Status validate_contact_inputs(const LocalMotionContact3&)const=0;
 // Original work/cost receipts and actual topology release/arrival, including
 // complete reverse coverage. This is not permission to repeat either write.
 virtual Status validate_edge_completion_consequences(const LocalMotionEdgeCompletion3&)const=0;
 // Complete historical body + terrain/portal set, exact earliest tied
 // contacts, original relation, consumed-once gate and all actual consumers.
 virtual Status validate_contact_consequences(const LocalMotionContact3&)const=0;
};
struct LocalEdgeCompletionAppliedSources3 {
 std::vector<MomentOwnerSource> actual_work_cost_source_refs,reservation_release_sources;
 // Borrowed from the actual topology result for adoption into the ONE cursor;
 // not another persistent reservation table.
 std::vector<SourceRef> remaining_reserved_topology_refs;
};
class MilitaryLocalEdgeCompletionWrite {
public:
 virtual ~MilitaryLocalEdgeCompletionWrite()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryLocalMotionState&)const noexcept=0;
 // Called only after the real completion head/row exists, with exact full
 // credit at the actual clock. No Common balance or topology algorithm lives
 // here: the concrete delegates the original same-candidate owners.
 virtual Outcome<LocalEdgeCompletionAppliedSources3> apply_actual_edge_work_and_topology(const LocalMotionEdgeCompletion3&)=0;
 virtual Status consume_actual_edge_completion(const MomentOwnerSource&)=0;
};
class MilitaryLocalContactWrite {
public:
 virtual ~MilitaryLocalContactWrite()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryLocalMotionState&)const noexcept=0;
 // Only the original work/resource owner writes receipts. Zero-prefix contact
 // must produce no work/cost sources. Topology reservations remain in place.
 virtual Outcome<std::vector<MomentOwnerSource>> apply_actual_contact_work(const LocalMotionContact3&)=0;
 // Called after ALL affected current phases have adopted this actual source.
 // Actual enemy contact/posture/objective/order consequences remain here.
 virtual Status consume_actual_local_contact(const MomentOwnerSource&)=0;
};
class PreparedLocalContact3 {
public:
 PreparedLocalContact3()=default;
 PreparedLocalContact3(PreparedLocalContact3&&)=default;
 PreparedLocalContact3& operator=(PreparedLocalContact3&&)=default;
 PreparedLocalContact3(const PreparedLocalContact3&)=delete;
 PreparedLocalContact3& operator=(const PreparedLocalContact3&)=delete;
private:
 const CommonStore* common_=nullptr;const MilitaryLocalMotionState* state_=nullptr;
 LocalMotionContact3 selection_;
 friend Outcome<PreparedLocalContact3> capture_actual_local_contact_candidate(const CommonStore&,
     const MilitaryLocalMotionState&,const MilitaryLocalBoundaryOriginalRead&,const CurrentMomentRead&,
     const StableId&,Integer,const WorldMoment&,ExactArithmeticBudget);
 friend Outcome<MomentOwnerSource> adopt_actual_local_contact_in_candidate(CommonStore&,
     MilitaryLocalMotionState&,const MilitaryLocalBoundaryOriginalRead&,MilitaryLocalContactWrite&,
     const CurrentMomentRead&,PreparedLocalContact3&&,ExactArithmeticBudget);
};
Outcome<PreparedLocalContact3> capture_actual_local_contact_candidate(const CommonStore&,
 const MilitaryLocalMotionState&,const MilitaryLocalBoundaryOriginalRead&,const CurrentMomentRead&,
 const StableId& session,Integer generation,const WorldMoment& requested_upper,ExactArithmeticBudget={});
Outcome<MomentOwnerSource> adopt_actual_local_contact_in_candidate(CommonStore&,MilitaryLocalMotionState&,
 const MilitaryLocalBoundaryOriginalRead&,MilitaryLocalContactWrite&,const CurrentMomentRead&,
 PreparedLocalContact3&&,ExactArithmeticBudget={});
class PreparedLocalEdgeCompletion3 {
public:
 PreparedLocalEdgeCompletion3()=default;
 PreparedLocalEdgeCompletion3(PreparedLocalEdgeCompletion3&&)=default;
 PreparedLocalEdgeCompletion3& operator=(PreparedLocalEdgeCompletion3&&)=default;
 PreparedLocalEdgeCompletion3(const PreparedLocalEdgeCompletion3&)=delete;
 PreparedLocalEdgeCompletion3& operator=(const PreparedLocalEdgeCompletion3&)=delete;
private:
 const CommonStore* common_=nullptr;const MilitaryLocalMotionState* state_=nullptr;
 StableId session_;Integer generation_=0;CommonActualFactPrefix prefix_;
 BodyMotionBoundaryInput3 input_;WorldMoment completed_at_;
 friend Outcome<PreparedLocalEdgeCompletion3> capture_local_edge_completion_for_selected_span(
     const CommonStore&,const MilitaryLocalMotionState&,const MilitaryLocalMotionOriginalRead&,
     const MilitaryLocalBoundaryOriginalRead&,const CurrentMomentRead&,const StableId&,Integer,
     const StableId&,const MomentSpan&,ExactArithmeticBudget);
 friend Outcome<MomentOwnerSource> adopt_local_edge_completion_in_candidate(CommonStore&,
     MilitaryLocalMotionState&,const MilitaryLocalMotionOriginalRead&,const MilitaryLocalBoundaryOriginalRead&,
     MilitaryLocalEdgeCompletionWrite&,const CurrentMomentRead&,PreparedLocalEdgeCompletion3&&,ExactArithmeticBudget);
};
// Called at the selected positive interval's true from, before any prefix is
// advanced. It selects this body's real current edge; no free caller credit.
Outcome<PreparedLocalEdgeCompletion3> capture_local_edge_completion_for_selected_span(
 const CommonStore&,const MilitaryLocalMotionState&,const MilitaryLocalMotionOriginalRead&,
 const MilitaryLocalBoundaryOriginalRead&,const CurrentMomentRead&,const StableId& session,Integer generation,
 const StableId& body_key,const MomentSpan& actual_selected_span,ExactArithmeticBudget={});
// The original positive prefix writer has already applied the selected span.
// This unique writer closes that real full-credit edge, never a future quote.
Outcome<MomentOwnerSource> adopt_local_edge_completion_in_candidate(CommonStore&,MilitaryLocalMotionState&,
 const MilitaryLocalMotionOriginalRead&,const MilitaryLocalBoundaryOriginalRead&,
 MilitaryLocalEdgeCompletionWrite&,const CurrentMomentRead&,PreparedLocalEdgeCompletion3&&,ExactArithmeticBudget={});
Outcome<const LocalMotionBoundarySource*> read_actual_local_motion_boundary_source(
 const CommonStore&,const MilitaryLocalMotionState&,const MilitaryLocalBoundaryOriginalRead&,
 const MomentOwnerSource&,const WorldMoment& actual_now,ExactArithmeticBudget={});
Status validate_actual_local_motion_boundary_sources(const CommonStore&,const MilitaryLocalMotionState&,
 const MilitaryLocalBoundaryOriginalRead&,const WorldMoment& actual_now,Integer selected_local_schema,ExactArithmeticBudget={});
class NativeMilitaryLocalBoundaryValidationRead final:public MilitaryLocalBoundaryValidationRead {
public:
 NativeMilitaryLocalBoundaryValidationRead(const CommonStore& c,const MilitaryLocalMotionState& s,
     const MilitaryLocalBoundaryOriginalRead& o,const CurrentMomentRead& clock,ExactArithmeticBudget b={})
     :common_(c),state_(s),original_(o),clock_(clock),budget_(b){}
 Status validate_original_local_boundary(const LocalMotionBoundarySource&)const override;
private:
 const CommonStore& common_;const MilitaryLocalMotionState& state_;
 const MilitaryLocalBoundaryOriginalRead& original_;const CurrentMomentRead& clock_;ExactArithmeticBudget budget_;
};
}
