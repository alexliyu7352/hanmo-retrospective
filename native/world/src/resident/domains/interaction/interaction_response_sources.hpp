// Fixed S3 §4.1 / I19.2: original Interaction roots, not a continuation ledger.
#pragma once
#include "s3_leaf_life.hpp"
#include "interaction_action_confirmation.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <memory>
namespace hanmo::resident {
struct InteractionState;struct InteractionSession;struct InteractionCatalog;struct InteractionAccess;class CommonStore;class CommonWrite;
class ProjectionWriter;struct StabilizeContinuationBoundary;class InteractionSessionDeathPending;
struct InteractionResponseNode {StableId node_id;};
struct InteractionResponseTerminal {};
using InteractionResponseNext=std::variant<InteractionResponseNode,InteractionResponseTerminal>;
// Borrowed inputs to the sole original payload planner. No SourceRef is copied
// until the original boundary's enduring charge has been acquired.
struct InteractionSessionBoundaryBuildView {
 const StableId& session_id;Integer generation=1;
 const SourceRef* current_response_fact_ref=nullptr;
 const SourceRef* last_consumed_response_fact_ref=nullptr;
};
struct InteractionSessionBoundary {
 CheckpointMemoryCharge copy_memory; // runtime ownership, never a wire field
 InteractionSessionBoundary()=default;
 explicit InteractionSessionBoundary(const InteractionSessionBoundaryBuildView&);
 InteractionSessionBoundary(const InteractionSessionBoundary&);
 InteractionSessionBoundary(InteractionSessionBoundary&&);
 InteractionSessionBoundary& operator=(const InteractionSessionBoundary&);
 InteractionSessionBoundary& operator=(InteractionSessionBoundary&&)noexcept;
 void swap(InteractionSessionBoundary&)noexcept;
 friend void swap(InteractionSessionBoundary&a,InteractionSessionBoundary&b)noexcept{a.swap(b);}
 // Only on a freshly owned boundary installed before the original pump. Drop
 // old-last storage before releasing its exact charge, then move the actual
 // current reference without allocating another persistent copy.
 Status consume_current_response();
 StableId session_id;Integer generation=1;
 std::optional<SourceRef> current_response_fact_ref,last_consumed_response_fact_ref;
};
struct InteractionResponseSource {
 CheckpointMemoryCharge copy_memory; // runtime ownership, never a wire field
 InteractionResponseSource()=default;
 InteractionResponseSource(const StableId& id,const StableId& session,Integer generation,
  const StableId& selected_node,const StableId& selected_choice,const StableId* original_next_node);
 // This original private row has no result yet. Reserve only the actual sealed
 // result here; the later consumed result is charged when it really exists.
 InteractionResponseSource(const InteractionResponseSource& prepared,const SourceRef& actual_result);
 InteractionResponseSource(const InteractionResponseSource&);
 InteractionResponseSource(InteractionResponseSource&&);
 InteractionResponseSource& operator=(const InteractionResponseSource&);
 InteractionResponseSource& operator=(InteractionResponseSource&&)noexcept;
 // Memory operation only: the original consumer must prove the actual seal.
 Status retain_consumed_result_copy(const SourceRef&);
 void swap(InteractionResponseSource&)noexcept;
 friend void swap(InteractionResponseSource&a,InteractionResponseSource&b)noexcept{a.swap(b);}
 StableId response_fact_id,session_id;Integer session_generation=0;
 StableId selected_node_id,selected_choice_id;SourceRef committed_result_ref;
 InteractionResponseNext next_node_or_terminal;
 std::optional<SourceRef> tail_consumed_result_ref;
};
struct SelectedResponseTail {
 CheckpointMemoryCharge copy_memory; // runtime ownership, never a wire field
 SelectedResponseTail()=default;
 SelectedResponseTail(const StableId& session,Integer generation,const SourceRef&,const InteractionResponseNext&);
 SelectedResponseTail(const SelectedResponseTail&);
 SelectedResponseTail(SelectedResponseTail&&);
 SelectedResponseTail& operator=(const SelectedResponseTail&);
 SelectedResponseTail& operator=(SelectedResponseTail&&)noexcept;
 void swap(SelectedResponseTail&)noexcept;
 friend void swap(SelectedResponseTail&a,SelectedResponseTail&b)noexcept{a.swap(b);}
 StableId session_id;Integer session_generation=0;SourceRef response_fact_ref;
 InteractionResponseNext next_node_or_terminal;
};
enum class InteractionSessionLifeRole {speaker,listener};
struct InteractionSessionLifeSource {
 // Runtime ownership only; the original four wire fields below are unchanged.
 // Declare first so the lease outlives every ordinary deep field.
 CheckpointCopyLease copy_memory;
 InteractionSessionLifeSource()=default;
 InteractionSessionLifeSource(const InteractionSessionLifeSource&);
 InteractionSessionLifeSource(InteractionSessionLifeSource&&);
 InteractionSessionLifeSource& operator=(const InteractionSessionLifeSource&);
 InteractionSessionLifeSource& operator=(InteractionSessionLifeSource&&);
 void swap(InteractionSessionLifeSource&);
 friend void swap(InteractionSessionLifeSource&a,InteractionSessionLifeSource&b){a.swap(b);}
 DeathRef death_ref;StableId session_id;
 InteractionSessionLifeRole role=InteractionSessionLifeRole::speaker;S3LeafLifeEffect effect;
};
struct InteractionResponseSources {
 Integer physical_version=0; // Selected externally; never an extra wire field.
 OrderedTable<InteractionSessionBoundary> session_boundaries;
 OrderedTable<InteractionResponseSource> response_sources;
 OrderedTable<SelectedResponseTail> selected_response_tails;
 OrderedTable<InteractionSessionLifeSource> life_sources;
 // Physical11/12/13 only; immutable actual confirmation sources, never pending.
 OrderedTable<InteractionActionConfirmationSource> action_confirmation_sources;
};
bool interaction_response_version(Integer)noexcept;
Outcome<Integer> interaction_response_base_version(Integer);
// Explicit finite selection only: 5/6/7 -> schema1, 8/9/10 and 11/12/13 -> schema2.
// Zero means unsupported, never an inferred/default source schema.
Integer interaction_response_source_schema_version(Integer selected_physical_version)noexcept;
Outcome<InteractionResponseSources> make_initial_interaction_response_sources(Integer);
SourceRef interaction_response_source(const StableId&);
bool same_interaction_response_next(const InteractionResponseNext&,const InteractionResponseNext&);
Status validate_interaction_response_source_shape(const InteractionResponseSource&,Integer source_schema=1);
// Only original session-opening writers call this, after allocating that real ID.
Status open_interaction_response_boundary_in_candidate(InteractionState&,const InteractionCatalog&,const StableId&session_id);
// 只活在当前候选的真实命令 capture 中；不进 Receipt、World 或任何 codec。
// 选择先验成功不等于已提交；原 Common seal 之前不建立持久 response/tail。
class InteractionSelectedResponsePending : public CheckpointAllocatedObject {
 friend Outcome<const InteractionResponseSource*> resume_interaction_selected_response_tail_in_candidate(
  CommonStore&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,
  const StabilizeContinuationBoundary&,const WorldMoment&,InteractionSelectedResponsePending&);
 friend Status bind_interaction_resumed_response_result_in_candidate(CommonStore&,InteractionState&,
  const InteractionCatalog&,const InteractionAccess&,InteractionSelectedResponsePending&,
  const SourceRef&,const WorldMoment&);
 friend Status prepare_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
  const InteractionCatalog&,const StableId&,const StableId&,const StableId&,const StableId&,
  const StableId&,const WorldMoment&,InteractionSelectedResponsePending&);
 friend Status retain_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
  const InteractionCatalog&,InteractionSelectedResponsePending&,const SourceRef&,const WorldMoment&);
 friend Status finalize_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
  const InteractionCatalog&,const InteractionAccess&,InteractionSelectedResponsePending&,
  const SourceRef&,const WorldMoment&);
 CheckpointMemoryCharge copy_memory_; // actor text only; response owns its own copies
 const CommonStore* common_=nullptr;const InteractionState* state_=nullptr;
 const InteractionCatalog* catalog_=nullptr;
 StableId actor_id_;InteractionResponseSource response_;WorldMoment selected_at_;
 Integer original_revision_=0;bool retained_=false,consumed_=false,resuming_=false;
 InteractionSelectedResponsePending(CommonStore&c,InteractionState&s,const InteractionCatalog&cat):common_(&c),state_(&s),catalog_(&cat){}
