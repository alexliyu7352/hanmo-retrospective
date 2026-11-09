// 真实临时目录核stage、原字节CAS、目录锁、取消及可见/耐久区别；不替代原槽语义codec。
#include "resident_slot_media.hpp"
#include <cassert>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/stat.h>
#include <type_traits>
#include <sys/wait.h>
#include <unistd.h>
using namespace hanmo::resident;
using namespace hanmo::resident::storage;
#ifndef HANMO_STORAGE_TEST_FAULTS
#error "This focused test requires HANMO_STORAGE_TEST_FAULTS"
#endif
// 仅写本测试自己创建的真实银行，用于制造CAS冲突与保全检查。
void write_fixture(const std::string& path,const Bytes& bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));assert(out.good());}
// 接受必须是具名预期失败；不把任意异常当作测试通过。
template<class Fn> void fails(Fn action,const char* code,SlotDurability durability=SlotDurability::not_published){bool caught=false;try{action();}catch(const SlotMediaError& e){caught=true;assert(std::string(e.what())==code&&e.durability()==durability);}assert(caught);}
// 独立拥有测试字节只核介质，不声称它是完整可恢复的世界。
std::shared_ptr<const SlotWriteRequest> make_request(const SlotBankPair& expected={}){SlotWriteRequest request;request.slot.id="manual-1";request.expected=expected;request.bytes={0,1,255,128,7};return std::make_shared<const SlotWriteRequest>(std::move(request));}
// 每一个故障点都保留旧银行，提升后的故障必须留可见未知并且允许只补同字节屏障。
void failure_matrix(const std::string& root,SlotMediaLimits limits){
    const SlotIoPoint points[]={SlotIoPoint::before_write,SlotIoPoint::file_sync,SlotIoPoint::file_close,SlotIoPoint::stage_readback,SlotIoPoint::before_publish,SlotIoPoint::after_rename,SlotIoPoint::directory_sync,SlotIoPoint::final_readback};
    const char* codes[]={"StorageWriteFailed","StorageFileSyncFailed","StorageCloseFailed","ReadbackFailed","StorageRenameFailed","StorageAfterRenameFailed","StorageDirectorySyncFailed","ReadbackFailed"};
    for(std::size_t index=0;index<sizeof(points)/sizeof(points[0]);++index){
        const auto directory=root+"/fault-"+std::to_string(index);bool armed=true;SlotIoTestHooks hooks;
        hooks.before=[&](SlotIoPoint point){return armed&&point==points[index]?EIO:0;};
        ResidentSlotMedia media(directory,limits,hooks);write_fixture(directory+"/manual-1.A.hmcheckpoint",Bytes{42});write_fixture(directory+"/manual-1.B.hmcheckpoint",Bytes{43});
        SlotCancellation read,cancel;auto before=media.inspect({"manual-1",SlotFileKind::resident_checkpoint},read);
        auto input=std::make_shared<SlotWriteRequest>(*make_request(before));input->expected_generation=1;input->new_generation=2;
        if(index<4){fails([&]{media.stage(input,cancel);},codes[index]);}
        else{
            auto prepared=media.stage(input,cancel);const auto durability=index<5?SlotDurability::not_published:SlotDurability::visible_unconfirmed;
            fails([&]{media.promote_after_owner_validation(*prepared,cancel);},codes[index],durability);
            if(index>=5){armed=false;assert(media.recover_same_bytes(*prepared)==SlotDurability::durable_confirmed);}
        }
        auto actual=media.inspect(input->slot,read);assert(actual.a.bytes==before.a.bytes);assert(actual.b.bytes==(index<5?before.b.bytes:input->bytes));
        for(const auto& entry:std::filesystem::directory_iterator(directory))assert(entry.path().filename().string().find(".tmp.")==std::string::npos);
    }
}
// 非普通文件和超限长度是准确读取失败，不能被选代逻辑当物理损坏静默回退。
void invalid_files(const std::string& root,SlotMediaLimits limits){
    const auto directory=root+"/invalid";ResidentSlotMedia media(directory,limits);SlotCancellation read;
    write_fixture(directory+"/target",Bytes{1});assert(::symlink("target",(directory+"/manual-1.A.hmcheckpoint").c_str())==0);
    fails([&]{media.inspect({"manual-1",SlotFileKind::resident_checkpoint},read);},"StorageReadFailed");
    write_fixture(directory+"/manual-2.A.hmcheckpoint",Bytes(limits.max_file_bytes+1,4));
    fails([&]{media.inspect({"manual-2",SlotFileKind::resident_checkpoint},read);},"StorageCapacityExceeded");
    auto input=make_request();SlotCancellation cancel;auto staged=media.stage(std::make_shared<const SlotWriteRequest>(SlotWriteRequest{{"manual-3",SlotFileKind::resident_checkpoint},SlotBank::b,{},0,1,{},input->bytes}),cancel);
    for(const auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().filename().string().find("manual-3.B.hmcheckpoint.tmp.")==0)write_fixture(entry.path(),Bytes{8});
    fails([&]{media.promote_after_owner_validation(*staged,cancel);},"ReadbackFailed");assert(!std::filesystem::exists(directory+"/manual-3.B.hmcheckpoint"));
}
// 聚焦介质证据包含真实系统调用，不把介质透传等同世界恢复的语义通过。
int main(){
    char pattern[]="/tmp/hanmo-slot-media-XXXXXX";const char* made=::mkdtemp(pattern);assert(made);const std::string directory=made;
    SlotMediaLimits limits{4096,8192,16384};bool fail_directory=false;
    static_assert(!std::is_copy_constructible<PreparedSlotWrite>::value&&!std::is_move_constructible<PreparedSlotWrite>::value,"stage cannot duplicate its cleanup lease");
    failure_matrix(directory,limits);invalid_files(directory,limits);
    auto excessive=limits;excessive.max_staging_disk_bytes=std::numeric_limits<std::uint64_t>::max();fails([&]{ResidentSlotMedia invalid(directory+"/bad-limits",excessive);},"StorageInvalidConfiguration");
    // symlink加点级可令词法路径与实际父目录不同；在产生任何目录副作用前明确拒绝。
    std::filesystem::create_directories(directory+"/real/deep");std::filesystem::create_directories(directory+"/lexical");
    assert(::symlink((directory+"/real/deep").c_str(),(directory+"/lexical/link").c_str())==0);
    fails([&]{ResidentSlotMedia ambiguous(directory+"/lexical/link/../slots",limits);},"StorageInvalidConfiguration");
    assert(!std::filesystem::exists(directory+"/real/slots")&&!std::filesystem::exists(directory+"/lexical/slots"));
    SlotIoTestHooks hooks;hooks.before=[&](SlotIoPoint point){return fail_directory&&(point==SlotIoPoint::directory_sync||point==SlotIoPoint::recovery_directory_sync)?EIO:0;};
    ResidentSlotMedia media(directory,limits,hooks);SlotCancellation read;
    assert(ResidentSlotMedia::valid_slot("auto-3")&&ResidentSlotMedia::valid_slot("manual-10"));
    for(const auto* bad:{"auto-0","manual-11","manual-01","manual-1\n","../manual-1"})assert(!ResidentSlotMedia::valid_slot(bad));
    // FIFO不能让只读worker永久阻塞；子进程alarm只是失败兜底，成功必须是具名拒绝。
    assert(::mkfifo((directory+"/manual-2.A.hmcheckpoint").c_str(),0600)==0);
    const auto fifo_child=::fork();assert(fifo_child>=0);
    if(fifo_child==0){::alarm(1);try{media.inspect({"manual-2",SlotFileKind::resident_checkpoint},read);::_exit(2);}catch(const SlotMediaError& e){::_exit(std::string(e.what())=="StorageInvalidFile"?0:3);}}
    int fifo_status=0;assert(::waitpid(fifo_child,&fifo_status,0)==fifo_child);
    assert(WIFEXITED(fifo_status)&&WEXITSTATUS(fifo_status)==0&&"FIFO read must reject without blocking");
    // 多个等待语义核验的tmp共用磁盘预留，不能各自通过后突破总预算。
    {
        ResidentSlotMedia bounded(directory+"/bounded",{4096,8192,6});SlotCancellation one,two;
        auto first=bounded.stage(make_request(),one);auto other=std::make_shared<SlotWriteRequest>(*make_request());other->slot.id="manual-2";
        fails([&]{bounded.stage(other,two);},"StorageCapacityExceeded");first.reset();
        auto second=bounded.stage(other,two);assert(second);
    }
    // 真实权限拒绝unlink后遗留tmp仍收费，恢复权限不能让新槽绕过旧占用。
    {
        const auto path=directory+"/unlink-denied";ResidentSlotMedia bounded(path,{4096,8192,6});SlotCancellation one,two;
        auto first=bounded.stage(make_request(),one);assert(::chmod(path.c_str(),0500)==0);first.reset();assert(::chmod(path.c_str(),0700)==0);
        auto other=std::make_shared<SlotWriteRequest>(*make_request());other->slot.id="manual-2";
        fails([&]{bounded.stage(other,two);},"StorageCapacityExceeded");
        std::size_t bytes=0;for(const auto& entry:std::filesystem::directory_iterator(path))if(entry.path().filename().string().find(".tmp.")!=std::string::npos)bytes+=entry.file_size();assert(bytes==5);
    }
    // shared_ptr<const>仍可能保有可变别名；更改调用者slot/代数/字节不能换掉已冻结包。
    {
        ResidentSlotMedia isolated(directory+"/alias",limits);auto mutable_input=std::make_shared<SlotWriteRequest>(*make_request());SlotCancellation alias;
        const auto bytes=mutable_input->bytes;auto frozen=isolated.stage(mutable_input,alias);
        mutable_input->slot.id="manual-2";mutable_input->new_generation=999;mutable_input->bytes={6,6,6};
        assert(frozen->request().slot.id=="manual-1"&&frozen->request().new_generation==1&&frozen->request().bytes==bytes);
        assert(isolated.promote_after_owner_validation(*frozen,alias)==SlotDurability::durable_confirmed);
        assert(std::filesystem::exists(directory+"/alias/manual-1.B.hmcheckpoint")&&!std::filesystem::exists(directory+"/alias/manual-2.B.hmcheckpoint"));
    }
    // 独立进程再开同目录必须明确拒绝，不能只用进程内mutex冒CAS排斥。
    const auto child=::fork();assert(child>=0);if(child==0){try{ResidentSlotMedia other(directory,limits);::_exit(2);}catch(const SlotMediaError& e){::_exit(std::string(e.what())=="StorageWriterBusy"?0:3);}}int status=0;assert(::waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    auto original=make_request();SlotCancellation cancel;auto stage=media.stage(original,cancel);
    assert(!std::filesystem::exists(directory+"/manual-1.B.hmcheckpoint"));assert(cancel.request());
    fails([&]{media.promote_after_owner_validation(*stage,cancel);},"StorageCancelled");stage.reset();assert(media.inspect(original->slot,read).b.bytes.empty());
    SlotCancellation run;stage=media.stage(original,run);fail_directory=true;
    fails([&]{media.promote_after_owner_validation(*stage,run);},"StorageDirectorySyncFailed",SlotDurability::visible_unconfirmed);
    assert(stage->is_published()&&!run.request());assert(media.inspect(original->slot,read).b.bytes==original->bytes);
    fails([&]{media.recover_same_bytes(*stage);},"StorageDirectorySyncFailed",SlotDurability::visible_unconfirmed);
    fail_directory=false;assert(media.recover_same_bytes(*stage)==SlotDurability::durable_confirmed);
    auto current=media.inspect(original->slot,read);auto next=std::make_shared<SlotWriteRequest>();next->slot=original->slot;next->inactive=SlotBank::a;next->expected=current;next->expected_generation=1;next->new_generation=2;next->bytes={9,8,7};
    SlotCancellation clash;auto pending=media.stage(next,clash);write_fixture(directory+"/manual-1.B.hmcheckpoint",Bytes{4,5,6});
    fails([&]{media.promote_after_owner_validation(*pending,clash);},"GenerationConflict");assert(!std::filesystem::exists(directory+"/manual-1.A.hmcheckpoint"));
    pending.reset();stage.reset();std::filesystem::remove_all(directory);
    std::cout<<"slot media real-file fsync/readback/CAS/process-lock/recovery checks passed; not world validation\n";
}
