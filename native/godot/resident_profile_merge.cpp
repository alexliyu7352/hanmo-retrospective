#include "resident_profile_merge.hpp"
#include "resident_world_import.hpp"
#include "resident_profile_copy.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <algorithm>
#include <cstring>
namespace hanmo::resident::storage {
namespace {
template<class T>Outcome<T> fail(const char* code){return {false,{},profile_admission_failure(code).error};}
bool entry_less(const EntryFingerprint&a,const EntryFingerprint&b){return a.key.source_world_id<b.key.source_world_id||(a.key.source_world_id==b.key.source_world_id&&a.key.delivery_id<b.key.delivery_id);}
const EntryFingerprint* find(const std::vector<EntryFingerprint>& list,const DeliveryKey& key){for(const auto& e:list)if(same_delivery_key(e.key,key))return &e;return nullptr;}
bool allowed(const AdmissionRecord& record,const EntryFingerprint& e){const auto* baseline=find(record.capsule.baseline,e.key);if(!baseline)return true;if(!same_entry_fingerprint(*baseline,e))return false;const auto* history=find(record.allowed_history,e.key);return history&&same_entry_fingerprint(*history,e);}
}
template<class Projection>
Outcome<ProfileMergeCandidate> merge_validated_projection(const ProfileMetadataSnapshot& before,const StableId& admission_id,Integer generation,const Projection& batch,const ProfileStoreLimits& limits,const std::vector<EntryFingerprint>* cards=nullptr,const std::vector<EntryFingerprint>* unlocks=nullptr){
    if(!batch.validated())return fail<ProfileMergeCandidate>("CommittedProfileProjectionRequired");
    if(generation!=before.generation)return fail<ProfileMergeCandidate>("ProfileGenerationConflict");const auto* record=find_profile_admission(before,admission_id);if(!record)return fail<ProfileMergeCandidate>("MissingLocalAdmissionRecord");if(record->capsule.source_world_id!=batch.source().world_id||!same_profile_namespace(before.name_space,record->capsule.name_space))return fail<ProfileMergeCandidate>("AdmissionNamespaceMismatch");auto verified=validate_admission_record(*record,limits.admission);if(!verified.ok)return {false,{},verified.error};if(generation<0||generation==INT64_MAX)return fail<ProfileMergeCandidate>("ProfileGenerationOverflow");
    ProfileMergeCandidate result;result.snapshot=before;result.snapshot.wire_version=2;
    reserve_profile_rows(result.accepted_delivery_ids,batch.entries().size(),result.result_memory);reserve_profile_rows(result.already_present_ids,batch.entries().size(),result.result_memory);reserve_profile_rows(result.historical_quarantined_ids,batch.entries().size(),result.result_memory);
    const auto result_id=[&](std::vector<StableId>& to,const StableId& id){(result.result_memory).grow(0);to.push_back(StableId(id));};
    for(const auto& row:batch.entries()){
        if(cards&&unlocks&&!find(*cards,row.fingerprint.key)&&!find(*unlocks,row.fingerprint.key))continue;
        if(row.entry.source_world_id!=batch.source().world_id||row.fingerprint.key.source_world_id!=row.entry.source_world_id||row.fingerprint.key.delivery_id!=row.entry.delivery_id)return fail<ProfileMergeCandidate>("ProfileDeliverySourceMismatch");
        if(const auto* baseline=find(record->capsule.baseline,row.fingerprint.key);baseline&&!same_entry_fingerprint(*baseline,row.fingerprint))return fail<ProfileMergeCandidate>("AdmissionBaselineFingerprintConflict");
        if(!allowed(*record,row.fingerprint)){result_id(result.historical_quarantined_ids,row.entry.delivery_id);continue;}auto shape=validate_resolved_profile_acquisition_shape(row.acquisition);if(!shape.ok)return {false,{},shape.error};
        if(const auto* ack=find(before.acknowledgements,row.fingerprint.key)){if(!same_entry_fingerprint(*ack,row.fingerprint))return fail<ProfileMergeCandidate>("ProfileDeliveryFingerprintConflict");result_id(result.already_present_ids,row.entry.delivery_id);continue;}
        bool added=false;const bool free=row.entry.mode==CharacterCreationMode::free;
        if(row.entry.kind==ProfileDeliveryKind::card_discovery){auto& cards=free?result.snapshot.free_cards:result.snapshot.standard_cards;auto at=std::lower_bound(cards.begin(),cards.end(),row.entry.subject_id);if(at==cards.end()||*at!=row.entry.subject_id){if(cards.size()>=4096)return fail<ProfileMergeCandidate>("CardDiscoveryCapacityExceeded");const auto pos=std::size_t(at-cards.begin());reserve_profile_rows(cards,cards.size()+1,result.snapshot.owned_memory);(result.snapshot.owned_memory).grow(0);cards.insert(cards.begin()+pos,StableId(row.entry.subject_id));added=true;}}
        else{auto& actors=free?result.snapshot.free_actors:result.snapshot.standard_actors;const auto target=row.acquisition.canonical.actor_unlock_target;if(!target||*target!=row.entry.subject_id)return fail<ProfileMergeCandidate>("ActorUnlockSourceRequired");auto at=std::lower_bound(actors.begin(),actors.end(),*target,[](const auto& a,const auto& id){return *a.canonical.actor_unlock_target<id;});if(at==actors.end()||*at->canonical.actor_unlock_target!=*target){if(actors.size()>=4096)return fail<ProfileMergeCandidate>("ActorUnlockCapacityExceeded");const auto pos=std::size_t(at-actors.begin());reserve_profile_rows(actors,actors.size()+1,result.snapshot.owned_memory);(result.snapshot.owned_memory).grow(0);actors.insert(actors.begin()+pos,ResolvedProfileAcquisition(row.acquisition));added=true;}}
        if(added)result_id(result.accepted_delivery_ids,row.entry.delivery_id);else result_id(result.already_present_ids,row.entry.delivery_id);
        // 获准且真实核源的重复subject同样记录本delivery ack；隔离entry从不借全局ack获准。
        if(result.snapshot.acknowledgements.size()>=admission_fingerprint_limit)return fail<ProfileMergeCandidate>("ProfileAcknowledgementCapacityExceeded");
        reserve_profile_rows(result.snapshot.acknowledgements,result.snapshot.acknowledgements.size()+1,result.snapshot.owned_memory);(result.snapshot.owned_memory).grow(0);result.snapshot.acknowledgements.push_back(row.fingerprint);
    }
    std::sort(result.snapshot.acknowledgements.begin(),result.snapshot.acknowledgements.end(),entry_less);result.snapshot.generation=generation+1;auto shape=validate_profile_metadata_complete(result.snapshot,limits);if(!shape.ok)return {false,{},shape.error};return {true,std::move(result),{}};
}
Outcome<ProfileMergeCandidate> merge_profile_deliveries(const ProfileMetadataSnapshot& before,const StableId& admission,Integer generation,const CommittedProfileDeliveries& batch,const ProfileStoreLimits& limits){return merge_validated_projection(before,admission,generation,batch,limits);}
Outcome<ProfileMergeCandidate> merge_profile_deliveries(const ProfileMetadataSnapshot& before,const StableId& id,Integer record_generation,Integer generation,const CommittedProfileDeliveries& batch,const ProfileStoreLimits& limits){
    const auto* record=find_profile_admission(before,id);if(!record)return fail<ProfileMergeCandidate>("MissingLocalAdmissionRecord");
    if(record->record_generation!=record_generation)return fail<ProfileMergeCandidate>("AdmissionGenerationConflict");
    return merge_validated_projection(before,id,generation,batch,limits);
}
Outcome<ProfileMergeCandidate> grant_imported_history(const ProfileMetadataSnapshot& before,const StableId& id,Integer record_generation,Integer generation,const std::vector<EntryFingerprint>& cards,const std::vector<EntryFingerprint>& unlocks,const CommittedProfileDeliveries& batch,const ProfileStoreLimits& limits){
    if(!batch.validated())return fail<ProfileMergeCandidate>("CommittedProfileProjectionRequired");
    if(generation!=before.generation)return fail<ProfileMergeCandidate>("ProfileGenerationConflict");
    const auto* original=find_profile_admission(before,id);if(!original)return fail<ProfileMergeCandidate>("MissingLocalAdmissionRecord");
    if(original->origin!=AdmissionOrigin::imported)return fail<ProfileMergeCandidate>("ImportedAdmissionRequired");
    if(original->record_generation!=record_generation)return fail<ProfileMergeCandidate>("AdmissionGenerationConflict");
    if(record_generation==INT64_MAX)return fail<ProfileMergeCandidate>("AdmissionGenerationOverflow");
    if(original->capsule.source_world_id!=batch.source().world_id)return fail<ProfileMergeCandidate>("ProfileDeliverySourceMismatch");
    if(cards.size()>admission_fingerprint_limit||unlocks.size()>admission_fingerprint_limit-cards.size())return fail<ProfileMergeCandidate>("AdmissionBaselineLimit");
    // 所有选择先核原baseline及真实完整来源；任一项不合法整批不产生候选。
    for(unsigned kind=0;kind<2;++kind){const auto& wanted=kind?unlocks:cards;
        for(std::size_t i=0;i<wanted.size();++i){const auto& e=wanted[i];const auto* baseline=find(original->capsule.baseline,e.key);
            if(!baseline||!same_entry_fingerprint(*baseline,e))return fail<ProfileMergeCandidate>("AdmissionHistoryOutsideBaseline");
            for(std::size_t j=0;j<i;++j)if(same_delivery_key(wanted[j].key,e.key))return fail<ProfileMergeCandidate>("DuplicateImportHistorySelection");
            if(find(kind?cards:unlocks,e.key))return fail<ProfileMergeCandidate>("DuplicateImportHistorySelection");
            const ResolvedProfileDelivery* actual=nullptr;for(const auto& row:batch.entries())if(same_entry_fingerprint(row.fingerprint,e)){actual=&row;break;}
            if(!actual||actual->entry.kind!=(kind?ProfileDeliveryKind::actor_unlock:ProfileDeliveryKind::card_discovery))return fail<ProfileMergeCandidate>("ImportHistorySelectionMismatch");
            auto shape=validate_resolved_profile_acquisition_shape(actual->acquisition);if(!shape.ok)return {false,{},shape.error};
        }
    }
    ProfileMetadataSnapshot candidate=before;AdmissionRecord* changed=nullptr;for(auto& record:candidate.records)if(record.capsule.admission_id==id){changed=&record;break;}
    for(const auto* selection:{&cards,&unlocks})for(const auto& e:*selection){const auto* existing=find(changed->allowed_history,e.key);
        if(existing){if(!same_entry_fingerprint(*existing,e))return fail<ProfileMergeCandidate>("AdmissionHistoryFingerprintConflict");continue;}
        reserve_profile_rows(changed->allowed_history,changed->allowed_history.size()+1,candidate.owned_memory);(candidate.owned_memory).grow(0);changed->allowed_history.push_back(e);
    }
    std::sort(changed->allowed_history.begin(),changed->allowed_history.end(),entry_less);++changed->record_generation;
    auto valid=validate_admission_record(*changed,limits.admission);if(!valid.ok)return {false,{},valid.error};
    return merge_validated_projection(candidate,id,generation,batch,limits,&cards,&unlocks);
}
Outcome<ProfileMergeCandidate> merge_import_admission(const ProfileMetadataSnapshot& before,const AdmissionRecord& record,Integer generation,const ValidatedImportDeliveries& batch,const ProfileStoreLimits& limits){
    if(!batch.validated()||record.origin!=AdmissionOrigin::imported||record.capsule.source_world_id!=batch.source().world_id)return fail<ProfileMergeCandidate>("ValidatedImportProjectionRequired");
    if(generation!=before.generation)return fail<ProfileMergeCandidate>("ProfileGenerationConflict");if(find_profile_admission(before,record.capsule.admission_id))return fail<ProfileMergeCandidate>("AdmissionIdentityConflict");if(before.records.size()>=limits.max_records)return fail<ProfileMergeCandidate>("AdmissionRecordCapacityExceeded");
    auto valid=validate_admission_record(record,limits.admission);if(!valid.ok)return {false,{},valid.error};if(record.capsule.baseline.size()!=batch.entries().size())return fail<ProfileMergeCandidate>("AdmissionBaselineMismatch");
    for(const auto& row:batch.entries()){const auto* original=find(record.capsule.baseline,row.fingerprint.key);if(!original||!same_entry_fingerprint(*original,row.fingerprint))return fail<ProfileMergeCandidate>("AdmissionBaselineMismatch");}
    ProfileMetadataSnapshot candidate=before;reserve_profile_rows(candidate.records,candidate.records.size()+1,candidate.owned_memory);(candidate.owned_memory).grow(0);candidate.records.push_back(record);std::sort(candidate.records.begin(),candidate.records.end(),[](const auto&a,const auto&b){return a.capsule.admission_id<b.capsule.admission_id;});return merge_validated_projection(candidate,record.capsule.admission_id,generation,batch,limits);
}
// 仅原同一accept的重试口。已有record不能换baseline，既有独立授权只能保留，不能回退或冒外来同意。
Outcome<ProfileMergeCandidate> resume_import_admission(const ProfileMetadataSnapshot& before,const AdmissionRecord& record,Integer generation,const ValidatedImportDeliveries& batch,const ProfileStoreLimits& limits){
    const auto* existing=find_profile_admission(before,record.capsule.admission_id);if(!existing)return merge_import_admission(before,record,generation,batch,limits);
    if(existing->origin!=AdmissionOrigin::imported||!same_admission_capsule(existing->capsule,record.capsule))return fail<ProfileMergeCandidate>("AdmissionRecordConflict");
    for(const auto& wanted:record.allowed_history){const auto* actual=find(existing->allowed_history,wanted.key);if(!actual||!same_entry_fingerprint(*actual,wanted))return fail<ProfileMergeCandidate>("AdmissionHistoryConflict");}
    return merge_validated_projection(before,record.capsule.admission_id,generation,batch,limits);
}
Outcome<AdmissionRecord> create_import_admission(const SlotAdmissionCapsule& capsule,const std::vector<EntryFingerprint>& outbox,const AdmissionCodecLimits& limits){AdmissionRecord record;record.capsule=capsule;record.origin=AdmissionOrigin::imported;record.record_generation=0;if(capsule.baseline.size()!=outbox.size())return fail<AdmissionRecord>("AdmissionBaselineMismatch");for(std::size_t n=0;n<outbox.size();++n)if(!same_entry_fingerprint(capsule.baseline[n],outbox[n]))return fail<AdmissionRecord>("AdmissionBaselineMismatch");auto valid=validate_admission_record(record,limits);if(!valid.ok)return {false,{},valid.error};return {true,std::move(record),{}};}
Outcome<AdmissionRecord> authorize_admission_history(const AdmissionRecord& original,const std::vector<EntryFingerprint>& requested,const AdmissionCodecLimits& limits){auto valid=validate_admission_record(original,limits);if(!valid.ok)return {false,{},valid.error};if(original.record_generation==INT64_MAX)return fail<AdmissionRecord>("AdmissionGenerationOverflow");AdmissionRecord next=original;for(const auto& e:requested){const auto* baseline=find(original.capsule.baseline,e.key);if(!baseline||!same_entry_fingerprint(*baseline,e))return fail<AdmissionRecord>("AdmissionHistoryOutsideBaseline");const auto* existing=find(next.allowed_history,e.key);if(existing&&!same_entry_fingerprint(*existing,e))return fail<AdmissionRecord>("AdmissionHistoryFingerprintConflict");if(!existing)next.allowed_history.push_back(e);}std::sort(next.allowed_history.begin(),next.allowed_history.end(),entry_less);next.record_generation++;valid=validate_admission_record(next,limits);if(!valid.ok)return {false,{},valid.error};return {true,std::move(next),{}};}
}
