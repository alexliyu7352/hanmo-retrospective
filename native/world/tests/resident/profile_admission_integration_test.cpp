// 真实原NativeBoundarySession与同一存储worker的本机准入闭环；不使用mock创建回执或假Acquisition。
#include "resident_profile_coordinator.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include "resident/kernel/exact_time_profile.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
#include <picosha2.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
using namespace std::chrono_literals;
namespace {
using V=BoundaryValue;
// 真实session仅转交原冷安装；故意不实现absence能力，以核默认unsupported不能冒同步准入。
// 此wrapper内部仍仅一个真正世界，不编造identity/checkpoint/创建成功。
class UnprovenActualSession final:public ResidentReplaySession {
public:
    Status initialize(const V& c,const V& s)override{return actual_->initialize(c,s);}
    Status initialize_checkpoint_with_continuation(const V& c,const Bytes& b,const Bytes& tail)override{return actual_->initialize_checkpoint_with_continuation(c,b,tail);}
    Outcome<ReplayCommandResult> execute(const V& v)override{return actual_->execute(v);}
    Outcome<V> export_state()const override{return actual_->export_state();}
    Outcome<WorldIdentity> identity()const override{return actual_->identity();}
    Outcome<Bytes> checkpoint()override{return actual_->checkpoint();}
    Status restore(const Bytes& b)override{return actual_->restore(b);}
private:
    std::unique_ptr<ResidentReplaySession> actual_=create_resident_replay_session();
};
int checks=0;
void check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
template<class T>T must(Outcome<T> v,const char* why){if(!v.ok)throw std::runtime_error(StableId{why}+": "+v.error.original.code+" "+v.error.original.path);++checks;return std::move(v.value);}
void must(Status v,const char* why){if(!v.ok)throw std::runtime_error(StableId{why}+": "+v.error.original.code);++checks;}
V obj(std::initializer_list<std::pair<StableId,V>> values){return V{V::Dict(values)};}
StableId str(const V& v,const char* key){auto* p=v.find(key);check(p&&p->as<StableId>(),"typed fixture string");return *p->as<StableId>();}
CatalogIdentity identity(const V& input){CatalogIdentity id;id.content_id=str(input,"content_id");id.content_version=str(input,"content_version");id.content_digest=str(input,"content_digest");id.schema_sha256=str(input,"schema_sha256");id.ruleset_id=dynamic_ruleset_id;id.artifact_sha256=picosha2::hash256_hex_string(must(render_boundary_json(input),"fixture canonical identity"));id.module_versions={{"dynamic_world","local_society_v1"},{"local_economy","1"},{"organization","2"},{"affairs","1"},{"director","2"},{"regional_life","1"}};must(append_exact_time_modules(id),"selected exact modules");return id;}
V request(const V& input){const auto id=identity(input);V::Array modules;for(const auto& m:id.module_versions)modules.push_back(obj({{"name",V{m.first}},{"version",V{m.second}}}));auto catalog=obj({{"content_id",V{id.content_id}},{"content_version",V{id.content_version}},{"content_digest",V{id.content_digest}},{"ruleset_id",V{id.ruleset_id}},{"schema_sha256",V{id.schema_sha256}},{"artifact_sha256",V{id.artifact_sha256}},{"module_versions",V{std::move(modules)}}});return obj({{"scenario_id",V{StableId{"engineering.exact_rest_only"}}},{"world_seed",V{StableId{"same-author-seed"}}},{"expected_catalog_identity",std::move(catalog)}});}
ProfileNamespace host(){return {"native.integration.local",ProfilePurpose::development,{}};}
ProfileStoreLimits limits(){return {{6u*1024u*1024u},64,32u*1024u*1024u};}
StorageServiceLimits media(){return {{32u*1024u*1024u,64u*1024u*1024u,128u*1024u*1024u},4,128u*1024u*1024u};}
// 生产无阻塞等待；这个有截止测试循环只等待实际worker明确终态。
ProfileAdmissionStatus settle(ResidentProfileCoordinator& c){const auto until=std::chrono::steady_clock::now()+10s;while(std::chrono::steady_clock::now()<until){auto advanced=c.advance();auto state=c.status();if(state.phase==ProfileAdmissionPhase::stored||state.phase==ProfileAdmissionPhase::blocked||state.phase==ProfileAdmissionPhase::awaiting_recovery||state.phase==ProfileAdmissionPhase::idle)return state;if(!advanced.ok)throw std::runtime_error("unexpected profile advance: "+advanced.error.original.code);std::this_thread::sleep_for(1ms);}throw std::runtime_error("profile worker did not settle");}
void close(ResidentProfileCoordinator& c){c.request_close();const auto until=std::chrono::steady_clock::now()+10s;while(!c.close_complete()&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(1ms);check(c.close_complete(),"real storage close ack");}
}
int main(int argc,char** argv){try{
    check(argc==2,"exact authored fixture required");std::ifstream in(argv[1]);std::ostringstream text;text<<in.rdbuf();auto catalog=must(parse_boundary_json(text.str()),"frozen fixture parse");auto start=request(catalog);
    char root_template[]="/tmp/hanmo-profile-admission-XXXXXX";const auto* made=::mkdtemp(root_template);check(made,"isolated native test directory");const std::string root=made;std::cout<<"PROFILE_ADMISSION_TEST_DIRECTORY="<<root<<'\n';
    Bytes checkpoint,capsule_bytes;AdmissionRecord original;WorldIdentity first_identity;
    {
        auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+"/normal",limits(),media());auto created=c.create_local_world(catalog,start);check(created.world_committed,"actual world committed before profile completion");first_identity=must(session->identity(),"same session actually installed");auto stored=settle(c);check(stored.admission_stored&&!stored.profile_sync_blocked&&stored.durability==SlotDurability::durable_confirmed&&stored.profile_generation==1,"real native record durable");
        original=must(c.read_admission(StableId(stored.admission_id.view())),"actual local record");check(original.origin==AdmissionOrigin::local_created&&original.capsule.baseline.empty()&&original.allowed_history.empty(),"proven empty local baseline");checkpoint=must(session->checkpoint(),"same session genuine checkpoint");check(original.capsule.origin_snapshot_sha256==profile_sha256(checkpoint.data(),checkpoint.size()),"origin binds actual first checkpoint");auto captured=must(c.capture(first_identity),"same live snapshot capture");capsule_bytes=must(encode_slot_admission_capsule(captured.capsule,limits().admission),"actual capsule encode");
        auto duplicate=c.create_local_world(catalog,start);check(duplicate.code=="LocalCreationAlreadyStarted"&&must(session->identity(),"identity after duplicate").world_id==first_identity.world_id,"duplicate create never makes a world");must(c.retry_admission(),"retry original immutable record");auto again=settle(c);check(again.admission_stored&&again.profile_generation==1&&again.admission_id==stored.admission_id,"same record retry does not make generation");
        // 真正命令通过同一session推进精确世界；胶囊origin不会随当前checkpoint更新。
        LiveOperation op;op.kind=LiveOperationKind::command;op.source=first_identity;op.payload=obj({{"actor_id",V{StableId{"actor.player"}}},{"kind",V{StableId{"advance"}}},{"params",obj({{"requested_to",must(encode_world_moment2({0,must(normalize_fraction("1","3"),"fraction")}),"exact requested moment")}})},{"request_seq",V{Integer{1}}},{"expected_revision",V{first_identity.revision}}});auto result=must(session->execute_live(op),"actual same-session world command");check(result.result.find("ok")&&result.result.find("ok")->as<bool>()&&*result.result.find("ok")->as<bool>(),"world command committed");auto advanced=must(session->identity(),"advanced same-session identity");check(advanced.revision>first_identity.revision,"world revision really advances");check(!c.capture(first_identity).ok,"stale capture refuses");auto after=must(c.capture(advanced),"fresh capture after world advance");check(same_admission_capsule(after.capsule,original.capsule),"save-as inherits exact original capsule");checkpoint=must(session->checkpoint(),"later real checkpoint");check(profile_sha256(checkpoint.data(),checkpoint.size())!=original.capsule.origin_snapshot_sha256,"later checkpoint not substituted for origin");close(c);
    }
    {
        auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+"/normal",limits(),media());must(c.begin_read(),"cold read actual metadata");check(settle(c).phase==ProfileAdmissionPhase::idle,"cold metadata loaded");auto restored=c.restore_local_world(catalog,checkpoint,{},capsule_bytes);check(restored.world_committed&&restored.admission_stored&&!restored.profile_sync_blocked,"actual cold world and local record associated");auto current=must(session->identity(),"cold actual identity");check(current.world_id==first_identity.world_id&&current.execution_epoch!=first_identity.execution_epoch,"same persistent world new execution epoch");check(same_admission_capsule(must(c.capture(current),"cold capture").capsule,original.capsule),"cold restore preserves immutable capsule");check(!c.retry_admission().ok,"cold file cannot become a local creation receipt");auto reread=c.begin_read();check(!reread.ok&&reread.error.original.code=="ProfileReadRequiresUnboundSession","cold association cannot silently replace its validated record snapshot");check(!c.status().profile_sync_blocked,"rejected reread preserves original validated association");close(c);
    }
    {
        UnprovenActualSession session;ResidentProfileCoordinator c(session,host(),root+"/normal",limits(),media());must(c.begin_read(),"actual record with unavailable proof");settle(c);
        auto restored=c.restore_local_world(catalog,checkpoint,{},capsule_bytes);
        check(restored.world_committed&&restored.admission_stored&&restored.profile_sync_blocked&&restored.code=="UnsupportedProfileDeliveryProjection","real cold world and record remain distinct from unavailable producer proof");
        check(must(session.identity(),"unproven actual world").world_id==first_identity.world_id,"proof failure does not undo real cold publication");
        // 总设计师 2026-10-29 Slack 确认（1793326841.345799）：可保存原胶囊，不能把受阻数据同步到 profile。
        const auto source=must(session.identity(),"unproven source");const auto blocked_before=c.status();
        auto preserved=must(c.capture(source),"blocked real world remains saveable");
        check(preserved.profile_sync_blocked&&same_admission_capsule(preserved.capsule,original.capsule),"blocked save preserves original capsule without sync permission");
        const auto& capacity=engineering_checkpoint_capacity_profile();const auto& slot=engineering_slot_capacity_profile();auto job=CheckpointJobBudget::create(capacity,slot);check(job.ok,"real blocked capture job");
        auto bytes=c.capture_bytes(source,capacity,slot,job.value);check(bytes.ok&&bytes.value.profile_sync_blocked,"owned byte capture preserves blocked status");
        check(bytes.value.capsule.bytes()==capsule_bytes,"blocked byte capture preserves exact original capsule bytes");
        auto merge=c.begin_merge(original.record_generation,blocked_before.profile_generation);check(!merge.ok&&merge.error.original.code=="ProfileSyncBlocked","blocked world cannot begin actual profile merge");
        auto grant=c.begin_grant_imported_history(original.capsule.admission_id,original.record_generation,blocked_before.profile_generation,ImportHistoryChoice{});check(!grant.ok&&grant.error.original.code=="ProfileSyncBlocked","blocked world cannot grant history into profile");
        const auto blocked_after=c.status();check(blocked_after.profile_sync_blocked&&blocked_after.profile_generation==blocked_before.profile_generation&&blocked_after.code==blocked_before.code&&!c.delivery_receipt(),"rejected sync preserves prior state and creates no receipt");
        check(same_admission_capsule(must(c.read_admission(original.capsule.admission_id),"blocked original admission remains").capsule,original.capsule),"rejected sync cannot rewrite original admission");close(c);
    }
    {
        auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+"/normal",limits(),media());auto created=c.create_local_world(catalog,start);check(created.world_committed,"same author seed genuine second new world");auto second=must(session->identity(),"new real world ID");check(second.world_id!=first_identity.world_id,"author seed is not persistent WorldId");auto stored=settle(c);check(stored.admission_stored&&stored.profile_generation==2,"second genuine world appends namespace record");check(must(c.read_admission(original.capsule.admission_id),"old record remains").capsule.source_world_id==first_identity.world_id,"new world does not overwrite prior admission");close(c);
    }
    {
        auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+"/missing",limits(),media());must(c.begin_read(),"genuinely missing local record");settle(c);auto restored=c.restore_local_world(catalog,checkpoint,{},capsule_bytes);check(restored.world_committed&&!restored.admission_stored&&restored.profile_sync_blocked&&restored.code=="AdmissionRecoveryRequired","valid world retained while missing record blocks sync");check(!c.retry_admission().ok,"external capsule cannot mint original creation");auto preserved=must(c.capture(must(session->identity(),"missing record real world")),"blocked capsule preserved for later saving");check(preserved.profile_sync_blocked&&same_admission_capsule(preserved.capsule,original.capsule),"missing record never clears capsule");close(c);
    }
    {
        auto session=create_resident_replay_session();auto wrong=host();wrong.purpose=ProfilePurpose::formal;ResidentProfileCoordinator c(*session,wrong,root+"/other-host",limits(),media());must(c.begin_read(),"other host metadata");settle(c);auto rejected=c.restore_local_world(catalog,checkpoint,{},capsule_bytes);check(!rejected.world_committed&&rejected.code=="AdmissionNamespaceMismatch"&&!session->identity().ok,"development file cannot choose formal namespace");close(c);
    }
    {
        auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+"/bad-create",limits(),media());auto bad=start;*bad.find("scenario_id")=V{StableId{"nonexistent"}};auto failed=c.create_local_world(catalog,bad);check(!failed.world_committed&&!session->identity().ok&&!failed.admission_stored,"rejected original creation creates no receipt");check(!c.retry_admission().ok,"failed create has no retry authority");close(c);check(!std::filesystem::exists(root+"/bad-create/profile_metadata.A.hmprofile")&&!std::filesystem::exists(root+"/bad-create/profile_metadata.B.hmprofile"),"no record file for failed world install");
    }
    {
        auto session=create_resident_replay_session();const auto directory=root+"/lost-record";ResidentProfileCoordinator c(*session,host(),directory,limits(),media());
        check(c.create_local_world(catalog,start).world_committed,"real creation before same-process record loss");auto before=settle(c);check(before.admission_stored,"record was actually stored before loss");const auto source=must(session->identity(),"record loss same actual world");
        // 只挪本测试拥有的实际文件并保留原件，不删除任何旧GD档案或历史测试资料。
        for(const auto bank:{"A","B"}){const auto path=directory+"/profile_metadata."+bank+".hmprofile";if(std::filesystem::exists(path))std::filesystem::rename(path,path+".preserved-before-loss");}
        must(c.retry_admission(),"original receipt may replenish genuinely lost record");const auto until=std::chrono::steady_clock::now()+10s;
        while(c.status().phase==ProfileAdmissionPhase::reading&&std::chrono::steady_clock::now()<until){must(c.advance(),"read actual missing record");std::this_thread::sleep_for(1ms);}
        auto pending=c.status();check(pending.phase==ProfileAdmissionPhase::staging&&!pending.admission_stored&&pending.profile_sync_blocked&&pending.durability==SlotDurability::not_published,"known lost record resets old durable status before pending rewrite");
        auto repaired=settle(c);check(repaired.admission_stored&&repaired.admission_id==before.admission_id,"replenish only same immutable record");check(must(session->identity(),"after record replenish").world_id==source.world_id,"replenishing record never creates world");close(c);
    }
