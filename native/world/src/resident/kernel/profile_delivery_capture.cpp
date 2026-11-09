#include "profile_delivery_capture.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
namespace hanmo::resident {
CommittedProfileDeliveries& CommittedProfileDeliveries::operator=(CommittedProfileDeliveries&& other)noexcept{if(this!=&other){this->~CommittedProfileDeliveries();new(this)CommittedProfileDeliveries(std::move(other));}return *this;}
namespace {
// 单条来源两遍写同一原S4字段；小对象复制也使用真实visitor的候选计费，不借整world长度估算。
class AcquisitionSink final:public CheckpointWriteSink {
public:
    explicit AcquisitionSink(std::uint64_t limit,CheckpointJobBytes* output=nullptr):limit(limit),output(output){}
    Status write(const std::uint8_t* data,std::size_t size)override{if(at>limit||size>limit-at)return common_failure("ProfileSourceCapacityExceeded",{},ErrorKind::internal);if(output){auto written=output->write(std::size_t(at),data,size);if(!written.ok)throw checkpoint_job_detail::FailureException{std::move(written.error)};}at+=size;return {true,{},{}};}
    std::uint64_t limit,at=0;CheckpointJobBytes* output;
};
CheckpointJobResult<ResolvedProfileAcquisition> copy_acquisition(const ProfileAcquisitionSource& source,const CardAcquisitionSources& sources,const CardAcquisitionState& acquisitions,const CapacityProfile& profile,CodecDecodeBudget& budget,CheckpointJobBudget& job){
    const auto limit=profile.max_section_bytes_by_registered_name.at("growth");AcquisitionSink measured(limit);CodecVisitor counter(measured,profile,"growth",limit,&job);auto result=visit_resolved_profile_acquisition_source(counter,sources,acquisitions,source);if(result.ok)result=counter.finish();if(!result.ok)return {false,{},job.capture_failure(std::move(result.error))};
    auto encoded=CheckpointJobBytes::allocate(job,CheckpointJobUse::scratch,measured.at);if(!encoded.ok)return {false,{},std::move(encoded.error)};AcquisitionSink output(measured.at,&encoded.value);CodecVisitor writer(output,profile,"growth",measured.at,&job);result=visit_resolved_profile_acquisition_source(writer,sources,acquisitions,source);if(result.ok)result=writer.finish();if(!result.ok)return {false,{},job.capture_failure(std::move(result.error))};if(output.at!=measured.at)return {false,{},job.capture_boundary_failure("ProfileSourceChanged",{},ErrorKind::rule)};
    ResolvedProfileAcquisition value;CodecVisitor reader(encoded.value.bytes(),profile,"growth",budget);result=visit_resolved_profile_acquisition(reader,value);if(result.ok)result=reader.finish();if(!result.ok)return {false,{},job.capture_failure(std::move(result.error))};return {true,std::move(value),{}};
}

}
struct ProfileDeliveryCaptureAccess {
template<class Batch>static CheckpointJobResult<Batch> capture(const CommonState& common,const ProfileDeliveryOutbox& outbox,const CardAcquisitionSources& sources,const CardAcquisitionState& acquisitions,const ProfileControlEligibilitySources& control,const CapacityProfile& profile,CheckpointJobBudget& job){
    try{auto valid=validate_profile_delivery_outbox(common,outbox,sources,acquisitions,control);if(!valid.ok)return {false,{},job.capture_failure(std::move(valid.error))};
        auto memory=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!memory.ok)return {false,{},std::move(memory.error)};ScopedCheckpointMemory scope(memory.value);
        CodecDecodeBudget budget(profile,job);auto charged=budget.charge_candidate(outbox.entries.size()*sizeof(ResolvedProfileDelivery));if(!charged.ok)return {false,{},job.capture_failure(std::move(charged.error))};Batch batch;batch.source_=common.identity;batch.entries_.reserve(outbox.entries.size());
        for(const auto& e:outbox.entries){auto digest=profile_delivery_fingerprint(e,sources,acquisitions,control,profile,job);if(!digest.ok)return {false,{},std::move(digest.error)};
            // 引用/条目每份字符串先收费，完整来源由原paired visitor构造私有候选。
            const auto ref_bytes=std::visit([](const auto& ref)->std::size_t{using T=std::decay_t<decltype(ref)>;if constexpr(std::is_same_v<T,CardAcquisitionReceiptRefV2>)return ref.receipt_id.size()+1;else return ref.mapping_id.size()+1;},e.acquisition_source);
            auto charge=budget.charge_candidate(2*(e.delivery_id.size()+1)+e.subject_id.size()+1+ref_bytes);if(!charge.ok)return {false,{},job.capture_failure(std::move(charge.error))};
            auto resolved=copy_acquisition(e.acquisition_source,sources,acquisitions,profile,budget,job);if(!resolved.ok)return {false,{},std::move(resolved.error)};
            batch.entries_.push_back({e,{{e.source_world_id,e.delivery_id},digest.value},std::move(resolved.value)});
        }batch.memory_=budget.take_job_reservation();batch.validated_=true;return {true,std::move(batch),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
};
CheckpointJobResult<CommittedProfileDeliveries> CommittedProfileDeliveries::capture(const CommonState& common,const ProfileDeliveryOutbox& outbox,const CardAcquisitionSources& sources,const CardAcquisitionState& acquisitions,const ProfileControlEligibilitySources& control,const CapacityProfile& profile,CheckpointJobBudget& job){return ProfileDeliveryCaptureAccess::capture<CommittedProfileDeliveries>(common,outbox,sources,acquisitions,control,profile,job);}
CheckpointJobResult<ValidatedImportDeliveries> ValidatedImportDeliveries::capture(const CommonState& common,const ProfileDeliveryOutbox& outbox,const CardAcquisitionSources& sources,const CardAcquisitionState& acquisitions,const ProfileControlEligibilitySources& control,const CapacityProfile& profile,CheckpointJobBudget& job){return ProfileDeliveryCaptureAccess::capture<ValidatedImportDeliveries>(common,outbox,sources,acquisitions,control,profile,job);}
ValidatedImportDeliveries& ValidatedImportDeliveries::operator=(ValidatedImportDeliveries&& other)noexcept{if(this!=&other){this->~ValidatedImportDeliveries();new(this)ValidatedImportDeliveries(std::move(other));}return *this;}
}
