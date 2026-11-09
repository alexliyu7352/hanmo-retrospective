// 常驻接触纯C++回归；固定答案取自原terrain_contact.gd与Godot 4.7.2，尤其保留旧数值边界行为。
#include "resident/indexes/contact_model.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace hanmo::resident;
int checks=0,failures=0;

// 每个断言独立记录，完整运行后以非零退出报告任何规则差异。
void check(bool value,const char* label) {
    ++checks;
    if(!value) { ++failures;std::cerr<<"FAIL: "<<label<<'\n'; }
}

// 不修改公共模型比较合同，仅逐坐标核对测试输出的持久整数位置。
bool point_is(PointI actual,PointI expected) { return actual.x==expected.x && actual.y==expected.y; }

// 余程长度与每个折点都必须保持，不能只核对最终目标。
bool points_are(const std::vector<PointI>& actual,const std::vector<PointI>& expected) {
    if(actual.size()!=expected.size()) return false;
    for(std::size_t i=0;i<actual.size();++i) if(!point_is(actual[i],expected[i])) return false;
    return true;
}

// 构造完整半日轨迹；目标独立于本次真实落点，允许预算用尽而尚未抵达。
TravelMotion trace(const StableId& actor,ContactPoint from,ContactPoint to,PointI target,Integer budget=100) {
    TravelMotion result;
    result.actor_id=actor;result.action_id="action:"+actor;result.map_id="map.river";result.origin="settlement.origin";
    result.params.points={target};result.params.payer_wallet_id="wallet:"+actor;result.params.money_per_tick=7;
    result.budget=budget;result.segments={{from,to,0.0,static_cast<double>(budget),0}};
    return result;
}

// 原地暂停仍保留未完成目标和原行动身份，不与无行动停驻混淆。
TravelMotion blocker(const StableId& actor,PointI point,PointI target) {
    auto result=trace(actor,{static_cast<double>(point.x),static_cast<double>(point.y)},
                      {static_cast<double>(point.x),static_cast<double>(point.y)},target);
    result.params.paused=true;
    return result;
}

// 错误须保原code/path与rule类别，并且不能让损坏输入进入求交。
void invalid_is(const TravelMotion& motion,const char* code) {
    const auto result=validate_terrain_trace(motion);
    check(!result.ok && result.error.original.code==code && result.error.original.path.empty()
          && result.error.kind==ErrorKind::rule,code);
    check(!terrain_intersection(motion,blocker("other",{60,50},{120,50})),"损坏轨迹不能产生接触");
}

// 同时核最早十亿分之一时刻、规范人物次序和两人的独立真实锚点。
void hit_is(const std::optional<ContactHit>& hit,Integer time,PointI a,PointI b,const char* label) {
    check(hit && hit->a=="a" && hit->b=="b" && hit->time_num==time && hit->time_den==1000000000
          && point_is(hit->first.anchor.point,a) && point_is(hit->second.anchor.point,b),label);
}