public:
 static Outcome<std::unique_ptr<InteractionSelectedResponsePending>> create(CommonStore&,InteractionState&,const InteractionCatalog&);
 bool bound_to(const CommonStore&,const InteractionState&,const InteractionCatalog&)const noexcept;
 InteractionSelectedResponsePending(const InteractionSelectedResponsePending&)=delete;
 InteractionSelectedResponsePending& operator=(const InteractionSelectedResponsePending&)=delete;
 InteractionSelectedResponsePending(InteractionSelectedResponsePending&&)=delete;
 InteractionSelectedResponsePending& operator=(InteractionSelectedResponsePending&&)=delete;
 bool has_response()const noexcept{return !response_.response_fact_id.empty();}
 bool consumed()const noexcept{return consumed_;}
 // Borrow only after the real post-seal driver succeeded; never exposes an unsealed row.
 Outcome<const InteractionResponseSource*> read_committed_response(const CommonStore&,const InteractionState&,const InteractionCatalog&)const;
};
Status prepare_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,const StableId&actor,const StableId&session,const StableId&node,
 const StableId&choice,const StableId&original_next,const WorldMoment&,InteractionSelectedResponsePending&);
// Original post-seal insertion phase. It accepts only this same candidate's
// real dialogue_choose result and preserves its exact already-selected tail.
// This does not select a scheduling/yield policy. Normal finalize below still
// immediately consumes it; a coordinator that retains it must finish all other
// original candidate captures and publish only a fully validated boundary.
// No unsealed pending data is published, and retries cannot add a second row.
Status retain_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,InteractionSelectedResponsePending&,
 const SourceRef&actual_sealed_result,const WorldMoment&actual);
