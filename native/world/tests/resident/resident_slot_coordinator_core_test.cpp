// 使用原1000人schema有效oracle和真实session核保存闭环；没有EchoSession或手工成功验证标志。
#include "resident_slot_coordinator.hpp"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <unistd.h>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
using namespace std::chrono_literals;
namespace {
int checks=0;
// 首个失败保留明确阶段，避免后续空值访问把真实错误盖掉。
void check(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
// 原只读oracle直接进入既有boundary parser；不建立替代规则或简化世界。
BoundaryValue read_json(const std::string& path){std::ifstream file(path);check(bool(file),"open oracle "+path);std::ostringstream text;text<<file.rdbuf();auto value=parse_boundary_json(text.str());check(value.ok,"parse oracle "+path);return std::move(value.value);}
// 真实目录读回用于核对已发布的旧封套完整字节。
Bytes read_bytes(const std::string& path){std::ifstream file(path,std::ios::binary);check(bool(file),"open bytes "+path);return Bytes(std::istreambuf_iterator<char>(file),{});}
// 故障门只改本测试临时目录，写入后真实介质仍须检测替换。
void write_bytes(const std::string& path,const Bytes& bytes){std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();check(bool(file),"write bytes "+path);}
// 仅清理本测试创建的临时目录；不会触碰oracle或工程存档。
struct TemporaryDirectory {
    std::string path;
    TemporaryDirectory(){char pattern[]="/tmp/hanmo-slot-core-XXXXXX";auto made=::mkdtemp(pattern);check(made,"mkdtemp");path=made;}
    ~TemporaryDirectory(){std::filesystem::remove_all(path);}
};
// 完整身份比较同时防止核验暗中分配epoch、推进revision或tick。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 捕获必须来自真实execute_live；continuation与checkpoint始终保留同一次返回pair。
LiveOperationResult capture(ResidentReplaySession& session){auto id=session.identity();check(id.ok,"capture identity");LiveOperation op;op.kind=LiveOperationKind::checkpoint;op.source=id.value;op.payload=BoundaryValue{BoundaryValue::Dict{}};auto result=session.execute_live(op);check(result.ok,"real core capture: "+result.error.original.code);check(result.value.checkpoint_bytes.has_value(),"capture bytes available");return std::move(result.value);}
// 只在测试循环推进原world owner，生产poll本身不运行核心或文件I/O。
SlotSavePoll wait_for(ResidentSlotCoordinator& service,ResidentReplaySession& session,const StorageOperationRef& ref,SlotSaveState desired){
    const auto until=std::chrono::steady_clock::now()+60s;
    while(std::chrono::steady_clock::now()<until){auto polled=service.poll(ref);check(polled.ok,"valid logical poll");if(polled.state==desired)return polled;
        if(polled.state==SlotSaveState::failed||polled.state==SlotSaveState::cancelled||polled.state==SlotSaveState::closed_unconfirmed||polled.state==SlotSaveState::completed)throw std::runtime_error("unexpected terminal: "+polled.code);
        auto advanced=service.advance(session);check(advanced.ok,"owner advance: "+advanced.code);std::this_thread::sleep_for(1ms);
    }throw std::runtime_error("owner operation timeout");
}
// 等待实际终态，不以请求取消成功替代磁盘清理完成。
SlotSavePoll terminal(ResidentSlotCoordinator& service,ResidentReplaySession& session,const StorageOperationRef& ref){
    const auto until=std::chrono::steady_clock::now()+60s;
    while(std::chrono::steady_clock::now()<until){auto polled=service.poll(ref);check(polled.ok,"terminal poll");if(polled.state==SlotSaveState::failed||polled.state==SlotSaveState::cancelled||polled.state==SlotSaveState::closed_unconfirmed||polled.state==SlotSaveState::completed)return polled;
        auto advanced=service.advance(session);check(advanced.ok,"terminal advance: "+advanced.code);std::this_thread::sleep_for(1ms);
    }throw std::runtime_error("terminal timeout");
}
// 关闭需要原owner继续收取缓存终态，再确认底层句柄已关闭。
void close_service(ResidentSlotCoordinator& service,ResidentReplaySession& session){service.request_close();const auto until=std::chrono::steady_clock::now()+10s;while(!service.close_complete()&&std::chrono::steady_clock::now()<until){service.advance(session);std::this_thread::sleep_for(1ms);}check(service.close_complete(),"close acknowledgement");}
// 预算显式容纳旧双银行与候选暂存，既有64MiB文件门仍由真实codec再核。
StorageServiceLimits limits(){return {{64u*1024u*1024u,128u*1024u*1024u,256u*1024u*1024u},4,512u*1024u*1024u};}
// 原状态与完整checkpoint同时不变，覆盖领域ID、RNG以及返回身份的纯读要求。
void unchanged(ResidentReplaySession& session,const WorldIdentity& identity,const Bytes& checkpoint,const BoundaryValue& state){check(same_identity(session.identity().value,identity),"validation leaves identity unchanged");check(session.checkpoint().value==checkpoint,"validation leaves all checkpoint bytes unchanged");check(compare_boundary(session.export_state().value,state).exact(),"validation leaves exact exported state unchanged");}
// 原存档capture后允许同epoch推进；所有候选持久身份仍需与其实际source精确对应。
void pure_validation(ResidentReplaySession& session,const BoundaryValue& command){
    const auto before=capture(session);auto travel=session.execute(command);check(travel.ok,"execute actual travel");const auto current=session.identity().value;check(current.execution_epoch==before.source_identity.execution_epoch&&current.revision>before.source_identity.revision,"actual same-epoch advance");
    const auto snapshot=session.checkpoint().value;const auto exported=session.export_state().value;
    auto accepted=session.validate_storage_checkpoint(before.source_identity,*before.checkpoint_bytes,before.continuation_bytes,std::nullopt,std::nullopt);check(accepted.ok,"past same-epoch captured bytes accepted");check(accepted.value.candidate_sha256==resident_slot_sha256(*before.checkpoint_bytes),"proof hashes actual candidate");check(same_identity(accepted.value.current_identity,current),"proof reports current owner identity");
    auto mismatch=session.validate_storage_checkpoint(current,*before.checkpoint_bytes,before.continuation_bytes,std::nullopt,std::nullopt);check(!mismatch.ok&&mismatch.error.original.code=="CheckpointIdentityMismatch","candidate/source revision mismatch rejected");
    auto future=current;++future.revision;auto refused=session.validate_storage_checkpoint(future,*before.checkpoint_bytes,before.continuation_bytes,std::nullopt,std::nullopt);check(!refused.ok&&refused.error.original.code=="StalePreview","future source rejected before candidate decode");
    auto wrong_world=before.source_identity;wrong_world.world_id[0]^=1;auto stale=session.validate_storage_checkpoint(wrong_world,*before.checkpoint_bytes,before.continuation_bytes,std::nullopt,std::nullopt);check(!stale.ok&&stale.error.original.code=="StaleWorld","different source world rejected");
    Bytes truncated=*before.checkpoint_bytes;truncated.pop_back();check(!session.validate_storage_checkpoint(before.source_identity,truncated,before.continuation_bytes,std::nullopt,std::nullopt).ok,"malformed candidate rejected by original core");
    check(!session.validate_storage_checkpoint(current,snapshot,std::nullopt,Bytes{1,2,3},std::nullopt).ok,"semantically invalid previous rejected by original core");unchanged(session,current,snapshot,exported);
    // 原实际行动建立停驻许可，随后保存必须保留真实core给出的continuation。
    auto action=*travel.value.receipt.find("details")->find("action_id")->as<StableId>();LiveOperation run;run.kind=LiveOperationKind::begin_run;run.source=current;
    run.payload=BoundaryValue{BoundaryValue::Dict{{"run_id",BoundaryValue{"run:storage-test"}},{"actor_id",BoundaryValue{"actor.player"}},{"action_id",BoundaryValue{action}},{"max_half_days",BoundaryValue{Integer{4}}},{"max_gross_outflow",BoundaryValue{Integer{100000}}}}};auto started=session.execute_live(run);check(started.ok&&*started.value.result.find("ok")->as<bool>(),"real parked run created");
}
// 对真实core返回pair做合法保存；public API没有输入source/payload/continuation或裸提升token。
void real_save_and_continuation(ResidentReplaySession& session){
    TemporaryDirectory directory;ResidentSlotCoordinator service(directory.path,limits());const auto before=capture(session);check(before.continuation_bytes&& !before.continuation_bytes->empty(),"real core continuation present");
    auto accepted=service.begin_save(session,"manual-1",0);check(accepted.ok,"coordinator captures real pair");
    LiveOperation grant;grant.kind=LiveOperationKind::grant;grant.source=session.identity().value;grant.payload=BoundaryValue{BoundaryValue::Dict{{"run_id",BoundaryValue{"run:storage-test"}},{"permit_seq",BoundaryValue{Integer{1}}},{"grant_half_days",BoundaryValue{Integer{1}}}}};auto advanced=session.execute_live(grant);check(advanced.ok&&*advanced.value.result.find("ok")->as<bool>(),"actual core advances after coordinator capture");check(session.identity().value.execution_epoch==before.source_identity.execution_epoch&&session.identity().value.tick>before.source_identity.tick,"in-flight capture keeps same epoch while world advances");
    const auto saved=terminal(service,session,accepted.reference);check(saved.state==SlotSaveState::completed&&saved.durability==SlotDurability::durable_confirmed,"real inspect stage core validate promote durable");
    auto bank=decode_resident_slot(read_bytes(directory.path+"/manual-1.B.hmcheckpoint"));check(bank.ok&&bank.envelope.format_version==2,"existing format2 emitted");check(bank.envelope.payload==*before.checkpoint_bytes&&bank.envelope.continuation==before.continuation_bytes,"immutable original capture pair preserved exactly");check(same_identity(saved.reference.source,before.source_identity),"logical receipt keeps actual source");
    const auto identity=session.identity().value;const auto current_checkpoint=session.checkpoint().value;const auto state=session.export_state().value;auto checked=session.validate_storage_checkpoint(before.source_identity,bank.envelope.payload,bank.envelope.continuation,std::nullopt,std::nullopt);check(checked.ok&&checked.value.candidate_continuation_sha256==resident_slot_sha256(*before.continuation_bytes),"published actual continuation passes original core");
    auto swapped=*before.continuation_bytes;swapped.back()^=1;check(!session.validate_storage_checkpoint(before.source_identity,bank.envelope.payload,swapped,std::nullopt,std::nullopt).ok,"substituted continuation rejected by original binding");unchanged(session,identity,current_checkpoint,state);
    check(service.release(accepted.reference).ok,"release saved logical record");check(!service.poll(accepted.reference).ok,"released reference cannot alias next save");
    auto next=service.begin_save(session,"manual-1",1);check(next.ok,"second generation admitted");auto second=terminal(service,session,next.reference);check(second.state==SlotSaveState::completed&&second.generation==2,"existing bank selected then core-verified next generation");check(decode_resident_slot(read_bytes(directory.path+"/manual-1.A.hmcheckpoint")).envelope.generation==2,"A bank receives second generation");check(service.release(next.reference).ok,"release second generation");
    auto conflict=service.begin_save(session,"manual-1",0);check(conflict.ok,"generation conflict checked after actual observation");auto failed=terminal(service,session,conflict.reference);check(failed.state==SlotSaveState::failed&&failed.code=="GenerationConflict"&&failed.durability==SlotDurability::not_published,"wrong expected generation fails without publication");check(service.release(conflict.reference).ok,"release failed generation");close_service(service,session);
}
// stage后换epoch由同一核验拒绝；旧票据不能被新epoch重绑为成功。
void stale_epoch_and_cancel(ResidentReplaySession& session){
    TemporaryDirectory directory;ResidentSlotCoordinator service(directory.path,limits());auto save=service.begin_save(session,"manual-2",0);check(save.ok,"epoch test capture");wait_for(service,session,save.reference,SlotSaveState::awaiting_validation);const auto old=capture(session);
    LiveOperation restore;restore.kind=LiveOperationKind::restore;restore.source=session.identity().value;restore.payload=BoundaryValue{BoundaryValue::Dict{}};restore.checkpoint_bytes=old.checkpoint_bytes;restore.continuation_bytes=old.continuation_bytes;auto restored=session.execute_live(restore);check(restored.ok,"real restore with original continuation pair");check(session.identity().value.execution_epoch!=old.source_identity.execution_epoch,"actual restore changes epoch");
    auto rejected=session.validate_storage_checkpoint(old.source_identity,*old.checkpoint_bytes,old.continuation_bytes,std::nullopt,std::nullopt);check(!rejected.ok&&rejected.error.original.code=="StaleWorld","old source after restore rejected");auto result=terminal(service,session,save.reference);check(result.state==SlotSaveState::failed&&result.code=="StaleWorld"&&result.durability==SlotDurability::not_published,"poststage core rejects epoch change");check(!std::filesystem::exists(directory.path+"/manual-2.B.hmcheckpoint"),"stale stage never published");check(service.release(save.reference).ok,"release stale source result");
    auto cancelled=service.begin_save(session,"manual-2",0);check(cancelled.ok,"cancel test capture");wait_for(service,session,cancelled.reference,SlotSaveState::awaiting_validation);auto transport=std::async(std::launch::async,[&]{return service.cancel(cancelled.reference);});check(transport.get().ok,"transport requests cancellation");auto stopped=terminal(service,session,cancelled.reference);check(stopped.state==SlotSaveState::cancelled&&stopped.durability==SlotDurability::not_published,"actual worker cleanup precedes cancelled result");check(!std::filesystem::exists(directory.path+"/manual-2.B.hmcheckpoint"),"cancelled stage never published");check(service.release(cancelled.reference).ok,"release cancelled operation");
    auto owner=std::async(std::launch::async,[&]{return service.begin_save(session,"manual-2",0);});auto wrong=owner.get();check(!wrong.ok&&wrong.code=="StorageWrongOwnerThread","wrong owner rejected before accessing session");close_service(service,session);
}
// 读回后外部替换tmp必须被真实promote再次比较拒绝，不能只信内存的冻结副本。
void replaced_stage(ResidentReplaySession& session){
    TemporaryDirectory directory;ResidentSlotCoordinator service(directory.path,limits());auto save=service.begin_save(session,"manual-3",0);check(save.ok,"tamper test capture");wait_for(service,session,save.reference,SlotSaveState::awaiting_validation);std::string temporary;
    for(const auto& entry:std::filesystem::directory_iterator(directory.path))if(entry.path().filename().string().find(".tmp.")!=std::string::npos)temporary=entry.path();
    check(!temporary.empty(),"actual staged file exists");write_bytes(temporary,Bytes{1,2,3});auto result=terminal(service,session,save.reference);check(result.state==SlotSaveState::failed&&result.durability==SlotDurability::not_published,"replaced physical stage rejected before rename");check(!std::filesystem::exists(directory.path+"/manual-3.B.hmcheckpoint"),"tampered stage never reaches bank");check(service.release(save.reference).ok,"release tamper failure");close_service(service,session);
}
// 物理JSON合法而核心无效的旧最高代不能被跳过或覆盖，原核心是唯一语义门。
void invalid_previous(ResidentReplaySession& session){
    TemporaryDirectory directory;auto envelope=encode_resident_slot(Bytes{1,2,3},1,"manual-4");check(envelope.ok,"physical legacy envelope prepared for negative test");write_bytes(directory.path+"/manual-4.B.hmcheckpoint",envelope.value);ResidentSlotCoordinator service(directory.path,limits());auto save=service.begin_save(session,"manual-4",1);check(save.ok,"bad previous observed asynchronously");auto result=terminal(service,session,save.reference);check(result.state==SlotSaveState::failed&&result.durability==SlotDurability::not_published,"original core rejects semantically invalid previous");check(read_bytes(directory.path+"/manual-4.B.hmcheckpoint")==envelope.value&&!std::filesystem::exists(directory.path+"/manual-4.A.hmcheckpoint"),"invalid previous preserved without fallback rewrite");check(service.release(save.reference).ok,"release previous rejection");close_service(service,session);
}
#ifdef HANMO_STORAGE_TEST_FAULTS
// 真正inspect被测试门暂停；只有放行后才继续实际open/read，明确覆盖未决release路径。
struct InspectGate {
    std::mutex mutex;std::condition_variable wake;bool entered=false,released=false;
    void wait_entered(){std::unique_lock<std::mutex> lock(mutex);check(wake.wait_for(lock,5s,[&]{return entered;}),"actual inspect gate entered");}
    void open(){std::lock_guard<std::mutex> lock(mutex);released=true;wake.notify_all();}
};
// 失败路径也先开测试门再销毁服务，避免用测试自身死锁掩盖原错误。
struct ReleaseGate {std::shared_ptr<InspectGate> gate;~ReleaseGate(){gate->open();}};
// 接纳后/首次poll分配失败都保可领取票据，唯一操作额度在终态release后可再次使用。
void allocation_failure_lifecycle(ResidentReplaySession& session,bool during_accept){
    TemporaryDirectory directory;auto gate=std::make_shared<InspectGate>();SlotIoTestHooks io;
    io.before=[gate](SlotIoPoint point){if(point==SlotIoPoint::before_inspect){std::unique_lock<std::mutex> lock(gate->mutex);gate->entered=true;gate->wake.notify_all();gate->wake.wait(lock,[&]{return gate->released;});}return 0;};
    bool injected=false;SlotCoordinatorTestHooks hooks;
    if(during_accept)hooks.after_inspect_accepted=[&]{if(!injected){injected=true;gate->wait_entered();throw std::bad_alloc{};}};
    else hooks.before_poll=[&]{if(!injected){injected=true;throw std::bad_alloc{};}};
    auto budget=limits();budget.max_operations=1;ResidentSlotCoordinator service(directory.path,budget,io,hooks);ReleaseGate release_gate{gate};
    auto save=service.begin_save(session,"manual-5",0);check(save.ok,"accepted request returns ticket despite later allocation failure");gate->wait_entered();
    if(!during_accept){auto failed=service.advance(session);check(!failed.ok&&failed.code=="StorageResourceLimit","first poll allocation failure reported");}
    auto pending=service.poll(save.reference);check(pending.ok&&pending.state==SlotSaveState::cancelling,"failure retains pending cancellation instead of orphan terminal");check(!service.release(save.reference).ok,"cannot release while real inspect still blocked");gate->open();
    auto result=terminal(service,session,save.reference);check(result.state==SlotSaveState::failed&&result.code=="StorageResourceLimit"&&result.durability==SlotDurability::not_published,"allocation failure reaches accurate failed terminal after worker cleanup");check(service.release(save.reference).ok,"allocation failure original record released");
    auto next=service.begin_save(session,"manual-5",0);check(next.ok,"sole operation budget restored without leaked record");service.request_close();auto closed=terminal(service,session,next.reference);check(closed.state==SlotSaveState::cancelled,"next request closes accurately");check(service.release(next.reference).ok,"closed request released");close_service(service,session);
}
// 核验并请求提升后换epoch，真实已接受的旧来源磁盘结果仍必须保留，不能反写当前世界。
void epoch_after_promotion(ResidentReplaySession& session){
    TemporaryDirectory directory;auto gate=std::make_shared<InspectGate>();SlotIoTestHooks io;
    io.before=[gate](SlotIoPoint point){if(point==SlotIoPoint::before_publish){std::unique_lock<std::mutex> lock(gate->mutex);gate->entered=true;gate->wake.notify_all();gate->wake.wait(lock,[&]{return gate->released;});}return 0;};
    ResidentSlotCoordinator service(directory.path,limits(),io);ReleaseGate release_gate{gate};auto save=service.begin_save(session,"manual-6",0);check(save.ok,"post-promotion epoch test capture");wait_for(service,session,save.reference,SlotSaveState::awaiting_validation);check(service.advance(session).ok,"original core validates and requests publication");check(service.poll(save.reference).state==SlotSaveState::promoting,"physical promotion accepted");gate->wait_entered();
    const auto old=capture(session);LiveOperation restore;restore.kind=LiveOperationKind::restore;restore.source=session.identity().value;restore.payload=BoundaryValue{BoundaryValue::Dict{}};restore.checkpoint_bytes=old.checkpoint_bytes;restore.continuation_bytes=old.continuation_bytes;check(session.execute_live(restore).ok,"new epoch installed after old physical request");const auto current=session.identity().value;const auto checkpoint=session.checkpoint().value;const auto exported=session.export_state().value;gate->open();
    auto result=terminal(service,session,save.reference);check(result.state==SlotSaveState::completed&&result.durability==SlotDurability::durable_confirmed,"old-source actual publication remains accurately durable");check(result.reference.source.execution_epoch==old.source_identity.execution_epoch&&result.reference.source.execution_epoch!=current.execution_epoch,"receipt never rebound to current epoch");unchanged(session,current,checkpoint,exported);check(service.release(save.reference).ok,"release old-source media receipt");close_service(service,session);
}

#endif

}
// 本门链接真实生产core；1000人输入是原schema有效测试扩展，不冒充发行内容或Godot验收。
int main(int argc,char** argv){try{
    check(argc==2,"oracle directory argument required");const std::string root=argv[1];auto index=read_json(root+"/oracle-index.json");auto catalog=read_json(root+"/expanded-catalog.json");auto state=read_json(root+"/"+*index.find("initial_state")->find("file")->as<StableId>());
    std::cerr<<"CORE initialize\n";auto session=create_resident_replay_session();auto initialized=session->initialize(catalog,state);check(initialized.ok,"initialize actual resident core: "+initialized.error.original.code);const auto& command=*index.find("commands")->as<BoundaryValue::Array>()->front().find("command");
    std::cerr<<"CORE pure validation\n";pure_validation(*session,command);std::cerr<<"CORE actual save\n";real_save_and_continuation(*session);std::cerr<<"CORE epoch and cancel\n";stale_epoch_and_cancel(*session);std::cerr<<"CORE stage replacement\n";replaced_stage(*session);std::cerr<<"CORE previous reject\n";invalid_previous(*session);
#ifdef HANMO_STORAGE_TEST_FAULTS
    std::cerr<<"CORE allocation lifecycle\n";allocation_failure_lifecycle(*session,true);allocation_failure_lifecycle(*session,false);std::cerr<<"CORE post-promotion epoch\n";epoch_after_promotion(*session);
#endif
    std::cout<<"RESIDENT_SLOT_COORDINATOR_CORE "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"RESIDENT_SLOT_COORDINATOR_CORE FAIL after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}}
