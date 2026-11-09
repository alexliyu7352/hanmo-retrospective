// HMAC/1严格规范编码；先核有限预算与全形状，再分配结果，不以摘要代替来源或同意。
#include "resident_profile_admission.hpp"
#include "resident_profile_copy.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include <picosha2.h>
#include <algorithm>
#include <limits>
#include <new>
#include <cstring>

namespace hanmo::resident::storage {
Status profile_admission_failure(const char* code,const StableId& path,ErrorKind kind){RuleError e;e.original={code,path};e.kind=kind;return {false,{},std::move(e)};}
namespace {
// 同一叶错误保原code；格式解析失败不会留下可消费的半胶囊。
template<class T>Outcome<T> fail(const char* code,const StableId& path={}){return {false,{},profile_admission_failure(code,path).error};}
template<class T>Outcome<T> failed(const Status& s){return {false,{},s.error};}
bool valid_world(const WorldId& w){return std::any_of(w.begin(),w.end(),[](auto b){return b!=0;});}
// String256按UTF-8字节而非字符数；拒超长、NUL、过长编码、代理与越界码点。
bool valid_string(const StableId& s){
    if(s.empty()||s.size()>256)return false;
    for(std::size_t i=0;i<s.size();){auto a=static_cast<unsigned char>(s[i++]);if(!a)return false;if(a<0x80)continue;
        unsigned count=0;std::uint32_t cp=0,min=0;if(a>=0xc2&&a<=0xdf){count=1;cp=a&31;min=0x80;}else if(a>=0xe0&&a<=0xef){count=2;cp=a&15;min=0x800;}else if(a>=0xf0&&a<=0xf4){count=3;cp=a&7;min=0x10000;}else return false;
        if(count>s.size()-i)return false;
        while(count--){auto b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
    }return true;
}
// UTF-8原字节顺序不依赖平台char是否有符号，不做Unicode归一化。
bool key_less(const DeliveryKey& a,const DeliveryKey& b){if(a.source_world_id!=b.source_world_id)return a.source_world_id<b.source_world_id;return std::lexicographical_compare(a.delivery_id.begin(),a.delivery_id.end(),b.delivery_id.begin(),b.delivery_id.end(),[](char x,char y){return static_cast<unsigned char>(x)<static_cast<unsigned char>(y);});}
bool add_size(std::uint64_t& n,std::uint64_t add){if(add>std::numeric_limits<std::uint64_t>::max()-n)return false;n+=add;return true;}
Status limits_ok(const AdmissionCodecLimits& l){if(!l.max_capsule_bytes||l.max_capsule_bytes>std::uint64_t(std::numeric_limits<Integer>::max()))return profile_admission_failure("InvalidAdmissionCapacityProfile");return {true,{},{}};}
// 行的数量/结构/规范序先验；不同hash的重复key也拒，不能折叠成单条。
Status baseline_shape(const WorldId& world,const std::vector<EntryFingerprint>& rows,std::uint64_t& bytes){
    if(!valid_world(world))return profile_admission_failure("InvalidAdmissionWorld");
    if(rows.size()>admission_fingerprint_limit)return profile_admission_failure("AdmissionBaselineLimit");
    bytes=4;
    for(std::size_t i=0;i<rows.size();++i){const auto& r=rows[i];if(r.key.source_world_id!=world||!valid_string(r.key.delivery_id))return profile_admission_failure("InvalidAdmissionFingerprint");if(i&&!key_less(rows[i-1].key,r.key))return profile_admission_failure(same_delivery_key(rows[i-1].key,r.key)?"DuplicateDeliveryKey":"NonCanonicalAdmissionBaseline");if(!add_size(bytes,16+2+r.key.delivery_id.size()+32))return profile_admission_failure("AdmissionCapacityExceeded");}return {true,{},{}};
}
// 增量摘要不分配第二份完整baseline字节。
// 固定依赖process会先复制整段；每次最多64字节，禁止一次喂入整个元档案。
void hash_data(picosha2::hash256_one_by_one& h,const std::uint8_t* bytes,std::size_t size){for(std::size_t at=0;at<size;){const auto n=std::min<std::size_t>(64,size-at);h.process(bytes+at,bytes+at+n);at+=n;}}
void hash_u32(picosha2::hash256_one_by_one& h,std::uint32_t n){std::uint8_t b[4];for(unsigned i=0;i<4;++i)b[i]=std::uint8_t(n>>(8*i));h.process(b,b+4);}
void hash_string(picosha2::hash256_one_by_one& h,const StableId& s){std::uint8_t b[2]={std::uint8_t(s.size()),std::uint8_t(s.size()>>8)};h.process(b,b+2);hash_data(h,reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
CheckpointDigest hash_baseline(const std::vector<EntryFingerprint>& rows){picosha2::hash256_one_by_one h;hash_u32(h,static_cast<std::uint32_t>(rows.size()));for(const auto& r:rows){h.process(r.key.source_world_id.begin(),r.key.source_world_id.end());hash_string(h,r.key.delivery_id);h.process(r.entry_sha256.begin(),r.entry_sha256.end());}h.finish();CheckpointDigest out{};h.get_hash_bytes(out.begin(),out.end());return out;}
// 测长核同一输入的完整形状与摘要，writer不补算或修复调用者提供的hash。
Outcome<std::uint64_t> capsule_size(const SlotAdmissionCapsule& c,const AdmissionCodecLimits& l){auto v=limits_ok(l);if(!v.ok)return failed<std::uint64_t>(v);v=validate_profile_namespace(c.name_space);if(!v.ok)return failed<std::uint64_t>(v);if(!valid_string(c.admission_id))return fail<std::uint64_t>("InvalidAdmissionId");std::uint64_t baseline=0;v=baseline_shape(c.source_world_id,c.baseline,baseline);if(!v.ok)return failed<std::uint64_t>(v);if(hash_baseline(c.baseline)!=c.baseline_sha256)return fail<std::uint64_t>("AdmissionBaselineDigestMismatch");std::uint64_t n=5+2+c.admission_id.size()+2+c.name_space.profile_uid.size()+2+16+32+32;if(c.name_space.preview_session_id&&!add_size(n,2+c.name_space.preview_session_id->size()))return fail<std::uint64_t>("AdmissionCapacityExceeded");if(!add_size(n,baseline)||n>l.max_capsule_bytes)return fail<std::uint64_t>("AdmissionCapacityExceeded");return {true,n,{}};}
void write_uint(Bytes& b,std::uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)b.push_back(std::uint8_t(n>>(8*i)));}
template<class T>void write_raw(Bytes& b,const T& r){b.insert(b.end(),r.begin(),r.end());}
void write_string(Bytes& b,const StableId& s){write_uint(b,s.size(),2);write_raw(b,s);}
// 两个运输出口共用这一个固定字段序，不能维护第二份胶囊格式。
template<class Sink>void emit_capsule(const SlotAdmissionCapsule& c,Sink& sink){
    auto number=[&](std::uint64_t n,unsigned width){std::uint8_t data[8];for(unsigned i=0;i<width;++i)data[i]=std::uint8_t(n>>(8*i));sink.write(data,width);};
    auto text=[&](const StableId& value){number(value.size(),2);sink.write(reinterpret_cast<const std::uint8_t*>(value.data()),value.size());};
    number(0x43414d48u,4);number(1,1);text(c.admission_id);text(c.name_space.profile_uid);number(static_cast<unsigned>(c.name_space.purpose),1);number(c.name_space.preview_session_id?1:0,1);if(c.name_space.preview_session_id)text(*c.name_space.preview_session_id);sink.write(c.source_world_id.data(),c.source_world_id.size());sink.write(c.origin_snapshot_sha256.data(),c.origin_snapshot_sha256.size());number(c.baseline.size(),4);for(const auto& f:c.baseline){sink.write(f.key.source_world_id.data(),f.key.source_world_id.size());text(f.key.delivery_id);sink.write(f.entry_sha256.data(),f.entry_sha256.size());}sink.write(c.baseline_sha256.data(),c.baseline_sha256.size());
}
struct VectorAdmissionSink {Bytes& value;void write(const std::uint8_t* p,std::size_t n){value.insert(value.end(),p,p+n);}};
struct JobAdmissionSink {CheckpointJobBytes& value;std::size_t at=0;void write(const std::uint8_t* p,std::size_t n){auto status=value.write(at,p,n);if(!status.ok)throw checkpoint_job_detail::FailureException{std::move(status.error)};at+=n;}};
// reader两遍共用原字段语法；外部源每次实际读的字节同时进入段摘要。
struct AdmissionReadFailure { RuleError error; };
class Reader {
public:
    explicit Reader(const Bytes& b,CodecDecodeBudget* budget=nullptr):bytes_(&b),length_(b.size()),budget_(budget){}
    Reader(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,CodecDecodeBudget* budget=nullptr)
        :source_(&source),begin_(begin),length_(length),budget_(budget){}
    std::uint64_t size()const noexcept{return length_;}
    bool take(std::uint8_t* out,std::size_t n){
        if(at>length_||n>length_-at){okay=false;return false;}
        if(bytes_)std::copy_n(bytes_->data()+static_cast<std::size_t>(at),n,out);
        else for(std::size_t done=0;done<n;){const auto chunk=std::min<std::size_t>(64,n-done);auto status=source_->read(begin_+at+done,out+done,chunk);if(!status.ok)throw AdmissionReadFailure{std::move(status.error)};hash_.process(out+done,out+done+chunk);done+=chunk;}
        at+=n;return true;
    }
    std::uint64_t number(unsigned width){std::uint8_t data[8]{};if(width>8||!take(data,width))return 0;std::uint64_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint64_t(data[i])<<(8*i);return v;}
    bool string(StableId& out){const auto n=number(2);std::uint8_t data[256];if(!okay||n<1||n>256||!take(data,static_cast<std::size_t>(n)))return okay=false;
        if(budget_){auto charged=budget_->charge_candidate(n+1);if(!charged.ok)throw AdmissionReadFailure{std::move(charged.error)};}
        // fresh range构造避免assign几何扩容；目标STL容量约束沿同CodecVisitor。
        StableId fresh(reinterpret_cast<const char*>(data),static_cast<std::size_t>(n));out.swap(fresh);return okay=valid_string(out);
    }
    template<class T>bool raw(T& out){return take(out.data(),out.size());}
    bool digest_matches(const CheckpointDigest& expected){if(!source_||at!=length_)return false;hash_.finish();CheckpointDigest actual{};hash_.get_hash_bytes(actual.begin(),actual.end());return actual==expected;}
    CodecDecodeBudget* budget()const noexcept{return budget_;}
    std::uint64_t at=0;bool okay=true;
private:
    const Bytes* bytes_=nullptr;CheckpointReadSource* source_=nullptr;std::uint64_t begin_=0,length_=0;CodecDecodeBudget* budget_=nullptr;picosha2::hash256_one_by_one hash_;
};
// 测长pass只保相邻key/单个fingerprint，拒绝短读/尾随/未知presence与错误摘要。
Outcome<SlotAdmissionCapsule> read_capsule(Reader& r,const AdmissionCodecLimits&,bool materialize){SlotAdmissionCapsule c;if(r.number(4)!=0x43414d48u||r.number(1)!=1)return fail<SlotAdmissionCapsule>("UnsupportedAdmissionCapsule");if(!r.string(c.admission_id)||!r.string(c.name_space.profile_uid))return fail<SlotAdmissionCapsule>("InvalidAdmissionString");const auto purpose=r.number(1),present=r.number(1);if(purpose>2||present>1||(purpose==1)!=(present==1))return fail<SlotAdmissionCapsule>("InvalidProfileNamespace");c.name_space.purpose=static_cast<ProfilePurpose>(purpose);if(present){StableId s;if(!r.string(s))return fail<SlotAdmissionCapsule>("InvalidAdmissionString");c.name_space.preview_session_id=std::move(s);}if(!r.raw(c.source_world_id)||!valid_world(c.source_world_id)||!r.raw(c.origin_snapshot_sha256))return fail<SlotAdmissionCapsule>("InvalidAdmissionWorld");const auto count=r.number(4);if(!r.okay||count>admission_fingerprint_limit||count>(r.size()-r.at)/51)return fail<SlotAdmissionCapsule>("AdmissionBaselineLimit");
    // 关闭纯工作内存估算；上方 count/剩余字节及后续完整形状、摘要门照常执行。
    if(materialize){if(r.budget()){auto charged=r.budget()->charge_candidate(count*sizeof(EntryFingerprint));if(!charged.ok)return failed<SlotAdmissionCapsule>(charged);}c.baseline.reserve(static_cast<std::size_t>(count));}picosha2::hash256_one_by_one h;hash_u32(h,static_cast<std::uint32_t>(count));std::optional<DeliveryKey> previous;
    for(std::uint64_t i=0;i<count;++i){EntryFingerprint f;if(!r.raw(f.key.source_world_id)||!r.string(f.key.delivery_id)||!r.raw(f.entry_sha256)||f.key.source_world_id!=c.source_world_id)return fail<SlotAdmissionCapsule>("InvalidAdmissionFingerprint");const DeliveryKey* prior=materialize?(c.baseline.empty()?nullptr:&c.baseline.back().key):(previous?&*previous:nullptr);if(prior&&!key_less(*prior,f.key))return fail<SlotAdmissionCapsule>(same_delivery_key(*prior,f.key)?"DuplicateDeliveryKey":"NonCanonicalAdmissionBaseline");if(!materialize){DeliveryKey next{f.key.source_world_id,StableId(f.key.delivery_id.data(),f.key.delivery_id.size())};previous=std::move(next);}h.process(f.key.source_world_id.begin(),f.key.source_world_id.end());hash_string(h,f.key.delivery_id);h.process(f.entry_sha256.begin(),f.entry_sha256.end());if(materialize)c.baseline.push_back(std::move(f));}
    if(!r.raw(c.baseline_sha256)||r.at!=r.size())return fail<SlotAdmissionCapsule>("InvalidAdmissionCapsuleLength");
    h.finish();CheckpointDigest digest{};h.get_hash_bytes(digest.begin(),digest.end());if(digest!=c.baseline_sha256)return fail<SlotAdmissionCapsule>("AdmissionBaselineDigestMismatch");return {true,std::move(c),{}};
}
}
bool same_profile_namespace(const ProfileNamespace& a,const ProfileNamespace& b)noexcept{return a.profile_uid==b.profile_uid&&a.purpose==b.purpose&&a.preview_session_id==b.preview_session_id;}
bool same_delivery_key(const DeliveryKey& a,const DeliveryKey& b)noexcept{return a.source_world_id==b.source_world_id&&a.delivery_id==b.delivery_id;}
bool same_entry_fingerprint(const EntryFingerprint& a,const EntryFingerprint& b)noexcept{return same_delivery_key(a.key,b.key)&&a.entry_sha256==b.entry_sha256;}
bool same_admission_capsule(const SlotAdmissionCapsule& a,const SlotAdmissionCapsule& b)noexcept{return a.admission_id==b.admission_id&&same_profile_namespace(a.name_space,b.name_space)&&a.source_world_id==b.source_world_id&&a.origin_snapshot_sha256==b.origin_snapshot_sha256&&a.baseline_sha256==b.baseline_sha256&&a.baseline.size()==b.baseline.size()&&std::equal(a.baseline.begin(),a.baseline.end(),b.baseline.begin(),same_entry_fingerprint);}
Status validate_profile_namespace(const ProfileNamespace& n){if(!valid_string(n.profile_uid)||static_cast<unsigned>(n.purpose)>2||(n.purpose==ProfilePurpose::author_preview)!=bool(n.preview_session_id)||(n.preview_session_id&&!valid_string(*n.preview_session_id)))return profile_admission_failure("InvalidProfileNamespace");return {true,{},{}};}
Outcome<CheckpointDigest> admission_baseline_digest(const WorldId& world,const std::vector<EntryFingerprint>& rows,const AdmissionCodecLimits& l){try{auto v=limits_ok(l);if(!v.ok)return failed<CheckpointDigest>(v);std::uint64_t bytes=0;v=baseline_shape(world,rows,bytes);if(!v.ok)return failed<CheckpointDigest>(v);if(bytes>l.max_capsule_bytes)return fail<CheckpointDigest>("AdmissionCapacityExceeded");return {true,hash_baseline(rows),{}};}catch(const std::bad_alloc&){return fail<CheckpointDigest>("AdmissionResourceExhausted");}}
Outcome<Bytes> encode_slot_admission_capsule(const SlotAdmissionCapsule& c,const AdmissionCodecLimits& l){try{auto measured=capsule_size(c,l);if(!measured.ok)return {false,{},measured.error};Bytes out;out.reserve(static_cast<std::size_t>(measured.value));VectorAdmissionSink sink{out};emit_capsule(c,sink);return {true,std::move(out),{}};}catch(const std::bad_alloc&){return fail<Bytes>("AdmissionResourceExhausted");}}
Outcome<SlotAdmissionCapsule> decode_slot_admission_capsule(const Bytes& b,const AdmissionCodecLimits& l){try{auto v=limits_ok(l);if(!v.ok)return failed<SlotAdmissionCapsule>(v);if(b.size()>l.max_capsule_bytes)return fail<SlotAdmissionCapsule>("AdmissionCapacityExceeded");{Reader scan(b);auto checked=read_capsule(scan,l,false);if(!checked.ok)return checked;}Reader reader(b);return read_capsule(reader,l,true);}catch(const std::bad_alloc&){return fail<SlotAdmissionCapsule>("AdmissionResourceExhausted");}}
Status validate_admission_record(const AdmissionRecord& r,const AdmissionCodecLimits& l){try{auto m=capsule_size(r.capsule,l);if(!m.ok)return {false,{},m.error};if(r.record_generation<0||static_cast<unsigned>(r.origin)>1)return profile_admission_failure("InvalidAdmissionRecord");if(r.origin==AdmissionOrigin::local_created&&!r.capsule.baseline.empty())return profile_admission_failure("InvalidLocalCreatedBaseline");std::uint64_t bytes=0;auto v=baseline_shape(r.capsule.source_world_id,r.allowed_history,bytes);if(!v.ok)return v;std::size_t j=0;for(const auto& f:r.allowed_history){while(j<r.capsule.baseline.size()&&key_less(r.capsule.baseline[j].key,f.key))++j;if(j==r.capsule.baseline.size()||!same_entry_fingerprint(f,r.capsule.baseline[j]))return profile_admission_failure("HistoryOutsideAdmissionBaseline");}return {true,{},{}};}catch(const std::bad_alloc&){return profile_admission_failure("AdmissionResourceExhausted",{},ErrorKind::internal);}}
Status verify_local_admission(const SlotAdmissionCapsule& c,const WorldId& world,const ProfileNamespace& host,const AdmissionRecord* r,const AdmissionCodecLimits& l){auto measured=capsule_size(c,l);if(!measured.ok)return {false,{},measured.error};auto valid=validate_profile_namespace(host);if(!valid.ok)return valid;if(c.source_world_id!=world)return profile_admission_failure("AdmissionCheckpointWorldMismatch");if(!same_profile_namespace(c.name_space,host))return profile_admission_failure("AdmissionNamespaceMismatch");if(!r)return profile_admission_failure("AdmissionRecoveryRequired");valid=validate_admission_record(*r,l);if(!valid.ok)return valid;if(!same_admission_capsule(c,r->capsule))return profile_admission_failure("AdmissionRecordMismatch");return {true,{},{}};}
CheckpointDigest profile_sha256(const std::uint8_t* bytes,std::size_t size){CheckpointDigest result{};picosha2::hash256_one_by_one h;hash_data(h,bytes,size);h.finish();h.get_hash_bytes(result.begin(),result.end());return result;}
// 命名空间绑定从宿主完整typed值规范编码，绝不作为目录片段使用。
Outcome<StableId> profile_namespace_binding(const ProfileNamespace& n){try{auto v=validate_profile_namespace(n);if(!v.ok)return {false,{},v.error};Bytes bytes;write_string(bytes,n.profile_uid);write_uint(bytes,static_cast<unsigned>(n.purpose),1);write_uint(bytes,n.preview_session_id?1:0,1);if(n.preview_session_id)write_string(bytes,*n.preview_session_id);return {true,profile_digest_hex(profile_sha256(bytes.data(),bytes.size())),{}};}catch(const std::bad_alloc&){return fail<StableId>("AdmissionResourceExhausted");}}
// 十六进制仅用在受信本机绑定键与诊断，不替代胶囊的32 raw bytes。
StableId profile_digest_hex(const CheckpointDigest& digest){static constexpr char digits[]="0123456789abcdef";StableId value;value.reserve(64);for(auto b:digest){value.push_back(digits[b>>4]);value.push_back(digits[b&15]);}return value;}
// 预留先于第二遍typed分配；深值销毁先于租约归还。
struct JobAdmissionAccess {
    static void retain(const JobAdmissionCapsule& value,CheckpointRetentionCensus& census){
        if(!census.accounts(value.reservation_))value.reservation_.retain_control(census);
        census.original_token(value.reservation_);
        // value_ is inline in its real owner, not a separate heap allocation.
    }
    static JobAdmissionCapsule own(SlotAdmissionCapsule&& value,CheckpointJobReservation&& token){JobAdmissionCapsule out;out.reservation_=std::move(token);out.value_=std::move(value);return out;}
};
JobAdmissionCapsule& JobAdmissionCapsule::operator=(JobAdmissionCapsule&& other)noexcept{if(this!=&other){this->~JobAdmissionCapsule();new(this)JobAdmissionCapsule(std::move(other));}return *this;}
namespace {
CheckpointJobStatus capsule_profiles(const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){auto checked=job.check_profile(capacity);if(!checked.ok)return checked;return job.check_profile(slot);}
}
CheckpointJobResult<CheckpointJobBytes> encode_slot_admission_capsule(const SlotAdmissionCapsule& capsule,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{auto checked=capsule_profiles(capacity,slot,job);if(!checked.ok)return {false,{},std::move(checked.error)};
        auto retention=job.reserve(CheckpointJobUse::encode,0);if(!retention.ok)return {false,{},std::move(retention.error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};
        auto measured=capsule_size(capsule,{slot.max_admission_capsule_bytes});if(!measured.ok)return {false,{},job.capture_failure(std::move(measured.error))};
        auto bytes=CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,measured.value);if(!bytes.ok)return bytes;JobAdmissionSink sink{bytes.value};emit_capsule(capsule,sink);return bytes;
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<CheckpointJobBytes> encode_slot_admission_capsule(const JobAdmissionCapsule& owner,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{auto checked=capsule_profiles(capacity,slot,job);if(!checked.ok)return {false,{},std::move(checked.error)};
        auto memory=checkpoint_memory_resource(job,CheckpointJobUse::scratch);if(!memory.ok)return {false,{},std::move(memory.error)};ScopedCheckpointMemory scope(memory.value);
        CheckpointRetentionCensus census(memory.value,CheckpointJobUse::encode);JobAdmissionAccess::retain(owner,census);
        auto retention=job.reserve(CheckpointJobUse::encode,0);if(!retention.ok)return {false,{},std::move(retention.error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};
        const auto& capsule=owner.value();auto measured=capsule_size(capsule,{slot.max_admission_capsule_bytes});if(!measured.ok)return {false,{},job.capture_failure(std::move(measured.error))};
        auto bytes=CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,measured.value);if(!bytes.ok)return bytes;JobAdmissionSink sink{bytes.value};emit_capsule(capsule,sink);return bytes;
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<JobAdmissionCapsule> decode_slot_admission_capsule(const CheckpointJobBytes& bytes,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{auto checked=capsule_profiles(capacity,slot,job);if(!checked.ok)return {false,{},std::move(checked.error)};if(!bytes.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        if(bytes.bytes().size()>slot.max_admission_capsule_bytes)return {false,{},job.capture_failure(profile_admission_failure("AdmissionCapacityExceeded",{},ErrorKind::internal).error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};const AdmissionCodecLimits limits{slot.max_admission_capsule_bytes};
        {Reader scan(bytes.bytes());auto first=read_capsule(scan,limits,false);if(!first.ok)return {false,{},job.capture_failure(std::move(first.error))};}
        CodecDecodeBudget budget(capacity,job);auto object=budget.charge_candidate(sizeof(SlotAdmissionCapsule));if(!object.ok)return {false,{},job.capture_failure(std::move(object.error))};Reader reader(bytes.bytes(),&budget);auto value=read_capsule(reader,limits,true);if(!value.ok)return {false,{},job.capture_failure(std::move(value.error))};return {true,JobAdmissionAccess::own(std::move(value.value),budget.take_job_reservation()),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(AdmissionReadFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<JobAdmissionCapsule> decode_slot_admission_capsule(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,const CheckpointDigest& descriptor,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{auto checked=capsule_profiles(capacity,slot,job);if(!checked.ok)return {false,{},std::move(checked.error)};
        if(length>slot.max_admission_capsule_bytes||begin>source.size()||length>source.size()-begin)return {false,{},job.capture_failure(profile_admission_failure("InvalidAdmissionCapsuleLength").error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};const AdmissionCodecLimits limits{slot.max_admission_capsule_bytes};
        {Reader scan(source,begin,length);auto first=read_capsule(scan,limits,false);if(!first.ok)return {false,{},job.capture_failure(std::move(first.error))};if(!scan.digest_matches(descriptor))return {false,{},job.capture_failure(profile_admission_failure("AdmissionSourceChanged").error)};}
        CodecDecodeBudget budget(capacity,job);auto object=budget.charge_candidate(sizeof(SlotAdmissionCapsule));if(!object.ok)return {false,{},job.capture_failure(std::move(object.error))};Reader reader(source,begin,length,&budget);auto value=read_capsule(reader,limits,true);if(!value.ok)return {false,{},job.capture_failure(std::move(value.error))};if(!reader.digest_matches(descriptor))return {false,{},job.capture_failure(profile_admission_failure("AdmissionSourceChanged").error)};return {true,JobAdmissionAccess::own(std::move(value.value),budget.take_job_reservation()),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(AdmissionReadFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}

CheckpointJobResult<std::uint64_t> write_slot_admission_capsule_to(CheckpointWriteSink* output,const SlotAdmissionCapsule& value,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{auto profiles=capsule_profiles(capacity,slot,job);if(!profiles.ok)return {false,{},std::move(profiles.error)};auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};
        auto measured=capsule_size(value,{slot.max_admission_capsule_bytes});if(!measured.ok)return {false,{},job.capture_failure(std::move(measured.error))};
        if(output){struct Sink{CheckpointWriteSink& out;void write(const std::uint8_t* p,std::size_t n){auto s=out.write(p,n);if(!s.ok)throw AdmissionReadFailure{std::move(s.error)};}} sink{*output};emit_capsule(value,sink);}return {true,std::move(measured.value),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(AdmissionReadFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
CheckpointJobResult<SlotAdmissionCapsule> decode_slot_admission_capsule_into_job(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,const CheckpointDigest& digest,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CodecDecodeBudget& budget,CheckpointJobBudget& job){
    try{auto profiles=capsule_profiles(capacity,slot,job);if(!profiles.ok)return {false,{},std::move(profiles.error)};if(!budget.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        if(length>slot.max_admission_capsule_bytes||begin>source.size()||length>source.size()-begin)return {false,{},job.capture_failure(profile_admission_failure("InvalidAdmissionCapsuleLength").error)};
        auto scratch=job.reserve(CheckpointJobUse::scratch,0);if(!scratch.ok)return {false,{},std::move(scratch.error)};const AdmissionCodecLimits limits{slot.max_admission_capsule_bytes};
        {Reader scan(source,begin,length);auto checked=read_capsule(scan,limits,false);if(!checked.ok)return {false,{},job.capture_failure(std::move(checked.error))};if(!scan.digest_matches(digest))return {false,{},job.capture_failure(profile_admission_failure("AdmissionSourceChanged").error)};}
        Reader reader(source,begin,length,&budget);auto value=read_capsule(reader,limits,true);if(!value.ok)return {false,{},job.capture_failure(std::move(value.error))};if(!reader.digest_matches(digest))return {false,{},job.capture_failure(profile_admission_failure("AdmissionSourceChanged").error)};return {true,std::move(value.value),{}};
    }catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(AdmissionReadFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}

}