// 原 seal 后、报告收集和第二 settle 前调用；失败必须撤回整个外层候选。
// 只转唯一原 pump；真正业务 action 仍停确认，不新增付款、签约或随机重抽。
// Actual retained-tail captures delegate their existing tag20 binder instead;
// they never re-enter normal choice retention or pump a second time.
// Call before the complete S4 postseal boundary validator reads this effect.
Status finalize_interaction_selected_response_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,const InteractionAccess&,InteractionSelectedResponsePending&,
 const SourceRef&actual_sealed_result,const WorldMoment&actual);
// 无私有选择的原命令封口仍可使用；不再把未封行补成已消费来源。
Status bind_interaction_selected_response_result_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,const SourceRef&actual_sealed_result);
// P17.5 original S3 operation only. S0 first validates/captures the actual S4
// candidate prefix. This runs exactly the already committed selected tail;
// it does not accept a new choice, confirm a business action, or advance time.
// The returned row is borrowed only to build the one actual receipt's
// settled_source_refs (interaction/response_selected with the selected schema). S0 writes Ready,
// zero world_half_days and no next_boundary_ref after this succeeds, then seals
// its original tag20 receipt. Until the post-seal binder completes the private
// candidate is transient and must not be published or cold-validated.
Outcome<const InteractionResponseSource*> resume_interaction_selected_response_tail_in_candidate(
 CommonStore&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,
 const StabilizeContinuationBoundary&,const WorldMoment&actual,
 InteractionSelectedResponsePending&);
// Invoke only after inserting that exact original Common seal/history. Failure
// aborts the entire outer candidate, including the original local pump effects.
Status bind_interaction_resumed_response_result_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,const InteractionAccess&,InteractionSelectedResponsePending&,
 const SourceRef&actual_sealed_result,const WorldMoment&actual);

// Immutable original selection dependency cut. It validates the actual choose
// seal and frozen original choice, not whether its later tail was consumed.
Outcome<const InteractionResponseSource*> read_original_interaction_response_selection(const CommonStore&,
 const InteractionState&,const InteractionCatalog&,const SourceRef&,Integer source_schema);
Outcome<const InteractionResponseSource*> read_interaction_response_source(const CommonStore&,
 const InteractionState&,const InteractionCatalog&,const SourceRef&,Integer source_schema);
// Only a writer-issued live death token can validate the unpublished closed
// session whose real Common tail-consumption result is not sealed yet.
Status validate_interaction_response_sources_in_death_capture(const CommonStore&,
 const InteractionState&,const InteractionCatalog&,const InteractionSessionDeathPending&);
Outcome<const SelectedResponseTail*> read_interaction_selected_response_tail(const CommonStore&,
 const InteractionState&,const InteractionCatalog&,const StableId&session_id);
// S3-only borrowed projection for the original P17.5 coordinator. Empty
// session and stable say/choice/action/trade/waiting are Ready; a real retained
// selected response is the sole NeedsLocalSettle leg. It grants no control or
// candidate eligibility and never advances an already-running long Action.
struct OriginalInteractionContinuationBoundary {
 const InteractionSession* session=nullptr;
 const InteractionSessionBoundary* boundary=nullptr;
 const SelectedResponseTail* selected_response_tail=nullptr;
};
Outcome<OriginalInteractionContinuationBoundary> read_original_interaction_continuation_boundary(
 const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,
 const StableId& candidate_actor_id);
Status validate_interaction_response_sources(const CommonStore&,const InteractionState&,const InteractionCatalog&);
class InteractionResponseSourcePinSink {public:virtual ~InteractionResponseSourcePinSink()=default;
 virtual Status original_source(const SourceRef&,Integer schema)=0;
};
// Legacy schema-only helper remains schema1. Complete selected callers must
// use the context-bearing overload; schema1 cannot read a schema2 root.
Outcome<Integer> interaction_response_source_schema(const SourceRef&);
Outcome<Integer> interaction_response_source_schema(const InteractionState&,const InteractionCatalog&,const SourceRef&);
Status visit_interaction_response_source_pins(const CommonStore&,const InteractionState&,
 const InteractionCatalog&,InteractionResponseSourcePinSink&);
// These functions append/read the four original roots, plus the finite
// confirmation appendix only in11/12/13. The selected
// base 2/3/4 remains the existing original Interaction codec and field order.
Status visit_interaction_response_roots(CodecVisitor&,InteractionResponseSources&,Integer selected_version);
Outcome<InteractionResponseSources> decode_interaction_response_roots(const BoundaryValue&,Integer selected_version);
Outcome<BoundaryValue> encode_interaction_response_roots(const InteractionResponseSources&,Integer selected_version);
Status emit_interaction_response_roots(ProjectionWriter&,const InteractionResponseSources&,Integer selected_version);
Status emit_interaction_response_source_fields(ProjectionWriter&,const InteractionResponseSource&,Integer source_schema=1);
}