// 对向、同步十字、大地图及上界预算均使用原引擎现场得到的精确答案。
void geometry_checks() {
    const auto a=trace("a",{10,50},{110,50},{110,50});
    const auto b=trace("b",{110,50},{10,50},{10,50},200);
    auto hit=terrain_intersection(a,b);
    hit_is(hit,480000000,{58,50},{62,50},"不同预算仍在同一半日0.48对向接触");
    hit_is(terrain_intersection(b,a),480000000,{58,50},{62,50},"反向读取仍规范同样人物和锚点");
    if(hit) check(hit->first.actor_id=="a" && hit->second.actor_id=="b"
                  && hit->first.anchor.map_id==a.map_id && hit->first.anchor.origin==a.origin,
                  "锚点保留完整人物与原野身份");
    hit_is(terrain_intersection(a,trace("b",{10,54},{110,54},{110,54})),0,{10,50},{10,54},"平行半径四含边界");
    check(!terrain_intersection(a,trace("b",{10,55},{110,55},{110,55})),"平行相距五不能接触");
    auto other=b;other.map_id="map.other";
    check(!terrain_intersection(a,other),"不同地图绝不接触");
    other=b;other.actor_id="a";
    check(!terrain_intersection(a,other),"相同人物不与自己接触");
    other=b;other.action_id=a.action_id;
    check(!terrain_intersection(a,other),"相同行动不能拆为两人接触");
    hit_is(terrain_intersection(a,trace("b",{60,0},{60,100},{60,100})),471715729,{57,50},{60,47},"同步十字交圆时刻及各自取整锚点");
    auto early=trace("b",{60,10},{60,110},{60,110});
    early.segments={{{60,10},{60,110},0,20,0},{{60,110},{60,110},20,100,1}};
    check(validate_terrain_trace(early).ok && !terrain_intersection(a,early),"几何交叉但先后经过没有接触");
    hit_is(terrain_intersection(trace("a",{99000,99000},{99900,99900},{99900,99900}),
                                trace("b",{99900,99000},{99000,99900},{99000,99900})),
           497777778,{99448,99448},{99452,99448},"大地图双精度保持固定答案");
    hit_is(terrain_intersection(trace("a",{0,1},{100000,1},{100000,2},4294967294LL),
                                blocker("b",{99999,1},{100000,1})),
           999950000,{99995,1},{99999,1},"最大合法预算及地图坐标仍保时刻");
    auto still=blocker("a",{30,50},{110,50});still.moving=true;
    auto second=blocker("b",{32,50},{110,50});second.moving=true;
    check(!terrain_intersection(still,second),"moving元数据不能制造真实位移");
    check(terrain_intersection(a,b).has_value(),"moving默认false也不能抹去真实位移");
}

// 折点截断必须消费正确前缀，保留暂停意愿和所有付费参数，并作废本半日信用。
void anchor_checks() {
    auto a=trace("a",{10,10},{60,60},{120,60});
    a.params.points={{60,10},{60,60},{120,60}};a.params.route_index=3;a.params.travel_credit=17;
    a.params.plan_id="journey:original";a.params.fatigue_per_tick=9;a.params.met_actions={"past:action"};
    a.segments={{{10,10},{60,10},0,50,0},{{60,10},{60,60},50,100,1}};
    const auto b=blocker("b",{60,40},{60,100});
    auto hit=terrain_intersection(a,b);
    hit_is(hit,760000000,{60,36},{60,40},"转折后按累计成本求接触");
    if(hit) {
        const auto& params=hit->first.trace.params;
        check(points_are(params.points,{{60,60},{120,60}}) && params.route_index==4,"只截已完成折点并同步route_index");
        check(params.travel_credit==0 && a.params.travel_credit==17 && a.params.route_index==3,"作废信用且不改冻结来源");
        check(params.plan_id==a.params.plan_id && params.payer_wallet_id==a.params.payer_wallet_id
              && params.money_per_tick==7 && params.fatigue_per_tick==9 && params.met_actions==a.params.met_actions,
              "余程完整保留行动和资源参数");
        check(hit->second.trace.params.paused && points_are(hit->second.trace.params.points,{{60,100}}),"暂停者保持原意愿和余程");
        hit->first.trace.params.points[0].x=999;hit->first.anchor.point.x=999;
        check(points_are(a.params.points,{{60,10},{60,60},{120,60}}) && a.segments[1].from.x==60,"返回结果没有可写来源别名");
    }
    hit=terrain_intersection(a,blocker("b",{64,10},{64,80}));
    hit_is(hit,500000000,{60,10},{64,10},"恰在中途折点接触");
    if(hit) check(points_are(hit->first.trace.params.points,{{60,60},{120,60}})
                  && hit->first.trace.params.route_index==4,"中途端点只删已抵达目标");
    a.params.points={{60,10},{60,10},{60,60},{120,60}};a.segments[1].point_index=2;
    hit=terrain_intersection(a,blocker("b",{64,10},{64,80}));
    check(hit && points_are(hit->first.trace.params.points,{{60,60},{120,60}})
          && hit->first.trace.params.route_index==5,"重复同位折点全部消费且后缀顺序不变");
    const auto partial=trace("a",{10,50},{50,50},{110,50});
    hit=terrain_intersection(partial,blocker("b",{54,50},{54,100}));
    hit_is(hit,1000000000,{50,50},{54,50},"半日预算耗尽的局部端点可接触");
    if(hit) check(points_are(hit->first.trace.params.points,{{110,50}}),"预算端点保未完成最终目标");
    auto finished=trace("a",{10,50},{30,50},{30,50});
    finished.segments={{{10,50},{30,50},0,20,0},{{30,50},{30,50},20,100,1}};
    check(validate_terrain_trace(finished).ok,"合法完成尾段可通过校验");
    check(!terrain_intersection(finished,trace("b",{110,50},{10,50},{10,50})),"完成后的静止尾段不可重新挂行动");
    check(!terrain_intersection(finished,blocker("b",{34,50},{34,100})),"最后折点完成瞬间也不可产生空余程");
    check(terrain_intersection(blocker("a",{30,50},{110,50}),trace("b",{110,50},{10,50},{10,50})).has_value(),
          "未完成的暂停旅人可被遇到");
}

