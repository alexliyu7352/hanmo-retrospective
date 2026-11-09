// 短栈服务拥有InteractionAccess，所有引用固定借同一World候选，不缓存过钟指针。
#pragma once
#include "resident/domains/interaction/visit_appointments/visit_appointment_director.hpp"
#include "visit_reception_director.hpp"
#include "visit_message_native.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_director.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_native.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_native.hpp"
#include "resident/kernel/exact_complete_world.hpp"
namespace hanmo::resident {
struct WorldBundle;
class VisitReceptionWorldAccess : public CheckpointAllocatedObject {
public:
    static Outcome<std::unique_ptr<VisitReceptionWorldAccess>> create(const WorldBundle&,const CompleteExactWorldReadBindings* actual=nullptr);
    // Inner constructor borrows the actual previously constructed Family owners.
    // It never calls the outer complete factory or captures pending P10 sources.
    static Outcome<std::unique_ptr<VisitReceptionWorldAccess>> create(const WorldBundle&,const CharacterConditionWorldRead&,const FamilyContinuationPeopleRead4&,const ProfileControlEligibilitySources&);
    VisitReceptionWorldAccess(const VisitReceptionWorldAccess&)=delete;
    VisitReceptionWorldAccess& operator=(const VisitReceptionWorldAccess&)=delete;
    const VisitReceptionRead& context()const{return read_;}
private:
    explicit VisitReceptionWorldAccess(const WorldBundle&);
    std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
    WorldCurrentMomentRead clock_;std::unique_ptr<NativeVisitMessageLifeRead> message_original_;
    std::unique_ptr<NativeVisitReplyLifeRead> reply_original_;std::unique_ptr<NativeVisitAppointmentTime2Read> invitation_original_;
    InteractionAccess access_;std::optional<VisitReplyNpcRead> replies_;std::optional<VisitAppointmentNpcRead> invitations_;VisitReceptionRead read_;
};
// 借同一候选的真实接待读面执行完整冷门，构造内部 owning 时不回取外层工厂。
Status validate_visit_reception_world(const WorldBundle&,const VisitReceptionRead&);
Status validate_visit_reception_world(const WorldBundle&);
Outcome<bool> visit_reception_conversation_world(const WorldBundle&,const StableId& actor_id,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<DecisionProposal> bind_visit_reception_world(WorldBundle&,const DecisionProposal&);
}
