// 常驻地形纯C++回归；固定答案取自Godot 4.7.2原world_terrain.gd，不伪造移动或预演行程。
#include "resident/geometry/terrain.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
namespace terrain=hanmo::resident::terrain;
using hanmo::resident::Integer;
using hanmo::resident::PointI;
using terrain::Mode;
int checks=0,failures=0;

// 独立行为失败保留标签并继续余下断言，最终统一退出非零。
void check(bool condition,const char* label) {
    ++checks;
    if(!condition) { ++failures;std::cerr<<"FAIL: "<<label<<'\n'; }
}

// 只创建拥有型测试地形；修改某格不影响其它格或正式目录。
terrain::TerrainMap plain_map(Integer width,Integer height,Integer size) {
    terrain::TerrainMap map;
    map.width=width;map.height=height;map.cell_size=size;
    map.cells.resize(static_cast<std::size_t>((width/size)*(height/size)));
    return map;
}

// 移植原测试区域fixture，保留河流、北山关、中段渡口和北桥全部格位。
terrain::TerrainMap regional_fixture() {
    auto map=plain_map(1000,600,20);map.id="map.terrain_fixture";
    for(Integer y=0;y<30;++y) {
        const Integer left=(y<4 || y==14)?22:23;
        const Integer right=y==14?27:(y>=16?28:26);
        for(Integer x=0;x<50;++x) {
            auto& cell=map.cells[static_cast<std::size_t>(y*50+x)];
            if(x>=left && x<=right) cell.terrain="water";
            else if((y>=4 && y<=9 && x<left) || (y>=2 && y<=9 && x>=33)) cell.terrain="mountain";
            else if((y>=16 && x<9) || (y>=19 && x>=36)) cell.terrain="forest";
            else if(y==14 || x==9 || (y==2 && x>=9 && x<=30)) cell.terrain="road";
        }
    }
    for(std::size_t y=4;y<10;++y) map.cells[y*50+9]={"mountain","pass","gate.north"};
    for(std::size_t x=22;x<28;++x) map.cells[14*50+x]={"water","ford","crossing.main"};
    for(std::size_t x=22;x<27;++x) map.cells[2*50+x]={"water","bridge","bridge.north"};
    for(const PointI point:std::vector<PointI>{{30,3},{30,4},{30,5},{31,6},{32,7},{32,8},{31,9},{31,10},{30,11},{30,12},{30,13}})
        map.cells[static_cast<std::size_t>(point.y*50+point.x)].terrain="road";
    return map;
}

// 第二尺寸的独立河道fixture，避免固定区域或据点特例通过回归。
terrain::TerrainMap crossing_fixture() {
    auto map=plain_map(160,120,20);
    for(std::size_t y=0;y<6;++y) map.cells[y*8+3].terrain="water";
    for(std::size_t x=0;x<8;++x) if(x!=3) map.cells[8+x].terrain="road";
    map.cells[8+3]={"water","ford","ford.closed"};
    map.cells[4*8+3]={"water","bridge","bridge.closed"};
    return map;
}

// 不借助operator==修改公共坐标合同，只核返回坐标的精确int64值。
bool points_equal(const std::vector<PointI>& actual,const std::vector<PointI>& expected) {
    if(actual.size()!=expected.size()) return false;
    for(std::size_t i=0;i<actual.size();++i) if(actual[i].x!=expected[i].x || actual[i].y!=expected[i].y) return false;
    return true;
}

