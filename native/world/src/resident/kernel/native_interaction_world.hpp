// Same-candidate S3 owners. The enclosing World lease owns CharacterFamily
// before this object, and destroys P10 / ActionTime borrowers before this object.
#pragma once
#include "native_character_family_world.hpp"
#include "native_interaction_catalog.hpp"
#include "resident/domains/interaction/interaction_session_life.hpp"
#include "resident/domains/interaction/interaction_growth_life.hpp"
#include "resident/domains/interaction/interaction_task_life.hpp"
#include "resident/domains/interaction/interaction_exact_phase.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_native.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_native.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_native.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_original_result_read.hpp"
#include "resident/domains/director/director_appointment_native2.hpp"
namespace hanmo::resident {
struct NativeExactResultAccess;
class NativeInteractionWorldRead final:public CheckpointAllocatedObject {
    const WorldBundle& world_;
    const NativeCharacterFamilyWorldRead& family_;
    NativeInteractionSectionVersions selected_;
    WorldCurrentMomentRead clock_;
    // The context borrows these true original readers; reverse destruction is required.
    std::unique_ptr<NativeInteractionStudyLifeRead> study_terminals_;
    std::unique_ptr<NativeInteractionGrowthLifeRead> growth_terminals_;
    std::unique_ptr<NativeInteractionTaskLifeRead> task_terminals_;
    std::optional<InteractionSessionLifeContext> session_life_;
    // Order is intentional: Inquiry and attendance borrow these actual contexts.
    std::unique_ptr<NativePersonalAppointmentTime2Read> personal_;
    std::unique_ptr<NativeContactInquiryTime2Read> inquiry_;
    std::unique_ptr<NativeVisitAppointmentTime2Read> visit_;
    std::unique_ptr<NativeVisitMessageLifeRead> messages_;
    std::unique_ptr<NativeVisitReplyLifeRead> replies_;
    std::unique_ptr<NativeVisitMessageReceiptRead> receipts_;
    std::unique_ptr<NativeDirectorAppointmentAttendanceRead2> attendance_;
    std::unique_ptr<VisitReceptionWorldAccess> reception_;
    std::shared_ptr<const NativeVisitOriginalResultRead> originals_;
    NativeInteractionWorldRead(const WorldBundle&,const NativeCharacterFamilyWorldRead&,NativeInteractionSectionVersions);
public:
    NativeInteractionWorldRead(const NativeInteractionWorldRead&)=delete;
    NativeInteractionWorldRead& operator=(const NativeInteractionWorldRead&)=delete;
    NativeInteractionWorldRead(NativeInteractionWorldRead&&)=delete;
    NativeInteractionWorldRead& operator=(NativeInteractionWorldRead&&)=delete;
    static Outcome<std::unique_ptr<NativeInteractionWorldRead>> create(const WorldBundle&,const NativeCharacterFamilyWorldRead&);
    bool bound_to(const WorldBundle&,const CharacterConditionWorldRead&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&)const noexcept;
    // Construction/binding never reads pending result IDs or captures all P10.
    // The explicit full gate is called only after actual seal/life consumers.
    Status validate_bindings()const;
    Status validate_original_sources()const;
    // Stable source/PIN walk only, never a pre-seal P10 capture dependency.
    Status visit_session_life_pins(InteractionSessionLifeSourcePinSink&)const;
    Status bind_result_sources(NativeExactResultAccess&)const;
    // Called at the real Death4 write cut, while the original capture is live.
    // Message custody is adopted before Reply, then original Action consumers.
    Status apply_actual_death_in_candidate(WorldBundle&,const DeathRef&,ExactPhaseWriteContext&)const;
    // Complete original Life coverage only after this capture's callbacks drain.
    // Cold source/PIN readers keep their existing strict source gates.
    Status validate_session_life_in_capture(const ExactPhaseWriteContext&)const;
    Status bind_actual_death_result_in_candidate(WorldBundle&,ExactPhaseWriteContext&,const SourceRef&)const;
    const NativePersonalAppointmentTime2Read* personal()const noexcept{return personal_.get();}
    const NativeContactInquiryTime2Read* inquiry()const noexcept{return inquiry_.get();}
    const NativeVisitAppointmentTime2Read* visit()const noexcept{return visit_.get();}
    const NativeVisitMessageLifeRead* messages()const noexcept{return messages_.get();}
    const NativeVisitReplyLifeRead* replies()const noexcept{return replies_.get();}
    const NativeVisitMessageReceiptRead* receipts()const noexcept{return receipts_.get();}
    const NativeDirectorAppointmentAttendanceRead2* attendance()const noexcept{return attendance_.get();}
    const VisitReceptionWorldAccess* reception()const noexcept{return reception_.get();}
};
// Borrow only the real subgroup of this existing complete lease.
Outcome<const NativeInteractionWorldRead*> read_native_interaction_world(const WorldBundle&,const CompleteExactWorldReadBindings&);
}
