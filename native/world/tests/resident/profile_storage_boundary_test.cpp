// 实际Linux worker目标隔离与无准入能力拒绝门；人工字节只测介质，不冒真实本机创建。
#include "resident_profile_admission.hpp"
#include "resident_storage_service.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unistd.h>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
using namespace std::chrono_literals;
#ifndef HANMO_STORAGE_TEST_FAULTS
#error "This focused test requires HANMO_STORAGE_TEST_FAULTS"
#endif
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Fn>void reject(Fn fn,const char* code){bool caught=false;try{fn();}catch(const SlotMediaError& e){caught=true;check(std::string(e.what())==code,"exact physical target error");}check(caught,"actual target was rejected");}
StoragePoll wait(ResidentStorageService& service,const StorageOperationRef& ref,StorageState state){const auto end=std::chrono::steady_clock::now()+5s;while(std::chrono::steady_clock::now()<end){auto p=service.poll(ref);check(p.ok,"original operation reference remains valid");if(p.state==state)return p;if(p.state==StorageState::failed)throw std::runtime_error(std::string("unexpected worker failure: ")+std::string(p.code.view()));std::this_thread::sleep_for(1ms);}throw std::runtime_error("physical worker timeout");}
void close(ResidentStorageService& s){s.request_close();const auto end=std::chrono::steady_clock::now()+5s;while(!s.close_complete()&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(1ms);check(s.close_complete(),"real close ack");}
}
int main(){try{
    static_assert(!std::is_constructible<ProfileValidatedStage>::value,"caller cannot mint profile promotion permission");
    static_assert(!std::is_copy_constructible<ProfileValidatedStage>::value,"capability is not copyable");
    char pattern[]="/tmp/hanmo-profile-media-XXXXXX";const char* made=::mkdtemp(pattern);check(made,"own bounded physical fixture");const std::string root=made;std::cout<<"PROFILE_STORAGE_TEST_DIRECTORY="<<root<<'\n';
    const ProfileNamespace host{"native.test",ProfilePurpose::development,{}};auto binding=profile_namespace_binding(host);check(binding.ok,"host-derived namespace binding");const SlotKey metadata{"profile_metadata",SlotFileKind::profile_metadata,binding.value};SlotMediaLimits world_limits{4096,8192,16384},profile_limits=world_limits;profile_limits.host_profile_namespace_binding=binding.value;
    {
        ResidentSlotMedia world(root+"/world",world_limits);SlotCancellation cancel;reject([&]{world.inspect(metadata,cancel);},"StorageProfileNamespaceMismatch");check(!world.inspect({"manual-1",SlotFileKind::resident_checkpoint},cancel).a.present,"original world allowlist still usable");
    }
    {
        ResidentSlotMedia profile(root+"/profile",profile_limits);SlotCancellation cancel;reject([&]{profile.inspect({"manual-1",SlotFileKind::resident_checkpoint},cancel);},"StorageTargetMismatch");auto wrong=metadata;wrong.profile_namespace_binding[0]=wrong.profile_namespace_binding[0]=='0'?'1':'0';reject([&]{profile.inspect(wrong,cancel);},"StorageProfileNamespaceMismatch");wrong=metadata;wrong.id="manual-1";reject([&]{profile.inspect(wrong,cancel);},"StorageProfileNamespaceMismatch");auto pair=profile.inspect(metadata,cancel);check(!pair.a.present&&!pair.b.present,"profile has its own fixed empty banks");
    }
    {
        ResidentStorageService world(root+"/world-worker",{world_limits,4,32768});check(!world.inspect(metadata,{}).ok,"world worker rejects profile target before accepting ticket");SlotWriteRequest request;request.slot=metadata;request.bytes={1,2,3};check(!world.stage(request).ok,"world worker cannot stage metadata");close(world);
    }
    {
        ResidentStorageService profile(root+"/profile-worker",{profile_limits,4,32768});check(!profile.inspect({"manual-1",SlotFileKind::resident_checkpoint},{}).ok,"profile worker rejects world target before accepting ticket");SlotWriteRequest request;request.slot=metadata;request.bytes={1,2,3};request.source.world_id[0]=1;request.source.execution_epoch=1;
        auto wrong=request;wrong.slot.profile_namespace_binding[0]=wrong.slot.profile_namespace_binding[0]=='0'?'1':'0';check(!profile.stage(wrong).ok,"host namespace cannot be rebound by request");auto accepted=profile.stage(request);check(accepted.ok,"physical staging alone may accept arbitrary frozen bytes");auto staged=wait(profile,accepted.reference,StorageState::awaiting_owner_validation);auto bypass=profile.promote_after_owner_validation(accepted.reference,staged.stage_token);check(!bypass.ok&&bypass.code=="ProfileValidatedStageRequired","old world promotion cannot publish profile bytes even with exact physical token");check(!std::filesystem::exists(root+"/profile-worker/profile_metadata.B.hmprofile"),"valid stage is not visible before semantic capability");check(profile.cancel(accepted.reference).ok,"cancel unapproved profile stage");wait(profile,accepted.reference,StorageState::cancelled);check(profile.release(accepted.reference).ok,"release actual cancelled operation");check(!profile.poll(accepted.reference).ok,"late old profile ticket cannot attach to another operation");auto next=profile.inspect(metadata,{});check(next.ok,"new genuine read accepted");auto read=wait(profile,next.reference,StorageState::completed);check(read.banks&&!read.banks->a.present&&!read.banks->b.present,"cancelled stage did not publish any bank");check(!profile.promote_after_owner_validation(next.reference,staged.stage_token).ok,"old token cannot promote new read operation");check(profile.release(next.reference).ok,"read released");close(profile);
    }
    std::cout<<"PROFILE_STORAGE_BOUNDARY_CHECKS="<<checks<<" FAILURES=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