// 每条返回路线同时核Godot固定答案及同源逐段复核，防止只对总成本凑数。
void route_is(const terrain::TerrainMap& map,PointI from,PointI to,const std::vector<PointI>& points,const std::vector<Integer>& costs,Integer total,Mode mode=Mode::foot,const terrain::Flags& flags={}) {
    const auto result=terrain::plan(map,from,to,mode,flags);
    check(result.ok,"规划返回成功");
    if(!result.ok) return;
    check(points_equal(result.value.points,points),"折线路径与原Godot固定答案一致");
    check(result.value.segment_costs==costs && result.value.cost==total,"逐段及总成本与原Godot一致");
    Integer sum=0;bool valid=true;
    for(std::size_t i=1;i<result.value.points.size();++i) {
        const auto part=terrain::segment_cost(map,result.value.points[i-1],result.value.points[i],mode,flags);
        if(!part.ok || part.value.cost!=costs[i-1]) valid=false;
        if(part.ok) sum+=part.value.cost;
    }
    check(valid && sum==total,"每段精确成本之和复核通过");
    const auto prepared=terrain::plan_prepared(map,from,to,mode,flags);
    check(prepared.ok && points_equal(prepared.value.points,points) && prepared.value.segment_costs==costs && prepared.value.cost==total,"预备目录入口保持相同路线与成本");
}

// binary64逐位核对成本，而非epsilon近似，能捕获正反向不同的最后一位。
std::uint64_t bits(double value) {
    std::uint64_t result=0;
    std::memcpy(&result,&value,sizeof(result));
    return result;
}

// 覆盖类型化边界仍可表示的原错误；JSON动态类型错误由外层目录解码负责。
void validation_checks() {
    const auto base=plain_map(40,40,20);
    check(terrain::validate_map(base).ok,"有效逐行地图");
    auto map=base;map.width=41;
    check(terrain::validate_map(map).error.original.code=="InvalidMapDimensions","非整除地图尺寸");
    map=base;map.cell_size=1;
    check(terrain::validate_map(map).error.original.code=="InvalidMapDimensions","格距小于二");
    map=base;map.height=std::numeric_limits<Integer>::max();
    check(terrain::validate_map(map).error.original.code=="InvalidMapDimensions","极大int64在乘法前拒绝");
    map=base;map.cells.pop_back();
    check(terrain::validate_map(map).error.original.code=="InvalidMapCells","缺格拒绝");
    map=plain_map(514,512,2);
    check(terrain::validate_map(map).error.original.code=="InvalidMapCells","超过65536格拒绝");
    map=base;map.cells[0].terrain="ford";
    check(terrain::validate_map(map).error.original.code=="InvalidTerrain","渡口不能替代地形");
    map=base;map.cells[0].crossing="";
    check(terrain::validate_map(map).error.original.code=="InvalidCrossing","存在但空的crossing不是缺省");
    map=base;map.cells[0].crossing="bridge";
    check(terrain::validate_map(map).error.original.code=="InvalidCrossingTerrain","陆地不能架水桥");
    map=base;map.cells[0]={"water","pass",{}};
    check(terrain::validate_map(map).error.original.code=="InvalidCrossingTerrain","水域不能伪装山关");
    map=base;map.cells[0].blocked_flag="";
    check(terrain::validate_map(map).error.original.code=="InvalidBlockedFlag","存在但空旗标拒绝");
    map.cells[0].blocked_flag=std::string(81,'a');
    check(terrain::validate_map(map).error.original.code=="InvalidBlockedFlag","非限定旗标80码点边界");
    map.cells[0].blocked_flag=std::string(32,'a')+"::"+std::string(80,'b');
    check(terrain::validate_map(map).ok,"限定旗标114字符上界");
    map.cells[0].blocked_flag="alpha::gate.bad\n";
    check(terrain::validate_map(map).error.original.code=="InvalidBlockedFlag","限定旗标尾换行拒绝");
    std::string chinese;
    for(int i=0;i<80;++i) chinese+="关";
    map.cells[0].blocked_flag=chinese;
    check(terrain::validate_map(map).ok,"中文旗标按码点而非字节计长");
    map.cells[0].blocked_flag=chinese+"关";
    check(terrain::validate_map(map).error.original.code=="InvalidBlockedFlag","中文旗标仍有80码点限制");
    map=base;map.costs[Mode::foot]["forest"]=-1;
    check(terrain::validate_map(map).error.original.code=="InvalidTravelCosts","负成本拒绝");
    map=base;map.costs[Mode::foot]["plain"]=10001;
    check(terrain::validate_map(map).error.original.code=="InvalidTravelCosts","覆盖成本上限");
    map=base;map.costs[Mode::foot]["lava"]=1;
    check(terrain::validate_map(map).error.original.code=="InvalidTravelCosts","未知成本键拒绝");
    const auto invalid=static_cast<Mode>(999);
    map=base;map.costs[invalid]["plain"]=1;
    check(terrain::validate_map(map).error.original.code=="InvalidTravelCosts","未知覆盖模式拒绝");
    check(terrain::plan(base,{1,1},{2,2},invalid).error.original.code=="InvalidTravelMode","未知旅行模式拒绝");
    map=base;map.width=41;
    check(terrain::plan(map,{1,1},{2,2},invalid).error.original.code=="InvalidMapDimensions","地图错误先于模式错误");
    check(!terrain::point_valid_prepared(base,{1,1},invalid) && terrain::segment_cost_prepared(base,{1,1},{2,2},invalid).error.original.code=="InvalidTravelMode" && terrain::segment_profile_prepared(base,{1,1},{2,2},invalid).error.original.code=="InvalidTravelMode" && terrain::plan_prepared(base,{1,1},{2,2},invalid).error.original.code=="InvalidTravelMode","所有预备入口均拒绝未知模式");
    const auto invalid_point=terrain::segment_cost(base,{-1,0},{40,0});
    check(invalid_point.error.original.code=="InvalidOrigin" && invalid_point.error.original.path.empty() && invalid_point.error.kind==hanmo::resident::ErrorKind::rule,"原点优先且保原错误空path");
    check(terrain::segment_cost(base,{0,0},{40,0}).error.original.code=="InvalidDestination","width属于图外");
    check(!terrain::point_valid(base,{0,40}) && !terrain::point_valid(base,{-1,0}),"半开地图边界");
    check(!terrain::point_valid(base,{1,1},Mode::foot,{{"",true}}),"缺省空旗标查询保持原行为");
}