#ifdef HANMO_STORAGE_TEST_FAULTS
    for(const auto point:{SlotIoPoint::before_write,SlotIoPoint::directory_sync}){
        const bool visible=point==SlotIoPoint::directory_sync;bool armed=true;SlotIoTestHooks hooks;hooks.before=[&](SlotIoPoint p){return armed&&p==point?EIO:0;};auto session=create_resident_replay_session();ResidentProfileCoordinator c(*session,host(),root+(visible?"/visible-failure":"/prepublish-failure"),limits(),media(),hooks);auto created=c.create_local_world(catalog,start);check(created.world_committed,"world remains committed despite upcoming disk failure");const auto before=must(session->identity(),"world prior to profile failure");auto failed=settle(c);check(failed.world_committed&&!failed.admission_stored&&failed.profile_sync_blocked,"disk failure never undoes actual world");check(failed.phase==(visible?ProfileAdmissionPhase::awaiting_recovery:ProfileAdmissionPhase::blocked),"visible unknown kept distinct");if(visible)check(failed.durability==SlotDurability::visible_unconfirmed,"rename visible is not durable");armed=false;must(c.retry_admission(),"retry only original record or original same-byte barrier");auto recovered=settle(c);check(recovered.admission_stored&&recovered.profile_generation==1&&recovered.durability==SlotDurability::durable_confirmed,"same request recovers without generation two");auto after=must(session->identity(),"world after profile retry");check(after.world_id==before.world_id&&after.execution_epoch==before.execution_epoch&&after.revision==before.revision,"profile retry never initializes or advances world");close(c);
    }
#endif
    std::cout<<"PROFILE_ADMISSION_NATIVE_CHECKS="<<checks<<" FAILURES=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
