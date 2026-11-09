// Short-lived original receipt readers, borrowing the same real Family owner.
#pragma once
#include "visit_message_receipts.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class NativeVisitMessageReceiptRead final:public CheckpointAllocatedObject {
 const WorldBundle& world_;const ProfileControlEligibilitySources& controls_;
 WorldCurrentMomentRead clock_;VisitMessageDirectorCommandSources commands_;
 VisitMessageReceiptRead context_;
 NativeVisitMessageReceiptRead(const WorldBundle&,const ProfileControlEligibilitySources&);
 Status binding()const;
public:
 static Outcome<std::unique_ptr<NativeVisitMessageReceiptRead>> create(const WorldBundle&,const ProfileControlEligibilitySources&);
 NativeVisitMessageReceiptRead(const NativeVisitMessageReceiptRead&)=delete;
 NativeVisitMessageReceiptRead&operator=(const NativeVisitMessageReceiptRead&)=delete;
 Outcome<const VisitMessageReceiptRead*> context()const;
};
}