// 原直线成本、模式能力和边界擦角反例均以真实纯函数执行。
void cost_checks() {
    auto map=plain_map(200,120,20);
    for(std::size_t x=0;x<10;++x) map.cells[2*10+x].terrain="road";
    for(std::size_t x:{2u,3u}) map.cells[4*10+x].terrain="forest";
    check(terrain::segment_cost(map,{10,10},{90,10}).value.cost==800,"平原距离成本");
    check(terrain::segment_cost(map,{10,50},{90,50}).value.cost==640,"道路距离成本");
    check(terrain::segment_cost(map,{10,90},{90,90}).value.cost==1120,"混合地形积分");
    check(terrain::segment_cost(map,{10,50},{90,50},Mode::army).value.cost==960,"軍团独立成本");
    map.costs[Mode::foot]["forest"]=25;
    check(terrain::segment_cost(map,{10,90},{90,90}).value.cost==1400,"作者成本覆盖");
    auto river=regional_fixture();
    check(!terrain::point_valid(river,{475,275}) && terrain::point_valid(river,{475,275},Mode::boat),"水陆模式驻留限制");
    check(terrain::segment_cost(river,{475,290},{525,290}).value.cost==1300,"渡口陆行成本");
    check(terrain::segment_cost(river,{475,290},{525,290},Mode::boat).value.cost==400,"渡口水上成本");
    river.costs[Mode::boat]["plain"]=1;river.costs[Mode::foot]["water"]=1;
    check(!terrain::point_valid(river,{175,425},Mode::boat) && !terrain::point_valid(river,{475,275}),"正成本不能覆盖水陆能力");
    river.costs[Mode::boat]["ford"]=10000;
    check(terrain::segment_cost(river,{475,290},{525,290},Mode::boat).value.cost==400,"船在渡口仍使用water成本");
    auto corner=plain_map(40,40,20);corner.cells[1].terrain="mountain";corner.cells[2].terrain="mountain";
    check(terrain::segment_cost(corner,{10,10},{30,30}).error.original.code=="SegmentBlocked" && terrain::segment_cost(corner,{30,30},{10,10}).error.original.code=="SegmentBlocked","正反斜线不能擦障碍角");
    check(!terrain::point_valid(corner,{20,20}) && terrain::plan(corner,{10,10},{30,30}).error.original.code=="NoRoute","格角不能驻留也不能穿墙");
    auto narrow=plain_map(400,20,2);
    for(std::size_t y=0;y<10;++y) narrow.cells[y*200+80].terrain="water";
    check(terrain::segment_cost(narrow,{1,1},{399,19}).error.original.code=="SegmentBlocked","长线不跳过两单位宽障碍");
    auto edge=plain_map(40,40,20);edge.cells[1].terrain="forest";
    check(terrain::segment_cost(edge,{20,1},{20,19}).value.cost==324,"格界沿线使用两侧最大成本");
    edge.cells[1].terrain="mountain";
    check(!terrain::point_valid(edge,{20,1}),"格界接触阻断格不可驻留");
    check(terrain::segment_cost(edge,{0,0},{0,39}).value.cost==390,"地图外沿不加入图外邻格");
    const auto still=terrain::segment_profile(edge,{0,0},{0,0});
    check(still.ok && still.value.cost==0 && still.value.raw_cost==0.0 && still.value.spans.empty(),"原地轨迹为空");
    auto large=plain_map(100000,100000,100000);large.costs[Mode::foot]["plain"]=10000;
    const auto profile=terrain::segment_profile(large,{0,0},{99999,99999});
    check(profile.ok && profile.value.cost==1414199421 && bits(profile.value.raw_cost)==UINT64_C(0x41d512be1f0f32bb),"大合法坐标以int64差和binary64积分");
}

