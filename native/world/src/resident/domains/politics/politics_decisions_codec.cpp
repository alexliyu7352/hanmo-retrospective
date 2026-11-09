// 政治策略仅接受封闭响应与有界名单；没有可执行脚本或“按成员默认接受”的逃生分支。
#include "politics_decisions.hpp"
#include "politics_projection_detail.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 包括重复键也拒绝，不能用两份不同响应让作者和原生选到不同策略。
void exact_keys(const V& input,std::initializer_list<const char*> names,const StableId& path){std::set<StableId> required;for(const auto* name:names)required.insert(name);for(const auto& row:boundary::object(input,path))if(!required.erase(row.first))boundary::fail("InvalidPoliticalPolicyField",path+"/"+row.first);if(!required.empty())boundary::fail("MissingPoliticalPolicyField",path+"/"+*required.begin());}
// 有界引用列表本身不可重复，实体及辖权的存在由实际候选验证。
std::vector<StableId> ids(const V& input,const StableId& path,std::size_t limit){auto result=boundary::strings(input,path);if(result.size()>limit)boundary::fail("PoliticalPolicyReferenceLimit",path);std::set<StableId> seen;for(const auto& id:result)if(id.empty()||!seen.insert(id).second)boundary::fail("InvalidPoliticalPolicyReference",path);return result;}
}
Outcome<PoliticalCareerPolicy> decode_political_career_policy(const BoundaryValue& input){try{const StableId path="/political_career";exact_keys(input,{"id","response","maximum_travel_quote_cost","acceptable_organization_ids","acceptable_jurisdiction_ids","staffing_posts"},path);PoliticalCareerPolicy out;out.id=boundary::get_string(input,"id",path);if(out.id.empty())boundary::fail("InvalidPoliticalCareerPolicy",path+"/id");out.maximum_travel_quote_cost=boundary::get_int(input,"maximum_travel_quote_cost",path);if(out.maximum_travel_quote_cost<0)boundary::fail("InvalidPoliticalTravelQuoteLimit",path+"/maximum_travel_quote_cost");const auto response=boundary::get_string(input,"response",path);if(response=="defer")out.response=PoliticalOfficeResponse::defer;else if(response=="accept_qualified")out.response=PoliticalOfficeResponse::accept_qualified;else if(response=="decline")out.response=PoliticalOfficeResponse::decline;else boundary::fail("InvalidPoliticalOfficeResponse",path+"/response");out.acceptable_organization_ids=ids(boundary::field(input,"acceptable_organization_ids",path),path+"/acceptable_organization_ids",64);out.acceptable_jurisdiction_ids=ids(boundary::field(input,"acceptable_jurisdiction_ids",path),path+"/acceptable_jurisdiction_ids",64);if(out.response==PoliticalOfficeResponse::accept_qualified&&(out.acceptable_organization_ids.empty()||out.acceptable_jurisdiction_ids.empty()))boundary::fail("PoliticalAcceptanceScopeRequired",path);const auto& posts=boundary::array(boundary::field(input,"staffing_posts",path),path+"/staffing_posts");if(posts.size()>8)boundary::fail("PoliticalStaffingPostLimit",path+"/staffing_posts");for(const auto& row:posts){const auto current=path+"/staffing_posts/"+std::to_string(out.staffing_posts.size());exact_keys(row,{"issuing_office_id","jurisdiction_id","appointment_policy_id","candidate_actor_ids"},current);PoliticalStaffingPost post;post.issuing_office_id=boundary::get_string(row,"issuing_office_id",current);post.jurisdiction_id=boundary::get_string(row,"jurisdiction_id",current);post.appointment_policy_id=boundary::get_string(row,"appointment_policy_id",current);post.candidate_actor_ids=ids(boundary::field(row,"candidate_actor_ids",current),current+"/candidate_actor_ids",16);if(post.issuing_office_id.empty()||post.jurisdiction_id.empty()||post.appointment_policy_id.empty()||post.candidate_actor_ids.empty())boundary::fail("InvalidPoliticalStaffingPost",current);out.staffing_posts.push_back(std::move(post));}return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 原策略的JSON字段直接借读；原encode和无树shape同走这一出口。
Status emit_political_career_policy_fields(ProjectionWriter& writer,const PoliticalCareerPolicy& value){
 std::string_view response;
 switch(value.response){case PoliticalOfficeResponse::defer:response="defer";break;case PoliticalOfficeResponse::accept_qualified:response="accept_qualified";break;case PoliticalOfficeResponse::decline:response="decline";break;}
 writer.field("id",value.id);writer.text("response",response);writer.field("maximum_travel_quote_cost",value.maximum_travel_quote_cost);
 auto ids=[](auto& fields,const auto& id){return fields.field("",id);};
 writer.sequence("acceptable_organization_ids",value.acceptable_organization_ids,ids);
 writer.sequence("acceptable_jurisdiction_ids",value.acceptable_jurisdiction_ids,ids);
 writer.sequence("staffing_posts",value.staffing_posts,[&](auto& item,const auto& post){return item.object("",[&](auto& fields){
  fields.field("issuing_office_id",post.issuing_office_id);fields.field("jurisdiction_id",post.jurisdiction_id);fields.field("appointment_policy_id",post.appointment_policy_id);
  fields.sequence("candidate_actor_ids",post.candidate_actor_ids,ids);return fields.status();
 });});return writer.status();
}
BoundaryValue encode_political_career_policy(const PoliticalCareerPolicy& value){return politics_projection_detail::encode_fields(value,emit_political_career_policy_fields);}

} // 调度策略只影响下一次提议，正式世界仍在同一命令预览与事务中决定能否采用。
