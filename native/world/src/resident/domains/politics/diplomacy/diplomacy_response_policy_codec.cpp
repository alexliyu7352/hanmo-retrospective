// 组织指令与数值评分分开，封闭作者结构不接收任意分数、事实或授权标记。
#include "diplomacy_response_policy.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
void exact(const V& input,std::initializer_list<const char*> names,const StableId& path){std::set<StableId> fields;for(const auto* name:names)fields.insert(name);for(const auto& row:boundary::object(input,path))if(!fields.erase(row.first))boundary::fail("InvalidDiplomaticResponsePolicyField",path+"/"+row.first);if(!fields.empty())boundary::fail("MissingDiplomaticResponsePolicyField",path+"/"+*fields.begin());}
StableId ref(const V& row,const char* key,const StableId& path){auto id=boundary::get_string(row,key,path);if(id.empty())boundary::fail("InvalidDiplomaticResponseReference",path+"/"+key);return id;}
}
Outcome<DiplomaticResponsePolicy> decode_diplomatic_response_policy(const BoundaryValue& input){try{
    const StableId path="/diplomatic_response";exact(input,{"id","office_id","directives"},path);DiplomaticResponsePolicy out;out.id=ref(input,"id",path);out.office_id=ref(input,"office_id",path);const auto& rows=boundary::array(boundary::field(input,"directives",path),path+"/directives");if(rows.size()>256)boundary::fail("DiplomaticResponseDirectiveLimit",path);
    std::set<std::pair<StableId,StableId>> seen;
    for(const auto& row:rows){const auto at=path+"/directives/"+std::to_string(out.directives.size());exact(row,{"mission_policy_id","terms_id","choice"},at);DiplomaticResponseDirective rule;rule.mission_policy_id=ref(row,"mission_policy_id",at);rule.terms_id=ref(row,"terms_id",at);const auto choice=boundary::get_string(row,"choice",at);if(choice=="defer")rule.choice=DiplomaticResponseChoice::defer;else if(choice=="accept")rule.choice=DiplomaticResponseChoice::accept;else if(choice=="refuse")rule.choice=DiplomaticResponseChoice::refuse;else boundary::fail("InvalidDiplomaticResponseChoice",at+"/choice");if(!seen.emplace(rule.mission_policy_id,rule.terms_id).second)boundary::fail("DuplicateDiplomaticResponseDirective",at);out.directives.push_back(std::move(rule));}
    return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 逐原指令输出同一字段序，不构造中间BoundaryValue行表。
Status emit_diplomatic_response_policy_fields(ProjectionWriter& writer,const DiplomaticResponsePolicy& value){
 writer.field("id",value.id);writer.field("office_id",value.office_id);
 writer.sequence("directives",value.directives,[](auto& item,const auto& directive){return item.object("",[&](auto& fields){
  std::string_view choice;switch(directive.choice){case DiplomaticResponseChoice::defer:choice="defer";break;case DiplomaticResponseChoice::accept:choice="accept";break;case DiplomaticResponseChoice::refuse:choice="refuse";break;}
  fields.field("mission_policy_id",directive.mission_policy_id);fields.field("terms_id",directive.terms_id);fields.text("choice",choice);return fields.status();
 });});return writer.status();
}
BoundaryValue encode_diplomatic_response_policy(const DiplomaticResponsePolicy& value){return politics_projection_detail::encode_fields(value,emit_diplomatic_response_policy_fields);}

}
