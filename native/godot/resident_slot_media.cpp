#include "resident/checkpoint/checkpoint_retention.hpp"
// Linux原生槽介质的真实系统调用；完整封套字节透明保留，所有业务核验仍由原owner办理。
#include "resident_slot_media.hpp"
#include "resident_slot_envelope.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <picosha2.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <string_view>
#include <filesystem>
#include <fcntl.h>
#include <limits>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hanmo::resident::storage {
namespace {
// 文件描述符唯一拥有；异常清理不能重试Linux close而误关复用后的另一个fd。
class File {
public:
    explicit File(int value=-1):value_(value){}
    ~File(){if(value_>=0)::close(value_);}
    File(const File&)=delete;File& operator=(const File&)=delete;
    File(File&& other)noexcept:value_(other.release()){}
    int get()const noexcept{return value_;}
    int release()noexcept{const int value=value_;value_=-1;return value;}
private:int value_;
};
// 目录nonce只用于互不覆盖的临时文件名，不作为世界ID或持久槽代数。
std::atomic<std::uint64_t> directory_nonce{0};
// 任一确定取消只发生在未开始提升前；不会把已rename文件伪称撤销。
void check_cancel(const SlotCancellation* cancel){if(cancel&&cancel->cancelled())throw SlotMediaError("StorageCancelled");}
// 内核可重试中断，但每次实际写仍检查短写并累计准确字节数。
void write_all(int fd,const Bytes& bytes,const SlotCancellation& cancel){
    std::size_t done=0;while(done<bytes.size()){
        check_cancel(&cancel);const auto count=std::min<std::size_t>(bytes.size()-done,64u*1024u);
        const auto written=::write(fd,bytes.data()+done,count);
        if(written<0){if(errno==EINTR)continue;throw SlotMediaError("StorageWriteFailed",errno);}
        if(written==0)throw SlotMediaError("StorageWriteFailed",EIO);
        done+=static_cast<std::size_t>(written);
    }
}
// fsync成功才形成对应屏障；读回和mtime均不能代替它。
void sync_file(int fd,const char* code){while(::fsync(fd)<0){if(errno==EINTR)continue;throw SlotMediaError(code,errno);}}
// create_directories可能新建多层；逐级同步父目录，不能只同步槽目录而遗漏它自身的目录项。
void sync_directory_chain(std::filesystem::path path){
    path=path.lexically_normal();
    for(;;){
        File directory(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        if(directory.get()<0)throw SlotMediaError("StorageDirectory",errno);
        sync_file(directory.get(),"StorageDirectorySyncFailed");
        const auto parent=path.parent_path();if(parent.empty()||parent==path)break;path=parent;
    }
}
// 所有文件必须是本目录内真实普通文件，拒绝符号链接或设备入口。
struct stat regular_stat(int fd){struct stat value{};if(::fstat(fd,&value)<0)throw SlotMediaError("StorageReadFailed",errno);if(!S_ISREG(value.st_mode)||value.st_size<0)throw SlotMediaError("StorageInvalidFile");return value;}
// 限额加法先核，不让坏输入溢出成小数后触发大分配或越过磁盘预算。
std::uint64_t checked_add(std::uint64_t left,std::uint64_t right){if(right>std::numeric_limits<std::uint64_t>::max()-left)throw SlotMediaError("StorageCapacityExceeded");return left+right;}
// 文件时刻只作为原列表的展示值，负值不参与物理选择。
std::uint64_t display_time(const struct stat& value){return value.st_mtime<0?0:static_cast<std::uint64_t>(value.st_mtime);}
}
// 目录fd与跨进程排他锁由同一共享lease保有，阶段未清理前不会悬挂。
struct SlotDirectory {
    int fd=-1,lock=-1;std::uint64_t nonce=0;
    std::atomic<std::uint64_t> staging_disk{0};
    ~SlotDirectory(){if(lock>=0)::close(lock);if(fd>=0)::close(fd);}
};
// 错误同时带原系统码和真实发布阶段，不把已可见失败降格成未写入。
SlotMediaError::SlotMediaError(std::string code,int system_error,SlotDurability durability):std::runtime_error(std::move(code)),system_error_(system_error),durability_(durability){}
// 原 runtime_error 诊断采用其 noexcept 共享复制；发布后只变准确阶段，不重构字符串。
SlotMediaError SlotMediaError::with_durability(SlotDurability durability)const noexcept{auto result=*this;result.durability_=durability;return result;}
// 只有未开始rename的请求可以取消；重复取消保持幂等。
bool SlotCancellation::request()noexcept{auto expected=State::open;return state_.compare_exchange_strong(expected,State::cancelled)||expected==State::cancelled;}
// worker在可中止的I/O边界读取取消位，不触及领域世界。
bool SlotCancellation::cancelled()const noexcept{return state_.load()==State::cancelled;}
// 最后取消与开始发布以同一原子CAS排序，只有一方能先获得权利。
bool SlotCancellation::begin_publish()noexcept{auto expected=State::open;return state_.compare_exchange_strong(expected,State::publishing);}
// rename成功后记录物理可见，尚不代表耐久屏障通过。
void SlotCancellation::published()noexcept{state_.store(State::published);}
// 已进入发布的工作必须领取实际结果，不能由close改写为取消成功。
bool SlotCancellation::publication_started()const noexcept{const auto value=state_.load();return value==State::publishing||value==State::published;}
// 阶段唯一拥有临时文件清理权和磁盘预留，构造只由介质办理。
PreparedSlotWrite::PreparedSlotWrite(std::shared_ptr<SlotDirectory> directory,std::shared_ptr<const SlotWriteRequest> request,std::string temporary,std::uint64_t charge):directory_(std::move(directory)),request_(std::move(request)),temporary_(std::move(temporary)),disk_charge_(charge){}
// 只删除本对象独占创建且尚未提升的tmp；绝不清理银行或其它任务的遗留件。
PreparedSlotWrite::~PreparedSlotWrite(){if(!cleanup_attempted_)cleanup();}
// rename已成功或unlink成功/明确不存在才退配额；清理失败的物理tmp继续占目录总预算。
int PreparedSlotWrite::cleanup()noexcept{
    cleanup_attempted_=true;
    if(!directory_||disk_charge_==0)return 0;
    if(!published_&&!temporary_.empty()&&::unlinkat(directory_->fd,temporary_.c_str(),0)<0&&errno!=ENOENT)return errno;
    directory_->staging_disk.fetch_sub(disk_charge_);disk_charge_=0;return 0;
}
// 受信绝对路径初始化、目录排他锁与新建父链屏障都在存储worker完成。
ResidentSlotMedia::ResidentSlotMedia(const std::string& path,SlotMediaLimits limits
#ifdef HANMO_STORAGE_TEST_FAULTS
,SlotIoTestHooks hooks
#endif
):limits_(limits)
#ifdef HANMO_STORAGE_TEST_FAULTS
,hooks_(std::move(hooks))
#endif
{
    const auto maximum=static_cast<std::uint64_t>(std::numeric_limits<Integer>::max());
    if(!limits.host_profile_namespace_binding.empty()&&!valid_key({"profile_metadata",SlotFileKind::profile_metadata,limits.host_profile_namespace_binding}))throw SlotMediaError("StorageInvalidConfiguration");
    if(path.empty()||!std::filesystem::path(path).is_absolute()||limits.max_file_bytes==0||limits.max_read_pair_bytes==0||limits.max_staging_disk_bytes==0||limits.max_file_bytes>maximum||limits.max_read_pair_bytes>maximum||limits.max_staging_disk_bytes>maximum)throw SlotMediaError("StorageInvalidConfiguration");
    // 路径不能含点级或截断字符；否则symlink/..的实际打开对象会与词法父链不同。
    if(path.find('\0')!=std::string::npos)throw SlotMediaError("StorageInvalidConfiguration");
    for(const auto& component:std::filesystem::path(path))if(component=="."||component=="..")throw SlotMediaError("StorageInvalidConfiguration");
    std::error_code error;std::filesystem::create_directories(path,error);if(error)throw SlotMediaError("StorageDirectory",error.value());
    auto directory=std::make_shared<SlotDirectory>();directory->fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(directory->fd<0)throw SlotMediaError("StorageDirectory",errno);
    directory->lock=::openat(directory->fd,".hanmo-storage.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK,0600);
    if(directory->lock<0)throw SlotMediaError("StorageLockFailed",errno);
    regular_stat(directory->lock);
    if(::flock(directory->lock,LOCK_EX|LOCK_NB)<0)throw SlotMediaError(errno==EWOULDBLOCK?"StorageWriterBusy":"StorageLockFailed",errno);
    sync_directory_chain(path);
    directory->nonce=++directory_nonce;if(directory->nonce==0)throw SlotMediaError("StorageOperationLimit");directory_=std::move(directory);
}
// 最后目录lease释放才关闭真实句柄；不会回调Godot或世界owner。
ResidentSlotMedia::~ResidentSlotMedia()=default;
// 复用原3auto/10manual闭合集，拒路径、前导零和尾换行。
bool ResidentSlotMedia::valid_slot(const std::string& name)noexcept{
    if(name.size()==6&&name.compare(0,5,"auto-")==0)return name[5]>='1'&&name[5]<='3';
    if(name=="manual-10")return true;
    return name.size()==8&&name.compare(0,7,"manual-")==0&&name[7]>='1'&&name[7]<='9';
}
// 原受信扩展与闭合集共同约束，调用者不能用路径或新格式替换另一类档案。
ResidentSlotMedia::BankName ResidentSlotMedia::bank_name(const SlotKey& key,SlotBank bank)const{
    if(key.kind==SlotFileKind::profile_metadata){
        if(!valid_key(key)||limits_.host_profile_namespace_binding.empty()||key.profile_namespace_binding!=limits_.host_profile_namespace_binding)throw SlotMediaError("StorageProfileNamespaceMismatch");
        if(bank!=SlotBank::a&&bank!=SlotBank::b)throw SlotMediaError("StorageInvalidBank");
        BankName result;const char* name=bank==SlotBank::a?"profile_metadata.A.hmprofile":"profile_metadata.B.hmprofile";
        std::copy_n(name,std::strlen(name)+1,result.bytes.data());return result;
    }
    if(!limits_.host_profile_namespace_binding.empty()||!key.profile_namespace_binding.empty())throw SlotMediaError("StorageTargetMismatch");
    if(!valid_slot(key.id))throw SlotMediaError("InvalidSlot");
    if(key.kind!=SlotFileKind::legacy_save&&key.kind!=SlotFileKind::resident_checkpoint&&key.kind!=SlotFileKind::slot_envelope_v2)throw SlotMediaError("StorageUnsupportedFormat");
    if(bank!=SlotBank::a&&bank!=SlotBank::b)throw SlotMediaError("StorageInvalidBank");
    BankName result;const char* extension=key.kind==SlotFileKind::slot_envelope_v2?".hmslot":key.kind==SlotFileKind::resident_checkpoint?".hmcheckpoint":".hmsave";
    const auto length=std::snprintf(result.bytes.data(),result.bytes.size(),"%s.%c%s",key.id.c_str(),bank==SlotBank::a?'A':'B',extension);
    if(length<0||std::size_t(length)>=result.bytes.size())throw SlotMediaError("StorageOperationLimit");return result;
}
// namespace摘要只做受信目标相等，不能成为文件名或任意路径。
bool ResidentSlotMedia::valid_key(const SlotKey& key)noexcept{
    if(key.kind!=SlotFileKind::profile_metadata)return (key.kind==SlotFileKind::legacy_save||key.kind==SlotFileKind::resident_checkpoint||key.kind==SlotFileKind::slot_envelope_v2)&&key.profile_namespace_binding.empty()&&valid_slot(key.id);
    return key.id=="profile_metadata"&&key.profile_namespace_binding.size()==64&&std::all_of(key.profile_namespace_binding.begin(),key.profile_namespace_binding.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
// 故障注入在生产编译完全不存在；每个错误码仍对应真实系统调用阶段。
void ResidentSlotMedia::point(SlotIoPoint where)const{
#ifdef HANMO_STORAGE_TEST_FAULTS
    if(hooks_.before){const int failure=hooks_.before(where);if(failure){
        const char* code="StorageInjectedFailure";
        switch(where){case SlotIoPoint::before_inspect:code="StorageReadFailed";break;case SlotIoPoint::before_write:code="StorageWriteFailed";break;case SlotIoPoint::file_sync:case SlotIoPoint::recovery_file_sync:code="StorageFileSyncFailed";break;case SlotIoPoint::file_close:code="StorageCloseFailed";break;case SlotIoPoint::stage_readback:case SlotIoPoint::final_readback:code="ReadbackFailed";break;case SlotIoPoint::before_publish:code="StorageRenameFailed";break;case SlotIoPoint::after_rename:code="StorageAfterRenameFailed";break;case SlotIoPoint::directory_sync:case SlotIoPoint::recovery_directory_sync:code="StorageDirectorySyncFailed";break;}
        throw SlotMediaError(code,failure);
    }}
#else
    (void)where;
#endif
}
// 独立拥有两个银行的真实有限字节；语义最高代选择留给既有codec。
SlotBankPair ResidentSlotMedia::inspect(const SlotKey& key,const SlotCancellation& cancel,CheckpointMemoryCharge* owned_memory){
    point(SlotIoPoint::before_inspect);
    SlotBankPair pair;std::size_t total=0;
    for(const auto bank:{SlotBank::a,SlotBank::b}){
        check_cancel(&cancel);const auto name=bank_name(key,bank);File file(::openat(directory_->fd,name.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
        if(file.get()<0){if(errno==ENOENT)continue;throw SlotMediaError("StorageReadFailed",errno);}
        const auto stat=regular_stat(file.get());const auto size=static_cast<std::uint64_t>(stat.st_size);
        if(size>limits_.max_file_bytes||size>limits_.max_read_pair_bytes-total)throw SlotMediaError("StorageCapacityExceeded");
        auto& image=bank==SlotBank::a?pair.a:pair.b;image.present=true;image.modified_unix=display_time(stat);if(owned_memory)owned_memory->grow(static_cast<std::size_t>(size));image.bytes.resize(static_cast<std::size_t>(size));total+=image.bytes.size();
        std::size_t done=0;while(done<image.bytes.size()){
            check_cancel(&cancel);const auto n=::read(file.get(),image.bytes.data()+done,std::min<std::size_t>(image.bytes.size()-done,64u*1024u));
            if(n<0){if(errno==EINTR)continue;throw SlotMediaError("StorageReadFailed",errno);}if(n==0)throw SlotMediaError("StorageChangedDuringRead");done+=static_cast<std::size_t>(n);
        }
        std::uint8_t extra;ssize_t tail;do{tail=::read(file.get(),&extra,1);}while(tail<0&&errno==EINTR);
        if(tail<0)throw SlotMediaError("StorageReadFailed",errno);
        if(tail!=0)throw SlotMediaError("StorageChangedDuringRead");
    }
    return pair;
}
// 流式核相同长度及每个字节，不将hash相等或某个旧银行当成这次读回。
void ResidentSlotMedia::verify_file(const char* name,const Bytes& expected,const SlotCancellation* cancel)const{
    File file(::openat(directory_->fd,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(file.get()<0)throw SlotMediaError("StorageReadFailed",errno);
    const auto stat=regular_stat(file.get());if(static_cast<std::uint64_t>(stat.st_size)!=expected.size())throw SlotMediaError("ReadbackFailed");
    std::array<std::uint8_t,64u*1024u> buffer{};std::size_t done=0;
    while(done<expected.size()){
        check_cancel(cancel);const auto amount=std::min(buffer.size(),expected.size()-done);const auto n=::read(file.get(),buffer.data(),amount);
        if(n<0){if(errno==EINTR)continue;throw SlotMediaError("StorageReadFailed",errno);}if(n==0)throw SlotMediaError("ReadbackFailed");
        if(!std::equal(buffer.begin(),buffer.begin()+n,expected.begin()+static_cast<std::ptrdiff_t>(done)))throw SlotMediaError("ReadbackFailed");
        done+=static_cast<std::size_t>(n);
    }
    std::uint8_t extra;ssize_t tail;do{tail=::read(file.get(),&extra,1);}while(tail<0&&errno==EINTR);
    if(tail<0)throw SlotMediaError("StorageReadFailed",errno);
    if(tail!=0)throw SlotMediaError("ReadbackFailed");
}
// 单银行CAS只比真实字节；不存在、不同值与未知读取失败分别返回准确结果。
void ResidentSlotMedia::compare_bank(const char* name,const SlotBankImage& expected,const SlotCancellation* cancel)const{
    check_cancel(cancel);
    if(!expected.present){struct stat stat{};if(::fstatat(directory_->fd,name,&stat,AT_SYMLINK_NOFOLLOW)==0)throw SlotMediaError("GenerationConflict");if(errno!=ENOENT)throw SlotMediaError("StorageReadFailed",errno);return;}
    try{verify_file(name,expected.bytes,cancel);}catch(const SlotMediaError& error){if(std::string_view(error.what())=="ReadbackFailed"||error.system_error()==ENOENT)throw SlotMediaError("GenerationConflict");throw;}
}
// CAS比较原观察的A/B完整字节；权限/未知I/O失败保持原错，不能假称物理损坏。
void ResidentSlotMedia::compare_banks(const SlotKey& key,const SlotBankPair& pair,const SlotCancellation* cancel)const{
    compare_bank(bank_name(key,SlotBank::a).c_str(),pair.a,cancel);
    compare_bank(bank_name(key,SlotBank::b).c_str(),pair.b,cancel);
}
// 提升后目标必须是本次完整字节，另一代仍是原件；逐流比较不额外复制两银行。
void ResidentSlotMedia::verify_published(const SlotWriteRequest& input)const{
    verify_file(bank_name(input.slot,input.inactive).c_str(),input.bytes,nullptr);
    const auto other=input.inactive==SlotBank::a?SlotBank::b:SlotBank::a;
    compare_bank(bank_name(input.slot,other).c_str(),other==SlotBank::a?input.expected.a:input.expected.b,nullptr);
}
// 直接介质调用也深冻输入，绝不把shared_ptr<const>误当无可变别名的证明。
std::shared_ptr<PreparedSlotWrite> ResidentSlotMedia::stage(std::shared_ptr<const SlotWriteRequest> request,SlotCancellation& cancel){
    if(!request)throw SlotMediaError("StorageInvalidRequest");
    if(request->bytes.size()>limits_.max_file_bytes||request->expected.a.bytes.size()>limits_.max_file_bytes||request->expected.b.bytes.size()>limits_.max_file_bytes)throw SlotMediaError("StorageCapacityExceeded");
    check_cancel(&cancel);
    // shared_ptr<const>不能证明没有可变别名，公开内部介质入口仍必须自行深复制。
    return stage_frozen(std::make_shared<const SlotWriteRequest>(*request),cancel);
}
// 服务友元拥有受控冻结输入；目录共享预留覆盖所有在途A+B+tmp，避免逐槽预算相加越限。
std::shared_ptr<PreparedSlotWrite> ResidentSlotMedia::stage_frozen(std::shared_ptr<const SlotWriteRequest> request,SlotCancellation& cancel){
    if(!request)throw SlotMediaError("StorageInvalidRequest");
    const auto& input=*request;
    const auto name=bank_name(input.slot,input.inactive);
    if(input.expected_generation<0||input.expected_generation==std::numeric_limits<Integer>::max()||input.new_generation!=input.expected_generation+1)throw SlotMediaError("InvalidGeneration");
    if(input.bytes.empty()||input.bytes.size()>limits_.max_file_bytes||input.expected.a.bytes.size()>limits_.max_file_bytes||input.expected.b.bytes.size()>limits_.max_file_bytes)throw SlotMediaError("StorageCapacityExceeded");
    if((!input.expected.a.present&&!input.expected.a.bytes.empty())||(!input.expected.b.present&&!input.expected.b.bytes.empty()))throw SlotMediaError("StorageInvalidObservation");
    const auto disk=checked_add(checked_add(input.expected.a.bytes.size(),input.expected.b.bytes.size()),input.bytes.size());if(disk>limits_.max_staging_disk_bytes)throw SlotMediaError("StorageCapacityExceeded");
    check_cancel(&cancel);compare_banks(input.slot,input.expected,&cancel);
    if(++temporary_sequence_==0)throw SlotMediaError("StorageOperationLimit");
    char temporary_buffer[128];const auto length=std::snprintf(temporary_buffer,sizeof(temporary_buffer),"%s.tmp.%lld.%llu.%llu",name.c_str(),static_cast<long long>(::getpid()),static_cast<unsigned long long>(directory_->nonce),static_cast<unsigned long long>(temporary_sequence_));
    if(length<0||std::size_t(length)>=sizeof(temporary_buffer))throw SlotMediaError("StorageOperationLimit");
    CheckpointMemoryCharge temporary_memory(0);
    std::string temporary(temporary_buffer,std::size_t(length));
    auto reserved=directory_->staging_disk.load();
    do{if(reserved>limits_.max_staging_disk_bytes||disk>limits_.max_staging_disk_bytes-reserved)throw SlotMediaError("StorageCapacityExceeded");}
    while(!directory_->staging_disk.compare_exchange_weak(reserved,reserved+disk));
    std::unique_ptr<PreparedSlotWrite> owner;
    try{owner.reset(new PreparedSlotWrite(directory_,std::move(request),std::move(temporary),disk));}
    catch(...){directory_->staging_disk.fetch_sub(disk);throw;}
    owner->owned_fields_=std::move(temporary_memory);const auto& temporary_path=owner->temporary_;
    // 先有对象承接配额，创建失败也只清理由本请求独占创建的tmp。
    File file(::openat(directory_->fd,temporary_path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));
    if(file.get()<0){const auto error=errno;owner->temporary_.clear();throw SlotMediaError("StorageWriteFailed",error);}
    auto staged=checkpoint_share(std::move(owner));
    try{
        point(SlotIoPoint::before_write);write_all(file.get(),input.bytes,cancel);point(SlotIoPoint::file_sync);sync_file(file.get(),"StorageFileSyncFailed");
        point(SlotIoPoint::file_close);if(::close(file.release())<0)throw SlotMediaError("StorageCloseFailed",errno);
        check_cancel(&cancel);point(SlotIoPoint::stage_readback);verify_file(temporary_path.c_str(),input.bytes,&cancel);return staged;
    }catch(...){const auto cleanup_error=staged->cleanup();if(cleanup_error)throw SlotMediaError("StorageCleanupFailed",cleanup_error);throw;}
}
// 原owner核验后只能提升同一阶段；二次精确CAS、最后取消、rename与耐久屏障按序执行。
SlotDurability ResidentSlotMedia::promote_after_owner_validation(PreparedSlotWrite& staged,SlotCancellation& cancel){
    if(staged.directory_!=directory_||staged.published_)throw SlotMediaError("StorageInvalidStage");
    if(staged.stream_request_)return promote_stream(staged,cancel);
    const auto& input=staged.request();
    check_cancel(&cancel);verify_file(staged.temporary_.c_str(),input.bytes,&cancel);point(SlotIoPoint::before_publish);compare_banks(input.slot,input.expected,&cancel);
    const auto name=bank_name(input.slot,input.inactive);
    if(!cancel.begin_publish())throw SlotMediaError("StorageCancelled");
    if(::renameat(directory_->fd,staged.temporary_.c_str(),directory_->fd,name.c_str())<0)throw SlotMediaError("StorageRenameFailed",errno);
    staged.published_=true;cancel.published();
    try{
        point(SlotIoPoint::after_rename);point(SlotIoPoint::directory_sync);sync_file(directory_->fd,"StorageDirectorySyncFailed");
        point(SlotIoPoint::final_readback);verify_published(input);
        return SlotDurability::durable_confirmed;
    }catch(const SlotMediaError& error){throw error.with_durability(SlotDurability::visible_unconfirmed);}
}
// 仅重核并补同一已提升字节的屏障；不分新代、不编码新世界、不二投profile。
SlotDurability ResidentSlotMedia::recover_same_bytes(PreparedSlotWrite& staged){
    if(staged.directory_!=directory_||!staged.published_)throw SlotMediaError("StorageInvalidStage");
    if(staged.stream_request_)return recover_stream(staged);
    const auto& input=staged.request();const auto name=bank_name(input.slot,input.inactive);
    try{
        verify_file(name.c_str(),input.bytes,nullptr);File file(::openat(directory_->fd,name.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(file.get()<0)throw SlotMediaError("StorageReadFailed",errno);regular_stat(file.get());
        point(SlotIoPoint::recovery_file_sync);sync_file(file.get(),"StorageFileSyncFailed");if(::close(file.release())<0)throw SlotMediaError("StorageCloseFailed",errno);
        point(SlotIoPoint::recovery_directory_sync);sync_file(directory_->fd,"StorageDirectorySyncFailed");
        verify_published(input);
        return SlotDurability::durable_confirmed;
    }catch(const SlotMediaError& error){throw error.with_durability(SlotDurability::visible_unconfirmed);}
}
// fd租约绑定打开时同一普通文件；每次pread前后核size/mtime/ctime防原inode原地变化。
namespace {
// 匿名暂存只在FD真正关闭后归还磁盘额；构造失败及取消沿同一RAII路径。
class ImportDiskReservation {
public:
    ImportDiskReservation(std::shared_ptr<SlotDirectory> directory,std::uint64_t bytes,std::uint64_t limit):directory_(std::move(directory)),bytes_(bytes){
        auto current=directory_->staging_disk.load();do{if(current>limit||bytes>limit-current)throw SlotMediaError("StorageCapacityExceeded");}while(!directory_->staging_disk.compare_exchange_weak(current,current+bytes));
    }
    ~ImportDiskReservation()noexcept{directory_->staging_disk.fetch_sub(bytes_);}
    ImportDiskReservation(const ImportDiskReservation&)=delete;ImportDiskReservation& operator=(const ImportDiskReservation&)=delete;
private:std::shared_ptr<SlotDirectory> directory_;std::uint64_t bytes_;
};
class StreamFile final:public SlotReadLease {
public:
    StreamFile(File&& fd,struct stat identity,CheckpointJobReservation token,std::shared_ptr<ImportDiskReservation> disk={}):token_(std::move(token)),disk_(std::move(disk)),fd_(std::move(fd)),identity_(identity){}
    ~StreamFile()override=default;
    std::uint64_t size()const override{return static_cast<std::uint64_t>(identity_.st_size);}
    const CheckpointDigest& full_sha256()const noexcept override{return digest_;}
    std::uint64_t modified_unix()const noexcept override{return display_time(identity_);}
    Status read(std::uint64_t offset,std::uint8_t* data,std::size_t length)override{
        if(offset>size()||length>size()-offset)return failure("StorageReadRange");
        if(!unchanged())return failure("StorageChangedDuringRead");
        std::size_t done=0;while(done<length){const auto n=::pread(fd_.get(),data+done,length-done,static_cast<off_t>(offset+done));if(n<0){if(errno==EINTR)continue;return failure("StorageReadFailed");}if(!n)return failure("StorageChangedDuringRead");done+=std::size_t(n);}
        return unchanged()?Status{true,{},{}}:failure("StorageChangedDuringRead");
    }
    CheckpointDigest digest_{};
private:
    static Status failure(const char* code){return {false,{},{{code,"storage/stream"},ErrorKind::internal,{},{},{},{}}};}
    bool unchanged()const noexcept{struct stat now{};return ::fstat(fd_.get(),&now)==0&&now.st_dev==identity_.st_dev&&now.st_ino==identity_.st_ino&&now.st_size==identity_.st_size&&now.st_mtim.tv_sec==identity_.st_mtim.tv_sec&&now.st_mtim.tv_nsec==identity_.st_mtim.tv_nsec&&now.st_ctim.tv_sec==identity_.st_ctim.tv_sec&&now.st_ctim.tv_nsec==identity_.st_ctim.tv_nsec;}
    CheckpointJobReservation token_;std::shared_ptr<ImportDiskReservation> disk_;File fd_;struct stat identity_;
};
void stream_status(CheckpointJobStatus value){if(!value.ok)throw checkpoint_job_detail::FailureException{std::move(value.error)};}
template<class T>T stream_take(CheckpointJobResult<T> value){if(!value.ok)throw checkpoint_job_detail::FailureException{std::move(value.error)};return std::move(value.value);}
void stream_status(Status value){if(!value.ok)throw SlotMediaError(value.error.original.code);}
// 实际落盘sink只办理短写/取消；上层单一封套编码器负责固定chunk和同源摘要。
class FileWriteSink final:public CheckpointWriteSink {
public:
    FileWriteSink(int fd,const SlotCancellation& cancellation):fd_(fd),cancellation_(cancellation){}
    Status write(const std::uint8_t* data,std::size_t length)override{
        std::size_t done=0;while(done<length){check_cancel(&cancellation_);const auto n=::write(fd_,data+done,length-done);if(n<0){if(errno==EINTR)continue;throw SlotMediaError("StorageWriteFailed",errno);}if(!n)throw SlotMediaError("StorageWriteFailed",EIO);done+=std::size_t(n);}return {true,{},{}};
    }
private:int fd_;const SlotCancellation& cancellation_;
};
// 重放同一冻结producer与真实文件逐字节比较，hash相同不替代本次readback。
class CompareStreamSink final:public CheckpointWriteSink {
public:
    CompareStreamSink(CheckpointReadSource& source,const SlotCancellation* cancel):source_(source),cancel_(cancel){}
    Status write(const std::uint8_t* expected,std::size_t length)override{
        std::array<std::uint8_t,64> chunk{};while(length){check_cancel(cancel_);const auto n=std::min(length,chunk.size());stream_status(source_.read(at_,chunk.data(),n));if(!std::equal(chunk.begin(),chunk.begin()+n,expected))throw SlotMediaError("ReadbackFailed");at_+=n;expected+=n;length-=n;}return {true,{},{}};
    }
    std::uint64_t at_=0;
private:CheckpointReadSource& source_;const SlotCancellation* cancel_;
};
}
// 获取有限流lease只分配控制对象；完整文件仅以64字节局部chunk流过SHA。
std::shared_ptr<SlotReadLease> ResidentSlotMedia::open_stream(const char* name,CheckpointJobBudget& job,const SlotCancellation* cancel,bool absent)const{
    check_cancel(cancel);File fd(::openat(directory_->fd,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(fd.get()<0){if(absent&&errno==ENOENT)return {};throw SlotMediaError("StorageReadFailed",errno);}
    auto identity=regular_stat(fd.get());if(static_cast<std::uint64_t>(identity.st_size)>limits_.max_file_bytes)throw SlotMediaError("StorageCapacityExceeded");
    auto resource=stream_take(checkpoint_memory_resource(job,CheckpointJobUse::slot));ScopedCheckpointMemory scope(resource);auto scratch=stream_take(job.reserve(CheckpointJobUse::scratch,1024));
    auto result=checkpoint_make_shared<StreamFile>(std::move(fd),identity,CheckpointJobReservation{});picosha2::hash256_one_by_one hash;std::array<std::uint8_t,64> chunk{};
    for(std::uint64_t at=0;at<result->size();){check_cancel(cancel);stream_status(scratch.grow(0));const auto n=std::size_t(std::min<std::uint64_t>(chunk.size(),result->size()-at));stream_status(result->read(at,chunk.data(),n));hash.process(chunk.begin(),chunk.begin()+n);at+=n;}
    hash.finish();hash.get_hash_bytes(result->digest_.begin(),result->digest_.end());return result;
}
// 外来文件只读：路径是宿主显式选择，不转换为manual槽或元档案名字。实际fd仍只活在worker。
std::shared_ptr<SlotReadLease> ResidentSlotMedia::open_import_file(const std::string& path,std::uint64_t limit,CheckpointJobBudget& job,const SlotCancellation& cancel){
    if(path.empty()||path.size()>4096||path.front()!='/'||path.find('\0')!=std::string::npos||!limit)throw SlotMediaError("InvalidImportPath");check_cancel(&cancel);
    File fd(::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(fd.get()<0)throw SlotMediaError("StorageReadFailed",errno);auto identity=regular_stat(fd.get());if(static_cast<std::uint64_t>(identity.st_size)>limit)throw SlotMediaError("StorageCapacityExceeded");
    auto resource=stream_take(checkpoint_memory_resource(job,CheckpointJobUse::slot));ScopedCheckpointMemory scope(resource);auto result=checkpoint_make_shared<StreamFile>(std::move(fd),identity,CheckpointJobReservation{});auto scratch=stream_take(job.reserve(CheckpointJobUse::scratch,1024));picosha2::hash256_one_by_one hash;std::array<std::uint8_t,64> chunk{};
    for(std::uint64_t at=0;at<result->size();){check_cancel(&cancel);stream_status(scratch.grow(0));const auto n=std::size_t(std::min<std::uint64_t>(chunk.size(),result->size()-at));stream_status(result->read(at,chunk.data(),n));hash.process(chunk.begin(),chunk.begin()+n);at+=n;}hash.finish();hash.get_hash_bytes(result->digest_.begin(),result->digest_.end());return result;
}
// 外部文件先冻结到本目录匿名普通文件，完整hash/屏障/读回通过后才交私有decoder。
// O_TMPFILE没有可重命名的路径或遗留temp；不支持的文件系统明确拒绝，不能回退解析外部FD。
std::shared_ptr<SlotReadLease> ResidentSlotMedia::stage_import_file(const std::string& path,const CheckpointDigest& expected,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job,const SlotCancellation& cancel){
    stream_status(job.check_profile(capacity));stream_status(job.check_profile(slot));
    auto resource=stream_take(checkpoint_memory_resource(job,CheckpointJobUse::slot));ScopedCheckpointMemory scope(resource);
    auto source=open_import_file(path,slot.max_slot_bytes,job,cancel);if(source->full_sha256()!=expected)throw SlotMediaError("ImportSourceChanged");
    const auto limit=std::min({limits_.max_staging_disk_bytes,capacity.max_staging_disk_bytes,slot.max_slot_staging_disk_bytes});
    auto disk=checkpoint_make_shared<ImportDiskReservation>(directory_,source->size(),limit);
#ifdef O_TMPFILE
    File temporary(::openat(directory_->fd,".",O_TMPFILE|O_RDWR|O_CLOEXEC,0600));
    if(temporary.get()<0)throw SlotMediaError("StoragePrivateStagingFailed",errno);
#else
    throw SlotMediaError("StoragePrivateStagingUnsupported");
    File temporary;
#endif
    auto scratch=stream_take(job.reserve(CheckpointJobUse::scratch,1024));picosha2::hash256_one_by_one copied;std::array<std::uint8_t,64> chunk{};
    const auto chunk_limit=std::size_t(std::min<std::uint64_t>(chunk.size(),capacity.stream_chunk_bytes));
    for(std::uint64_t offset=0;offset<source->size();){
        check_cancel(&cancel);stream_status(scratch.grow(0));const auto count=std::size_t(std::min<std::uint64_t>(chunk_limit,source->size()-offset));stream_status(source->read(offset,chunk.data(),count));copied.process(chunk.begin(),chunk.begin()+count);
        std::size_t done=0;while(done<count){check_cancel(&cancel);const auto n=::write(temporary.get(),chunk.data()+done,count-done);if(n<0){if(errno==EINTR)continue;throw SlotMediaError("StorageWriteFailed",errno);}if(!n)throw SlotMediaError("StorageWriteFailed",EIO);done+=std::size_t(n);}offset+=count;
    }
    copied.finish();CheckpointDigest copied_digest{};copied.get_hash_bytes(copied_digest.begin(),copied_digest.end());if(copied_digest!=expected)throw SlotMediaError("ImportSourceChanged");
    check_cancel(&cancel);sync_file(temporary.get(),"StoragePrivateStagingSyncFailed");auto identity=regular_stat(temporary.get());if(std::uint64_t(identity.st_size)!=source->size()||identity.st_nlink!=0)throw SlotMediaError("StorageInvalidPrivateStaging");
    auto result=checkpoint_make_shared<StreamFile>(std::move(temporary),identity,CheckpointJobReservation{},std::move(disk));picosha2::hash256_one_by_one readback;
    for(std::uint64_t offset=0;offset<result->size();){check_cancel(&cancel);stream_status(scratch.grow(0));const auto count=std::size_t(std::min<std::uint64_t>(chunk_limit,result->size()-offset));stream_status(result->read(offset,chunk.data(),count));readback.process(chunk.begin(),chunk.begin()+count);offset+=count;}
    readback.finish();readback.get_hash_bytes(result->digest_.begin(),result->digest_.end());if(result->digest_!=expected)throw SlotMediaError("ReadbackFailed");return result;
}
std::shared_ptr<const SlotStreamBankPair> ResidentSlotMedia::inspect_stream(const SlotKey& key,CheckpointJobBudget& job,const SlotCancellation& cancel){
    point(SlotIoPoint::before_inspect);auto resource=stream_take(checkpoint_memory_resource(job,CheckpointJobUse::slot));ScopedCheckpointMemory scope(resource);
    auto owner=checkpoint_make_shared<SlotStreamBankPair>();
    owner->a=open_stream(bank_name(key,SlotBank::a).c_str(),job,&cancel,true);owner->b=open_stream(bank_name(key,SlotBank::b).c_str(),job,&cancel,true);
    const auto a=owner->a?owner->a->size():0,b=owner->b?owner->b->size():0;if(a>limits_.max_staging_disk_bytes||b>limits_.max_staging_disk_bytes-a)throw SlotMediaError("StorageCapacityExceeded");
    return owner;
}
// 两银行CAS核观察时的完整摘要/长度；未知读错不伪装成物理坏档或空槽。
void ResidentSlotMedia::compare_stream_banks(const SlotKey& key,const SlotStreamBankPair& expected,CheckpointJobBudget& job,const SlotCancellation* cancel)const{
    for(auto bank:{SlotBank::a,SlotBank::b}){const auto& before=bank==SlotBank::a?expected.a:expected.b;auto current=open_stream(bank_name(key,bank).c_str(),job,cancel,true);
        if(bool(before)!=bool(current)||(before&&(before->size()!=current->size()||before->full_sha256()!=current->full_sha256())))throw SlotMediaError("GenerationConflict");}
}
void ResidentSlotMedia::verify_stream_file(const char* name,const FrozenSlotEnvelope& frozen,CheckpointJobBudget& job,const SlotCancellation* cancel)const{
    auto source=open_stream(name,job,cancel);if(source->size()!=frozen.index().total_file_bytes)throw SlotMediaError("ReadbackFailed");
    auto scratch=stream_take(job.reserve(CheckpointJobUse::scratch,128));CompareStreamSink sink(*source,cancel);stream_status(frozen.write(sink,job));if(sink.at_!=source->size())throw SlotMediaError("ReadbackFailed");
}
// 复用原阶段/磁盘总预留/唯一worker，只把整Bytes运输替换为同一冻结三载荷的流重放。
std::shared_ptr<PreparedSlotWrite> ResidentSlotMedia::stage_stream(std::shared_ptr<const SlotStreamWriteRequest> input,SlotCancellation& cancel){
    if(!input||!input->job||!input->envelope||!input->expected||input->slot.kind!=SlotFileKind::slot_envelope_v2)throw SlotMediaError("StorageInvalidRequest");
    if(input->expected_generation<0||input->expected_generation==INT64_MAX||input->new_generation!=input->expected_generation+1||input->envelope->index().generation!=input->new_generation||input->envelope->index().save_id!=input->slot.id)throw SlotMediaError("InvalidGeneration");
    auto& job=*input->job;
    // 标量和银行容器仍真正冻结；每个对象/control由实际allocator收费，字符串单独fresh-copy预留。
    auto resource=stream_take(checkpoint_memory_resource(job,CheckpointJobUse::slot));ScopedCheckpointMemory scope(resource);
    struct Frozen {CheckpointMemoryCharge memory;SlotStreamWriteRequest value;Frozen(CheckpointMemoryCharge&& charge,const SlotStreamWriteRequest& original):memory(std::move(charge)),value(original){}};
    auto frozen=checkpoint_make_shared<Frozen>(CheckpointMemoryCharge(0),*input);frozen->value.expected=checkpoint_make_shared<const SlotStreamBankPair>(*input->expected);input=std::shared_ptr<const SlotStreamWriteRequest>(frozen,&frozen->value);
    if(!valid_key(input->slot)||!limits_.host_profile_namespace_binding.empty()||(input->inactive!=SlotBank::a&&input->inactive!=SlotBank::b))throw SlotMediaError("StorageInvalidRequest");
    const auto size=input->envelope->index().total_file_bytes;if(!size||size>limits_.max_file_bytes)throw SlotMediaError("StorageCapacityExceeded");
    const auto disk=checked_add(size,checked_add(input->expected->a?input->expected->a->size():0,input->expected->b?input->expected->b->size():0));
    compare_stream_banks(input->slot,*input->expected,job,&cancel);if(++temporary_sequence_==0)throw SlotMediaError("StorageOperationLimit");
    char temporary_name[128];const auto length=std::snprintf(temporary_name,sizeof(temporary_name),"%s.%c.hmslot.tmp.%lld.%llu.%llu",input->slot.id.c_str(),input->inactive==SlotBank::a?'A':'B',static_cast<long long>(::getpid()),static_cast<unsigned long long>(directory_->nonce),static_cast<unsigned long long>(temporary_sequence_));
    if(length<0||std::size_t(length)>=sizeof(temporary_name))throw SlotMediaError("StorageOperationLimit");
    CheckpointMemoryCharge name_memory(0);std::string temporary(temporary_name,std::size_t(length));
    auto reserved=directory_->staging_disk.load();do{if(reserved>limits_.max_staging_disk_bytes||disk>limits_.max_staging_disk_bytes-reserved)throw SlotMediaError("StorageCapacityExceeded");}while(!directory_->staging_disk.compare_exchange_weak(reserved,reserved+disk));
    std::unique_ptr<PreparedSlotWrite> owner;try{owner.reset(new PreparedSlotWrite(directory_,{},std::move(temporary),disk));}catch(...){directory_->staging_disk.fetch_sub(disk);throw;}
    owner->owned_fields_=std::move(name_memory);owner->stream_request_=std::move(input);auto& request=*owner->stream_request_;
    const auto& temporary_path=owner->temporary_;File fd(::openat(directory_->fd,temporary_path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));if(fd.get()<0){const int e=errno;owner->temporary_.clear();throw SlotMediaError("StorageWriteFailed",e);}
    auto stage=checkpoint_share(std::move(owner));try{
        point(SlotIoPoint::before_write);FileWriteSink sink(fd.get(),cancel);stream_status(request.envelope->write(sink,job));point(SlotIoPoint::file_sync);sync_file(fd.get(),"StorageFileSyncFailed");point(SlotIoPoint::file_close);if(::close(fd.release())<0)throw SlotMediaError("StorageCloseFailed",errno);
        point(SlotIoPoint::stage_readback);verify_stream_file(temporary_path.c_str(),*request.envelope,job,&cancel);stage->stream_readback_=open_stream(temporary_path.c_str(),job,&cancel);return stage;
    }catch(...){const auto e=stage->cleanup();if(e)throw SlotMediaError("StorageCleanupFailed",e);throw;}
}
SlotDurability ResidentSlotMedia::promote_stream(PreparedSlotWrite& stage,SlotCancellation& cancel){
    const auto& r=*stage.stream_request_;auto& job=*r.job;check_cancel(&cancel);verify_stream_file(stage.temporary_.c_str(),*r.envelope,job,&cancel);point(SlotIoPoint::before_publish);compare_stream_banks(r.slot,*r.expected,job,&cancel);
    const auto name=bank_name(r.slot,r.inactive);if(!cancel.begin_publish())throw SlotMediaError("StorageCancelled");if(::renameat(directory_->fd,stage.temporary_.c_str(),directory_->fd,name.c_str())<0)throw SlotMediaError("StorageRenameFailed",errno);stage.published_=true;cancel.published();
    try{point(SlotIoPoint::after_rename);point(SlotIoPoint::directory_sync);sync_file(directory_->fd,"StorageDirectorySyncFailed");point(SlotIoPoint::final_readback);verify_stream_file(name.c_str(),*r.envelope,job,nullptr);return SlotDurability::durable_confirmed;}
    catch(const SlotMediaError& e){throw e.with_durability(SlotDurability::visible_unconfirmed);}
}
SlotDurability ResidentSlotMedia::recover_stream(PreparedSlotWrite& stage){
    const auto& r=*stage.stream_request_;const auto name=bank_name(r.slot,r.inactive);try{verify_stream_file(name.c_str(),*r.envelope,*r.job,nullptr);File file(::openat(directory_->fd,name.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));if(file.get()<0)throw SlotMediaError("StorageReadFailed",errno);regular_stat(file.get());point(SlotIoPoint::recovery_file_sync);sync_file(file.get(),"StorageFileSyncFailed");if(::close(file.release())<0)throw SlotMediaError("StorageCloseFailed",errno);point(SlotIoPoint::recovery_directory_sync);sync_file(directory_->fd,"StorageDirectorySyncFailed");verify_stream_file(name.c_str(),*r.envelope,*r.job,nullptr);return SlotDurability::durable_confirmed;}catch(const SlotMediaError& e){throw e.with_durability(SlotDurability::visible_unconfirmed);}
}
} // 所有成功仅描述本次精确槽字节的介质阶段，不代表世界或准入业务成功。

namespace hanmo::resident::storage {
void JobSlotBankPair::retain(CheckpointRetentionCensus& c)const{
    c.owner(owner_);c.owner(limit_owner_);c.owner(service_owner_);
    if(!c.accounts(token_))token_.retain_control(c);
    c.original_token(token_);
}
}
