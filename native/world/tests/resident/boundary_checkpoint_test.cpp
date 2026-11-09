// 独立外封装测试包含旧world许可隔离、域缺失、损坏及目录不匹配。
#include "resident/checkpoint/checkpoint_envelope.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main() {
    CheckpointEnvelope source; source.identity.world_id[0]=7; source.identity.revision=9; source.identity.tick=4;
    source.identity.catalog.content_id="alpha"; source.identity.catalog.content_digest="digest"; source.identity.catalog.artifact_sha256="actual-expanded-artifact";
    source.sections={{"common",1,{1,2,3}},{"travel",1,{4,5}}};
    auto encoded=encode_checkpoint(source); assert(encoded.ok);
    const std::vector<CheckpointDomain> expected={{"common",1},{"travel",1}};
    auto decoded=decode_checkpoint(encoded.value,source.identity.catalog,expected);
    assert(decoded.ok&&decoded.value.identity.world_id==source.identity.world_id&&decoded.value.identity.tick==4);
    assert(decoded.value.sections[1].payload==source.sections[1].payload);
    auto wrong=source.identity.catalog; wrong.content_digest="another";
    assert(!decode_checkpoint(encoded.value,wrong,expected).ok);
    wrong=source.identity.catalog; wrong.artifact_sha256="different-synthetic-catalog-same-legacy-digest";
    assert(!decode_checkpoint(encoded.value,wrong,expected).ok);
    assert(!decode_checkpoint(encoded.value,source.identity.catalog,{{"common",1}}).ok);
    assert(!decode_checkpoint(encoded.value,source.identity.catalog,{{"travel",1},{"common",1}}).ok);
    source.sections.push_back(source.sections[0]); assert(!encode_checkpoint(source).ok);
    auto damaged=encoded.value; damaged[damaged.size()/2]^=1; assert(!decode_checkpoint(damaged,source.identity.catalog,expected).ok);
    for(std::size_t i=0;i<encoded.value.size();++i) { const CodecVisitor::Buffer truncated(encoded.value.begin(),encoded.value.begin()+i); assert(!decode_checkpoint(truncated,source.identity.catalog,expected).ok); }
    std::cout<<"checkpoint envelope checks passed\n";
}
