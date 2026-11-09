// 用真实公开API核对纯路线缓存的生命周期；只提交初始travel，不执行半日或压力矩阵。
#include "hanmo/resident_world_api.hpp"
#include "resident/kernel/world_codec.hpp"
#include <fstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 失败保留具体标签，缓存统计不能替代完整世界或回执相等。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 冷边界失败立即结束当前用例，不使用缺省世界继续比较。
template<class T> T take(Outcome<T> value){if(!value.ok)throw std::runtime_error(value.error.original.code+":"+value.error.original.path);return std::move(value.value);}
// 仅读冻结oracle输入，不改写其目录、状态或索引文件。
BoundaryValue read(const StableId& path){std::ifstream file(path);return take(parse_boundary_json(std::string(std::istreambuf_iterator<char>(file),{})));}
// 固定逻辑world ID让两个独立安装的检查点字节可直接比较；epoch仍由公开API生成。
InitialWorldSeed seed(const BoundaryValue& source,const PreparedCatalog& catalog){auto value=take(decode_world_seed(source,catalog));value.common.identity.world_id.fill(17);return value;}
// 每次使用该world当前epoch，避免旧回调错误掩盖实际缓存查询。
CommandEnvelope command(ResidentWorld& world,const CommandPayload& payload){auto id=take(world.identity());CommandEnvelope value;value.world_id=id.world_id;value.execution_epoch=id.execution_epoch;value.actor_id="actor.player";value.request_seq=1;value.expected_revision=id.revision;value.payload=std::make_shared<const CommandPayload>(payload);return value;}
// 共同冷codec核对原字典顺序及整数类型；不使用摘要宽松相等。
void equal(const BoundaryValue& a,const BoundaryValue& b,const char* label){check(compare_boundary(a,b).exact(),label);}
}
// 真实提交触发中央WorldBundle复制；恢复分别覆盖共享旧目录与独立重新准备的相同目录。
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const StableId root=argv[1];auto index=read(root+"/oracle-index.json");auto input=read(root+"/expanded-catalog.json");auto source=read(root+"/"+boundary::get_string(*index.find("initial_state"),"file","initial"));
    auto prepared=take(prepare_catalog(input,source));auto cache=prepared->travel->route_cache;
    auto payload=take(decode_resident_command("travel",*boundary::array(*index.find("commands"),"commands").front().find("command")->find("params")));
    auto world=std::make_unique<ResidentWorld>();check(world->install(prepared,seed(source,*prepared)).ok,"actual seed installed");
    auto initial=take(world->checkpoint());check(cache->stats().entries==0,"install and checkpoint do not prepare routes");
    auto preview=world->preview(command(*world,payload));check(preview.ok,"actual travel preview");
    check(cache->stats().misses>0&&cache->stats().entries>0,"real preview warms only pure geometry");
    check(take(world->checkpoint())==initial,"preview cache writes leave checkpoint bytes unchanged");
    const auto hits=cache->stats().hits;auto receipt=world->submit(command(*world,payload));check(receipt.ok,"actual clone-backed travel commit");
    check(cache->stats().hits>hits,"commit reuses immutable catalog geometry across candidate clone");
    auto committed=take(world->checkpoint());

    auto uncached_catalog=std::make_shared<PreparedCatalog>(*prepared);auto uncached_travel=std::make_shared<TravelCatalog>(*prepared->travel);uncached_travel->route_cache.reset();uncached_catalog->travel=uncached_travel;
    ResidentWorld uncached;check(uncached.install(uncached_catalog,seed(source,*uncached_catalog)).ok,"uncached control uses same actual seed");
    auto uncached_preview=uncached.preview(command(uncached,payload));
    equal(take(encode_resident_preview(preview,0)),take(encode_resident_preview(uncached_preview,0)),"cached and uncached preview exactly equal");
    auto uncached_receipt=uncached.submit(command(uncached,payload));
    equal(take(encode_resident_receipt(receipt)),take(encode_resident_receipt(uncached_receipt)),"cached and uncached receipt exactly equal");
    check(take(uncached.checkpoint())==committed,"cached and uncached committed world exactly equal");

    ResidentWorld restored;const auto warm=cache->stats();check(restored.restore(initial,prepared).ok,"restore with same warmed catalog");
    check(cache->stats().hits==warm.hits&&cache->stats().misses==warm.misses,"restore does not run or serialize route planner");
    auto restored_preview=restored.preview(command(restored,payload));
    check(cache->stats().hits>warm.hits,"restored world may reuse same immutable catalog cache");
    equal(take(encode_resident_preview(preview,0)),take(encode_resident_preview(restored_preview,0)),"warmed restore keeps exact quote");
    check(take(restored.checkpoint())==initial,"restored warm preview leaves exact source checkpoint");

    auto fresh=take(prepare_catalog(input,source));check(fresh->travel->route_cache!=cache&&fresh->travel->route_cache->stats().entries==0,"reprepared identical catalog starts with independent empty cache");
    check(restored.restore(initial,fresh).ok,"restore accepts independently prepared same catalog");
    auto fresh_preview=restored.preview(command(restored,payload));check(fresh_preview.ok&&fresh->travel->route_cache->stats().misses>0,"fresh restore calculates its own geometry");
    equal(take(encode_resident_preview(preview,0)),take(encode_resident_preview(fresh_preview,0)),"fresh and warm restored quote exactly equal");
    check(take(restored.checkpoint())==initial,"fresh cache state is absent from checkpoint");

    auto changed_input=input;*changed_input.find("content_version")=BoundaryValue{StableId{"cache-lifecycle-changed"}};
    auto changed=take(prepare_catalog(changed_input,source));auto before=take(restored.identity());auto rejected=restored.restore(initial,changed);
    check(!rejected.ok&&rejected.error.original.code=="CheckpointCatalogMismatch","changed catalog identity rejects old checkpoint");
    auto after=take(restored.identity());check(before.world_id==after.world_id&&before.execution_epoch==after.execution_epoch,"catalog mismatch preserves running identity");
    check(take(restored.checkpoint())==initial,"catalog mismatch preserves complete committed world");
    check(changed->travel->route_cache->stats().entries==0,"rejected catalog keeps its own empty cache");

    // 同名同版本仍不能把改写地形当旧目录；身份由中央实际准备内容计算，不靠缓存key补救。
    auto changed_geometry=input;auto* map=changed_geometry.find("data")->find("world_maps")->find(prepared->travel->world_map_id);
    auto* costs=map->find("costs");if(!costs){map->as<BoundaryValue::Dict>()->emplace_back("costs",BoundaryValue{BoundaryValue::Dict{}});costs=map->find("costs");}
    auto* foot=costs->find("foot");if(!foot){costs->as<BoundaryValue::Dict>()->emplace_back("foot",BoundaryValue{BoundaryValue::Dict{}});foot=costs->find("foot");}
    if(auto* plain=foot->find("plain"))*plain=BoundaryValue{Integer{20}};else foot->as<BoundaryValue::Dict>()->emplace_back("plain",BoundaryValue{Integer{20}});
    auto replacement=take(prepare_catalog(changed_geometry,source));
    check(replacement->identity.content_digest==fresh->identity.content_digest&&replacement->identity.artifact_sha256!=fresh->identity.artifact_sha256,"changed geometry gets distinct prepared identity despite unchanged declared digest");
    auto geometry_rejected=restored.restore(initial,replacement);
    check(!geometry_rejected.ok&&geometry_rejected.error.original.code=="CheckpointCatalogMismatch","changed same-name geometry rejects old checkpoint");
    check(take(restored.checkpoint())==initial&&replacement->travel->route_cache->stats().entries==0,"geometry mismatch retains old world and never warms rejected cache");

    std::weak_ptr<const PreparedCatalog> old_catalog=prepared;std::weak_ptr<terrain::RouteCache> old_cache=cache;
    world.reset();prepared.reset();cache.reset();check(old_catalog.expired()&&old_cache.expired(),"replacement worlds do not retain obsolete prepared catalog or cache");
    equal(take(encode_resident_preview(preview,0)),take(encode_resident_preview(fresh_preview,0)),"returned quote owns geometry after original catalog destruction");
}catch(const std::exception& e){++failures;std::cerr<<"FAIL exception "<<e.what()<<'\n';}
    std::cout<<"TRAVEL_CACHE_LIFECYCLE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
