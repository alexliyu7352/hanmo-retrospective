#include "resident/kernel/catalog_section_observation.hpp"
// 外封装复用同一typed visitor和已锁SHA实现；完整解码/目录匹配后才交中央验证候选。
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/kernel/typed_supply_observation.hpp"
#include <exception>
#include <limits>
#include "checkpoint_selection.hpp"
#include "resident/kernel/checkpoint_phase_timing.hpp"
#include <picosha2.h>
#include <set>
#include <cerrno>

// 各目录目标已借用原 checkpoint 定义单元；只分派同步观察，不读业务或资源。
namespace hanmo::resident::catalog_section_observation {
namespace {
thread_local Binding current_binding;
thread_local Scope* current_scope = nullptr;
thread_local std::uint64_t next_call = 0;
}
Binding exchange_binding(Binding next) noexcept {
  const auto old = current_binding; current_binding = next; return old;
}
Scope::Scope(Kind kind, std::uint32_t site, const char* category,
             std::string_view detail, const void* root,
             std::int64_t codec_version) noexcept {
  if (!current_binding.sink || next_call == std::numeric_limits<std::uint64_t>::max()) return;
  previous_ = current_scope;
  if (previous_ && previous_->event_.depth == std::numeric_limits<std::uint32_t>::max()) return;
  binding_ = current_binding;
  event_ = {Edge::begin, kind, site, category, detail, root, codec_version,
            ++next_call, previous_ ? previous_->event_.call_id : 0,
            previous_ ? previous_->event_.depth + 1 : 0, 0, false};
  exceptions_ = std::uncaught_exceptions(); current_scope = this; emit(Edge::begin);
}
Scope::~Scope() noexcept {
  if (!binding_.sink) return;
  emit(std::uncaught_exceptions() > exceptions_ ? Edge::abort : Edge::returned_unknown);
  current_scope = previous_;
}
void Scope::emit(Edge edge) noexcept {
  if (!binding_.sink) return;
  event_.edge = edge;
  const int saved = errno;
  binding_.sink(binding_.context, event_);
  errno = saved;
}
void Scope::stop(std::uint64_t bytes, bool has_bytes) noexcept {
  if (!binding_.sink) return;
  event_.bytes = bytes; event_.has_bytes = has_bytes;
  emit(Edge::end_success); current_scope = previous_; binding_ = {};
}
}
// 独立检查点目标也拥有此纯标量分派；不反向依赖 ResidentWorld。
namespace hanmo::resident::world_stage_observation {
namespace { thread_local NodeBinding node_binding; thread_local ByteBinding byte_binding; }
NodeBinding exchange_node_binding(NodeBinding next) noexcept {
  const auto old = node_binding; node_binding = next; return old;
}
ByteBinding exchange_byte_binding(ByteBinding next) noexcept {
  const auto old = byte_binding; byte_binding = next; return old;
}
void observe_node(const NodeEvent& event) noexcept {
  const auto binding = node_binding;
  if (!binding.sink) return;
  const int saved = errno; binding.sink(binding.context, event); errno = saved;
}
void observe_bytes(const ByteEvent& event) noexcept {
  const auto binding = byte_binding;
  if (!binding.sink) return;
  const int saved = errno; binding.sink(binding.context, event); errno = saved;
}
}
// 独立 checkpoint 目标已有本定义单元；纯观测不反向依赖 World 或业务资源。
namespace hanmo::resident::typed_supply_observation {
namespace {
thread_local Binding current_binding;
thread_local Scope* current_scope = nullptr;
thread_local std::uint64_t next_call = 0;
const char* label(Stage stage) noexcept {
  switch(stage) {
  case Stage::roster_all_supply: return "military.roster.all_supply_proof";
  case Stage::field_supply_reader: return "military.supply.field_reader";
  case Stage::period_reader: return "military.supply.period_reader";
  case Stage::calendar_identity: return "military.supply.calendar_identity";
  }
  return "UNKNOWN";
}
}
Binding exchange_binding(Binding next) noexcept {
  const auto old = current_binding; current_binding = next; return old;
}
Scope::Scope(Stage stage, Context context) noexcept : context_(context), stage_(stage) {
  if(!current_binding.sink || next_call==std::numeric_limits<std::uint64_t>::max()) return;
  binding_=current_binding; previous_=current_scope;
  if(previous_) { parent_id_=previous_->call_id_; depth_=previous_->depth_+1; }
  call_id_=++next_call; exceptions_=std::uncaught_exceptions(); current_scope=this;
  emit(Edge::begin);
  // begin 中的编码借用不可悬挂到后续回调；宿主已同步复制必要字节。
  context_.actual_now=nullptr;
}
Scope::~Scope() noexcept {
  if(!binding_.sink) return;
  emit(std::uncaught_exceptions()>exceptions_?Edge::abort:Edge::incomplete);
  current_scope=previous_;
}
void Scope::emit(Edge edge,const char* name,const EncodedWorldMoment* moment_value) noexcept {
  if(!binding_.sink) return;
  const int saved=errno;
  binding_.sink(binding_.context,{edge,stage_,label(stage_),call_id_,parent_id_,depth_,context_,name,moment_value});
  errno=saved;
}
void Scope::stop(bool success) noexcept {
  if(!binding_.sink) return;
  emit(success?Edge::end:Edge::returned_failure); current_scope=previous_; binding_={};
}
void Scope::moment(const char* name,const EncodedWorldMoment& value) noexcept {
  emit(Edge::context,name,&value);
}
void Scope::cut(std::uint64_t value) noexcept {
  context_.source_cut=value; context_.has_source_cut=true; emit(Edge::context);
}
}
namespace hanmo::resident {
namespace {
// 外层版本与域版本独立；源world持久身份不等同某一执行epoch。
constexpr const char* magic="HM-RESIDENT-CHECKPOINT/1\n";
// 固定目录所有身份字段，包含作者模块原序；不从缺省值推断匹配。
Status visit_catalog(CodecVisitor& c,CatalogIdentity& x) {
    c.field("content_id",x.content_id); c.field("content_version",x.content_version);
    c.field("artifact_sha256",x.artifact_sha256);
    c.field("content_digest",x.content_digest); c.field("ruleset_id",x.ruleset_id); c.field("schema_sha256",x.schema_sha256);
    c.optional("pack_set_digest",x.pack_set_digest,[](CodecVisitor& v,std::string& s){return v.field("value",s);});
    return c.sequence("module_versions",x.module_versions,[](CodecVisitor& v,std::pair<StableId,StableId>& p){v.field("name",p.first); return v.field("version",p.second);});
}
// 世界元数据没有run_id/permit/epoch，恢复不会重新授予旧自动推进权限。
Status visit_envelope(CodecVisitor& c,CheckpointEnvelope& x) {
    std::string world(reinterpret_cast<const char*>(x.identity.world_id.data()),x.identity.world_id.size());
    c.field("world_id",world); if(world.size()!=16) return c.reject("InvalidCheckpointWorldId");
    if(c.reading()) std::copy(world.begin(),world.end(),x.identity.world_id.begin());
    c.field("revision",x.identity.revision); c.field("tick",x.identity.tick);
    if(x.identity.revision<0||x.identity.tick<0) return c.reject("InvalidCheckpointClock");
    c.object("catalog",[&](CodecVisitor& v){return visit_catalog(v,x.identity.catalog);});
    std::set<std::string> domains;
    c.sequence("domains",x.sections,[&](CodecVisitor& v,CheckpointSection& s){
        v.field("domain",s.domain); v.field("codec_version",s.codec_version);
        if(s.domain.empty()||s.codec_version<1||!domains.insert(s.domain).second) return v.reject("InvalidCheckpointDomain");
        std::string blob(s.payload.begin(),s.payload.end()); v.field("payload",blob);
        if(v.reading()&&v.status().ok) s.payload.assign(blob.begin(),blob.end());
        return v.status();
    });
    if(x.sections.size()>64) return c.reject("CheckpointDomainLimit");
    return c.status();
}
// catalog比较仍走同一visitor，避免再写一份目录相等字段列表。
bool same_catalog(const CatalogIdentity& a,const CatalogIdentity& b) {
    auto left=a,right=b; CodecVisitor::Buffer l,r; CodecVisitor lc(l),rc(r);
    return visit_catalog(lc,left).ok&&visit_catalog(rc,right).ok&&l==r;
}
// 限定失败只带诊断，不返回部分解码结果。
template<class T> Outcome<T> failure(const std::string& code) {return {false,{},{{code,"checkpoint"},ErrorKind::rule,{},{},{},{}}};}
}
Outcome<CodecVisitor::Buffer> encode_checkpoint(const CheckpointEnvelope& input) {
    try {
        if(input.container_version!=1)return failure<CodecVisitor::Buffer>("CheckpointContainerVersionMismatch");
        CheckpointEnvelope copy=input; CodecVisitor::Buffer payload; CodecVisitor writer(payload);
        const auto visited=visit_envelope(writer,copy); if(!visited.ok) return {false,{},visited.error};
        const auto complete=writer.finish(); if(!complete.ok) return {false,{},complete.error};
        std::string raw(payload.begin(),payload.end()); const auto digest=picosha2::hash256_hex_string(raw);
        CodecVisitor::Buffer result; const std::string prefix=magic;
        if(prefix.size()+payload.size()+digest.size()>CodecVisitor::max_bytes) return failure<CodecVisitor::Buffer>("CheckpointTooLarge");
        result.insert(result.end(),prefix.begin(),prefix.end()); result.insert(result.end(),payload.begin(),payload.end()); result.insert(result.end(),digest.begin(),digest.end());
        return {true,std::move(result),{}};
    } catch(const std::bad_alloc&) { return failure<CodecVisitor::Buffer>("CheckpointResourceLimit"); }
}
static Outcome<CheckpointEnvelope> read_checkpoint(const CodecVisitor::Buffer& bytes) {
    try {
        const std::string prefix=magic;
        if(bytes.size()>CodecVisitor::max_bytes||bytes.size()<prefix.size()+64||!std::equal(prefix.begin(),prefix.end(),bytes.begin())) return failure<CheckpointEnvelope>("InvalidCheckpointEnvelope");
        const std::string raw(bytes.begin()+prefix.size(),bytes.end()-64),digest(bytes.end()-64,bytes.end());
        if(picosha2::hash256_hex_string(raw)!=digest) return failure<CheckpointEnvelope>("CheckpointDigestMismatch");
        const CodecVisitor::Buffer payload(raw.begin(),raw.end()); CodecVisitor reader(payload); CheckpointEnvelope result;
        const auto visited=visit_envelope(reader,result); if(!visited.ok) return {false,{},visited.error};
        const auto complete=reader.finish(); if(!complete.ok) return {false,{},complete.error};
        return {true,std::move(result),{}};
    } catch(const std::bad_alloc&) { return failure<CheckpointEnvelope>("CheckpointResourceLimit"); }
}
// 原恢复保持目录先于域版本的错误顺序；选择探查没有可替代的缺省目录。
static Outcome<CheckpointEnvelope> checked_domains(Outcome<CheckpointEnvelope> result,const std::vector<CheckpointDomain>& expected) {
    if(!result.ok)return result;
    if(result.value.sections.size()!=expected.size())return failure<CheckpointEnvelope>("CheckpointDomainMismatch");
    for(std::size_t i=0;i<expected.size();++i)if(result.value.sections[i].domain!=expected[i].name||result.value.sections[i].codec_version!=expected[i].codec_version)return failure<CheckpointEnvelope>("CheckpointDomainMismatch");
    return result;
}
Outcome<CheckpointEnvelope> inspect_checkpoint(const CodecVisitor::Buffer& bytes,const std::vector<CheckpointDomain>& expected) {
    return checked_domains(read_checkpoint(bytes),expected);
}
// 只消除候选版本间重复摘要/解析；格式、摘要错误及未知域仍沿同一首错返回。
// 只在完整摘要/封套校验后调用选择器，并严格对比其唯一目录。
Outcome<CheckpointEnvelope> inspect_checkpoint_selected(const CodecVisitor::Buffer& bytes,const CheckpointDomainSelector& select) {
    auto result=read_checkpoint(bytes);if(!result.ok)return result;
    auto expected=select(result.value.identity);if(!expected.ok)return {false,{},expected.error};
    return checked_domains(std::move(result),expected.value);
}
Outcome<CheckpointEnvelope> inspect_checkpoint_variants(const CodecVisitor::Buffer& bytes,const std::vector<std::vector<CheckpointDomain>>& variants) {
    auto result=read_checkpoint(bytes);if(!result.ok)return result;
    for(const auto& expected:variants){
        if(result.value.sections.size()!=expected.size())continue;
        bool same=true;for(std::size_t i=0;i<expected.size();++i)if(result.value.sections[i].domain!=expected[i].name||result.value.sections[i].codec_version!=expected[i].codec_version){same=false;break;}
        if(same)return result;
    }
    return failure<CheckpointEnvelope>("CheckpointDomainMismatch");
}
Outcome<CheckpointEnvelope> decode_checkpoint(const CodecVisitor::Buffer& bytes,const CatalogIdentity& catalog,const std::vector<CheckpointDomain>& expected) {
    try{
        auto result=read_checkpoint(bytes);if(!result.ok)return result;
        if(!same_catalog(result.value.identity.catalog,catalog))return failure<CheckpointEnvelope>("CheckpointCatalogMismatch");
        return checked_domains(std::move(result),expected);
    }catch(const std::bad_alloc&){return failure<CheckpointEnvelope>("CheckpointResourceLimit");}
}
} // 本模块不能发布世界；任何失败仍由调用者保全已提交世界。

