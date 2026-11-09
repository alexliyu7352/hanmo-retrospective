// HMCN的显式同job入口直接两遍量测/写入；叶从有限source区间读取，不复制整leaf Bytes。
#include "continuation_snapshot.hpp"
#include "world_timeline_codec.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
namespace hanmo::resident {
namespace {
struct WireFailure{RuleError error;};
void need(bool yes,const char* code="InvalidContinuationSnapshot"){if(!yes)throw WireFailure{{{code,"continuation"},ErrorKind::rule,{},{},{},{}}};}
void status(Status s){if(!s.ok)throw WireFailure{std::move(s.error)};}
void status(CheckpointJobStatus s){if(!s.ok)throw checkpoint_job_detail::FailureException{std::move(s.error)};}
template<class T>T take(CheckpointJobResult<T> r){if(!r.ok)throw checkpoint_job_detail::FailureException{std::move(r.error)};return std::move(r.value);}
template<class T>T take(Outcome<T> r){if(!r.ok)throw WireFailure{std::move(r.error)};return std::move(r.value);}
template<class T,class F>CheckpointJobResult<T> guard(CheckpointJobBudget& job,F f){try{return {true,f(),{}};}catch(checkpoint_job_detail::FailureException& e){return {false,{},std::move(e.error)};}catch(WireFailure& e){return {false,{},job.capture_failure(std::move(e.error))};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}catch(const std::length_error&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};}}
std::optional<std::uint8_t> stop_code(RunStop stop){switch(stop){case RunStop::awaiting_permit:return 0;case RunStop::awaiting_input:return 1;case RunStop::arrived:return 2;case RunStop::paused:return 3;case RunStop::stranded:return 4;case RunStop::blocked:return 5;case RunStop::cancelled:return 6;case RunStop::budget_exhausted:return 7;case RunStop::failed:return 8;case RunStop::money_budget_exhausted:return 9;case RunStop::action_replaced:return 10;case RunStop::restored:return 11;case RunStop::completed:return 12;}return {};}
std::optional<RunStop> read_stop(std::uint64_t code){switch(code){case 0:return RunStop::awaiting_permit;case 1:return RunStop::awaiting_input;case 2:return RunStop::arrived;case 3:return RunStop::paused;case 4:return RunStop::stranded;case 5:return RunStop::blocked;case 6:return RunStop::cancelled;case 7:return RunStop::budget_exhausted;case 8:return RunStop::failed;case 9:return RunStop::money_budget_exhausted;case 10:return RunStop::action_replaced;case 11:return RunStop::restored;case 12:return RunStop::completed;default:return {};}}
// 量测和写入采用同一固定长度sink，输出预留在真正分配之前取得。
class WireSink:public CheckpointWriteSink {
public:
    explicit WireSink(std::uint64_t limit,CheckpointJobBytes* bytes=nullptr):limit_(limit),bytes_(bytes){}
    Status write(const std::uint8_t* data,std::size_t n)override{if(at>limit_||n>limit_-at)return {false,{},{{"ContinuationCapacityExceeded","continuation"},ErrorKind::internal,{},{},{},{}}};if(bytes_)status(bytes_->write(std::size_t(at),data,n));at+=n;return {true,{},{}};}
    void number(std::uint64_t value,unsigned width=8){std::uint8_t data[8]{};for(unsigned i=0;i<width;++i)data[i]=std::uint8_t(value>>(8*i));status(write(data,width));}
    void identifier(const StableId& id){need(!id.empty()&&id.size()<=256);number(id.size(),2);status(write(reinterpret_cast<const std::uint8_t*>(id.data()),id.size()));}
    std::uint64_t at=0;
private:std::uint64_t limit_;CheckpointJobBytes* bytes_;
};
// 原RWM writer只读原字段；两遍借同一拥有值，不复制BigNat或创建第二份leaf。
template<class T,class F>void leaf(WireSink& out,const T& value,F visit,const CapacityProfile& profile,std::uint64_t limit,CheckpointJobBudget& job){
    auto& borrowed=const_cast<T&>(value);WireSink measure(limit);CodecVisitor m(measure,profile,"world_timeline",limit,&job);status(visit(m,borrowed));status(m.finish());out.number(measure.at);
    CodecVisitor writer(out,profile,"world_timeline",measure.at,&job);status(visit(writer,borrowed));status(writer.finish());need(writer.encoded_bytes()==measure.at,"ContinuationMeasureMismatch");
}
void write_exact(WireSink& out,const ContinuationSnapshot& s,const CapacityProfile& p,std::uint64_t limit,CheckpointJobBudget& job){
    const std::uint8_t magic[]{'H','M','C','N',2};status(out.write(magic,5));status(out.write(s.source_identity.world_id.data(),16));out.number(s.source_identity.revision);
    leaf(out,*s.exact_source_moment,visit_world_moment2,p,limit,job);out.identifier(s.actor_id);out.identifier(s.action_id);out.identifier(s.action_kind);out.identifier(s.wallet_id);
    const auto& ref=*s.action_started_source;out.identifier(ref.domain);out.identifier(ref.kind);out.identifier(ref.id);out.number(ref.schema,4);leaf(out,*s.exact_started_at,visit_world_moment2,p,limit,job);out.number(s.max_half_days);out.number(s.max_gross_outflow);leaf(out,*s.exact_committed_duration,visit_half_day_duration2,p,limit,job);out.number(s.gross_outflow);out.number(*stop_code(s.stop),1);
}
// 所有字符串先入同一个候选账；区间由外层HMCB真实长度与slot descriptor共同限定。
class WireReader {
public:
    WireReader(CheckpointReadSource& s,std::uint64_t begin,std::uint64_t length,CodecDecodeBudget& budget):source(s),begin(begin),length(length),budget(budget){}
    void raw(std::uint8_t* data,std::size_t n){need(at<=length&&n<=length-at);for(std::size_t done=0;done<n;){auto chunk=std::min<std::size_t>(n-done,std::size_t(budget.profile.stream_chunk_bytes));status(source.read(begin+at+done,data+done,chunk));done+=chunk;}at+=n;}
    std::uint64_t number(unsigned width=8){std::uint8_t data[8]{};raw(data,width);std::uint64_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint64_t(data[i])<<(8*i);need(width<8||v<=std::uint64_t(INT64_MAX));return v;}
    StableId identifier(){const auto n=number(2);need(n>0&&n<=256&&n<=length-at); status(budget.charge_candidate(std::size_t{0}));StableId value(std::size_t(n),'\0');raw(reinterpret_cast<std::uint8_t*>(value.data()),std::size_t(n));return value;}
    template<class T,class F>T leaf(F visit,const CapacityProfile& p){const auto n=number();need(n>0&&n<=length-at);T value;CodecVisitor reader(source,begin+at,n,p,"world_timeline",budget);status(visit(reader,value));status(reader.finish());at+=n;return value;}
    CheckpointReadSource& source;std::uint64_t begin,length,at=0;CodecDecodeBudget& budget;
};
}
CheckpointJobResult<CheckpointJobBytes> encode_continuation_snapshot(const ContinuationSnapshot& snapshot,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CheckpointJobBudget& job){return guard<CheckpointJobBytes>(job,[&]{
    status(job.check_profile(capacity));status(job.check_profile(slot));need(snapshot.continuation_version==2,"ContinuationVersionMismatch");need(slot.max_continuation_bytes>45,"ContinuationCapacityExceeded");
    auto memory=take(checkpoint_memory_resource(job,CheckpointJobUse::encode));ScopedCheckpointMemory scope(memory);status(validate_continuation_shape(snapshot));
    const auto limit=slot.max_continuation_bytes-45;WireSink measured(limit);write_exact(measured,snapshot,capacity,limit,job);auto bytes=take(CheckpointJobBytes::allocate(job,CheckpointJobUse::encode,measured.at));WireSink output(measured.at,&bytes);write_exact(output,snapshot,capacity,limit,job);need(output.at==measured.at,"ContinuationMeasureMismatch");return bytes;
});}
// caller必须持私有候选及同预算；此函数不安装世界、重绑epoch或授予继续许可。
CheckpointJobStatus decode_continuation_snapshot_into_candidate(CheckpointReadSource& source,std::uint64_t begin,std::uint64_t length,const CapacityProfile& capacity,const SlotCapacityProfile& slot,CodecDecodeBudget& budget,CheckpointJobBudget& job,ContinuationSnapshot& output){return guard<Done>(job,[&]{
    status(job.check_profile(capacity));status(job.check_profile(slot));
    if(!budget.belongs_to(job))throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
    need(begin<=source.size()&&length<=source.size()-begin&&length>=29&&slot.max_continuation_bytes>=45&&length<=slot.max_continuation_bytes-45);
    auto memory=take(checkpoint_memory_resource(job,CheckpointJobUse::candidate));ScopedCheckpointMemory scope(memory);ContinuationSnapshot candidate;WireReader r(source,begin,length,budget);std::uint8_t prefix[5]{};r.raw(prefix,5);need(std::memcmp(prefix,"HMCN",4)==0&&prefix[4]==2,"ContinuationVersionMismatch");candidate.continuation_version=2;r.raw(candidate.source_identity.world_id.data(),16);candidate.source_identity.revision=Integer(r.number());candidate.exact_source_moment=r.leaf<WorldMoment>(visit_world_moment2,capacity);candidate.source_identity.tick=candidate.exact_source_moment->half_day_tick;
    candidate.actor_id=r.identifier();candidate.action_id=r.identifier();candidate.action_kind=r.identifier();candidate.wallet_id=r.identifier();ContinuationStartedSource started;started.domain=r.identifier();started.kind=r.identifier();started.id=r.identifier();started.schema=std::uint32_t(r.number(4));candidate.action_started_source=std::move(started);candidate.exact_started_at=r.leaf<WorldMoment>(visit_world_moment2,capacity);candidate.started_tick=candidate.exact_started_at->half_day_tick;candidate.max_half_days=Integer(r.number());candidate.max_gross_outflow=Integer(r.number());candidate.exact_committed_duration=r.leaf<HalfDayDuration>(visit_half_day_duration2,capacity);candidate.gross_outflow=Integer(r.number());const auto stop=r.number(1);const auto stopped=read_stop(stop);need(stopped.has_value());candidate.stop=*stopped;need(r.at==length);status(validate_continuation_shape(candidate));candidate.committed_half_days=take(floor_fraction(candidate.exact_committed_duration->value));output=std::move(candidate);return Done{};
});}
} // HMCN1旧重载仍保持2048和原字节；此完整同job路径显式只接HMCN2。