// 入口区间要合并双方排除范围，并保留原十亿分之一的离城推进。
void entrance_checks() {
    const auto a=trace("a",{10,50},{110,50},{110,50});
    const auto opposite=trace("b",{110,50},{10,50},{10,50});
    check(!terrain_intersection(a,opposite,{{"map.river",{{60,50}}}}),"城门内相遇排除");
    check(terrain_intersection(a,opposite,{{"map.other",{{60,50}}}}).has_value(),"别图入口不干扰本图");
    const auto b=trace("b",{13,50},{113,50},{113,50});
    hit_is(terrain_intersection(a,b,{{"map.river",{{10,50}}}}),40000001,{14,50},{17,50},"共同离城取第一次严格城外时间");
    hit_is(terrain_intersection(a,b,{{"map.river",{{10,50},{20,50}}}}),140000001,{24,50},{27,50},"多入口合并双方连续排除区间");
    hit_is(terrain_intersection(a,b,{{"map.river",{{20,50},{10,50},{10,50}}}}),140000001,{24,50},{27,50},"入口逆序和重复不改变答案");
    check(!terrain_intersection(a,b,{{"map.river",{{-1,50}}}}),"本图非法入口拒绝求交");
    check(terrain_intersection(a,b,{{"map.other",{{-1,50}}}}).has_value(),"无关地图坏入口不污染本图");
}

// 批次按量化时间与人物元组选择；重复身份只统计有效轨迹且不能读序择一。
void selection_checks() {
    const auto a=trace("a",{10,50},{110,50},{110,50});
    auto b=blocker("b",{60,50},{120,50});auto c=blocker("c",{60,50},{120,50});
    std::vector<TravelMotion> entries={a,b,c};std::vector<int> order={0,1,2};
    do {
        const auto hits=detect_terrain_contacts({entries[order[0]],entries[order[1]],entries[order[2]]});
        check(hits.size()==1 && hits[0].a=="a" && hits[0].b=="b" && hits[0].time_num==460000000,"全部六种读序仍按人物ID决胜");
    } while(std::next_permutation(order.begin(),order.end()));
    c=blocker("c",{40,50},{120,50});
    auto hits=detect_terrain_contacts({a,b,c});
    check(hits.size()==1 && hits[0].b=="c" && hits[0].time_num==260000000,"真实较早时刻优先于人物ID");
    check(detect_terrain_contacts({a,a,b}).empty(),"重复人物整组拒绝");
    auto twin=a;twin.actor_id="z";
    check(detect_terrain_contacts({a,twin,b}).empty(),"不同人物重复行动整组拒绝");
    auto bad=a;bad.budget=0;
    check(detect_terrain_contacts({a,bad,b}).size()==1,"无效重复身份不排除有效原轨迹");
    b.params.met_actions={a.action_id};
    check(!terrain_intersection(a,b),"任一方记载已遇行动即可去重");
    b.params.met_actions.clear();b.params.encounter_id="encounter:old";
    check(detect_terrain_contacts({a,b}).empty(),"已有会面不重复分配");
    b.params.encounter_id.clear();
    auto d=trace("c",{10,150},{110,150},{110,150});auto e=blocker("d",{60,150},{120,150});
    hits=detect_terrain_contacts({e,d,b,a});
    check(hits.size()==2 && hits[0].a=="a" && hits[0].b=="b" && hits[1].a=="c" && hits[1].b=="d",
          "互不占用的同刻会面按稳定人物元组输出");
    ContactHit first,second;first.a="a";first.b="z";first.time_num=1;first.time_den=2;
    second.a="b";second.b="c";second.time_num=2;second.time_den=4;
    check(contact_before(first,second) && !contact_before(second,first),"before比较等值分数后按a决胜");
    second.a="a";second.b="y";
    check(contact_before(second,first),"before同a按b决胜");
    second.time_num=1;
    check(contact_before(second,first),"before时刻优先于人物元组");
}

