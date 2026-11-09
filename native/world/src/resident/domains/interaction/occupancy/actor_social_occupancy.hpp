// Original Interaction/Party occupancy history. Current occupancy remains in
// InteractionState.interactions and CommonState.parties; these are actual results.
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/interaction/interaction_context.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <string_view>
namespace hanmo::resident {
struct InteractionState;
struct InteractionCatalog;
struct InteractionAccess;
struct ActorSocialOccupancy {std::optional<StableId> interaction_id,party_id;};
enum class ActorSocialOccupancyCause {
 dialogue_open,dialogue_replaced,end_interaction,encounter_leave,dialogue_end,
 completed_dialogue_unavailable,local_move,party_invitation_accepted,
 party_recruited,party_left,party_dismissed,born_dialogue_open,known_dialogue_open,personal_battle_started,session_participant_died
};
enum class PartyAcceptanceKind {legacy_recruitment,declared_recruitment,authored_invitation};
struct PartyAcceptanceNode {StableId session_actor_id,session_id,dialogue_id,node_id,node_kind;};
// Immutable evidence of the existing successful Party acceptance. Values are
// the actual locked role quote; this is not an independent fee or member ledger.
struct PartyMembershipAcceptance {
 PartyAcceptanceKind kind=PartyAcceptanceKind::legacy_recruitment;
 StableId leader_actor_id,role_id;Integer cost_per_tick=0;InteractionContext context;
 std::optional<PartyAcceptanceNode> authored_node;std::optional<StableId> authored_declaration_id;
};
struct InteractionSessionOpening {
 StableId session_id,actor_id,dialogue_id;InteractionContext context;
};
bool actor_social_occupancy_version(Integer)noexcept;
bool actor_social_occupancy_opening_cause(ActorSocialOccupancyCause)noexcept;
struct ActorSocialOccupancyTransition {
 StableId id,actor_id;SourceRef previous_source;ActorSocialOccupancy before,after;
 WorldMoment occurred_at;ActorSocialOccupancyCause cause=ActorSocialOccupancyCause::dialogue_open;
 StableId original_context_id,initiator_actor_id;std::optional<SourceRef> actual_cause_source;
 std::optional<PartyMembershipAcceptance> party_acceptance;
 std::optional<InteractionSessionOpening> session_opening; // physical /3 only
};
struct ActorSocialOccupancySources {Integer version=1;OrderedTable<ActorSocialOccupancyTransition> transitions;};
struct ActorSocialOccupancyInitial {
 ActorSocialOccupancy value;WorldMoment created_at;MomentOwnerSource actual_origin;
};
// The original immutable installed seed / actual actor Origin owner supplies the
// base. It cannot be reconstructed from today's empty tables.
class ActorSocialOccupancyInitialRead {
public:virtual ~ActorSocialOccupancyInitialRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<ActorSocialOccupancyInitial> read_actor_initial_social_occupancy(const StableId&)const=0;
};
class ActorSocialOccupancyChangeVisitor {public:virtual ~ActorSocialOccupancyChangeVisitor()=default;virtual Status changed(const ActorSocialOccupancyTransition&)=0;};
struct ActorSocialOccupancyView {ActorSocialOccupancy value;std::vector<SourceRef> original_sources;};
Outcome<ActorSocialOccupancy> read_current_actor_social_occupancy(const CommonStore&,const InteractionState&,const StableId&);
class NativeActorSocialOccupancyRead {
 const CommonStore& common_;const InteractionState& interaction_;const ActorSocialOccupancyInitialRead& initial_;const CurrentMomentRead& clock_;const InteractionCatalog* catalog_=nullptr;
public:
 NativeActorSocialOccupancyRead(const CommonStore&c,const InteractionState&i,const ActorSocialOccupancyInitialRead&original,const CurrentMomentRead&clock,const InteractionCatalog*catalog=nullptr):common_(c),interaction_(i),initial_(original),clock_(clock),catalog_(catalog){}
 bool bound_to(const CommonStore&c)const noexcept{return &c==&common_;}
 bool bound_to(const CommonStore&c,const InteractionState&i)const noexcept{return &c==&common_&&&i==&interaction_;}
 bool party_history_bound_to(const CommonStore&c,const InteractionState&i,const InteractionCatalog&catalog)const noexcept{return &c==&common_&&&i==&interaction_&&catalog_==&catalog;}
 Outcome<ActorSocialOccupancyView> read_actual_actor_social_occupancy(const StableId&,const WorldMoment&)const;
 Outcome<ActorSocialOccupancyView> read_actor_social_occupancy_at_source_head(const StableId&,const WorldMoment&,const std::vector<SourceRef>&,const WorldMoment& actual_now)const;
 Outcome<ActorSocialOccupancyView> read_actor_social_occupancy_before_source(const StableId&,const SourceRef&actual_boundary,const WorldMoment&actual_boundary_at)const;
 Outcome<Integer> read_actor_social_occupancy_source_order(const SourceRef&,Integer schema)const;
 Outcome<WorldMoment> read_source_moment(const SourceRef&,Integer schema)const;
 Status validate_all()const;
 Status visit_actor_social_occupancy_changes(const StableId&,const WorldMoment&from,const SourceRef&from_head,const WorldMoment&to,const SourceRef&to_head,const WorldMoment&actual_now,ActorSocialOccupancyChangeVisitor&)const;
};
// Same-candidate callbacks only; the original writer has already applied its
// actual mutation. No-op creates no source. Failure rolls back the outer world.
Outcome<std::optional<SourceRef>> record_actor_social_occupancy_change_in_candidate(CommonStore&,InteractionState&,const ActorSocialOccupancyInitialRead&,const CurrentMomentRead&,const StableId&actor,const ActorSocialOccupancy&actual_before,ActorSocialOccupancyCause,const StableId&original_context,const StableId&initiator,const std::optional<SourceRef>&actual_cause={},const PartyMembershipAcceptance* actual_party_acceptance=nullptr);
class ActorSocialOccupancyWrite {
public:virtual ~ActorSocialOccupancyWrite()=default;
 virtual bool bound_to(const CommonStore&,const InteractionState&)const noexcept=0;
 virtual Outcome<ActorSocialOccupancy> capture(const CommonStore&,const StableId&)const=0;
 virtual Status record(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const std::optional<SourceRef>&)=0;
 virtual Status record_party_member(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const PartyMembershipAcceptance&){return common_failure("OriginalPartyAcceptanceWriterRequired",{},ErrorKind::unsupported_capability);}
};
class NativeActorSocialOccupancyWrite final:public ActorSocialOccupancyWrite {
 CommonStore& common_;InteractionState& interaction_;const ActorSocialOccupancyInitialRead& initial_;const CurrentMomentRead& clock_;
public:NativeActorSocialOccupancyWrite(CommonStore&c,InteractionState&i,const ActorSocialOccupancyInitialRead&o,const CurrentMomentRead&clock):common_(c),interaction_(i),initial_(o),clock_(clock){}
 bool bound_to(const CommonStore&c,const InteractionState&i)const noexcept override{return &c==&common_&&&i==&interaction_;}
 Outcome<ActorSocialOccupancy> capture(const CommonStore&,const StableId&)const override;
 Status record(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const std::optional<SourceRef>&)override;
 Status record_party_member(const StableId&,const ActorSocialOccupancy&,ActorSocialOccupancyCause,const StableId&,const StableId&,const PartyMembershipAcceptance&)override;
};
// The optional token exists only for the explicitly selected history profile.
Outcome<std::optional<ActorSocialOccupancy>> capture_actor_social_occupancy(const CommonRead&,const InteractionState&,const InteractionAccess&,const StableId&);
Outcome<std::optional<ActorSocialOccupancy>> capture_actor_social_occupancy(const CommonStore&,const InteractionAccess&,const StableId&);
Status record_captured_actor_social_occupancy(const InteractionAccess&,const StableId&,const std::optional<ActorSocialOccupancy>&,ActorSocialOccupancyCause,const StableId&,const StableId&,const std::optional<SourceRef>& source={});
Status record_captured_party_member_acceptance(const InteractionAccess&,const StableId&,const std::optional<ActorSocialOccupancy>&,ActorSocialOccupancyCause,const StableId&party,const StableId&initiator,const PartyMembershipAcceptance&);
Status erase_actor_interaction_with_source_in_candidate(const CommonRead&,InteractionState&,const InteractionAccess&,const StableId&actor,ActorSocialOccupancyCause,const StableId&initiator,const std::optional<SourceRef>& source={});
// Source-local borrowed opening: no current role, living-state, or mutable
// session is used to prove an ended session's original context.
Outcome<const ActorSocialOccupancyTransition*> read_original_interaction_session_opening(
 const CommonStore&,const InteractionState&,const InteractionCatalog&,const StableId&session_id);
Status validate_interaction_session_opening_sources(const CommonStore&,const InteractionState&,const InteractionCatalog&);
Outcome<Integer> actor_social_occupancy_source_schema(const SourceRef&);
Status visit_actor_social_occupancy_checkpoint(CodecVisitor&,ActorSocialOccupancySources&);
Outcome<ActorSocialOccupancySources> decode_actor_social_occupancy(const BoundaryValue&);
Outcome<BoundaryValue> encode_actor_social_occupancy(const ActorSocialOccupancySources&);
class ProjectionWriter;
Outcome<bool> emit_actor_social_occupancy_fact_payload_fields(ProjectionWriter&,const CommonStore&,const InteractionState&,const NativeActorSocialOccupancyRead&,const FactRecord&);
Status emit_actor_social_occupancy_projection(ProjectionWriter&,std::string_view,const ActorSocialOccupancySources&);
}
