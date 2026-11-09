#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "profile_delivery.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/growth/card_acquisition_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <picosha2.h>
#include <algorithm>
namespace hanmo::resident {
namespace {
Status reject(const char* code){return common_failure(code);}
bool identity(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool key(const ProfileDeliveryEntry& a,ProfileDeliveryKind kind,CharacterCreationMode mode,const StableId& subject){return a.kind==kind&&a.mode==mode&&a.subject_id==subject;}
Integer acquired_tick(const ActorFoundationAt& at){return at.kind==ActorFoundationAt::Kind::legacy_tick?at.legacy_tick:at.exact_moment.half_day_tick;}
Status entry_shape(const ProfileDeliveryEntry& e){if(e.delivery_id.empty()||e.delivery_id.size()>256||e.subject_id.empty()||e.subject_id.size()>256||e.source_tick<0||(e.mode!=CharacterCreationMode::standard&&e.mode!=CharacterCreationMode::free)||(e.kind!=ProfileDeliveryKind::card_discovery&&e.kind!=ProfileDeliveryKind::actor_unlock))return reject("InvalidProfileDelivery");return common_success();}
Status verify_source(const ProfileDeliveryEntry& entry,const CardAcquisitionSources& sources,const CardAcquisitionState& state,const ProfileControlEligibilitySources& control){
    auto eligibility=read_profile_acquisition_delivery_eligibility(control,sources,state,entry.acquisition_source,entry.delivery_id);if(!eligibility.ok)return {false,{},std::move(eligibility.error)};if(entry.kind==ProfileDeliveryKind::actor_unlock&&!eligibility.value.actor_unlock_eligible)return reject("ControlledActorUnlockRequired");
    auto shape=entry_shape(entry);if(!shape.ok)return shape;auto read=read_card_acquisition_v2(sources,state,entry.acquisition_source);if(!read.ok)return {false,{},std::move(read.error)};
    const auto& r=read.value;if(entry.source_world_id!=r.canonical_identity.world_id||entry.mode!=r.mode||entry.source_tick!=acquired_tick(r.acquired_at))return reject("ProfileDeliverySourceMismatch");
    if(entry.kind==ProfileDeliveryKind::card_discovery?entry.subject_id!=r.canonical_identity.card_id:!r.actor_unlock_target||entry.subject_id!=*r.actor_unlock_target)return reject("ProfileDeliverySubjectMismatch");return common_success();
}
// 同一七字段回调用于binary、原JSON与无树投影。只有binary保原entry对象标记；
// JSON sequence的子访客本身已经是每项object，不能另加不存在的entry根。
template<class C>bool retention_only(C&)noexcept{return false;}
bool retention_only(CodecVisitor& c)noexcept{return false;}
template<class C>Status visit_entry(C& c,ProfileDeliveryEntry& e){
    const bool retained=retention_only(c);
    if(!c.reading()&&!retained){auto valid=entry_shape(e);if(!valid.ok)return valid;}
    c.field("delivery_id",e.delivery_id);
    if(!retained){
        // GCC14/libstdc++ fresh 32-byte text has one 33-byte ordinary allocation.
        // The source WorldId itself is inline; census never retains this temporary.
        CheckpointMemoryCharge world_memory(0);
        StableId world;if(!c.reading()){constexpr char digits[]="0123456789abcdef";world.resize(32);for(std::size_t n=0;n<16;++n){world[n*2]=digits[e.source_world_id[n]>>4];world[n*2+1]=digits[e.source_world_id[n]&15];}}
        c.field("source_world_id",world);
        if(c.reading()&&c.status().ok){if(world.size()!=32)return c.reject("InvalidProfileDeliveryWorld");for(std::size_t n=0;n<16;++n){auto digit=[](char v){return v>='0'&&v<='9'?v-'0':v>='a'&&v<='f'?v-'a'+10:-1;};const int hi=digit(world[n*2]),lo=digit(world[n*2+1]);if(hi<0||lo<0)return c.reject("InvalidProfileDeliveryWorld");e.source_world_id[n]=std::uint8_t(hi*16+lo);}}
    }
    c.field("source_tick",e.source_tick);
    if(!retained){
        StableId mode=e.mode==CharacterCreationMode::standard?"standard":"free",kind=e.kind==ProfileDeliveryKind::card_discovery?"card_discovery":"actor_unlock";
        c.field("mode",mode);c.field("kind",kind);
        if(c.status().ok&&((mode!="standard"&&mode!="free")||(kind!="card_discovery"&&kind!="actor_unlock")))return c.reject("InvalidProfileDeliveryTag");
        if(c.reading()&&c.status().ok){e.mode=mode=="standard"?CharacterCreationMode::standard:CharacterCreationMode::free;e.kind=kind=="card_discovery"?ProfileDeliveryKind::card_discovery:ProfileDeliveryKind::actor_unlock;}
    }
    c.field("subject_id",e.subject_id);
    // Binary/JSON visitors still take StableId names; the projection archive borrows a view.
    CheckpointMemoryCharge name_memory(0);
    c.object("acquisition_source",[&](auto& v){return visit_profile_acquisition_source(v,e.acquisition_source);});
    if(!c.status().ok)return c.status();return retained?c.status():entry_shape(e);
}
Status visit_entry_item(CodecVisitor& c,ProfileDeliveryEntry& e){return c.object("entry",[&](auto& v){return visit_entry(v,e);});}
Status visit_entry_item(ActorFoundationJsonVisitor& c,ProfileDeliveryEntry& e){return visit_entry(c,e);}
Status visit_entry_item(ActorFoundationProjectionArchive& c,ProfileDeliveryEntry& e){return visit_entry(c,e);}
// Subject cap is the standard/free UNION per kind, not two independent 4096 sets.
// Bounded borrowed comparisons preserve physical entry order and allocate no index copy.
Status outbox_shape(const ProfileDeliveryOutbox& out){
    if(out.entries.size()>16384)return reject("ProfileDeliveryCapacity");std::size_t subjects[2]{};
    for(std::size_t n=0;n<out.entries.size();++n){const auto& e=out.entries[n];auto valid=entry_shape(e);if(!valid.ok)return valid;bool seen_subject=false;
        for(std::size_t prior=0;prior<n;++prior){const auto& old=out.entries[prior];if(old.delivery_id==e.delivery_id||key(old,e.kind,e.mode,e.subject_id))return reject("DuplicateProfileDelivery");if(old.kind==e.kind&&old.subject_id==e.subject_id)seen_subject=true;}
        if(!seen_subject&&++subjects[std::size_t(e.kind)]>4096)return reject("ProfileDeliveryPartitionCapacity");
    }return common_success();
}
template<class C>Status visit_outbox(C& c,ProfileDeliveryOutbox& out){
    if(!c.reading()&&!retention_only(c)){auto valid=outbox_shape(out);if(!valid.ok)return valid;}
    auto valid=c.sequence("entries",out.entries,[](auto& row,auto& entry){return visit_entry_item(row,entry);});
    if(!valid.ok||!c.reading())return valid;return outbox_shape(out);
}
class FingerprintSink final:public CheckpointWriteSink {
public:
    Status write(const std::uint8_t* p,std::size_t n)override{while(n){const auto take=std::min<std::size_t>(n,64);hash.process(p,p+take);p+=take;n-=take;}return common_success();}
    CheckpointDigest finish(){hash.finish();CheckpointDigest result{};hash.get_hash_bytes(result.begin(),result.end());return result;}
    picosha2::hash256_one_by_one hash;
};
}
Status append_profile_acquisition_deliveries(CommonStore& common,ProfileDeliveryOutbox& out,const CardAcquisitionSources& sources,CardAcquisitionState& state,const ProfileControlEligibilitySources& control,const ProfileAcquisitionSource& source){
    auto eligibility=read_current_profile_acquisition_eligibility(control,sources,state,source);if(!eligibility.ok)return {false,{},std::move(eligibility.error)};
    auto resolved=read_card_acquisition_v2(sources,state,source);if(!resolved.ok)return {false,{},resolved.error};const auto& acquisition=resolved.value;if(acquisition.canonical_identity.actor_id!=common.state().metadata.player_actor_id)return reject("ControlledAcquisitionRequired");if(acquisition.canonical_identity.world_id!=common.state().identity.world_id)return reject("ProfileDeliveryWorldMismatch");
    auto append=[&](ProfileDeliveryKind kind,const StableId& subject)->Status{
        for(const auto& e:out.entries)if(key(e,kind,acquisition.mode,subject))return verify_source(e,sources,state,control);
        bool known_subject=false;std::size_t unique_subjects=0;
        for(std::size_t n=0;n<out.entries.size();++n){const auto& e=out.entries[n];if(e.kind!=kind)continue;if(e.subject_id==subject)known_subject=true;bool seen=false;for(std::size_t prior=0;prior<n;++prior)if(out.entries[prior].kind==kind&&out.entries[prior].subject_id==e.subject_id){seen=true;break;}if(!seen)++unique_subjects;}
        if(!known_subject&&unique_subjects>=4096)return reject("ProfileDeliveryPartitionCapacity");
        auto id=common.next_id("profile_delivery");if(!id.ok)return {false,{},id.error};ProfileDeliveryEntry entry{id.value,acquisition.canonical_identity.world_id,acquired_tick(acquisition.acquired_at),acquisition.mode,kind,subject,source};auto valid=verify_source(entry,sources,state,control);if(!valid.ok)return valid;out.entries.push_back(std::move(entry));
        if(auto mapping=std::get_if<LegacyCardSourceMappingRefV1>(&source)){auto* row=state.legacy_card_source_mappings.find(mapping->mapping_id);if(!row)return reject("ProfileDeliverySourceMissing");if(!row->profile_delivery_source_ref)row->profile_delivery_source_ref=id.value;}return common_success();};
    auto discovered=append(ProfileDeliveryKind::card_discovery,acquisition.canonical_identity.card_id);if(!discovered.ok)return discovered;if(acquisition.actor_unlock_target&&eligibility.value.actor_unlock_eligible)return append(ProfileDeliveryKind::actor_unlock,*acquisition.actor_unlock_target);return common_success();
}
Status validate_profile_delivery_outbox(const CommonState& common,const ProfileDeliveryOutbox& out,const CardAcquisitionSources& sources,const CardAcquisitionState& state,const ProfileControlEligibilitySources& control){
    if(out.entries.size()>16384)return reject("ProfileDeliveryCapacity");std::size_t subjects[2]{};
    // Keep the original source-first failure order for every cold-validated row.
    for(std::size_t n=0;n<out.entries.size();++n){const auto& e=out.entries[n];auto valid=verify_source(e,sources,state,control);if(!valid.ok)return valid;if(e.source_world_id!=common.identity.world_id||e.source_tick>common.identity.tick)return reject("ProfileDeliveryWorldMismatch");bool seen_subject=false;
        for(std::size_t prior=0;prior<n;++prior)if(out.entries[prior].kind==e.kind&&out.entries[prior].subject_id==e.subject_id)seen_subject=true;
        if(!seen_subject&&++subjects[std::size_t(e.kind)]>4096)return reject("ProfileDeliveryPartitionCapacity");
        for(std::size_t prior=0;prior<n;++prior)if(out.entries[prior].delivery_id==e.delivery_id||key(out.entries[prior],e.kind,e.mode,e.subject_id))return reject("DuplicateProfileDelivery");
    }return common_success();
}
Outcome<PendingProfileDeliveries> read_pending_profile_deliveries(const WorldIdentity& source,const ProfileDeliveryOutbox& out,std::optional<ProfileDeliveryCursor> cursor,Integer limit){
    if(limit<1||limit>100)return {false,{},reject("InvalidProfileDeliveryPage").error};if(cursor&&!identity(cursor->source,source))return {false,{},reject("StaleProfileDeliveryCursor").error};const auto start=cursor?cursor->offset:0;if(start>out.entries.size())return {false,{},reject("InvalidProfileDeliveryCursor").error};PendingProfileDeliveries page;page.source_identity=source;const auto end=std::min<std::uint64_t>(out.entries.size(),start+std::uint64_t(limit));page.entries.assign(out.entries.begin()+start,out.entries.begin()+end);if(end<out.entries.size())page.cursor=ProfileDeliveryCursor{source,end};return {true,std::move(page),{}};
}
Status visit_profile_delivery_outbox(CodecVisitor& c,ProfileDeliveryOutbox& out){return visit_outbox(c,out);}
Status emit_profile_delivery_outbox_fields(ProjectionWriter& sink,const ProfileDeliveryOutbox& out){ActorFoundationProjectionArchive archive(sink);return visit_outbox(archive,const_cast<ProfileDeliveryOutbox&>(out));}
Outcome<BoundaryValue> encode_profile_delivery_outbox(const ProfileDeliveryOutbox& out){BoundaryValue value;BoundaryProjectionWriter sink(value);auto valid=sink.object("",[&](auto& writer){return emit_profile_delivery_outbox_fields(writer,out);});if(valid.ok)valid=sink.finish();return {valid.ok,std::move(value),std::move(valid.error)};}
Outcome<ProfileDeliveryOutbox> decode_profile_delivery_outbox(const BoundaryValue& input){
    // Reject the original finite row cap before a reader may allocate any entry block.
    const auto* root=input.as<BoundaryValue::Dict>();if(!root)return {false,{},reject("InvalidResidentObject").error};
    for(const auto& field:*root)if(field.first=="entries"){const auto* entries=field.second.as<BoundaryValue::Array>();if(entries&&entries->size()>16384)return {false,{},reject("ProfileDeliveryCapacity").error};}
    ProfileDeliveryOutbox out;ActorFoundationJsonVisitor reader(input);auto valid=visit_outbox(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),std::move(valid.error)};
}
CheckpointJobResult<CheckpointDigest> profile_delivery_fingerprint(const ProfileDeliveryEntry& entry,const CardAcquisitionSources& sources,const CardAcquisitionState& state,const ProfileControlEligibilitySources& control,const CapacityProfile& profile,CheckpointJobBudget& job){
    try{auto checked=job.check_profile(profile);if(!checked.ok)return {false,{},std::move(checked.error)};auto valid=verify_source(entry,sources,state,control);if(!valid.ok)return {false,{},job.capture_failure(std::move(valid.error))};auto reserved=job.reserve(CheckpointJobUse::scratch,1024);if(!reserved.ok)return {false,{},std::move(reserved.error)};
        FingerprintSink sink;CodecVisitor codec(sink,profile,"growth",profile.max_checkpoint_bytes);
        // entry字符串不复制：写visitor只借已提交typed条目；authority仍是上面的真实S4核验。
        auto& borrowed=const_cast<ProfileDeliveryEntry&>(entry);auto wrote=visit_entry(codec,borrowed);if(!wrote.ok)return {false,{},job.capture_failure(std::move(wrote.error))};wrote=codec.object("resolved_acquisition_source",[&](auto& v){return visit_resolved_profile_acquisition_source(v,sources,state,entry.acquisition_source);});if(!wrote.ok)return {false,{},job.capture_failure(std::move(wrote.error))};wrote=codec.finish();if(!wrote.ok)return {false,{},job.capture_failure(std::move(wrote.error))};return {true,sink.finish(),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
} // 调用者的原WorldBundle transaction负责失败回滚，不能把单独Common savepoint冒完整事务。
