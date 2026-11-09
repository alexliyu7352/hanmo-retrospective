// 真正已提交WorldBundle才能产生的完整跨局投影；worker只持拥有结果。
#pragma once
#include "profile_delivery.hpp"
namespace hanmo::resident {
class NativeBoundarySession;class ResidentWorld;class WorldRestoreCandidate;struct ProfileDeliveryCaptureAccess;
struct ResolvedProfileDelivery {ProfileDeliveryEntry entry;EntryFingerprint fingerprint;ResolvedProfileAcquisition acquisition;};
class CommittedProfileDeliveries {
public:
    CommittedProfileDeliveries()=default;
    CommittedProfileDeliveries(const CommittedProfileDeliveries&)=delete;
    CommittedProfileDeliveries& operator=(const CommittedProfileDeliveries&)=delete;
    CommittedProfileDeliveries(CommittedProfileDeliveries&&)noexcept=default;
    CommittedProfileDeliveries& operator=(CommittedProfileDeliveries&&)noexcept;
    bool validated()const noexcept{return validated_;}
    const WorldIdentity& source()const noexcept{return source_;}
    const std::vector<ResolvedProfileDelivery>& entries()const noexcept{return entries_;}
private:
    friend class ResidentWorld;friend class NativeBoundarySession;friend struct ProfileDeliveryCaptureAccess;
    static CheckpointJobResult<CommittedProfileDeliveries> capture(const CommonState&,const ProfileDeliveryOutbox&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileControlEligibilitySources&,const CapacityProfile&,CheckpointJobBudget&);
    CheckpointJobReservation memory_;
    WorldIdentity source_;
    std::vector<ResolvedProfileDelivery> entries_;
    bool validated_=false;
};
// 私有冷候选的原source反验结果；没有已提交身份，不能传入自动Merge入口。
class ValidatedImportDeliveries {
public:
    ValidatedImportDeliveries()=default;
    ValidatedImportDeliveries(const ValidatedImportDeliveries&)=delete;
    ValidatedImportDeliveries& operator=(const ValidatedImportDeliveries&)=delete;
    ValidatedImportDeliveries(ValidatedImportDeliveries&&)noexcept=default;
    ValidatedImportDeliveries& operator=(ValidatedImportDeliveries&&)noexcept;
    bool validated()const noexcept{return validated_;}
    const WorldIdentity& source()const noexcept{return source_;}
    const std::vector<ResolvedProfileDelivery>& entries()const noexcept{return entries_;}
private:
    friend class WorldRestoreCandidate;friend struct ProfileDeliveryCaptureAccess;
    static CheckpointJobResult<ValidatedImportDeliveries> capture(const CommonState&,const ProfileDeliveryOutbox&,const CardAcquisitionSources&,const CardAcquisitionState&,const ProfileControlEligibilitySources&,const CapacityProfile&,CheckpointJobBudget&);
    CheckpointJobReservation memory_;
    WorldIdentity source_;
    std::vector<ResolvedProfileDelivery> entries_;
    bool validated_=false;
};
}