// 无行动停驻严格对应canonical构造；不得携带能够恢复或兑现的伪造行动参数。
void stationary_checks() {
    const WildernessLocation location{"map.river","settlement.origin",{60,50}};
    const auto stationary=stationary_terrain_trace("b",location);
    check(stationary && validate_terrain_trace(*stationary).ok,"canonical无行动停驻合法");
    check(!stationary_terrain_trace("",location),"无人物不能构造停驻");
    check(!stationary_terrain_trace("b",{"","settlement.origin",{60,50}}),"无地图不能构造停驻");
    check(!stationary_terrain_trace("b",{"map.river","",{60,50}}),"无原点不能构造停驻");
    check(!stationary_terrain_trace("b",{"map.river","settlement.origin",{-1,50}}),"图外不能构造停驻");
    if(!stationary) return;
    auto a=trace("a",{10,50},{110,50},{110,50});
    auto hit=terrain_intersection(a,*stationary);
    hit_is(hit,460000000,{56,50},{60,50},"行旅可遇到无行动停驻者");
    if(hit) check(hit->second.trace.stationary && hit->second.trace.action_id.empty()
                  && hit->second.trace.budget==1 && hit->second.trace.params.travel_credit==0
                  && points_are(hit->second.trace.params.points,{{60,50}}),"停驻锚点不制造行动或旅行预算");
    const auto another=stationary_terrain_trace("c",{"map.river","settlement.origin",{90,50}});
    const auto hits=detect_terrain_contacts({a,*stationary,*another});
    check(hits.size()==1 && hits[0].b=="b","多个空action_id不被当成重复行动");
    check(!terrain_intersection(*stationary,*another),"双方停驻不产生会面");
    a.params.met_actions={""};
    check(!terrain_intersection(a,*stationary),"旧met_actions中的空行动仍按原语义去重");
    auto bad=*stationary;bad.stationary_location.reset();invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.params.travel_credit=1;invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.params.route_index=2;invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.params.paused=true;invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.params.encounter_id="encounter:unexpected";invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.params.met_actions={"unexpected"};invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.action_id="action:unexpected";invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.segments[0].to.x=61;invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.segments[0].from.x=60.0000000000005;invalid_is(bad,"InvalidStationaryTerrainTrace");
    bad=*stationary;bad.segments[0].to.y=50.0000000000005;invalid_is(bad,"InvalidStationaryTerrainTrace");
}

