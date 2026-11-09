// HMCB2只验证真实原HMCN2绑定与受信技术额度，不伪造世界业务核验成功。
#include "resident/kernel/continuation_binding.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <picosha2.h>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool okay,const char* label){++checks;if(!okay){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class T>T must(Outcome<T> v){if(!v.ok)throw std::runtime_error(v.error.original.code);return std::move(v.value);}
template<class T>T must(CheckpointJobResult<T> v){if(!v.ok){v.error.try_materialize_rule_error();throw std::runtime_error(v.error.rule_error()?v.error.rule_error()->original.code:"fixed job failure");}return std::move(v.value);}
CanonicalFraction fraction(const std::string& n,const std::string& d){return must(normalize_fraction(n,d));}
ContinuationSnapshot exact(){ContinuationSnapshot s;s.continuation_version=2;s.source_identity.world_id[0]=1;s.source_identity.execution_epoch=77;s.source_identity.revision=8;s.source_identity.tick=2;s.actor_id="player";s.action_id="service:3";s.action_kind="service";s.wallet_id="wallet";s.max_half_days=8;s.max_gross_outflow=100;s.gross_outflow=40;s.stop=RunStop::restored;s.exact_source_moment=WorldMoment{2,fraction("1","3")};s.exact_started_at=WorldMoment{1,fraction("1","4")};s.exact_committed_duration=HalfDayDuration{fraction("1","2")};s.action_started_source=ContinuationStartedSource{"growth","growth_fixed_service_boundary","growth_fixed_service_boundary:5",1};return s;}
ContinuationSnapshot legacy(){ContinuationSnapshot s;s.source_identity.world_id[0]=1;s.source_identity.execution_epoch=7;s.source_identity.revision=8;s.source_identity.tick=8;s.actor_id="actor.player";s.action_id="action:4";s.action_kind="study_skill";s.wallet_id="wallet.player";s.action_start_tick=1;s.started_tick=2;s.max_half_days=8;s.max_gross_outflow=100;s.committed_half_days=3;s.gross_outflow=40;s.stop=RunStop::cancelled;return s;}
void put(Bytes& b,std::size_t at,std::uint64_t n){for(unsigned i=0;i<8;++i)b[at+i]=std::uint8_t(n>>(8*i));}
}
int main(){try{
    auto s=exact();const Bytes checkpoint{'b','i','n','d','i','n','g','-','f','i','x','t','u','r','e'};auto p=engineering_slot_capacity_profile();CheckpointDigest digest{};picosha2::hash256(checkpoint.begin(),checkpoint.end(),digest.begin(),digest.end());auto inner=must(encode_continuation_snapshot(s,p.max_continuation_bytes-45));auto bound=must(bind_continuation_checkpoint(s,checkpoint,p));
    Bytes golden{'H','M','C','B',2};golden.insert(golden.end(),digest.begin(),digest.end());for(unsigned i=0;i<8;++i)golden.push_back(std::uint8_t(std::uint64_t(inner.size())>>(8*i)));golden.insert(golden.end(),inner.begin(),inner.end());check(bound==golden,"HMCB2 exact raw32/u64/HMCN2 wire");
    check(must(bind_continuation_checkpoint_v2(s,digest,p))==bound,"stream digest and Bytes adapter same binding");auto decoded=must(unbind_continuation_checkpoint(bound,checkpoint,p));check(decoded.continuation_version==2&&decoded.source_identity.execution_epoch==0&&decoded.action_started_source->id==s.action_started_source->id,"actual HMCN2 preserved without epoch");check(must(encode_continuation_snapshot(decoded,p.max_continuation_bytes-45))==inner,"exact continuation canonical roundtrip");
    auto exact_budget=p;exact_budget.max_continuation_bytes=bound.size();check(bind_continuation_checkpoint(s,checkpoint,exact_budget).ok&&unbind_continuation_checkpoint(bound,checkpoint,exact_budget).ok,"exact wrapper inclusive capacity accepted");exact_budget.max_continuation_bytes--;check(!bind_continuation_checkpoint(s,checkpoint,exact_budget).ok&&!unbind_continuation_checkpoint(bound,checkpoint,exact_budget).ok,"one-byte continuation overlimit rejected");
    auto changed=checkpoint;changed[0]^=1;check(unbind_continuation_checkpoint(bound,changed,p).error.original.code=="ContinuationCheckpointMismatch","different checkpoint refused");auto wrong=digest;wrong[0]^=1;check(!unbind_continuation_checkpoint_v2(bound,wrong,p).ok,"different raw stream digest refused");
    bool prefixes=true;for(std::size_t n=0;n<bound.size();++n)if(unbind_continuation_checkpoint(Bytes(bound.begin(),bound.begin()+n),checkpoint,p).ok)prefixes=false;check(prefixes,"all truncated wrappers reject");
    for(const auto n:{std::uint64_t(0),std::uint64_t(inner.size()+1),UINT64_MAX}){auto bad=bound;put(bad,37,n);check(!unbind_continuation_checkpoint(bad,checkpoint,p).ok,"checked u64 length rejects mismatch/overflow");}
    auto bad=bound;bad.push_back(0);check(!unbind_continuation_checkpoint(bad,checkpoint,p).ok,"trailing bytes rejected");bad=bound;bad[4]=3;check(!unbind_continuation_checkpoint(bad,checkpoint,p).ok,"unknown wrapper version rejected");bad=bound;bad[49]=1;check(!unbind_continuation_checkpoint(bad,checkpoint,p).ok,"HMCB2 cannot carry HMCN1");
    auto large=s;large.exact_committed_duration=HalfDayDuration{fraction("1","1"+std::string(2400,'0')+"7")};auto large_bound=must(bind_continuation_checkpoint(large,checkpoint,p));check(large_bound.size()>2048,"large legal exact scope exceeds old cap");auto large_back=must(unbind_continuation_checkpoint(large_bound,checkpoint,p));check(must(compare_fractions(large_back.exact_committed_duration->value,large.exact_committed_duration->value))==0,"large denominator retained exactly");auto tiny=p;tiny.max_continuation_bytes=2048;check(!bind_continuation_checkpoint(large,checkpoint,tiny).ok,"smaller trusted slot never drops scope");
    auto old=legacy();auto old_bound=must(bind_continuation_checkpoint(old,checkpoint));auto old_inner=must(encode_continuation_snapshot(old));auto hex=picosha2::hash256_hex_string(checkpoint.begin(),checkpoint.end());Bytes old_golden{'H','M','C','B',1};old_golden.insert(old_golden.end(),hex.begin(),hex.end());old_golden.insert(old_golden.end(),old_inner.begin(),old_inner.end());check(old_bound==old_golden,"HMCB1 old hex and bytes unchanged");check(must(bind_continuation_checkpoint(old,checkpoint,p))==old_bound,"trusted overload preserves legacy bytes");check(unbind_continuation_checkpoint(old_bound,checkpoint).ok&&unbind_continuation_checkpoint(old_bound,checkpoint,p).ok,"legacy routes still read original snapshot");check(!bind_continuation_checkpoint(s,checkpoint).ok,"old HMCB1 API cannot silently wrap HMCN2");old_bound.resize(2049);check(!unbind_continuation_checkpoint(old_bound,checkpoint,p).ok,"HMCB1 original2048 unchanged under large profile");
    Bytes smuggled{'H','M','C','B',1};smuggled.insert(smuggled.end(),hex.begin(),hex.end());smuggled.insert(smuggled.end(),inner.begin(),inner.end());check(smuggled.size()<2048,"HMCN2 smuggling fixture below old cap");check(!unbind_continuation_checkpoint(smuggled,checkpoint).ok&&!unbind_continuation_checkpoint(smuggled,checkpoint,p).ok,"HMCB1 cannot carry HMCN2 in either overload");
    // 本叶只对已经拥有的真实HMCN2字节核外壳；取消内存账不取消同job和实际输出拥有。
    auto cp=engineering_checkpoint_capacity_profile();auto job=must(CheckpointJobBudget::create(cp,p));
    auto owned_inner=must(CheckpointJobBytes::copy(job,CheckpointJobUse::slot,inner));
    auto owned_bound=must(bind_encoded_continuation_checkpoint_v2(owned_inner,digest,p,job));
    check(owned_bound.bytes()==bound,"same-job HMCB2 golden preserved");
    check(owned_inner.belongs_to(job)&&owned_bound.belongs_to(job)&&owned_inner.bytes()==inner&&owned_inner.bytes().data()!=owned_bound.bytes().data(),"wrapper owns distinct real output while exact inner bytes remain live");
    auto owned_back=must(unbind_encoded_continuation_checkpoint_v2(owned_bound,digest,p,job));
    check(owned_back.bytes()==inner&&owned_back.belongs_to(job)&&owned_back.bytes().data()!=owned_inner.bytes().data(),"same-job physical unwrap preserves independent real HMCN2 ownership");
    check(decode_continuation_snapshot(owned_back.bytes(),p.max_continuation_bytes-45).ok,"unwrapped real typed HMCN still valid");
    auto other=must(CheckpointJobBudget::create(cp,p));
    auto foreign=bind_encoded_continuation_checkpoint_v2(owned_inner,digest,p,other);
    check(!foreign.ok&&foreign.value.bytes().empty(),"HMCB rejects different job owner without output");
    auto shortslot=p;shortslot.max_continuation_bytes=bound.size()-1;
    auto shortjob=must(CheckpointJobBudget::create(cp,shortslot));
    auto shortinner=must(CheckpointJobBytes::copy(shortjob,CheckpointJobUse::slot,inner));
    auto shortbound=bind_encoded_continuation_checkpoint_v2(shortinner,digest,shortslot,shortjob);
    check(!shortbound.ok&&shortbound.error.rule_error()&&shortbound.error.rule_error()->original.code=="ContinuationCapacityExceeded"&&shortbound.error.rule_error()->original.path=="checkpoint/capacity/continuation_bytes"&&shortbound.error.rule_error()->reason==std::optional<StableId>(std::to_string(bound.size()))&&shortbound.value.bytes().empty()&&shortinner.belongs_to(shortjob)&&shortinner.bytes()==inner,"wrapper real byte limit reports the exact required extent without altering its owned HMCN2 input");
    job.cancel();auto cancelled=unbind_encoded_continuation_checkpoint_v2(owned_bound,digest,p,job);
    check(!cancelled.ok&&cancelled.value.bytes().empty(),"cancelled wrapper refuses new output");
    auto returned_failure=[&](){auto local=must(CheckpointJobBudget::create(cp,p));auto local_inner=must(CheckpointJobBytes::copy(local,CheckpointJobUse::slot,inner));auto local_bound=must(bind_encoded_continuation_checkpoint_v2(local_inner,digest,p,local));auto different=digest;different[0]^=1;return unbind_encoded_continuation_checkpoint_v2(local_bound,different,p,local);}();check(!returned_failure.ok&&returned_failure.error.rule_error()&&returned_failure.error.rule_error()->original.code=="ContinuationCheckpointMismatch","actual HMCB first failure lease survives local job and inputs");
    auto invalid=p;invalid.max_slot_bytes=256000001;check(!bind_continuation_checkpoint(s,checkpoint,invalid).ok,"whole-slot coarse cap cannot be raised by a trusted profile");invalid=p;invalid.max_continuation_bytes=0;check(!bind_continuation_checkpoint(s,checkpoint,invalid).ok,"zero trusted capacity invalid");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<"FAIL exception "<<e.what()<<'\n';return 1;}}
