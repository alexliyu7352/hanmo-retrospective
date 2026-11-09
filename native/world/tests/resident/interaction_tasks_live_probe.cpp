// 正式NativeBoundarySession链只提交合法命令；目录与NativeInit请求原件由调用者传入，不改世界或伪造已知任务。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;namespace fs=std::filesystem;
// 生产库探针使用异常检查，Release构建也不能跳过真实失败。
void require(bool value,const StableId& why){if(!value)throw std::runtime_error(why);}
template<class T>T must(Outcome<T> value,const StableId& why){require(value.ok,why+": "+value.error.original.code);return std::move(value.value);}
// 原件读取与输出仅是测试附件，不作为会话中的任意世界修改入口。
V read(const fs::path& path){std::ifstream input(path);require(bool(input),"read "+path.string());std::ostringstream text;text<<input.rdbuf();return must(parse_boundary_json(text.str()),"parse input");}
StableId json(const V& value){return must(render_boundary_json(value),"encode output");}
bool ok(const V& value){const auto* entry=value.find("ok");return entry&&entry->as<bool>()&&*entry->as<bool>();}
void write(const fs::path& path,const V& value){std::ofstream output(path);require(bool(output),"write "+path.string());output<<json(value)<<'\n';}
// 所有有限查询使用刚读到的完整当前来源；不导出全世界替代任务页面。
Outcome<LiveOperationResult> query(ResidentReplaySession& world,const StableId& id,const V& args=V{V::Dict{}}){LiveOperation op;op.source=must(world.identity(),"query source");op.payload=boundary::obj({{"query_id",V{id}},{"args",args}});return world.execute_live(op);}
// 请求序号只读正常command_context，不编辑或复制世界状态。
V envelope(ResidentReplaySession& world,const StableId& kind,const V& params){const auto context=must(query(world,"command_context"),"command context").result;return boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",boundary::field(context,"actor_id","context")},{"request_seq",boundary::field(context,"next_request_seq","context")},{"expected_revision",boundary::field(context,"revision","context")}});}
// 提交走与薄Bridge相同的live口，普通拒绝保其原回执。
V submit(ResidentReplaySession& world,const StableId& kind,const V& params,V::Array& trace){LiveOperation op;op.kind=LiveOperationKind::command;op.source=must(world.identity(),"submit source");op.payload=envelope(world,kind,params);const auto result=must(world.execute_live(op),"submit "+kind).result;trace.push_back(boundary::obj({{"command",op.payload},{"receipt",result}}));require(ok(result),kind+" rejected: "+json(result));return result;}
// 预览不写入收据、资金或任务；失败也必须可保存为可读原领域码。
V preview(ResidentReplaySession& world,const StableId& kind,const V& params){const auto context=must(query(world,"command_context"),"preview context").result;LiveOperation op;op.kind=LiveOperationKind::preview;op.source=must(world.identity(),"preview source");op.payload=boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",boundary::field(context,"actor_id","context")}});return must(world.execute_live(op),"preview "+kind).result;}
}
// 实际原目录无静态offers时明确走其原动态Need话题承接；不能把fixture手填为已知任务。
int main(int argc,char** argv){fs::path output;try{
    require(argc==4,"usage: interaction_tasks_live_probe PREPARED NATIVE_REQUEST OUTPUT");output=argv[3];fs::create_directories(output);const auto catalog=read(argv[1]),request=read(argv[2]);auto world=create_resident_replay_session();require(bool(world),"production factory");auto initialized=world->initialize_native(catalog,request);require(initialized.ok,"native init: "+initialized.error.original.code);V::Array trace;
    const auto blank=must(world->checkpoint(),"initial bank");auto no_meeting=query(*world,"interaction_tasks");require(!no_meeting.ok&&no_meeting.error.original.code=="NoConversation","query requires actual meeting");require(blank==must(world->checkpoint(),"no meeting query bank"),"rejected query changed world");
    submit(*world,"talk",boundary::obj({{"role_id",V{"society::role.city_supply"}}}),trace);auto empty=must(query(*world,"interaction_tasks"),"current conversation query").result;write(output/"known-before-accept.json",empty);require(boundary::array(boundary::field(empty,"rows","tasks"),"rows").empty(),"unseen tasks leaked into conversation");submit(*world,"end_interaction",V{V::Dict{}},trace);
    auto topics=must(query(*world,"regional_life_topics",boundary::obj({{"place_id",V{"society::place.courtyard"}}})),"formal need topics").result;write(output/"source-topics.json",topics);
    submit(*world,"talk",boundary::obj({{"role_id",V{"society::role.city_supply"}},{"topic_id",V{"topic.city.grain"}}}),trace);submit(*world,"dialogue_next",V{V::Dict{}},trace);submit(*world,"dialogue_choose",boundary::obj({{"option_id",V{"accept"}}}),trace);
    const auto before_quote=must(world->checkpoint(),"confirmation quote bank");auto accept_quote=preview(*world,"dialogue_confirm",V{V::Dict{}});write(output/"accept-preview.json",accept_quote);require(ok(accept_quote),"formal source acceptance unavailable");require(before_quote==must(world->checkpoint(),"confirmation quote after"),"preview mutated bank");submit(*world,"dialogue_confirm",V{V::Dict{}},trace);
    for(int count=0;count<4;++count){const auto context=must(query(*world,"action_context"),"action context").result;const auto* action=context.find("action_id");if(!action||!action->as<StableId>()||action->as<StableId>()->empty())break;submit(*world,"advance",V{V::Dict{}},trace);}
    auto interaction=must(query(*world,"interaction_view"),"after acceptance interaction").result;if(boundary::get_string(interaction,"phase","interaction")!="scene")submit(*world,"end_interaction",V{V::Dict{}},trace);
    submit(*world,"talk",boundary::obj({{"role_id",V{"society::role.city_supply"}}}),trace);const auto before_page=must(world->checkpoint(),"known task bank");auto page=must(query(*world,"interaction_tasks"),"accepted task page").result;write(output/"known-after-accept.json",page);const auto repeat=must(query(*world,"interaction_tasks"),"repeat task page").result;require(compare_boundary(page,repeat).exact()&&before_page==must(world->checkpoint(),"after task read"),"task reads changed source or world");
    const auto& rows=boundary::array(boundary::field(page,"rows","page"),"rows");require(!rows.empty(),"real accepted task missing from current conversation");std::optional<V> cancel;
    for(const auto& row:rows)for(const auto& action:boundary::array(boundary::field(row,"actions","row"),"actions")){
        const auto kind=boundary::get_string(action,"command","action");const auto& params=boundary::field(action,"params","action");auto quoted=preview(*world,kind,params);write(output/(kind+"-preview.json"),quoted);
        if(kind=="report_offer")require(!ok(quoted)&&json(quoted).find("InsufficientContractGoods")!=StableId::npos,"empty hands must not report delivery");
        if(kind=="cancel_task"){require(ok(quoted),"actual issuer can handle cancellation");cancel=params;}
    }
    require(cancel.has_value(),"real cancellation option missing");require(before_page==must(world->checkpoint(),"after action previews"),"action previews changed world");submit(*world,"cancel_task",*cancel,trace);auto cancelled=must(query(*world,"interaction_tasks"),"post-cancellation page").result;write(output/"after-cancel.json",cancelled);
    auto repeated_cancel=preview(*world,"cancel_task",*cancel);require(!ok(repeated_cancel)&&json(repeated_cancel).find("TaskNotActive")!=StableId::npos,"settled task must not cancel twice");
    write(output/"commands.json",V{std::move(trace)});write(output/"result.json",boundary::obj({{"ok",V{true}},{"registered_query",V{true}},{"real_runtime_source",V{true}},{"pure_reads",V{true}},{"empty_handed_report_rejected",V{true}},{"cancel_once",V{true}},{"tick",V{must(world->identity(),"result tick").tick}}}));std::cout<<"interaction_tasks_live_probe: production NativeBoundarySession source, query, previews and cancellation passed\n";return 0;
}catch(const std::exception& error){if(!output.empty())write(output/"failure.json",boundary::obj({{"ok",V{false}},{"error",V{StableId(error.what())}}}));std::cerr<<error.what()<<'\n';return 1;}}