// A*精确路线来自原Godot现场输出，涵盖旗标改道、权重、奇数格距与完全对称决胜。
void route_checks() {
    auto map=regional_fixture();
    route_is(map,{190,430},{830,290},{{190,430},{330,290},{830,290}},{1924,6160},8084);
    route_is(map,{190,430},{830,290},{{190,430},{190,70},{210,50},{550,50},{650,150},{650,210},{730,290},{830,290}},{4080,227,2720,1245,540,1104,800},10716,Mode::foot,{{"crossing.main",true}});
    route_is(map,{475,25},{525,575},{{475,25},{525,575}},{4419},4419,Mode::boat);
    check(terrain::plan(map,{190,430},{830,290},Mode::foot,{{"crossing.main",true},{"bridge.north",true}}).error.original.code=="NoRoute","两跨河点关闭后无路");
    check(terrain::plan(map,{190,430},{830,290},Mode::foot,{{"crossing.main",true},{"gate.north",true}}).error.original.code=="NoRoute","渡口与北关关闭后无路");
    map=crossing_fixture();
    route_is(map,{15,35},{145,35},{{15,35},{145,35}},{1400},1400);
    route_is(map,{15,35},{145,35},{{15,35},{50,70},{50,90},{90,90},{145,35}},{481,200,360,764},1805,Mode::foot,{{"ford.closed",true}});
    route_is(map,{17,89},{15,35},{{17,89},{15,35}},{531},531,Mode::foot,{{"ford.closed",true}});
    route_is(map,{17,89},{17,89},{{17,89}},{},0);
    map=plain_map(300,150,50);
    for(std::size_t x=0;x<6;++x) map.cells[x].terrain="road";
    map.cells[8].terrain="forest";map.cells[9].terrain="forest";
    map.costs[Mode::foot]={{"road",2},{"forest",120}};
    route_is(map,{25,75},{275,75},{{25,75},{25,25},{275,25},{275,75}},{300,500,300},1100);
    map=plain_map(9,9,3);map.cells[4].terrain="mountain";
    route_is(map,{0,0},{8,8},{{0,0},{4,1},{7,1},{7,4},{8,8}},{42,30,30,42},144);
    map=plain_map(100,100,20);map.cells[12].terrain="mountain";
    route_is(map,{10,50},{90,50},{{10,50},{30,30},{70,30},{90,50}},{283,400,283},966);
}

