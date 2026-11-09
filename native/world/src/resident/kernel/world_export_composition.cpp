#include "world_export_composition.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {
namespace {
using namespace boundary;
Status success(){return {true,{},{}};}
Status invalid(const StableId& code,const StableId& path={}){return {false,{},common_failure(code,path).error};}
BoundaryValue::Dict& dictionary(BoundaryValue& value){auto* result=value.as<BoundaryValue::Dict>();if(!result)fail("InvalidResidentObject","");return *result;}
void set_value(BoundaryValue& value,const StableId& key,BoundaryValue replacement){if(auto* old=value.find(key))*old=std::move(replacement);else dictionary(value).emplace_back(key,std::move(replacement));}
void merge_values(BoundaryValue& to,const BoundaryValue& from){for(const auto& row:object(from,"merge")){if(to.find(row.first))fail("DuplicateDomainPayload",row.first);dictionary(to).push_back(row);}}
std::string_view borrowed_key(const ProjectionKeyIndex::Key& key){return {key.data(),key.size()};}
// 重复键仍原码失败；只在失败路径注明实际合成入口，保留内部原路径供定位。
void annotate_export_duplicate_source(RuleError& error,WorldExportPart part,std::string_view domain){
    if(error.original.code!="DuplicateBoundaryKey")return;
    const char* source="unknown";
    switch(part){
    case WorldExportPart::common:source="common";break;
    case WorldExportPart::split:source="split";break;
    case WorldExportPart::root_fields:source="root_fields";break;
    case WorldExportPart::section:source="section";break;
    case WorldExportPart::fact_payloads:source="fact_payloads";break;
    case WorldExportPart::action_payloads:source="action_payloads";break;
    case WorldExportPart::population_actions:source="population_actions";break;
    case WorldExportPart::validation_only:source="validation_only";break;
    }
    try{
    StableId path="world_export/";path+=source;path+='/';
    for(char ch:domain){if(ch=='~')path+="~0";else if(ch=='/')path+="~1";else path+=ch;}
    path+='/';path+=error.original.path;error.original.path.swap(path);
    }catch(...){/* 失败侧注记不是守门；构造失败仍保留原码和原路径。 */}
}
Status battle_id(std::string_view key,Integer& number){if(key.size()<8||key.substr(0,7)!="battle:")return invalid("InvalidBattleId");auto parsed=std::from_chars(key.data()+7,key.data()+key.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=key.data()+key.size()||number<1)return invalid("InvalidBattleId");return success();}
}
TreeWorldExportComposition::TreeWorldExportComposition(const CommonState& common):common_(common),output_(BoundaryValue::Dict{}),params_(BoundaryValue::Dict{}),facts_(BoundaryValue::Dict{}),battles_(BoundaryValue::Dict{}){}
Status TreeWorldExportComposition::append(WorldExportPart part,std::string_view domain,EmitCall emit,const void* call){
    BoundaryValue value;BoundaryProjectionWriter writer(value);
    try{
        auto checked=emit(writer,call);if(checked.ok)checked=writer.finish();
        if(!checked.ok){annotate_export_duplicate_source(checked.error,part,domain);return checked;}
    }catch(boundary::DecodeFailure& error){
        annotate_export_duplicate_source(error.error,part,domain);throw;
    }
    switch(part){
    case WorldExportPart::validation_only:break;
    case WorldExportPart::common:output_=std::move(value);break;
    case WorldExportPart::split:
        if(const auto* rows=value.find("action_params"))merge_values(params_,*rows);
        if(const auto* rows=value.find("facts"))merge_values(facts_,*rows);
        if(const auto* rows=value.find("battles"))merge_values(battles_,*rows);
        for(auto& row:dictionary(value))if(row.first!="action_params"&&row.first!="facts"&&row.first!="battles")set_value(output_,row.first,std::move(row.second));
        break;
    case WorldExportPart::root_fields:for(auto& row:dictionary(value))set_value(output_,row.first,std::move(row.second));break;
    case WorldExportPart::section:set_value(output_,StableId(domain),std::move(value));break;
    case WorldExportPart::fact_payloads:merge_values(facts_,value);break;
    case WorldExportPart::action_payloads:merge_values(params_,value);break;
    case WorldExportPart::population_actions:merge_values(params_,field(value,"actions",StableId(domain)));break;
    }
    return success();
}
bool TreeWorldExportComposition::has_action_payload(const StableId& id)const{return params_.find(id)!=nullptr;}
bool TreeWorldExportComposition::has_fact_payload(const StableId& id)const{return facts_.find(id)!=nullptr;}
void TreeWorldExportComposition::adopt_fact_domain(const StableId& id,const StableId& domain){auto* all=output_.find("facts");auto* row=all?all->find(id):nullptr;if(!row)fail("MissingCommonFact",id);set_value(*row,"domain",BoundaryValue{domain});}
void TreeWorldExportComposition::adopt_fact_payload(const StableId& id){auto* all=output_.find("facts");auto* row=all?all->find(id):nullptr;const auto* payload=facts_.find(id);if(!row||!payload)fail("MissingDomainFactPayload",id);*row=*payload;}
Status TreeWorldExportComposition::adopt_action_payload(const StableId& id,bool original_empty){
    auto* all=output_.find("actions");auto* row=all?all->find(id):nullptr;if(!row)return invalid("MissingCommonAction",id);
    if(const auto* payload=params_.find(id))set_value(*row,"params",*payload);else if(original_empty)set_value(*row,"params",BoundaryValue{BoundaryValue::Dict{}});else return invalid("MissingDomainActionPayload",id);
    if(const auto* order=common_.action_field_order.find(id))return apply_field_order(*row,*order);return success();
}
Status TreeWorldExportComposition::finish_battles(){
    auto order=common_.battle_order;std::vector<std::pair<Integer,StableId>> added;
    for(const auto& row:dictionary(battles_))if(std::find(order.begin(),order.end(),row.first)==order.end()){Integer number=0;auto checked=battle_id(row.first,number);if(!checked.ok){checked.error.original.path=row.first;return checked;}added.emplace_back(number,row.first);}
    std::sort(added.begin(),added.end());for(const auto& row:added)order.push_back(row.second);auto checked=apply_field_order(battles_,order);if(!checked.ok)return checked;set_value(output_,"battles",std::move(battles_));return success();
}
Status TreeWorldExportComposition::finish(){return apply_field_order(output_,common_.root_field_order);}
BoundaryValue TreeWorldExportComposition::take()&&{return std::move(output_);}
// Observe only actual fields emitted to this sink. Detached validation branches
// deliberately do not contribute keys to the composed output.
class ShapeWorldExportComposition::Observation final:public ProjectionObservation {
public:
    Observation(ShapeWorldExportComposition& owner,WorldExportPart part):index(owner.memory_),owner_(owner),part_(part){}
    ProjectionCompositionIndex index;
    Status field_seen(std::size_t depth,std::string_view parent,std::string_view name)override{
        auto checked=index.field_seen(depth,parent,name);if(!checked.ok)return checked;
        if(part_==WorldExportPart::population_actions&&depth==2&&parent=="actions")return index.action_params.insert_unique(name);
        if(part_!=WorldExportPart::common)return success();
        if(depth==1)current_=nullptr;
        if(depth==2&&parent=="actions"){
            const auto ordinal=owner_.actions_.size();const auto& rows=owner_.common_.actions.rows();
            if(ordinal>=rows.size()||std::string_view(rows[ordinal].first)!=name)return invalid("CommonActionProjectionMismatch");
            owner_.actions_.emplace_back(rows[ordinal].first,owner_.memory_);current_=&owner_.actions_.back();
        }else if(depth==3&&current_&&std::string_view(*current_->id)==parent)return current_->fields.insert_unique(name,"DuplicateBoundaryKey");
        return success();
    }
private:
    ShapeWorldExportComposition& owner_;WorldExportPart part_;ActionShape* current_=nullptr;
};
ShapeWorldExportComposition::ShapeWorldExportComposition(const CommonState& common,const CapacityProfile& capacity,CheckpointJobBudget& job,std::shared_ptr<CheckpointMemoryResource> memory):common_(common),capacity_(capacity),job_(job),memory_(std::move(memory)),roots_(memory_),params_(memory_),facts_(memory_),battles_(memory_),actions_(CheckpointAllocator<ActionShape>{memory_}){}
Status ShapeWorldExportComposition::append(WorldExportPart part,std::string_view domain,EmitCall emit,const void* call){
    Observation seen(*this,part);ProjectionShapeWriter writer(capacity_,domain,job_,&seen);auto checked=emit(writer,call);if(checked.ok)checked=writer.finish();if(!checked.ok)return checked;
    const auto& index=seen.index;
    switch(part){
    case WorldExportPart::validation_only:break;
    case WorldExportPart::common:
        if(actions_.size()!=common_.actions.size())return invalid("CommonActionProjectionMismatch");
        for(const auto& key:index.roots.rows())roots_.set(borrowed_key(key));break;
    case WorldExportPart::split:
        checked=params_.merge_unique(index.action_params);if(!checked.ok)return checked;
        checked=facts_.merge_unique(index.facts);if(!checked.ok)return checked;
        checked=battles_.merge_unique(index.battles);if(!checked.ok)return checked;
        for(const auto& key:index.roots.rows()){auto name=borrowed_key(key);if(name!="action_params"&&name!="facts"&&name!="battles")roots_.set(name);}break;
    case WorldExportPart::root_fields:for(const auto& key:index.roots.rows())roots_.set(borrowed_key(key));break;
    case WorldExportPart::section:roots_.set(domain);break;
    case WorldExportPart::fact_payloads:return facts_.merge_unique(index.roots);
    case WorldExportPart::action_payloads:return params_.merge_unique(index.roots);
    case WorldExportPart::population_actions:
        if(!index.roots.contains("actions"))return invalid("MissingResidentField","actions");
        return params_.merge_unique(index.action_params);
    }
    return success();
}
bool ShapeWorldExportComposition::has_action_payload(const StableId& id)const{return params_.contains(id);}
bool ShapeWorldExportComposition::has_fact_payload(const StableId& id)const{return facts_.contains(id);}
void ShapeWorldExportComposition::adopt_fact_domain(const StableId& id,const StableId& domain){const auto* header=common_.facts.find(id);if(!header||header->domain!=domain)fail("MissingCommonFact",id);}
void ShapeWorldExportComposition::adopt_fact_payload(const StableId& id){if(!common_.facts.find(id)||!facts_.contains(id))fail("MissingDomainFactPayload",id);}
Status ShapeWorldExportComposition::adopt_action_payload(const StableId& id,bool original_empty){
    if(!params_.contains(id)&&!original_empty)return invalid("MissingDomainActionPayload",id);
    for(auto& action:actions_)if(*action.id==id){auto checked=action.fields.insert_unique("params","DuplicateBoundaryKey");if(!checked.ok)return checked;if(const auto* order=common_.action_field_order.find(id))return action.fields.validate_order(*order);return success();}
    return invalid("MissingCommonAction",id);
}
Status ShapeWorldExportComposition::finish_battles(){
    const auto& order=common_.battle_order;
    for(std::size_t i=0;i<order.size();++i){if(!battles_.contains(order[i]))return invalid("BoundaryFieldOrderMismatch");for(std::size_t j=0;j<i;++j)if(order[j]==order[i])return invalid("BoundaryFieldOrderMismatch");}
    for(const auto& key:battles_.rows()){const auto name=borrowed_key(key);if(std::none_of(order.begin(),order.end(),[&](const auto& value){return std::string_view(value)==name;})){Integer number=0;auto checked=battle_id(name,number);if(!checked.ok)return checked;}}
    roots_.set("battles");return success();
}
Status ShapeWorldExportComposition::finish(){return roots_.validate_order(common_.root_field_order);}
}
