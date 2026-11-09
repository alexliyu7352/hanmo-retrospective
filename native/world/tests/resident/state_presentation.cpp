// 有限名字/文本端口只读同一候选；覆盖原身份回退、Unicode长度和冷目录所有权。
#include "resident/views/common_presentation.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace hanmo::resident;

// 构造最小真实冷目录输入；不借第二份运行人物状态实现查询。
BoundaryValue catalog_input(){
    auto parsed=parse_boundary_json(R"({"items":{},"places":{},"roles":{},"organizations":{},"wallets":{},"inventories":{},"actors":{"actor.a":{"name_key":"name.a"},"actor.b":{"name_key":"name.b"},"actor.unnamed":{},"actor.empty":{"name_key":"name.empty"}},"active_static_actor_ids":["actor.a"],"texts":{"name.a":"甲","name.b":"乙","name.empty":"","line":"第一句\n第二句","":"空键作者文本"}})");
    assert(parsed.ok);return std::move(parsed.value);
}

// 每个名字结果必须成功且深复制；合法空作者文本不能误判为缺失。
void expect_name(const PresentationRead& read,const StableId& id,const std::string& expected){
    auto result=read.display_name(id);assert(result.ok&&result.value==expected);
}

// 可选读取一个已冻结开局，只验证名字/文本冷查询，不运行世界或重做旅程门。
void actual_initial(const char* catalog_path,const char* state_path){
    const auto read_file=[](const char* path){std::ifstream file(path);assert(file);auto value=parse_boundary_json(std::string((std::istreambuf_iterator<char>(file)),{}));assert(value.ok);return std::move(value.value);};
    auto catalog_source=read_file(catalog_path),state_source=read_file(state_path);
    const auto& data=*catalog_source.find("data");auto catalog=decode_common_catalog(data);assert(catalog.ok);
    auto state=decode_common_seed(state_source,catalog.value);assert(state.ok);auto before=encode_common_state(state.value);
    CommonPresentation read(state.value,catalog.value);std::size_t names=0;
    for(const auto& row:state.value.actors.rows()){
        const auto* definition=data.find("actors")->find(row.first);assert(definition);
        const auto key=boundary::get_string(*definition,"name_key",row.first);
        const auto expected=boundary::string(*data.find("texts")->find(key),key);
        expect_name(read,row.first,expected);auto selected=read.text(key);assert(selected.ok&&selected.value==expected);++names;
    }
    assert(compare_boundary(before,encode_common_state(state.value)).exact());
    std::cout<<"state_presentation: actual initial names="<<names<<" exact; common state unchanged\n";
}

int main(int argc,char** argv){
    auto source=catalog_input();auto decoded=decode_common_catalog(source);assert(decoded.ok);
    auto catalog=std::move(decoded.value);CommonState state;
    for(auto id:{"actor.a","actor.b","actor.unnamed","actor.empty"}){ActorRecord actor;actor.id=id;state.actors.insert(id,std::move(actor));}
    CommonPresentation read(state,catalog);
    expect_name(read,"actor.a","甲");expect_name(read,"actor.b","乙");
    expect_name(read,"actor.unnamed","空键作者文本");expect_name(read,"actor.empty","");
    expect_name(read,"actor.missing","空键作者文本");
    auto* actor=state.actors.find("actor.a");actor->prototype_id="actor.b";
    expect_name(read,"actor.a","乙");
    actor->display_name="自定名";expect_name(read,"actor.a","自定名");
    actor->prototype_id="absent";expect_name(read,"actor.a","空键作者文本");
    actor->prototype_id="actor.b";
    for(const auto& bad:std::vector<std::string>{""," 前","后 ","前\t后","前\n后",std::string("前\x7f后"),std::string("前\0后",7)}){actor->display_name=bad;expect_name(read,"actor.a","空键作者文本");}
    std::string eighty;for(int i=0;i<80;++i)eighty+="汉";
    actor->display_name=eighty;expect_name(read,"actor.a",eighty);
    actor->display_name=eighty+"汉";expect_name(read,"actor.a","空键作者文本");
    std::string astral;for(int i=0;i<80;++i)astral+="𠮷";
    actor->display_name=astral;expect_name(read,"actor.a",astral);
    actor->display_name=astral+"𠮷";expect_name(read,"actor.a","空键作者文本");
    actor->display_name=std::string("\xc0\xaf",2);expect_name(read,"actor.a","空键作者文本");
    actor->display_name=std::string("\xed\xa0\x80",3);expect_name(read,"actor.a","空键作者文本");
    actor->display_name.reset();catalog.texts.set("name.b",eighty+"汉");expect_name(read,"actor.a",eighty+"汉");
    // Godot strip_edges只去<=32的码点；NBSP、全角空格不擅自正规化。
    actor->display_name=" 汉　";expect_name(read,"actor.a"," 汉　");
    actor->display_name="名 中";expect_name(read,"actor.a","名 中");
    actor->display_name.reset();catalog.texts.erase("name.b");expect_name(read,"actor.a","actor.a");
    catalog.texts.erase("");expect_name(read,"actor.missing","");expect_name(read,"actor.unnamed","actor.unnamed");
    actor->display_name="";expect_name(read,"actor.a","");
    // 原始边界销毁不影响不可变目录；已交出的名字不借候选或目录字符串。
    actor->display_name="原名";auto owned=read.display_name("actor.a");actor->display_name="改名";assert(owned.ok&&owned.value=="原名");
    source=BoundaryValue{};auto line=read.text("line");assert(line.ok&&line.value=="第一句\n第二句");
    auto empty=read.text("name.empty");assert(empty.ok&&empty.value.empty());
    auto missing=read.text("absent");assert(!missing.ok&&missing.error.original.code=="MissingPresentationText"&&missing.error.original.path=="texts/absent"&&missing.error.kind==ErrorKind::internal);
    catalog.texts.set("line","后来文本");assert(line.value=="第一句\n第二句");
    CommonState clone=state;clone.actors.find("actor.a")->display_name="克隆名";CommonPresentation rebound(clone,catalog);
    expect_name(read,"actor.a","改名");expect_name(rebound,"actor.a","克隆名");
    auto no_text=catalog_input();auto& root=std::get<BoundaryValue::Dict>(no_text.data);root.erase(std::remove_if(root.begin(),root.end(),[](const auto& row){return row.first=="texts";}),root.end());auto absent_texts=decode_common_catalog(no_text);assert(absent_texts.ok&&absent_texts.value.texts.empty());
    auto bad_name=catalog_input();bad_name.find("actors")->find("actor.a")->find("name_key")->data=Integer{4};auto rejected=decode_common_catalog(bad_name);assert(!rejected.ok&&rejected.error.original.code=="InvalidResidentString");
    auto bad_text=catalog_input();bad_text.find("texts")->find("line")->data=Integer{4};rejected=decode_common_catalog(bad_text);assert(!rejected.ok&&rejected.error.original.code=="InvalidResidentString");
    assert(argc==1||argc==3);if(argc==3)actual_initial(argv[1],argv[2]);
    std::cout<<"state_presentation: PASS (name/prototype/empty-key fallback, Unicode limits, exact text errors, deep-owned results, clone rebinding, cold typed decode)\n";
}
