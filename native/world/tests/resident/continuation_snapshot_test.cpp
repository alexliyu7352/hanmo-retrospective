// 许可冷字节的实际回归：有限长度、严格尾部、整数边界与坏停止码均拒绝。
#include "resident/kernel/continuation_snapshot.hpp"
#include <iostream>
#include <limits>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
ContinuationSnapshot sample(){ContinuationSnapshot s;s.source_identity.world_id[0]=1;s.source_identity.execution_epoch=7;s.source_identity.revision=8;s.source_identity.tick=8;s.actor_id="actor.player";s.action_id="action:4";s.action_kind="study_skill";s.wallet_id="wallet.player";s.action_start_tick=1;s.started_tick=2;s.max_half_days=8;s.max_gross_outflow=100;s.committed_half_days=3;s.gross_outflow=40;s.stop=RunStop::cancelled;return s;}
}
int main(){
    auto source=sample();auto encoded=encode_continuation_snapshot(source);check(encoded.ok&&encoded.value.size()<continuation_snapshot_byte_limit,"bounded bytes encode");if(!encoded.ok)return 1;
    auto decoded=decode_continuation_snapshot(encoded.value);check(decoded.ok,"typed bytes round trip");if(decoded.ok){const auto& value=decoded.value;check(value.source_identity.world_id==source.source_identity.world_id&&value.source_identity.execution_epoch==7&&value.source_identity.revision==8&&value.source_identity.tick==8,"source identity preserved");check(value.actor_id==source.actor_id&&value.action_id==source.action_id&&value.action_kind==source.action_kind&&value.wallet_id==source.wallet_id,"scope preserved");check(value.action_start_tick==1&&value.started_tick==2&&value.max_half_days==8&&value.max_gross_outflow==100&&value.committed_half_days==3&&value.gross_outflow==40&&value.stop==RunStop::cancelled,"original budget and usage preserved");}
    auto trailing=encoded.value;trailing.push_back(0);check(!decode_continuation_snapshot(trailing).ok,"trailing bytes refused");
    bool all_truncated=true;for(std::size_t n=0;n<encoded.value.size();++n)if(decode_continuation_snapshot(Bytes(encoded.value.begin(),encoded.value.begin()+n)).ok)all_truncated=false;check(all_truncated,"every truncated prefix refused");
    auto large=encoded.value;large.resize(continuation_snapshot_byte_limit+1);check(!decode_continuation_snapshot(large).ok,"oversize frame refused");auto unknown=encoded.value;unknown[4]=2;check(!decode_continuation_snapshot(unknown).ok,"unknown version refused");
    auto bad_stop=encoded.value;bad_stop[bad_stop.size()-8]=255;check(!decode_continuation_snapshot(bad_stop).ok,"unknown stop refused");auto huge_integer=encoded.value;huge_integer[28]=128;check(!decode_continuation_snapshot(huge_integer).ok,"signed integer overflow refused");
    for(const auto& kind:{"travel","long_rest","study_skill"}){auto value=sample();value.action_kind=kind;check(encode_continuation_snapshot(value).ok,"supported action accepted");}
    auto invalid=sample();invalid.action_kind="military_train";check(!encode_continuation_snapshot(invalid).ok,"new action cannot obtain scope");invalid=sample();invalid.max_gross_outflow=-1;check(!encode_continuation_snapshot(invalid).ok,"negative money refused");invalid=sample();invalid.gross_outflow=101;check(!encode_continuation_snapshot(invalid).ok,"spent beyond approved money refused");invalid=sample();invalid.committed_half_days=9;check(!encode_continuation_snapshot(invalid).ok,"used time beyond approval refused");invalid=sample();invalid.started_tick=7;check(!encode_continuation_snapshot(invalid).ok,"impossible committed chronology refused");invalid=sample();invalid.action_start_tick=3;check(!encode_continuation_snapshot(invalid).ok,"action younger than run refused");invalid=sample();invalid.actor_id=StableId(257,'a');check(!encode_continuation_snapshot(invalid).ok,"oversize actor ID refused");invalid=sample();invalid.actor_id="actor"+StableId(1,'\0')+"player";check(!encode_continuation_snapshot(invalid).ok,"embedded NUL refused");invalid=sample();invalid.max_gross_outflow=0;invalid.gross_outflow=0;check(encode_continuation_snapshot(invalid).ok,"zero-cost approval valid");
    std::cout<<"CONTINUATION_SNAPSHOT "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
