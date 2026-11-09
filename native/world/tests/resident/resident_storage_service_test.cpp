// 专用槽服务门核实际阻塞文件写、深字节、票据、取消与关闭；不声称世界或新槽/2可用。
#include "resident_storage_service.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
using namespace std::chrono_literals;
#ifndef HANMO_STORAGE_TEST_FAULTS
#error "This focused test requires HANMO_STORAGE_TEST_FAULTS"
#endif
// 门停在真正write之前；放行后仍使用真实Linux文件/fsync/readback，不用内存替身冒充介质。
struct Gate { std::mutex mutex;std::condition_variable wake;bool entered=false,released=false;std::thread::id writer; };
// 原测试固定的旧JSON format1字节；本门只核精确透传，世界语义由原codec的独立门负责。
SlotWriteRequest request() {
    SlotWriteRequest r;r.slot.id="manual-1";r.source.world_id[0]=9;r.source.execution_epoch=7;
    const std::string old="{\"format_version\":1,\"generation\":1,\"magic\":\"HM-RESIDENT-SAVE\",\"payload_base64\":\"AQID\",\"payload_length\":3,\"payload_sha256\":\"039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81\",\"save_id\":\"manual-1\"}";
    r.bytes.assign(old.begin(),old.end());return r;
}
// 等待明确状态只供测试，生产消费者每帧有限poll。
StoragePoll wait_state(ResidentStorageService& service,const StorageOperationRef& ref,StorageState expected) {
    const auto until=std::chrono::steady_clock::now()+3s;
    while(std::chrono::steady_clock::now()<until){auto p=service.poll(ref);assert(p.ok);if(p.state==expected)return p;std::this_thread::yield();}
    assert(false&&"storage state did not settle");return {};
}
// 返回ack以后才允许销毁，测试不会把析构join当作非阻塞关闭。
void close_service(ResidentStorageService& service) {
    service.request_close();const auto until=std::chrono::steady_clock::now()+3s;
    while(!service.close_complete()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(service.close_complete());
}
// 真实保存往返核独立worker、深输入和不可替换的阶段票据。
void background_and_bindings() {
    char pattern[]="/tmp/hanmo-storage-service-XXXXXX";const char* made=::mkdtemp(pattern);assert(made);const std::string directory=made;
    StorageServiceLimits limits{{4096,8192,16384},4,32768};
    auto gate=std::make_shared<Gate>();SlotIoTestHooks hooks;
    hooks.before=[gate](SlotIoPoint point){if(point==SlotIoPoint::before_write){std::unique_lock<std::mutex> lock(gate->mutex);gate->entered=true;gate->writer=std::this_thread::get_id();gate->wake.notify_all();gate->wake.wait(lock,[&]{return gate->released;});}return 0;};
    ResidentStorageService service(directory,limits,hooks);auto input=request();const auto original=input.bytes;
    const auto caller=std::this_thread::get_id();
    auto issued=std::async(std::launch::async,[&]{return service.stage(input);});
    const bool returned=issued.wait_for(500ms)==std::future_status::ready;
    // 失败也先放行，避免测试以挂死掩盖“request在调用线程做I/O”的真实反例。
    if(!returned){std::lock_guard<std::mutex> lock(gate->mutex);gate->released=true;gate->wake.notify_all();}
    assert(returned&&"stage blocked caller on file I/O");const auto ticket=issued.get();assert(ticket.ok);
    {std::unique_lock<std::mutex> lock(gate->mutex);assert(gate->wake.wait_for(lock,3s,[&]{return gate->entered;}));assert(gate->writer!=caller);}
    input.bytes.assign(4,0xff);assert(service.poll(ticket.reference).ok);
    auto stale=ticket.reference;++stale.source.execution_epoch;assert(!service.poll(stale).ok);
    auto wrong=ticket.reference;++wrong.instance;assert(!service.poll(wrong).ok);
    {std::lock_guard<std::mutex> lock(gate->mutex);gate->released=true;gate->wake.notify_all();}
    auto ready=wait_state(service,ticket.reference,StorageState::awaiting_owner_validation);
    assert(ready.frozen_write&&ready.frozen_write->bytes==original&&ready.stage_token!=0);
    assert(!std::filesystem::exists(directory+"/manual-1.B.hmcheckpoint"));
    assert(!service.promote_after_owner_validation(ticket.reference,ready.stage_token+1).ok);
    assert(service.promote_after_owner_validation(ticket.reference,ready.stage_token).ok);
    auto done=wait_state(service,ticket.reference,StorageState::completed);assert(done.durability==SlotDurability::durable_confirmed);
    assert(!service.cancel(ticket.reference).ok);assert(service.release(ticket.reference).ok);
    auto read=service.inspect({"manual-1",SlotFileKind::resident_checkpoint},input.source);assert(read.ok);
    auto banks=wait_state(service,read.reference,StorageState::completed);assert(banks.banks&&banks.banks->b.bytes==original&&!banks.banks->a.present);
    assert(service.release(read.reference).ok);close_service(service);assert(!service.stage(request()).ok);
    // 原读回结果可在worker结束后保有独立bytes，不能悬挂到目录/Node或可变世界。
    assert(ready.frozen_write->bytes==original);std::filesystem::remove_all(directory);
}
// 每个门有独立真实目录；测试结束只清理自己创建的目录。
struct TemporaryDirectory {
    std::string path;
    TemporaryDirectory(){char pattern[]="/tmp/hanmo-storage-cases-XXXXXX";const auto made=::mkdtemp(pattern);assert(made);path=made;}
    ~TemporaryDirectory(){std::filesystem::remove_all(path);}
};
// 受控门只暂停具名实际I/O阶段，用于无时间竞态地观察取消/关闭边界。
SlotIoTestHooks pause_at(const std::shared_ptr<Gate>& gate,SlotIoPoint selected){
    SlotIoTestHooks hooks;
    hooks.before=[gate,selected](SlotIoPoint point){if(point==selected){std::unique_lock<std::mutex> lock(gate->mutex);gate->entered=true;gate->writer=std::this_thread::get_id();gate->wake.notify_all();gate->wake.wait(lock,[&]{return gate->released;});}return 0;};return hooks;
}
// 测试等待worker确实进入I/O门，不用固定睡眠猜测已经执行到哪一步。
void entered(const std::shared_ptr<Gate>& gate){std::unique_lock<std::mutex> lock(gate->mutex);assert(gate->wake.wait_for(lock,3s,[&]{return gate->entered;}));}
// 所有失败观察后仍可释放门，避免析构join掩盖错误。
void unblock(const std::shared_ptr<Gate>& gate){std::lock_guard<std::mutex> lock(gate->mutex);gate->released=true;gate->wake.notify_all();}
// 关闭接纳是立即动作；已接纳的运行及排队请求都保留具名取消终态。
void cancellation_and_close(){
    TemporaryDirectory directory;auto gate=std::make_shared<Gate>();StorageServiceLimits limits{{4096,8192,16384},4,32768};
    ResidentStorageService service(directory.path,limits,pause_at(gate,SlotIoPoint::before_write));
    auto input=request();auto first=service.stage(input);assert(first.ok);entered(gate);
    auto read=service.inspect({"manual-2",SlotFileKind::resident_checkpoint},input.source);assert(read.ok);
    input.slot.id="manual-3";auto queued=service.stage(input);assert(queued.ok);
    assert(service.cancel(first.reference).ok);
    auto close=std::async(std::launch::async,[&]{service.request_close();});const bool returned=close.wait_for(500ms)==std::future_status::ready;
    if(!returned)unblock(gate);
    assert(returned&&"request_close waited for disk");close.get();
    assert(!service.close_complete());assert(!service.inspect(input.slot,input.source).ok);unblock(gate);close_service(service);
    for(const auto& ref:{first.reference,read.reference,queued.reference}){auto result=service.poll(ref);assert(result.ok&&result.state==StorageState::cancelled&&result.durability==SlotDurability::not_published);assert(service.release(ref).ok);}
    assert(!std::filesystem::exists(directory.path+"/manual-1.B.hmcheckpoint"));
    assert(!std::filesystem::exists(directory.path+"/manual-3.B.hmcheckpoint"));
    // close ack确实释放跨进程目录锁，不能只关闭接纳而遗留文件句柄。
    ResidentSlotMedia reopened(directory.path,limits.media);
}
// 进入rename以后取消不能撒谎成撤销；关闭仍领取实际耐久终态。
void close_after_publication(){
    TemporaryDirectory directory;auto gate=std::make_shared<Gate>();StorageServiceLimits limits{{4096,8192,16384},4,32768};
    ResidentStorageService service(directory.path,limits,pause_at(gate,SlotIoPoint::after_rename));
    auto ticket=service.stage(request());assert(ticket.ok);auto ready=wait_state(service,ticket.reference,StorageState::awaiting_owner_validation);
    assert(service.promote_after_owner_validation(ticket.reference,ready.stage_token).ok);entered(gate);
    auto cancelled=service.cancel(ticket.reference);assert(!cancelled.ok&&cancelled.code=="StoragePublicationStarted");
    service.request_close();assert(!service.close_complete());unblock(gate);close_service(service);
    auto result=service.poll(ticket.reference);assert(result.ok&&result.state==StorageState::completed&&result.durability==SlotDurability::durable_confirmed);
}
// 可见未确认持续占槽；重复读取不能补耐久，只有原阶段补真实屏障后能完成。
void unknown_recovery(){
    TemporaryDirectory directory;std::atomic<bool> fail{true};SlotIoTestHooks hooks;
    hooks.before=[&](SlotIoPoint point){return fail.load()&&(point==SlotIoPoint::directory_sync||point==SlotIoPoint::recovery_directory_sync)?EIO:0;};
    StorageServiceLimits limits{{4096,8192,16384},4,32768};ResidentStorageService service(directory.path,limits,hooks);
    auto input=request();auto ticket=service.stage(input);assert(ticket.ok);auto ready=wait_state(service,ticket.reference,StorageState::awaiting_owner_validation);
    assert(service.promote_after_owner_validation(ticket.reference,ready.stage_token).ok);auto unknown=wait_state(service,ticket.reference,StorageState::awaiting_recovery);
    assert(unknown.durability==SlotDurability::visible_unconfirmed&&unknown.system_error==EIO);
    auto again=service.stage(input);assert(!again.ok&&again.code=="StorageSlotBusy");assert(!service.release(ticket.reference).ok);assert(!service.cancel(ticket.reference).ok);
    auto inspection=service.inspect(input.slot,input.source);assert(inspection.ok);auto same=wait_state(service,inspection.reference,StorageState::completed);
    assert(same.banks&&same.banks->b.bytes==input.bytes&&same.durability==SlotDurability::not_published);
    assert(service.poll(ticket.reference).durability==SlotDurability::visible_unconfirmed);
    assert(!service.recover_same_bytes(ticket.reference,ready.stage_token+1).ok);assert(service.recover_same_bytes(ticket.reference,ready.stage_token).ok);
    assert(wait_state(service,ticket.reference,StorageState::awaiting_recovery).durability==SlotDurability::visible_unconfirmed);
    fail=false;assert(service.recover_same_bytes(ticket.reference,ready.stage_token).ok);
    auto complete=wait_state(service,ticket.reference,StorageState::completed);assert(complete.durability==SlotDurability::durable_confirmed&&complete.frozen_write->bytes==input.bytes);
    close_service(service);
}
// 关闭纯字节账后小额度不挡下一操作；旧领取结果仍深拥有，旧票据仍不能授权新阶段。
void retained_byte_budget(){
    TemporaryDirectory directory;auto input=request();StorageServiceLimits limits{{4096,8192,16384},1,input.bytes.size()};
    ResidentStorageService service(directory.path,limits);auto first=service.stage(input);assert(first.ok);auto held=wait_state(service,first.reference,StorageState::awaiting_owner_validation);
    assert(service.cancel(first.reference).ok);wait_state(service,first.reference,StorageState::cancelled);assert(service.release(first.reference).ok);
    const auto original=input.bytes;input.slot.id="manual-2";auto second=service.stage(input);assert(second.ok);
    assert(held.frozen_write&&held.frozen_write->bytes==original);
    auto busy=service.stage(input);assert(!busy.ok&&busy.code=="StorageBusy");
    auto ready=wait_state(service,second.reference,StorageState::awaiting_owner_validation);
    assert(ready.frozen_write&&ready.frozen_write->bytes==original&&held.frozen_write->bytes==original);
    assert(!service.promote_after_owner_validation(second.reference,held.stage_token).ok);assert(service.cancel(second.reference).ok);
    wait_state(service,second.reference,StorageState::cancelled);close_service(service);
}
// 关闭未确认的可见写必须形成可领取/释放终态；不能留下不能恢复的awaiting_recovery。
void close_unconfirmed(){
    TemporaryDirectory directory;SlotIoTestHooks hooks;hooks.before=[](SlotIoPoint point){return point==SlotIoPoint::directory_sync?EIO:0;};
    StorageServiceLimits limits{{4096,8192,16384},4,32768};ResidentStorageService service(directory.path,limits,hooks);
    auto ticket=service.stage(request());assert(ticket.ok);auto ready=wait_state(service,ticket.reference,StorageState::awaiting_owner_validation);
    assert(service.promote_after_owner_validation(ticket.reference,ready.stage_token).ok);wait_state(service,ticket.reference,StorageState::awaiting_recovery);close_service(service);
    auto closed=service.poll(ticket.reference);
    assert(closed.ok&&closed.state==StorageState::closed_unconfirmed&&closed.durability==SlotDurability::visible_unconfirmed);
    assert(closed.system_error==EIO&&closed.code=="StorageDirectorySyncFailed"&&closed.frozen_write);
    assert(!service.recover_same_bytes(ticket.reference,ready.stage_token).ok);assert(service.release(ticket.reference).ok);
    ResidentSlotMedia reopened(directory.path,limits.media);SlotCancellation read;auto actual=reopened.inspect(closed.frozen_write->slot,read);
    assert(actual.b.bytes==closed.frozen_write->bytes);
}
// 实际权限变化令取消清理失败时，回报失败与原errno；配额不会因结果release提前消失。
void cleanup_failure(){
    TemporaryDirectory directory;StorageServiceLimits limits{{4096,8192,4096},4,32768};ResidentStorageService service(directory.path,limits);
    auto input=request();input.bytes.assign(3000,7);auto first=service.stage(input);assert(first.ok);wait_state(service,first.reference,StorageState::awaiting_owner_validation);
    assert(::chmod(directory.path.c_str(),0500)==0);assert(service.cancel(first.reference).ok);auto failure=wait_state(service,first.reference,StorageState::failed);
    assert(failure.code=="StorageCleanupFailed"&&failure.system_error==EACCES&&failure.durability==SlotDurability::not_published);
    assert(::chmod(directory.path.c_str(),0700)==0);assert(service.release(first.reference).ok);input.slot.id="manual-2";
    auto second=service.stage(input);assert(second.ok);assert(wait_state(service,second.reference,StorageState::failed).code=="StorageCapacityExceeded");close_service(service);
}
// 门只宣称内部介质服务的实测边界，不冒充原codec/core或P14端到端交付。
int main(){
    background_and_bindings();cancellation_and_close();close_after_publication();unknown_recovery();retained_byte_budget();close_unconfirmed();cleanup_failure();
    std::cout<<"storage service real-file background/deep-value/ticket/cancel/close/recovery/budget checks passed; not complete save or P14 evidence\n";
}