// typed合同之外仍逐项验证范围、连续性和错误优先级，不容许不完整时间覆盖。
void validation_checks() {
    const auto source=trace("a",{10,50},{110,50},{110,50});
    check(validate_terrain_trace(source).ok,"标准轨迹合法");
    auto bad=source;bad.actor_id.clear();bad.budget=0;invalid_is(bad,"InvalidTerrainTraceIdentity");
    bad=source;bad.map_id.clear();invalid_is(bad,"InvalidTerrainTraceIdentity");
    bad=source;bad.origin.clear();invalid_is(bad,"InvalidTerrainTraceIdentity");
    bad=source;bad.action_id.clear();invalid_is(bad,"InvalidTerrainTraceIdentity");
    bad=source;bad.budget=0;bad.params.points.clear();invalid_is(bad,"InvalidTerrainTraceBudget");
    bad=source;bad.budget=4294967295LL;invalid_is(bad,"InvalidTerrainTraceBudget");
    bad=source;bad.params.points.clear();bad.segments.clear();invalid_is(bad,"InvalidTerrainTracePoints");
    bad=source;bad.params.points[0].x=-1;invalid_is(bad,"InvalidTerrainTracePoints");
    bad=source;bad.params.points[0].y=100001;invalid_is(bad,"InvalidTerrainTracePoints");
    bad=source;bad.params.route_index=0;bad.params.travel_credit=-1;invalid_is(bad,"InvalidTerrainTracePointIndex");
    bad=source;bad.params.travel_credit=-1;invalid_is(bad,"InvalidTerrainTraceBudget");
    bad=source;bad.params.travel_credit=2147483648LL;invalid_is(bad,"InvalidTerrainTraceBudget");
    bad=source;bad.params.travel_credit=2147483647LL;check(validate_terrain_trace(bad).ok,"信用int32上界合法");
    bad=source;bad.segments.clear();invalid_is(bad,"InvalidTerrainTraceSegments");
    bad=source;bad.segments[0].from.x=std::numeric_limits<double>::quiet_NaN();invalid_is(bad,"InvalidTerrainTraceSegments");
    bad=source;bad.segments[0].to.y=std::numeric_limits<double>::infinity();invalid_is(bad,"InvalidTerrainTraceSegments");
    bad=source;bad.segments[0].from.x=-0.01;invalid_is(bad,"InvalidTerrainTraceSegments");
    bad=source;bad.segments[0].start_cost=1;invalid_is(bad,"InvalidTerrainTraceTime");
    bad=source;bad.segments[0].end_cost=0;invalid_is(bad,"InvalidTerrainTraceTime");
    bad=source;bad.segments[0].end_cost=101;invalid_is(bad,"InvalidTerrainTraceTime");
    bad=source;bad.segments[0].end_cost=std::numeric_limits<double>::infinity();invalid_is(bad,"InvalidTerrainTraceTime");
    bad=source;bad.segments[0].start_cost=std::numeric_limits<double>::quiet_NaN();invalid_is(bad,"InvalidTerrainTraceTime");
    bad=source;bad.segments[0].end_cost=99;invalid_is(bad,"IncompleteTerrainTrace");
    bad=source;bad.segments[0].point_index=-1;invalid_is(bad,"InvalidTerrainTracePointIndex");
    bad=source;bad.segments[0].point_index=2;invalid_is(bad,"InvalidTerrainTracePointIndex");
    bad=source;bad.segments[0].point_index=1;invalid_is(bad,"InvalidTerrainTraceFinishedTail");
    bad=source;bad.segments={{{10,50},{60,50},0,50,0},{{61,50},{110,50},50,100,0}};
    invalid_is(bad,"DiscontinuousTerrainTrace");
    bad=source;bad.params.points={{60,50},{110,50}};
    bad.segments={{{10,50},{60,50},0,50,1},{{60,50},{110,50},50,100,0}};
    invalid_is(bad,"InvalidTerrainTracePointIndex");
    bad=source;bad.segments={{{10,50},{110,50},0,50,0},{{110,50},{111,50},50,100,1}};
    invalid_is(bad,"InvalidTerrainTraceFinishedTail");
    bad=source;bad.segments={{{10,50},{100,50},0,50,0},{{100,50},{100,50},50,100,1}};
    invalid_is(bad,"InvalidTerrainTraceFinishedTail");
}

