// 新海军段只在独立codec内回环；不将本组件当成已注册World版本。
#define main naval_asset_rule_cases
#include "naval_assets_test.cpp"
#undef main
#include "resident/domains/military/naval_asset_codec.hpp"
#include "resident/storage/common_codec.hpp"
// typed银行与审计JSON必须表达同一实船绑定，坏候选不修复、不吞未知结果。
int main(){try{
 Fixture f;auto formed=take(commit_naval_formation(f.state,f.catalog,f.observed,f.request,"fleet:4","naval_action:5"));auto json=naval_asset_codec::encode(formed);auto decoded=take(naval_asset_codec::decode(json,f.catalog));check(decoded.vessels.size()==2&&decoded.fleets[0].assignments[0].extra_inventory_ids==std::vector<StableId>{"army_store"},"JSON preserves original real container bindings");
 CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(naval_asset_codec::visit(writer,decoded,f.catalog).ok&&writer.finish().ok,"independent typed naval asset write");NavalAssetState restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));check(naval_asset_codec::visit(reader,restored,f.catalog).ok&&reader.finish().ok,"independent typed naval asset read");CodecVisitor::Buffer again;CodecVisitor rewrite(again);check(naval_asset_codec::visit(rewrite,restored,f.catalog).ok&&rewrite.finish().ok&&bytes==again,"typed round trip preserves complete original bytes");
 const auto before=take(render_boundary_json(naval_asset_codec::encode(restored)));check(before==take(render_boundary_json(json)),"typed/JSON keep same original field order and optional anchor");
 auto truncated=bytes;truncated.pop_back();NavalAssetState damaged;CodecVisitor broken(static_cast<const CodecVisitor::Buffer&>(truncated));check(!naval_asset_codec::visit(broken,damaged,f.catalog).ok||!broken.finish().ok,"truncated typed naval state rejects");
 auto invalid=formed;invalid.vessels[0].field_order={"id"};CodecVisitor::Buffer bad;CodecVisitor missing(bad);check(!naval_asset_codec::visit(missing,invalid,f.catalog).ok,"typed field order cannot hide owned properties");
 auto extra=json;extra.as<BoundaryValue::Dict>()->emplace_back("sinking_result",BoundaryValue{"all_dead"});check(!naval_asset_codec::decode(extra,f.catalog).ok,"unsupported sinking outcomes cannot enter JSON state");
 auto free=f.state;free.vessels[0].berth_id.clear();free.vessels[0].afloat_location=WildernessLocation{"river","north",{40,60}};auto free_json=naval_asset_codec::encode(free);auto anchored=take(naval_asset_codec::decode(free_json,f.catalog));check(anchored.vessels[0].afloat_location&&std::holds_alternative<WildernessLocation>(*anchored.vessels[0].afloat_location),"disabled/free hull can retain one true stationary world anchor");
 auto invalid_profile=f.catalog;invalid_profile.profile="sinking_v1";check(!naval_asset_codec::decode(json,invalid_profile).ok,"new segment cannot imply an unsupported outcome policy");
 std::cout<<"NAVAL_ASSET_CODEC_CHECKS="<<checks<<" FAILURES=0 SCOPE=COMPONENT\n";return 0;
 }catch(const std::exception& e){std::cerr<<"NAVAL_ASSET_CODEC_FAIL "<<e.what()<<'\n';return 1;}}