// /2容器与旧/1实现隔离；typed producer只编码一次，同一份受限字节用于摘要和输出。
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
namespace {
constexpr char magic2[]="HM-RESIDENT-CHECKPOINT/2\n";
constexpr std::uint64_t prefix2=sizeof(magic2)-1+8+8+4;
// 使用原RuleError传播失败，不能把短写/预算故障转成部分成功。
struct StreamFailure { RuleError error; };
void require_job(CheckpointJobStatus value){if(!value.ok)throw checkpoint_job_detail::FailureException{std::move(value.error)};}
template<class T> void require_job_result(CheckpointJobResult<T>& result){if(!result.ok)throw checkpoint_job_detail::FailureException{std::move(result.error)};}
// The existing first-error owner is used before constructing an active-job
// diagnostic. Foreign Status producers still own their first construction.
void require_stream(Status value,CheckpointJobBudget* job=nullptr){if(!value.ok){if(job)throw checkpoint_job_detail::FailureException{job->capture_failure(std::move(value.error))};throw StreamFailure{std::move(value.error)};}}
void require_rule(bool okay,const char* code,CheckpointJobBudget* job=nullptr,ErrorKind kind=ErrorKind::rule,std::initializer_list<std::string_view> path={"checkpoint"}){if(!okay){if(job)throw checkpoint_job_detail::FailureException{job->capture_boundary_failure(code,path,kind)};throw StreamFailure{make_checkpoint_boundary_error(code,path,kind)};}}
void require_capacity(bool okay,const CapacityProfile& p,std::initializer_list<std::string_view> resource,std::uint64_t needed,CheckpointJobBudget* job=nullptr){if(!okay){if(job)throw checkpoint_job_detail::FailureException{job->capture_capacity_failure(resource,needed,p.profile_id,p.revision)};throw StreamFailure{make_checkpoint_capacity_error(resource,needed,p.profile_id,p.revision)};}}
void require_capacity(bool okay,const CapacityProfile& p,std::string_view resource,std::uint64_t needed,CheckpointJobBudget* job=nullptr){require_capacity(okay,p,{resource},needed,job);}
std::uint64_t add_checked(std::uint64_t a,std::uint64_t b,CheckpointJobBudget* job=nullptr){require_rule(a<=std::uint64_t(INT64_MAX)&&b<=std::uint64_t(INT64_MAX)-a,"CheckpointLengthOverflow",job);return a+b;}
template<class T,class Fn> Outcome<T> guarded_stream(Fn fn,CheckpointJobBudget* job=nullptr){try{return {true,fn(),{}};}catch(StreamFailure& e){if(job)throw checkpoint_job_detail::FailureException{job->capture_failure(std::move(e.error))};return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){if(job)throw checkpoint_job_detail::FailureException{job->capture_boundary_failure("CheckpointResourceLimit",{"checkpoint"},ErrorKind::internal)};return {false,{},{{"CheckpointResourceLimit","checkpoint"},ErrorKind::internal,{},{},{},{}}};}catch(const std::length_error&){if(job)throw checkpoint_job_detail::FailureException{job->capture_boundary_failure("CheckpointResourceLimit",{"checkpoint"},ErrorKind::internal)};return {false,{},{{"CheckpointResourceLimit","checkpoint"},ErrorKind::internal,{},{},{},{}}};}}
template<class T,class Fn> CheckpointJobResult<T> guarded_job(CheckpointJobBudget& job,Fn fn){try{return {true,fn(),{}};}catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(StreamFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}catch(const std::length_error&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};}}
// 目录必须是规范UTF8字节，拒NUL、过长编码、surrogate和越界码点。
bool valid_domain(const StableId& value){
    if(value.empty()||value.size()>256)return false;
    for(std::size_t i=0;i<value.size();){auto c=static_cast<unsigned char>(value[i++]);if(!c)return false;if(c<128)continue;
        unsigned n=0;std::uint32_t cp=0,min=0;if(c>=0xc2&&c<=0xdf){n=1;cp=c&31;min=128;}else if(c>=0xe0&&c<=0xef){n=2;cp=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){n=3;cp=c&7;min=65536;}else return false;
        if(n>value.size()-i)return false;while(n--){auto b=static_cast<unsigned char>(value[i++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
    }return true;
}
bool domain_less(const StableId& a,const StableId& b){return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[](char x,char y){return static_cast<unsigned char>(x)<static_cast<unsigned char>(y);});}
// /2目录比较只读原字段；不再为比较重复编码完整header。
bool same_catalog2(const CatalogIdentity& a,const CatalogIdentity& b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.artifact_sha256==b.artifact_sha256&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions;}
// header只含既定身份，原catalog visitor顺序无改动。
Status visit_header2(CodecVisitor& c,CheckpointIdentity& x){
    // 写端只生成固定16字节身份；读端继续由同一typed字段核验真实长度。
    std::string world;
    if(!c.reading())world.assign(reinterpret_cast<const char*>(x.world_id.data()),x.world_id.size());
    c.field("world_id",world);
    if(c.status().ok&&world.size()!=16)return c.reject("InvalidCheckpointWorldId");if(c.reading()&&c.status().ok)std::copy(world.begin(),world.end(),x.world_id.begin());
    c.field("revision",x.revision);c.field("tick",x.tick);if(x.revision<0||x.tick<0)return c.reject("InvalidCheckpointClock");
    c.object("catalog",[&](CodecVisitor& v){return visit_catalog(v,x.catalog);});return c.status();
}
// 固定工作空间SHA摘要；可接下游chunk，摘要始终对应同一批实际字节。
class HashSink final:public CheckpointWriteSink {
public:
    explicit HashSink(CheckpointWriteSink* downstream=nullptr,CheckpointJobBudget* job=nullptr):downstream_(downstream),job_(job){}
    Status write(const std::uint8_t* bytes,std::size_t size) override {
        if(size>std::uint64_t(INT64_MAX)-count){if(job_)require_rule(false,"CheckpointLengthOverflow",job_);return failure<Done>("CheckpointLengthOverflow");}
        if(downstream_){auto status=downstream_->write(bytes,size);if(!status.ok)return status;}
        // 上游字段可能巨大；PicoSHA2::process先copy整range，必须在这里按SHA块切片。
        auto remaining=size;auto* cursor=bytes;while(remaining){const auto n=std::min<std::size_t>(remaining,64);hash_.process(cursor,cursor+n);cursor+=n;remaining-=n;}
        count+=size;return {true,{},{}};
    }
    CheckpointDigest digest()const{auto copy=hash_;copy.finish();CheckpointDigest result{};copy.get_hash_bytes(result.begin(),result.end());return result;}
    std::uint64_t count=0;
private:CheckpointWriteSink* downstream_;CheckpointJobBudget* job_;picosha2::hash256_one_by_one hash_;
};
// 唯一输出缓冲按受信chunk一次分配；外部sink从不收到超过chunk的借用片段。
class ChunkSink final:public CheckpointWriteSink {
public:
    ChunkSink(CheckpointWriteSink& target,std::size_t size,CheckpointJobReservation* active=nullptr):target_(target),buffer_(size),active_(active){}
    Status write(const std::uint8_t* bytes,std::size_t size)override{while(size){const auto n=std::min(size,buffer_.size()-used_);std::copy_n(bytes,n,buffer_.data()+used_);used_+=n;bytes+=n;size-=n;if(used_==buffer_.size()){auto s=flush();if(!s.ok)return s;}}return {true,{},{}};}
    Status flush(){if(active_){auto live=active_->grow(0);if(!live.ok)throw checkpoint_job_detail::FailureException{std::move(live.error)};}if(!used_)return {true,{},{}};auto s=target_.write(buffer_.data(),used_);if(s.ok)used_=0;return s;}
private:CheckpointWriteSink& target_;CodecVisitor::Buffer buffer_;std::size_t used_=0;CheckpointJobReservation* active_;
};
void write_number(CheckpointWriteSink& out,std::uint64_t value,unsigned width,CheckpointJobBudget* job=nullptr){std::uint8_t bytes[8];for(unsigned i=0;i<width;++i)bytes[i]=static_cast<std::uint8_t>(value>>(8*i));require_stream(out.write(bytes,width),job);}
// expected是core授予的精确合法集合；producer本身不能自报新的域版本。
std::vector<const CheckpointDomain*> ordered_domains(const std::vector<CheckpointDomain>& expected,const CapacityProfile& p,CheckpointJobBudget* job=nullptr){
    require_rule(!expected.empty()&&expected.size()<=256,"CheckpointDomainLimit",job);
    // The original comparison/check order uses a borrowed sorted view. Original
    // expected or selector output remains alive until every pointer is discarded.
    std::vector<const CheckpointDomain*> result;result.reserve(expected.size());for(const auto& d:expected)result.push_back(&d);
    std::sort(result.begin(),result.end(),[](const auto* a,const auto* b){return domain_less(a->name,b->name);});
    for(std::size_t i=0;i<result.size();++i){const auto& d=*result[i];require_rule(valid_domain(d.name)&&d.codec_version>0&&std::uint64_t(d.codec_version)<=UINT32_MAX,"InvalidCheckpointDomain",job);require_rule(!i||result[i-1]->name!=d.name,"CheckpointDomainMismatch",job);require_rule(p.max_section_bytes_by_registered_name.count(d.name)!=0,"UnregisteredCheckpointDomain",job);}return result;
}
// 只对常量源执行写visitor；这些field/optional/sequence在写模式不赋值源对象。
void write_header2(CheckpointWriteSink& sink,const CheckpointIdentity& identity,const CapacityProfile& p,CheckpointJobBudget* job=nullptr){CodecVisitor c(sink,p,"$header",p.max_header_bytes,job);require_stream(visit_header2(c,const_cast<CheckpointIdentity&>(identity)),job);require_stream(c.finish(),job);}
// 本Bytes适配器仅是受限sink；整档输出长度先核，job token只保归属与生命周期。
class BytesSink final:public CheckpointWriteSink {
public:
    BytesSink(const CapacityProfile& p,std::uint64_t limit,CheckpointJobBudget* job=nullptr):profile_(p),limit_(limit),job_(job){}
    std::uint64_t limit()const{return limit_;}
    void prepare(std::uint64_t total){
        require_capacity(total<=limit_&&total<=std::numeric_limits<std::size_t>::max(),profile_,"bytes_transport",total,job_);
        if(job_){auto held=job_->reserve(CheckpointJobUse::encode,total);require_job_result(held);reservation=std::move(held.value);}
        bytes.reserve(static_cast<std::size_t>(total));prepared_=total;
    }
    Status write(const std::uint8_t* data,std::size_t size)override{
        if(job_)require_job(reservation.grow(0));
        require_rule(bytes.size()<=prepared_&&size<=prepared_-bytes.size(),"CheckpointLengthMismatch",job_);
        require_capacity(bytes.size()<=limit_&&size<=limit_-bytes.size(),profile_,"bytes_transport",size>UINT64_MAX-bytes.size()?UINT64_MAX:bytes.size()+size,job_);
        bytes.insert(bytes.end(),data,data+size);return {true,{},{}};
    }
    CheckpointJobReservation reservation; // bytes先析构；token不能早于其拥有值。
    CodecVisitor::Buffer bytes;
private:const CapacityProfile& profile_;std::uint64_t limit_,prepared_=0;CheckpointJobBudget* job_;
};
// header和各段只向此私有缓冲编码一次；每次增长前连同目录/尾摘要核整档粗限额。
// 缓冲尚未完整时不触外部sink，producer失败不会发布部分成功回执。
class TypedBytesSink final:public CheckpointWriteSink {
public:
    TypedBytesSink(const CapacityProfile& profile,std::uint64_t overhead,std::uint64_t transport_limit,CheckpointJobBudget* job,CheckpointJobReservation* active)
        :profile_(profile),overhead_(overhead),transport_limit_(transport_limit),job_(job),active_(active){check_total(overhead_);}
    Status write(const std::uint8_t* data,std::size_t size)override{
        if(active_)require_job(active_->grow(0));
        if(!size)return {true,{},{}};
        const auto needed=add_checked(bytes.size(),size,job_);
        check_total(add_checked(overhead_,needed,job_));
        require_rule(needed<=bytes.max_size(),"CheckpointLengthOverflow",job_);
        if(needed>bytes.capacity()){
            const auto limit=std::min(std::min({profile_.max_checkpoint_bytes,profile_.max_staging_disk_bytes,transport_limit_})-overhead_,std::uint64_t(bytes.max_size()));
            const auto capacity=std::uint64_t(bytes.capacity());
            const auto grown=capacity>limit/2?limit:capacity*2;
            bytes.reserve(static_cast<std::size_t>(std::max(needed,grown)));
        }
        bytes.insert(bytes.end(),data,data+size);return {true,{},{}};
    }
    CodecVisitor::Buffer bytes;
private:
    void check_total(std::uint64_t total){
        require_capacity(total<=profile_.max_checkpoint_bytes,profile_,"checkpoint_bytes",total,job_);
        require_capacity(total<=profile_.max_staging_disk_bytes,profile_,"staging_disk_bytes",total,job_);
        require_capacity(total<=transport_limit_,profile_,"bytes_transport",total,job_);
    }
    const CapacityProfile& profile_;std::uint64_t overhead_,transport_limit_;CheckpointJobBudget* job_;CheckpointJobReservation* active_;
};
// 借用已有不可变小档，不另复制整输入；完整stream source由介质私有staging持有。
class BytesSource final:public CheckpointReadSource {
public:
    explicit BytesSource(const CodecVisitor::Buffer& b,CheckpointJobBudget* job=nullptr):bytes_(b),job_(job){}
    std::uint64_t size()const override{return bytes_.size();}
    Status read(std::uint64_t offset,std::uint8_t* out,std::size_t count)override{if(offset>bytes_.size()||count>bytes_.size()-offset){if(job_)require_rule(false,"TruncatedCheckpoint",job_);return failure<Done>("TruncatedCheckpoint");}std::copy_n(bytes_.data()+static_cast<std::size_t>(offset),count,out);return {true,{},{}};}
private:const CodecVisitor::Buffer& bytes_;CheckpointJobBudget* job_;
};
// 解析器所有范围先用减法核验；目录小标量采用栈字节，不读未核长度的大块。
class StreamReader {
public:
    explicit StreamReader(CheckpointReadSource& source,CheckpointJobBudget* job=nullptr):actual(nullptr,job),source_(source),job_(job){}
    void read(std::uint8_t* out,std::size_t count){require_rule(at<=source_.size()&&count<=source_.size()-at,"TruncatedCheckpoint",job_);require_stream(source_.read(at,out,count),job_);require_stream(actual.write(out,count),job_);at+=count;}
    std::uint64_t number(unsigned width){std::uint8_t bytes[8]{};read(bytes,width);std::uint64_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint64_t(bytes[i])<<(8*i);return value;}
    std::uint64_t at=0;HashSink actual;
private:CheckpointReadSource& source_;CheckpointJobBudget* job_;
};
CheckpointDigest hash_range(CheckpointReadSource& source,std::uint64_t offset,std::uint64_t bytes,CodecVisitor::Buffer& chunk,HashSink* whole=nullptr,CheckpointJobBudget* job=nullptr){HashSink hash(nullptr,job);while(bytes){const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(bytes,chunk.size()));require_stream(source.read(offset,chunk.data(),count),job);require_stream(hash.write(chunk.data(),count),job);if(whole)require_stream(whole->write(chunk.data(),count),job);offset+=count;bytes-=count;}return hash.digest();}
}
// 单次typed编码完成后才写介质；目录摘要、长度及整档摘要全部来自同一不可变缓冲。
static Outcome<CheckpointStreamReceipt> encode_checkpoint_v2_impl(const CheckpointIdentity& identity,const std::vector<CheckpointSectionProducer>& producers,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointWriteSink& target,CheckpointJobBudget* job){return guarded_stream<CheckpointStreamReceipt>([&]{
    checkpoint_phase_timing::Scope prepare_time(checkpoint_phase_timing::Stage::binary_prepare);
    if(job)require_job(job->check_profile(profile));
    require_rule(valid_capacity_profile_shape(profile),"InvalidCapacityProfile",job,ErrorKind::internal,{"checkpoint/capacity"});require_rule(!expected.empty()&&expected.size()<=256&&producers.size()==expected.size(),"CheckpointDomainMismatch",job);
    for(const auto& d:expected)require_rule(valid_domain(d.name),"InvalidCheckpointDomain",job);
    CheckpointJobReservation temporary;if(job){auto held=job->reserve(CheckpointJobUse::encode,0);require_job_result(held);temporary=std::move(held.value);}
    const auto domains=ordered_domains(expected,profile,job);
    std::vector<const CheckpointSectionProducer*> ordered;ordered.reserve(producers.size());for(const auto& p:producers)ordered.push_back(&p);
    std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return domain_less(a->domain,b->domain);});
    for(std::size_t i=0;i<domains.size();++i){require_rule(ordered[i]->domain==domains[i]->name&&ordered[i]->codec_version==domains[i]->codec_version&&bool(ordered[i]->snapshot),"CheckpointDomainMismatch",job);require_rule(ordered[i]->snapshot==ordered.front()->snapshot,"CheckpointSnapshotLeaseMismatch",job);}
    const auto& owned_identity=ordered.front()->snapshot->identity();
    require_rule(identity.world_id==owned_identity.world_id&&identity.revision==owned_identity.revision&&identity.tick==owned_identity.tick&&same_catalog2(identity.catalog,owned_identity.catalog),"CheckpointSnapshotIdentityMismatch",job);
    std::uint64_t directory_bytes=0;for(const auto* d:domains)directory_bytes=add_checked(directory_bytes,2+d->name.size()+4+8+8+32,job);
    const auto before_typed=add_checked(prefix2,directory_bytes,job),overhead=add_checked(before_typed,32,job);
    auto* bytes_target=dynamic_cast<BytesSink*>(&target);
    TypedBytesSink typed(profile,overhead,bytes_target?bytes_target->limit():profile.max_checkpoint_bytes,job,job?&temporary:nullptr);
    std::vector<CheckpointSectionIndex> index;index.reserve(ordered.size());
    prepare_time.stop();
    // 沿用既有计时槽；此段现在是唯一序列化及逐段摘要，不是预演第二遍visitor。
    checkpoint_phase_timing::Scope serialize_time(checkpoint_phase_timing::Stage::binary_measure);
    // 只量原 header 编码，不重新计算身份或字节。
    catalog_section_observation::Scope header_observation(catalog_section_observation::Kind::header_encode, 10001, "checkpoint_header", "$header", &identity);
    write_header2(typed,identity,profile,job);const auto header_size=typed.bytes.size();
    header_observation.stop(header_size, true);
    for(const auto* producer:ordered){
        // 每个 section 原 producer 只执行一次；含其原派发与严格校验。
        catalog_section_observation::Scope section_observation(catalog_section_observation::Kind::section_visit, 10002, "checkpoint_section", producer->domain, producer->snapshot.get(), producer->codec_version);
        const auto begin=typed.bytes.size();
        CodecVisitor visitor(typed,profile,producer->domain,profile.max_section_bytes_by_registered_name.at(producer->domain),job);
        require_stream(producer->snapshot->visit_section(producer->domain,visitor),job);require_stream(visitor.finish(),job);
        if(job)require_job(temporary.grow(0));
        const auto size=typed.bytes.size()-begin;require_rule(size>0,"EmptyCheckpointSection",job);
        section_observation.stop(size, true);
        // 摘要仍用原缓冲与原 HashSink；和 visit 是兄弟段，不重复累计。
        catalog_section_observation::Scope hash_observation(catalog_section_observation::Kind::section_hash, 10003, "checkpoint_section", producer->domain, producer->snapshot.get(), producer->codec_version);
        HashSink hash(nullptr,job);require_stream(hash.write(typed.bytes.data()+begin,size),job);
        index.push_back({producer->domain,producer->codec_version,add_checked(before_typed,begin,job),size,hash.digest()});
        hash_observation.stop(size, true);
        world_stage_observation::observe_bytes({world_stage_observation::ByteMetric::section,producer->domain,producer->codec_version,size,index.back().offset,header_size,directory_bytes});
    }
    const auto total=add_checked(overhead,typed.bytes.size(),job);
    serialize_time.stop();
    checkpoint_phase_timing::Scope write_time(checkpoint_phase_timing::Stage::binary_write);
    // 原封套输出同时含目录、整档 hash 和复制；不伪称仅 hash 成本。
    catalog_section_observation::Scope envelope_observation(catalog_section_observation::Kind::envelope_emit, 10004, "checkpoint_envelope", "$complete", &identity);
    if(bytes_target)bytes_target->prepare(total);
    ChunkSink chunk(target,static_cast<std::size_t>(std::min(profile.stream_chunk_bytes,total)),job?&temporary:nullptr);HashSink whole(&chunk,job);
    require_stream(whole.write(reinterpret_cast<const std::uint8_t*>(magic2),sizeof(magic2)-1),job);write_number(whole,total,8,job);write_number(whole,header_size,8,job);write_number(whole,index.size(),4,job);
    require_stream(whole.write(typed.bytes.data(),header_size),job);
    for(const auto& s:index){write_number(whole,s.domain.size(),2,job);require_stream(whole.write(reinterpret_cast<const std::uint8_t*>(s.domain.data()),s.domain.size()),job);write_number(whole,s.codec_version,4,job);write_number(whole,s.offset,8,job);write_number(whole,s.bytes,8,job);require_stream(whole.write(s.sha256.data(),32),job);}
    require_stream(whole.write(typed.bytes.data()+header_size,typed.bytes.size()-header_size),job);
    require_rule(whole.count==total-32,"CheckpointLengthMismatch",job);const auto digest=whole.digest();require_stream(whole.write(digest.data(),digest.size()),job);const auto full_digest=whole.digest();require_stream(chunk.flush(),job);if(job)require_job(temporary.grow(0));world_stage_observation::observe_bytes({world_stage_observation::ByteMetric::complete,{},0,total,0,header_size,directory_bytes});envelope_observation.stop(total, true);return CheckpointStreamReceipt{total,digest,full_digest};
},job);}
Outcome<CheckpointStreamReceipt> encode_checkpoint_v2(const CheckpointIdentity& identity,const std::vector<CheckpointSectionProducer>& producers,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointWriteSink& sink){return encode_checkpoint_v2_impl(identity,producers,expected,profile,sink,nullptr);}
CheckpointJobResult<CheckpointStreamReceipt> encode_checkpoint_v2(const CheckpointIdentity& identity,const std::vector<CheckpointSectionProducer>& producers,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointWriteSink& sink,CheckpointJobBudget& job){return guarded_job<CheckpointStreamReceipt>(job,[&]{auto result=encode_checkpoint_v2_impl(identity,producers,expected,profile,sink,&job);if(!result.ok)throw StreamFailure{std::move(result.error)};return result.value;});}
CheckpointJobResult<CheckpointJobBytes> encode_checkpoint_v2_bytes(const CheckpointIdentity& identity,const std::vector<CheckpointSectionProducer>& producers,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,std::uint64_t limit,CheckpointJobBudget& job){
    return guarded_job<CheckpointJobBytes>(job,[&]{require_job(job.check_profile(profile));require_capacity(limit&&limit<=profile.max_checkpoint_bytes&&limit<=std::numeric_limits<std::size_t>::max(),profile,"bytes_transport",limit,&job);BytesSink sink(profile,limit,&job);auto result=encode_checkpoint_v2_impl(identity,producers,expected,profile,sink,&job);if(!result.ok)throw StreamFailure{std::move(result.error)};auto owned=CheckpointJobBytes::adopt_reserved(std::move(sink.bytes),std::move(sink.reservation));require_job_result(owned);return std::move(owned.value);});
}
Outcome<CodecVisitor::Buffer> encode_checkpoint_v2_bytes(const CheckpointIdentity& identity,const std::vector<CheckpointSectionProducer>& producers,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,std::uint64_t transport_limit){
    return guarded_stream<CodecVisitor::Buffer>([&]{require_stream(validate_capacity_profile(profile));require_capacity(transport_limit&&transport_limit<=profile.max_checkpoint_bytes&&transport_limit<=std::numeric_limits<std::size_t>::max(),profile,"bytes_transport",transport_limit);BytesSink sink(profile,transport_limit);auto result=encode_checkpoint_v2(identity,producers,expected,profile,sink);if(!result.ok)throw StreamFailure{std::move(result.error)};return std::move(sink.bytes);});
}
static Outcome<CheckpointStreamIndex> inspect_checkpoint_v2_stream_impl(CheckpointReadSource& source,const CheckpointDomainSelector& selector,const CapacityProfile& profile,CheckpointJobBudget* job=nullptr,CheckpointJobReservation* output=nullptr,const std::vector<CheckpointDomain>* trusted_expected=nullptr){return guarded_stream<CheckpointStreamIndex>([&]{
    if(job)require_job(job->check_profile(profile));
    require_rule(valid_capacity_profile_shape(profile),"InvalidCapacityProfile",job,ErrorKind::internal,{"checkpoint/capacity"});const auto size=source.size();require_capacity(size<=profile.max_checkpoint_bytes,profile,"checkpoint_bytes",size,job);require_rule(size>=prefix2+32,"InvalidCheckpointEnvelope",job);
    CheckpointJobReservation temporary;if(job){auto held=job->reserve(CheckpointJobUse::scratch,0);require_job_result(held);temporary=std::move(held.value);}
    StreamReader reader(source,job);std::array<std::uint8_t,sizeof(magic2)-1> magic_bytes{};reader.read(magic_bytes.data(),magic_bytes.size());require_rule(std::equal(magic_bytes.begin(),magic_bytes.end(),reinterpret_cast<const std::uint8_t*>(magic2)),"InvalidCheckpointEnvelope",job);
    const auto total=reader.number(8),header_size=reader.number(8),count=reader.number(4);require_rule(total==size,"CheckpointLengthMismatch",job);
    CodecVisitor::Buffer chunk(static_cast<std::size_t>(std::min(profile.stream_chunk_bytes,size)));CheckpointDigest stored{};require_stream(source.read(size-32,stored.data(),32),job);require_rule(hash_range(source,0,size-32,chunk,nullptr,job)==stored,"CheckpointDigestMismatch",job);
    require_rule(count>0&&count<=256,"CheckpointDomainLimit",job);if(job)require_rule(trusted_expected&&!trusted_expected->empty(),"CheckpointDomainMismatch",job);require_capacity(header_size<=profile.max_header_bytes&&header_size<=std::numeric_limits<std::size_t>::max(),profile,"header_bytes",header_size,job);
    require_rule(header_size<=size-32-reader.at,"TruncatedCheckpoint",job);require_rule(count<=(size-32-reader.at-header_size)/55,"TruncatedCheckpoint",job);
    CheckpointJobReservation metadata_held;
    if(job){auto held=job->reserve(CheckpointJobUse::candidate_scratch,0);require_job_result(held);metadata_held=std::move(held.value);}
    CodecDecodeBudget legacy_budget(profile);std::optional<CodecDecodeBudget> job_budget;if(job)job_budget.emplace(profile,*job,CheckpointJobUse::candidate_scratch);auto& budget=job?*job_budget:legacy_budget;
    CodecVisitor::Buffer header(static_cast<std::size_t>(header_size));reader.read(header.data(),header.size());CheckpointStreamIndex result;CodecVisitor visit(header,profile,"$header",budget);require_stream(visit_header2(visit,result.identity),job);require_stream(visit.finish(),job);result.sections.reserve(static_cast<std::size_t>(count));
    for(std::uint64_t i=0;i<count;++i){const auto length=reader.number(2);require_rule(length&&length<=256,"InvalidCheckpointDomain",job);std::array<std::uint8_t,256> name{};reader.read(name.data(),static_cast<std::size_t>(length));CheckpointSectionIndex s;std::string domain(reinterpret_cast<const char*>(name.data()),static_cast<std::size_t>(length));s.domain.swap(domain);require_rule(valid_domain(s.domain),"InvalidCheckpointDomain",job);require_rule(result.sections.empty()||domain_less(result.sections.back().domain,s.domain),"CheckpointDirectoryOrderMismatch",job);s.codec_version=reader.number(4);require_rule(s.codec_version>0,"InvalidCheckpointDomain",job);s.offset=reader.number(8);s.bytes=reader.number(8);reader.read(s.sha256.data(),s.sha256.size());require_rule(s.offset<=size-32&&s.bytes<=size-32-s.offset&&s.bytes>0,"CheckpointSectionRangeMismatch",job);const auto limit=profile.max_section_bytes_by_registered_name.find(s.domain);require_rule(limit!=profile.max_section_bytes_by_registered_name.end(),"UnregisteredCheckpointDomain",job);require_capacity(s.bytes<=limit->second,profile,{"section_bytes/",s.domain},s.bytes,job);result.sections.push_back(std::move(s));}
    auto next=reader.at;for(const auto& s:result.sections){require_rule(s.offset==next,"CheckpointSectionRangeMismatch",job);next=add_checked(next,s.bytes,job);}require_rule(next==size-32,"CheckpointSectionRangeMismatch",job);
    for(const auto& s:result.sections)require_rule(hash_range(source,s.offset,s.bytes,chunk,&reader.actual,job)==s.sha256,"CheckpointSectionDigestMismatch",job);
    require_rule(reader.actual.count==size-32&&reader.actual.digest()==stored,"CheckpointSourceChanged",job);CheckpointDigest final_digest{};require_stream(source.read(size-32,final_digest.data(),32),job);require_rule(source.size()==size&&final_digest==stored,"CheckpointSourceChanged",job);
    Outcome<std::vector<CheckpointDomain>> selected;const auto* selected_domains=trusted_expected;
    if(!selected_domains){require_rule(bool(selector),"CheckpointDomainMismatch",job);selected=selector(result.identity);if(!selected.ok)throw StreamFailure{std::move(selected.error)};selected_domains=&selected.value;}
    // 先从已校验目录报告真实格式版本；拒绝后才允许建立期望顺序表。
    for(const auto& wanted:*selected_domains)for(const auto& observed:result.sections){
      if(wanted.name==observed.domain&&wanted.codec_version!=observed.codec_version&&
         (wanted.name=="growth"||wanted.name=="growth_permanent_contributions"||
          wanted.name=="character_condition_extensions"||wanted.name=="population_military"||wanted.name=="trade-grant")){
        RuleError error;error.original={"CheckpointFormatVersionUnsupported",wanted.name};error.kind=ErrorKind::unsupported_capability;
        error.phase="checkpoint_format";error.domain=wanted.name;
        error.reason="observed="+std::to_string(observed.codec_version)+", expected="+std::to_string(wanted.codec_version)+"; no migration";
        require_stream({false,{},std::move(error)},job);
      }
    }
    require_rule(count==selected_domains->size(),"CheckpointDomainMismatch",job);
    const auto expected=ordered_domains(*selected_domains,profile,job);require_rule(expected.size()==result.sections.size(),"CheckpointDomainMismatch",job);for(std::size_t i=0;i<expected.size();++i)require_rule(expected[i]->name==result.sections[i].domain&&expected[i]->codec_version==result.sections[i].codec_version,"CheckpointDomainMismatch",job);require_stream(reader.actual.write(final_digest.data(),final_digest.size()),job);result.receipt={size,stored,reader.actual.digest()};if(job){require_job(temporary.grow(0));auto identity_held=budget.take_job_reservation();require_job(metadata_held.merge(std::move(identity_held)));*output=std::move(metadata_held);}return result;
},job);}
// stream与Bytes入口共享真实范围/摘要校验；结果token只保同job归属。
Outcome<CheckpointStreamIndex> inspect_checkpoint_v2_stream(CheckpointReadSource& source,const CheckpointDomainSelector& selector,const CapacityProfile& profile){return inspect_checkpoint_v2_stream_impl(source,selector,profile);}
Outcome<CheckpointEnvelope> inspect_checkpoint_v2_selected(const CodecVisitor::Buffer& bytes,const CheckpointDomainSelector& selector,const CapacityProfile& profile){return guarded_stream<CheckpointEnvelope>([&]{
    BytesSource source(bytes);auto inspected=inspect_checkpoint_v2_stream_impl(source,selector,profile);if(!inspected.ok)throw StreamFailure{std::move(inspected.error)};
    // 兼容口保留原Bytes和段副本；所有长度已由整档范围校验，不再推算内存总账。
    CheckpointEnvelope result;result.identity=std::move(inspected.value.identity);result.container_version=2;result.sections.reserve(inspected.value.sections.size());for(auto& s:inspected.value.sections){CheckpointSection section;section.domain=std::move(s.domain);section.codec_version=s.codec_version;require_rule(s.bytes<=std::numeric_limits<std::size_t>::max(),"CheckpointLengthOverflow");section.payload.resize(static_cast<std::size_t>(s.bytes));require_stream(source.read(s.offset,section.payload.data(),section.payload.size()));HashSink copied;require_stream(copied.write(section.payload.data(),section.payload.size()));require_rule(copied.digest()==s.sha256,"CheckpointSourceChanged");result.sections.push_back(std::move(section));}
    HashSink full;require_stream(full.write(bytes.data(),bytes.size()));require_rule(full.digest()==inspected.value.receipt.full_checkpoint_sha256,"CheckpointSourceChanged");return result;
});}
Outcome<CheckpointEnvelope> decode_checkpoint_v2_bytes(const CodecVisitor::Buffer& bytes,const CatalogIdentity& catalog,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile){
    return inspect_checkpoint_v2_selected(bytes,[&](const CheckpointIdentity& identity)->Outcome<std::vector<CheckpointDomain>>{if(!same_catalog2(identity.catalog,catalog))return failure<std::vector<CheckpointDomain>>("CheckpointCatalogMismatch");return {true,expected,{}};},profile);
}
class CheckpointJobResultAccess {
public:
    static CheckpointJobStreamIndex index(CheckpointStreamIndex&& value,CheckpointJobReservation&& held){CheckpointJobStreamIndex result;result.reservation_=std::move(held);result.value_=std::move(value);return result;}
    static CheckpointJobEnvelope envelope(CheckpointEnvelope&& value,CheckpointJobReservation&& held){CheckpointJobEnvelope result;result.reservation_=std::move(held);result.value_=std::move(value);return result;}
};
CheckpointJobStreamIndex& CheckpointJobStreamIndex::operator=(CheckpointJobStreamIndex&& other)noexcept{if(this!=&other){value_.~CheckpointStreamIndex();reservation_=std::move(other.reservation_);new(&value_)CheckpointStreamIndex(std::move(other.value_));}return *this;}
CheckpointJobEnvelope& CheckpointJobEnvelope::operator=(CheckpointJobEnvelope&& other)noexcept{if(this!=&other){value_.~CheckpointEnvelope();reservation_=std::move(other.reservation_);new(&value_)CheckpointEnvelope(std::move(other.value_));}return *this;}
namespace {
void check_job_domains(const std::vector<CheckpointDomain>& expected,CheckpointJobBudget& job){require_rule(!expected.empty()&&expected.size()<=256,"CheckpointDomainMismatch",&job);for(const auto& d:expected)require_rule(valid_domain(d.name),"InvalidCheckpointDomain",&job);}
}
CheckpointJobResult<CheckpointJobStreamIndex> inspect_checkpoint_v2_stream(CheckpointReadSource& source,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointJobBudget& job){return guarded_job<CheckpointJobStreamIndex>(job,[&]{
    require_job(job.check_profile(profile));check_job_domains(expected,job);CheckpointJobReservation held;
    auto inspected=inspect_checkpoint_v2_stream_impl(source,{},profile,&job,&held,&expected);
    if(!inspected.ok)throw StreamFailure{std::move(inspected.error)};return CheckpointJobResultAccess::index(std::move(inspected.value),std::move(held));
});}
CheckpointJobResult<CheckpointJobEnvelope> inspect_checkpoint_v2_selected(const CheckpointJobBytes& input,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointJobBudget& job){return guarded_job<CheckpointJobEnvelope>(job,[&]{
    require_job(job.check_profile(profile));require_rule(input.belongs_to(job),"CheckpointJobReservationMismatch",&job,ErrorKind::internal,{"checkpoint/job"});require_job(input.add_constraints(CheckpointJobUse::scratch));check_job_domains(expected,job);
    BytesSource source(input.bytes(),&job);CheckpointJobReservation held;
    auto inspected=inspect_checkpoint_v2_stream_impl(source,{},profile,&job,&held,&expected);if(!inspected.ok)throw StreamFailure{std::move(inspected.error)};
    CheckpointEnvelope result;result.identity=std::move(inspected.value.identity);result.container_version=2;result.sections.reserve(inspected.value.sections.size());
    for(auto& section:inspected.value.sections){CheckpointSection out;out.domain=std::move(section.domain);out.codec_version=section.codec_version;require_rule(section.bytes<=std::numeric_limits<std::size_t>::max(),"CheckpointLengthOverflow",&job);out.payload.resize(static_cast<std::size_t>(section.bytes));require_stream(source.read(section.offset,out.payload.data(),out.payload.size()),&job);HashSink hash(nullptr,&job);require_stream(hash.write(out.payload.data(),out.payload.size()),&job);require_rule(hash.digest()==section.sha256,"CheckpointSourceChanged",&job);result.sections.push_back(std::move(out));}
    HashSink full(nullptr,&job);require_stream(full.write(input.bytes().data(),input.bytes().size()),&job);require_rule(full.digest()==inspected.value.receipt.full_checkpoint_sha256,"CheckpointSourceChanged",&job);require_job(held.grow(0));return CheckpointJobResultAccess::envelope(std::move(result),std::move(held));
});}
CheckpointJobResult<CheckpointJobEnvelope> decode_checkpoint_v2_bytes(const CheckpointJobBytes& input,const CatalogIdentity& catalog,const std::vector<CheckpointDomain>& expected,const CapacityProfile& profile,CheckpointJobBudget& job){return guarded_job<CheckpointJobEnvelope>(job,[&]{auto result=inspect_checkpoint_v2_selected(input,expected,profile,job);require_job_result(result);require_rule(same_catalog2(result.value.value().identity.catalog,catalog),"CheckpointCatalogMismatch",&job);return std::move(result.value);});}

}