// 临时小数格界和分段成本不能提前取整；旧时间epsilon也不能被空间包围盒过滤掉。
void fractional_checks() {
    auto a=trace("a",{5,5},{55,25},{105,45},711);
    a.segments={{{5,5},{20,11},0.0,161.554944214035,0},
                {{20,11},{40,19},161.554944214035,549.286810327719,0},
                {{40,19},{55,25},549.286810327719,710.841754541754,0},
                {{55,25},{55,25},710.841754541754,711.0,0}};
    check(validate_terrain_trace(a).ok,"小数成本与同目标跨格片段合法");
    hit_is(terrain_intersection(a,blocker("b",{35,17},{100,17})),534956180,{31,16},{35,17},"小数跨度保留原Godot时刻和独立锚点");
    a.segments={{{5.25,5.5},{20.125,11.3125},0.0,161.554944214035,0},
                {{20.125,11.3125},{40.625,19.125},161.554944214035,549.286810327719,0},
                {{40.625,19.125},{55.75,25.25},549.286810327719,711.0,0}};
    check(validate_terrain_trace(a).ok,"非整数临时坐标也合法");
    hit_is(terrain_intersection(a,blocker("b",{35,17},{100,17})),523656708,{31,16},{35,17},"分段小数坐标不能先量化成持久整数");
    const auto tangent=trace("a",{10,50},{90,50},{100,50});
    hit_is(terrain_intersection(tangent,trace("b",{50,54.00000000000005},{50,54.00000000000005},{100,55})),
           500000000,{50,50},{50,54},"半径平方负epsilon内的切线仍按原规则接触");
    check(!terrain_intersection(tangent,trace("b",{50,54.000000000001},{50,54.000000000001},{100,55})),
          "超出半径平方epsilon的近切线仍拒绝");
    a=trace("a",{10,50},{20,50},{100,50});
    a.segments={{{10,50},{20,50},0.0,1e-11,0},{{20,50},{20,50},1e-11,100,0}};
    const auto b=blocker("b",{25,50},{100,50});
    check(validate_terrain_trace(a).ok,"极短但有限合法片段保留旧输入接受范围");
    hit_is(terrain_intersection(a,b),0,{20,50},{25,50},"原时间epsilon允许短段外推后夹回边界");
    Integer candidates=-1,exact=-1;
    const auto hits=detect_terrain_contacts({a,b},{},&candidates,&exact);
    check(hits.size()==1 && hits[0].time_num==0 && point_is(hits[0].first.anchor.point,{20,50})
          && point_is(hits[0].second.anchor.point,{25,50}),"批次不能用固定空间padding删除旧精确求交结果");
    check(candidates==1 && exact==1,"真实短段候选必须进入精确求交");
    const auto none=detect_terrain_contacts({}, {},&candidates,&exact);
    check(none.empty() && candidates==0 && exact==0,"空批次重置统计而不泄漏上次结果");
}
// 高密度同刻候选只改变内部拷贝时点；最终pair、余程和读版本必须深拥有且与读序无关。
void dense_selection_checks() {
    std::vector<TravelMotion> sources;
    std::vector<StableId> ids;
    for(int i=0;i<64;++i) {
        const auto id="actor.dense."+std::to_string(i);ids.push_back(id);
        auto t=trace(id,{50,50},{100,50},{150,50});
        t.params.route_index=3;t.params.travel_credit=17;t.params.met_actions={"action:old"};
        t.read_versions={{{ResourceKey::Kind::wallet,t.params.payer_wallet_id},7}};
        sources.push_back(std::move(t));
    }
    std::sort(ids.begin(),ids.end());Integer candidates=0,exact=0;
    auto hits=detect_terrain_contacts(sources,{},&candidates,&exact);
    check(hits.size()==32&&candidates==2016&&exact==2016,"所有同刻pair仍精算，只选不争用的32对");
    bool ordered=hits.size()==32,owned=hits.size()==32;
    for(std::size_t i=0;i<hits.size();++i) {
        const auto& hit=hits[i];ordered=ordered&&hit.a==ids[2*i]&&hit.b==ids[2*i+1]&&hit.time_num==0;
        for(const auto* endpoint:{&hit.first,&hit.second})owned=owned&&endpoint->trace.params.route_index==3&&endpoint->trace.params.travel_credit==0&&endpoint->trace.params.met_actions==std::vector<StableId>{"action:old"}&&endpoint->trace.read_versions.size()==1&&endpoint->trace.read_versions.front().version==7;
    }
    check(ordered,"高密度候选排序保持原人物稳定平局顺序");
    check(owned,"仅被选中结果仍完整保留余程参数与资源读版本");
    std::reverse(sources.begin(),sources.end());const auto reversed=detect_terrain_contacts(sources);
    bool unchanged=reversed.size()==hits.size();for(std::size_t i=0;i<reversed.size()&&i<hits.size();++i)unchanged=unchanged&&reversed[i].a==hits[i].a&&reversed[i].b==hits[i].b;
    check(unchanged,"借用型候选不使最终选择依赖输入读序");
    if(!hits.empty()){hits[0].first.trace.params.points.front().x=999;hits[0].first.trace.read_versions.front().version=999;}
    bool pure=true;for(const auto& t:sources)pure=pure&&t.params.points.front().x==150&&t.params.travel_credit==17&&t.read_versions.front().version==7;
    check(pure,"最终修改不会透过候选借用改动输入权威轨迹");
}
} // 纯函数测试不安装encounter，不触碰世界或玩家存档。

// 独立局部门只链接接触几何，不能用事务或JSON兼容路径替代纯数值验证。
int main() {
    geometry_checks();anchor_checks();entrance_checks();selection_checks();stationary_checks();validation_checks();fractional_checks();dense_selection_checks();
    std::cout<<"RESIDENT_CONTACT_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
    return failures==0?0:1;
}
