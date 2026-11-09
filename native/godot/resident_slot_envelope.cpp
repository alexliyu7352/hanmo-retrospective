// 三载荷固定目录的流式编码/读取；错误只撤本job候选，任何摘要都不授准入权限。
#include "resident_slot_envelope.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <picosha2.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace hanmo::resident::storage {
namespace {
constexpr char magic[]="HM-SLOT-ENVELOPE";
constexpr std::uint64_t descriptor_bytes=52,minimum_header=16+2+2+8+2+1+8+8+1+3*descriptor_bytes;
constexpr std::uint64_t hash_scratch=1024;
using Code=CheckpointJobFailureCode;
struct WireFailure {RuleError error;};
// 已有job首错沿move-only租约穿过此叶，legacy语法错误只在同一预留窗口捕获一次。
void require(bool yes,const char* code){if(!yes)throw WireFailure{{{code,"slot/envelope"},ErrorKind::rule,{},{},{},{}}};}
void status(Status s){if(!s.ok)throw WireFailure{std::move(s.error)};}
void status(CheckpointJobStatus s){if(!s.ok)throw checkpoint_job_detail::FailureException{std::move(s.error)};}
template<class T>T take(CheckpointJobResult<T> r){if(!r.ok)throw checkpoint_job_detail::FailureException{std::move(r.error)};return std::move(r.value);}
template<class T,class F>CheckpointJobResult<T> guard(CheckpointJobBudget& job,F f){try{return {true,f(),{}};}catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(WireFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(Code::allocation_failed)};}catch(const std::length_error&){return {false,{},job.capture_fixed_failure(Code::size_overflow)};}}
std::uint64_t plus(std::uint64_t a,std::uint64_t b){require(a<=std::uint64_t(INT64_MAX)&&b<=std::uint64_t(INT64_MAX)-a,"SlotLengthOverflow");return a+b;}
bool zero(const CheckpointDigest& d){return std::all_of(d.begin(),d.end(),[](auto b){return b==0;});}
// 本机槽ID来自原有限allowlist；其ASCII子集也满足String256的严格UTF-8规范。
void slot_id(const StableId& id){require(ResidentSlotMedia::valid_slot(id)&&id.size()<=256,"InvalidSlotId");}
class HashSink final:public CheckpointWriteSink {
public:
    explicit HashSink(CheckpointWriteSink* downstream=nullptr):downstream_(downstream){}
    Status write(const std::uint8_t* bytes,std::size_t n)override{
        if(n>std::uint64_t(INT64_MAX)-count)return {false,{},{{"SlotLengthOverflow","slot"},ErrorKind::internal,{},{},{},{}}};
        for(std::size_t at=0;at<n;){const auto k=std::min<std::size_t>(64,n-at);hash_.process(bytes+at,bytes+at+k);at+=k;}
        count+=n;return downstream_?downstream_->write(bytes,n):Status{true,{},{}};
    }
    CheckpointDigest digest()const{auto copy=hash_;copy.finish();CheckpointDigest result{};copy.get_hash_bytes(result.begin(),result.end());return result;}
    std::uint64_t count=0;
private:picosha2::hash256_one_by_one hash_;CheckpointWriteSink* downstream_;
};
// 对所有下游写入实施同一受信chunk上界，header/trailer也不例外。
class BoundedSink final:public CheckpointWriteSink {
public:
    BoundedSink(CheckpointWriteSink& sink,std::uint64_t chunk):sink_(sink),chunk_(chunk){}
    Status write(const std::uint8_t* data,std::size_t size)override{
        while(size){const auto n=std::size_t(std::min<std::uint64_t>(size,chunk_));
            if(!n)return {false,{},{{"InvalidStreamChunk","slot"},ErrorKind::internal,{},{},{},{}}};
            auto result=sink_.write(data,n);if(!result.ok)return result;data+=n;size-=n;}return {true,{},{}};
    }
private:CheckpointWriteSink& sink_;std::uint64_t chunk_;
};
// 写入固定header预留，绝不让vector几何扩容逃离原job。
class HeaderWriter {
public:
    explicit HeaderWriter(CheckpointJobBytes& b):bytes(b){}
    void raw(const std::uint8_t* p,std::size_t n){status(bytes.write(at,p,n));at+=n;}
    void number(std::uint64_t value,unsigned width){std::uint8_t b[8]{};for(unsigned i=0;i<width;++i)b[i]=std::uint8_t(value>>(i*8));raw(b,width);}
    void text(const StableId& s){number(s.size(),2);raw(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
    CheckpointJobBytes& bytes;std::size_t at=0;
};
class Reader {
public:
    Reader(CheckpointReadSource& s,HashSink& h):source(s),actual(h){}
    void raw(std::uint8_t* p,std::size_t n){require(at<=source.size()&&n<=source.size()-at,"TruncatedSlotEnvelope");status(source.read(at,p,n));status(actual.write(p,n));at+=n;}
    std::uint64_t number(unsigned width){std::uint8_t b[8]{};raw(b,width);std::uint64_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint64_t(b[i])<<(8*i);require(width<8||n<=std::uint64_t(INT64_MAX),"SlotIntegerOverflow");return n;}
    CheckpointReadSource& source;HashSink& actual;std::uint64_t at=0;
};
// 全程chunk读取，计算本次真正读取的字节；取消在每个chunk之前检查。
CheckpointDigest hash_range(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,
    CheckpointJobBytes& chunk,CheckpointJobReservation& live,HashSink* actual=nullptr,std::array<std::uint8_t,69>* prefix=nullptr){
    std::size_t captured=0;
    HashSink hash;while(length){status(live.grow(0));const auto n=std::size_t(std::min<std::uint64_t>(length,chunk.bytes().size()));
        // 该唯一私有chunk从未被异步分享；source的输出只在其预留范围内。
        auto* p=const_cast<std::uint8_t*>(chunk.bytes().data());status(source.read(begin,p,n));if(prefix&&captured<prefix->size()){const auto keep=std::min<std::size_t>(n,prefix->size()-captured);std::copy(p,p+keep,prefix->begin()+captured);captured+=keep;}status(hash.write(p,n));if(actual)status(actual->write(p,n));begin+=n;length-=n;}
    return hash.digest();
}
CheckpointDigest bytes_digest(const CheckpointJobBytes& bytes){HashSink h;status(h.write(bytes.bytes().data(),bytes.bytes().size()));return h.digest();}
void append_owned(HashSink& out,const CheckpointJobBytes& bytes,CheckpointJobReservation& active,std::uint64_t chunk){for(std::size_t at=0;at<bytes.bytes().size();){status(active.grow(0));const auto n=std::size_t(std::min<std::uint64_t>(chunk,bytes.bytes().size()-at));status(out.write(bytes.bytes().data()+at,n));at+=n;}}
// 内层魔数和HMCB摘要只作物理绑定；真实scope和capsule权限仍必须由原core/profile核验。
void verify_payload_magic(const std::array<std::array<std::uint8_t,69>,3>& prefixes,const SlotEnvelopeIndex& index){
    const auto& c=index.payloads[0];const char* expected=c.codec==1?"HM-RESIDENT-CHECKPOINT/1\n":"HM-RESIDENT-CHECKPOINT/2\n";
    constexpr std::size_t prefix=sizeof("HM-RESIDENT-CHECKPOINT/2\n")-1;const auto* b=prefixes[0].data();require(c.length>=prefix,"InvalidCheckpointEnvelope");require(std::memcmp(b,expected,prefix)==0,"SlotCheckpointCodecMismatch");
    const auto& run=index.payloads[1];if(run.present){const auto n=run.codec==1?69u:37u;require(run.length>n,"InvalidContinuationSnapshot");b=prefixes[1].data();require(std::memcmp(b,"HMCB",4)==0&&b[4]==run.codec,"SlotContinuationCodecMismatch");if(run.codec==2)require(std::equal(c.sha256.begin(),c.sha256.end(),b+5),"ContinuationCheckpointMismatch");else{const char hex[]="0123456789abcdef";for(std::size_t i=0;i<32;++i)require(b[5+2*i]==hex[c.sha256[i]>>4]&&b[6+2*i]==hex[c.sha256[i]&15],"ContinuationCheckpointMismatch");}}
    const auto& admission=index.payloads[2];require(admission.length>=5,"InvalidAdmissionCapsule");b=prefixes[2].data();require(std::memcmp(b,"HMAC",4)==0&&b[4]==1,"SlotAdmissionCodecMismatch");
}
}
struct SlotEnvelopeAccess {
    static std::shared_ptr<FrozenSlotEnvelope> create(){auto value=std::unique_ptr<FrozenSlotEnvelope>(new FrozenSlotEnvelope);return checkpoint_share(std::move(value));}
    static SlotEnvelopeInspection inspected(SlotEnvelopeIndex&& index,CheckpointJobReservation&& token){SlotEnvelopeInspection r;r.reservation_=std::move(token);r.value_=std::move(index);return r;}
    static void initialize(FrozenSlotEnvelope& out,SlotEnvelopeIndex&& index,std::shared_ptr<const SlotCheckpointProducer> source,
        CheckpointJobBytes&& header,CheckpointJobBytes&& run,CheckpointJobBytes&& admission,CheckpointJobReservation&& control,std::uint64_t chunk){
        out.chunk_bytes_=chunk;out.control_=std::move(control);out.index_=std::move(index);out.checkpoint_=std::move(source);out.header_=std::move(header);out.continuation_=std::move(run);out.admission_=std::move(admission);
    }
};
SlotEnvelopeInspection& SlotEnvelopeInspection::operator=(SlotEnvelopeInspection&& other)noexcept{if(this!=&other){value_.~SlotEnvelopeIndex();reservation_=std::move(other.reservation_);new(&value_)SlotEnvelopeIndex(std::move(other.value_));}return *this;}
CheckpointJobResult<std::shared_ptr<const FrozenSlotEnvelope>> freeze_slot_envelope(const StableId& id,Integer generation,std::uint64_t saved,
    std::shared_ptr<const SlotCheckpointProducer> checkpoint,CheckpointJobBytes&& run,CheckpointJobBytes&& admission,
    const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){return guard<std::shared_ptr<const FrozenSlotEnvelope>>(job,[&]{
    status(job.check_profile(capacity));status(job.check_profile(slot));slot_id(id);require(generation>0&&saved<=std::uint64_t(INT64_MAX),"InvalidSlotGeneration");
    require(bool(checkpoint)&&(checkpoint->codec_version()==1||checkpoint->codec_version()==2),"SlotCheckpointCodecMismatch");
    require(admission.belongs_to(job)&&!admission.bytes().empty(),"MissingAdmissionCapsule");if(!run.bytes().empty())require(run.belongs_to(job),"CheckpointJobReservationMismatch");
    require(admission.bytes().size()<=slot.max_admission_capsule_bytes&&run.bytes().size()<=slot.max_continuation_bytes,"SlotPayloadCapacityExceeded");
    auto resource=take(checkpoint_memory_resource(job,CheckpointJobUse::encode));ScopedCheckpointMemory scope(resource);
    auto control=take(job.reserve(CheckpointJobUse::encode,id.size()+1));auto scratch=take(job.reserve(CheckpointJobUse::encode,hash_scratch));
    HashSink measure;auto measured=take(checkpoint->write(measure,job));require(measured.total_bytes==measure.count&&measured.full_checkpoint_sha256==measure.digest(),"CheckpointMeasureMismatch");require(measure.count>0&&measure.count<=capacity.max_checkpoint_bytes,"SlotCheckpointCapacityExceeded");
    SlotEnvelopeIndex index;index.save_id=StableId(id.begin(),id.end());index.generation=generation;index.saved_at_unix_seconds=saved;
    const auto header_size=plus(minimum_header,id.size()-1);require(header_size<=slot.max_slot_header_bytes,"SlotHeaderCapacityExceeded");
    auto at=header_size;index.payloads[0]={1,1,checkpoint->codec_version(),at,measure.count,measured.full_checkpoint_sha256};at=plus(at,measure.count);
    if(!run.bytes().empty()){require(run.bytes().size()>=5&&std::memcmp(run.bytes().data(),"HMCB",4)==0&&(run.bytes()[4]==1||run.bytes()[4]==2),"InvalidContinuationSnapshot");require(run.bytes()[4]!=1||run.bytes().size()<=2048,"InvalidContinuationSnapshot");index.payloads[1]={2,1,run.bytes()[4],at,run.bytes().size(),bytes_digest(run)};at=plus(at,run.bytes().size());}else index.payloads[1].kind=2;
    require(admission.bytes().size()>=5&&std::memcmp(admission.bytes().data(),"HMAC",4)==0&&admission.bytes()[4]==1,"InvalidAdmissionCapsule");index.payloads[2]={3,1,1,at,admission.bytes().size(),bytes_digest(admission)};at=plus(at,admission.bytes().size());index.total_file_bytes=plus(at,32);require(index.total_file_bytes<=slot.max_slot_bytes,"SlotCapacityExceeded");
    std::array<std::array<std::uint8_t,69>,3> prefixes{};
    const auto* checkpoint_magic=checkpoint->codec_version()==1?"HM-RESIDENT-CHECKPOINT/1\n":"HM-RESIDENT-CHECKPOINT/2\n";
    std::copy(checkpoint_magic,checkpoint_magic+sizeof("HM-RESIDENT-CHECKPOINT/2\n")-1,prefixes[0].begin());
    for(const auto pair:{std::pair<std::size_t,const CheckpointJobBytes*>{1,&run},{2,&admission}}){const auto& b=pair.second->bytes();std::copy_n(b.begin(),std::min(b.size(),prefixes[pair.first].size()),prefixes[pair.first].begin());}
    verify_payload_magic(prefixes,index);
    auto header=take(CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,header_size));HeaderWriter w(header);w.raw(reinterpret_cast<const std::uint8_t*>(magic),16);w.number(2,2);w.number(0,2);w.number(index.total_file_bytes,8);w.text(id);w.number(generation,8);w.number(saved,8);w.number(3,1);
    for(const auto& d:index.payloads){w.number(d.kind,1);w.number(d.present,1);w.number(d.codec,2);w.number(d.offset,8);w.number(d.length,8);w.raw(d.sha256.data(),32);}require(w.at==header_size,"InvalidSlotHeader");
    auto result=SlotEnvelopeAccess::create();SlotEnvelopeAccess::initialize(*result,std::move(index),std::move(checkpoint),std::move(header),std::move(run),std::move(admission),std::move(control),capacity.stream_chunk_bytes);return std::shared_ptr<const FrozenSlotEnvelope>(std::move(result));
});}
CheckpointJobStatus FrozenSlotEnvelope::write(CheckpointWriteSink& sink,CheckpointJobBudget& job)const{return guard<Done>(job,[&]{
    require(header_.belongs_to(job)&&admission_.belongs_to(job),"CheckpointJobReservationMismatch");auto scratch=take(job.reserve(CheckpointJobUse::encode,hash_scratch));BoundedSink bounded(sink,chunk_bytes_);HashSink whole(&bounded);
    status(whole.write(header_.bytes().data(),header_.bytes().size()));HashSink payload(&whole);auto actual=take(checkpoint_->write(payload,job));const auto& descriptor=index_.payloads[0];require(actual.total_bytes==descriptor.length&&actual.full_checkpoint_sha256==descriptor.sha256&&payload.count==descriptor.length&&payload.digest()==descriptor.sha256,"CheckpointMeasureMismatch");
    append_owned(whole,continuation_,scratch,chunk_bytes_);append_owned(whole,admission_,scratch,chunk_bytes_);require(whole.count==index_.total_file_bytes-32,"SlotMeasureMismatch");const auto digest=whole.digest();status(scratch.grow(0));status(bounded.write(digest.data(),digest.size()));return Done{};
});}
static SlotEnvelopeInspection inspect_slot_envelope_impl(CheckpointReadSource& source,const StableId* expected,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    status(job.check_profile(capacity));status(job.check_profile(slot));if(expected)slot_id(*expected);const auto size=source.size();require(size>=minimum_header+32,"TruncatedSlotEnvelope");require(size<=slot.max_slot_bytes&&size<=std::uint64_t(INT64_MAX),"SlotCapacityExceeded");
    auto scratch=take(job.reserve(CheckpointJobUse::scratch,hash_scratch));auto chunk=take(CheckpointJobBytes::allocate(job,CheckpointJobUse::scratch,capacity.stream_chunk_bytes));
    HashSink prefix_hash;Reader pre(source,prefix_hash);std::uint8_t prefix[16]{};pre.raw(prefix,16);require(std::memcmp(prefix,magic,16)==0,"InvalidSlotEnvelope");require(pre.number(2)==2,"UnsupportedSlotEnvelopeVersion");require(pre.number(2)==0,"InvalidSlotFlags");require(pre.number(8)==size,"InvalidSlotLength");
    CheckpointDigest trailer{};status(source.read(size-32,trailer.data(),32));require(hash_range(source,0,size-32,chunk,scratch)==trailer,"SlotDigestMismatch");
    auto owned=take(job.reserve(CheckpointJobUse::candidate_scratch,sizeof(SlotEnvelopeIndex)+257));SlotEnvelopeIndex index;HashSink actual;Reader read(source,actual);read.raw(prefix,16);require(std::memcmp(prefix,magic,16)==0&&read.number(2)==2&&read.number(2)==0&&read.number(8)==size,"SlotSourceChanged");
    auto length=read.number(2);require(length>0&&length<=256,"InvalidSlotId");std::uint8_t name[256]{};read.raw(name,std::size_t(length));index.save_id=StableId(reinterpret_cast<const char*>(name),std::size_t(length));slot_id(index.save_id);if(expected)require(index.save_id==*expected,"SlotIdentityMismatch");index.generation=Integer(read.number(8));require(index.generation>0,"InvalidSlotGeneration");index.saved_at_unix_seconds=read.number(8);index.total_file_bytes=size;require(read.number(1)==3,"InvalidSlotDescriptorCount");
    for(std::size_t n=0;n<3;++n){auto& d=index.payloads[n];d.kind=std::uint8_t(read.number(1));d.present=std::uint8_t(read.number(1));d.codec=std::uint16_t(read.number(2));d.offset=read.number(8);d.length=read.number(8);read.raw(d.sha256.data(),32);require(d.kind==n+1,"InvalidSlotDescriptorKind");}
    require(read.at<=slot.max_slot_header_bytes,"SlotHeaderCapacityExceeded");auto at=read.at;std::array<std::array<std::uint8_t,69>,3> prefixes{};
    for(std::size_t n=0;n<3;++n){const auto& d=index.payloads[n];if(n==1&&!d.present){require(d.codec==0&&d.offset==0&&d.length==0&&zero(d.sha256),"InvalidAbsentContinuation");continue;}require(d.present==1&&d.length>0&&d.offset==at&&at<=size-32&&d.length<=size-32-at,"InvalidSlotPayloadRange");require(n==2?d.codec==1:(d.codec==1||d.codec==2),"UnsupportedSlotPayloadCodec");const auto limit=n==0?capacity.max_checkpoint_bytes:n==1?slot.max_continuation_bytes:slot.max_admission_capsule_bytes;require(d.length<=limit&&(n!=1||d.codec!=1||d.length<=2048),"SlotPayloadCapacityExceeded");require(hash_range(source,at,d.length,chunk,scratch,&actual,&prefixes[n])==d.sha256,"SlotPayloadDigestMismatch");at+=d.length;}
    require(at==size-32&&actual.count==size-32&&actual.digest()==trailer,"SlotSourceChanged");verify_payload_magic(prefixes,index);
    // 内层魔数来自上面参与摘要的同一批真实字节，不用另一次读取替代解释字节。
    CheckpointDigest final{};status(source.read(size-32,final.data(),32));require(source.size()==size&&final==trailer,"SlotSourceChanged");index.envelope_sha256=trailer;status(owned.grow(0));return SlotEnvelopeAccess::inspected(std::move(index),std::move(owned));
}
CheckpointJobResult<SlotEnvelopeInspection> inspect_slot_envelope(CheckpointReadSource& source,const StableId& expected,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){return guard<SlotEnvelopeInspection>(job,[&]{return inspect_slot_envelope_impl(source,&expected,capacity,slot,job);});}
CheckpointJobResult<SlotEnvelopeInspection> inspect_import_slot_envelope(CheckpointReadSource& source,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){return guard<SlotEnvelopeInspection>(job,[&]{return inspect_slot_envelope_impl(source,nullptr,capacity,slot,job);});}
CheckpointJobResult<SlotEnvelopeBankScan> scan_slot_envelope_bank(CheckpointReadSource& source,const StableId& expected,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){
    try{SlotEnvelopeBankScan result;result.inspection=inspect_slot_envelope_impl(source,&expected,capacity,slot,job);return {true,std::move(result),{}};}
    catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}
    catch(WireFailure& error){
        SlotEnvelopeBankScan result;const auto& code=error.error.original.code;
        if(code=="TruncatedSlotEnvelope")result.corruption=SlotBankCorruption::truncated;
        else if(code=="SlotDigestMismatch")result.corruption=SlotBankCorruption::digest;
        else if(code=="SlotPayloadDigestMismatch")result.corruption=SlotBankCorruption::payload_digest;
        else return {false,{},job.capture_failure(std::move(error.error))};
        // 物理拒绝只销毁该银行私有候选与暂存；同一job不清零，也不把它写成不可恢复首错。
        return {true,std::move(result),{}};
    }catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(Code::allocation_failed)};}catch(const std::length_error&){return {false,{},job.capture_fixed_failure(Code::size_overflow)};}
}
CheckpointJobResult<SelectedSlotEnvelope> select_slot_envelope_banks(const SlotStreamBankPair& banks,const StableId& id,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){return guard<SelectedSlotEnvelope>(job,[&]{
    SelectedSlotEnvelope selected;SlotEnvelopeBankScan a,b;
    if(banks.a){a=take(scan_slot_envelope_bank(*banks.a,id,capacity,slot,job));selected.a_corruption=a.corruption;}
    if(banks.b){b=take(scan_slot_envelope_bank(*banks.b,id,capacity,slot,job));selected.b_corruption=b.corruption;}
    const bool good_a=banks.a&&a.corruption==SlotBankCorruption::none,good_b=banks.b&&b.corruption==SlotBankCorruption::none;
    if(!good_a&&!good_b)return selected;
    if(good_a&&good_b){const auto& first=a.inspection.value();const auto& second=b.inspection.value();require(first.generation!=second.generation||first.envelope_sha256==second.envelope_sha256,"SlotGenerationConflict");}
    selected.present=true;selected.bank=good_a&&(!good_b||a.inspection.value().generation>=b.inspection.value().generation)?SlotBank::a:SlotBank::b;
    selected.inspection=selected.bank==SlotBank::a?std::move(a.inspection):std::move(b.inspection);return selected;
});}
Status SlotPayloadReadSource::read(std::uint64_t offset,std::uint8_t* bytes,std::size_t n){if(offset>descriptor_.length||n>descriptor_.length-offset||descriptor_.offset>UINT64_MAX-offset)return {false,{},{{"SlotPayloadReadRange","slot/payload"},ErrorKind::internal,{},{},{},{}}};return source_.read(descriptor_.offset+offset,bytes,n);}
} // 本文件只返回物理目录，未调用世界安装、profile并集或耐久成功接口。