// 位级答案由原脚本PackedByteArray.encode_double导出；逆向末位不同必须保留。
void profile_checks() {
    auto map=plain_map(60,60,20);map.cells[1].terrain="forest";
    const auto forward=terrain::segment_profile(map,{5,5},{55,25});
    const auto backward=terrain::segment_profile(map,{55,25},{5,5});
    check(forward.ok && backward.ok && forward.value.cost==711 && backward.value.cost==711,"正反成本同为711");
    check(bits(forward.value.raw_cost)==UINT64_C(0x408636bbe9ce20c4) && bits(backward.value.raw_cost)==UINT64_C(0x408636bbe9ce20c3),"正反积分最后一位精确保持Godot顺序");
    check(forward.value.spans.size()==3 && backward.value.spans.size()==3,"三段地形轨迹");
    if(forward.value.spans.size()==3 && backward.value.spans.size()==3) {
        const auto& first=forward.value.spans[0];const auto& middle=forward.value.spans[1];const auto& last=forward.value.spans[2];
        check(first.from.x==5.0 && first.from.y==5.0 && first.to.x==20.0 && first.to.y==11.0 && first.rate==10 && bits(first.end_cost)==UINT64_C(0x406431c21a5e4c54),"第一格界与成本逐位核对");
        check(middle.to.x==40.0 && middle.to.y==19.0 && middle.rate==18 && bits(middle.start_cost)==UINT64_C(0x406431c21a5e4c54) && bits(middle.end_cost)==UINT64_C(0x40812a4b63368dae),"林地区间逐位核对");
        check(last.to.x==55.0 && last.to.y==25.0 && last.rate==10 && bits(last.end_cost)==bits(forward.value.raw_cost),"末区间截齐raw_cost");
        check(bits(backward.value.spans.back().end_cost)==bits(backward.value.raw_cost),"反向末区间也截齐自身raw_cost");
    }
    const auto prepared=terrain::segment_profile_prepared(map,{5,5},{55,25});
    check(prepared.ok && bits(prepared.value.raw_cost)==bits(forward.value.raw_cost) && prepared.value.spans.size()==3,"预备积分同样逐位一致");
    map=plain_map(120,20,20);
    for(std::size_t i=0;i<6;++i) map.cells[i].terrain=i<3?"road":"forest";
    const auto mixed=terrain::segment_profile(map,{10,10},{110,10});
    check(mixed.ok && mixed.value.cost==1300 && mixed.value.raw_cost==1300.0 && mixed.value.spans.size()==2,"同费率相邻格合并");
    if(mixed.value.spans.size()==2) check(mixed.value.spans[0].to.x==60.0 && mixed.value.spans[0].end_cost==400.0 && mixed.value.spans[1].rate==18,"道路到x60的成本400");
    const auto saved=terrain::plan(map,{10,10},{110,10});
    auto changed=saved;
    if(changed.ok) changed.value.points[0].x=999;
    check(terrain::plan(map,{10,10},{110,10}).value.points[0].x==10 && map.cells[0].terrain=="road","返回数组不别名输入或后续结果");
}
} // 测试目录与答案不进入生产权威状态。

// 直接g++运行的局部门，失败不给出成功标记且退出非零。
int main() {
    validation_checks();cost_checks();route_checks();profile_checks();
    std::cout<<"RESIDENT_TERRAIN_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
    return failures==0?0:1;
}
