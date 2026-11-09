// 本次新许可/撤回/激活和真实 CommitResult 一对一；首缓存裁剪也不采用同 tick latest 回猜。
#include "disciple_control_receipts.hpp"
#include "family_legacy_receipt.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {namespace {
Integer serial(const StableId& id,const char* name){const auto prefix=StableId{name}+":";if(id.rfind(prefix,0)!=0)return -1;auto text=id.substr(prefix.size());Integer n=0;auto read=std::from_chars(text.data(),text.data()+text.size(),n);return read.ec==std::errc{}&&read.ptr==text.data()+text.size()&&n>0&&std::to_string(n)==text?n:-1;}
struct Source {StableId request,actor,kind;Integer tick=0;std::optional<Integer> source_revision;};
// 所有业务条款仍从原永久 legacy 三表读取；结果尾没有重新解释或复制条款的机会。
Outcome<Source> source(const FamilyLegacyState& legacy,const StableId& id){
    const auto* c=legacy.consents.find(id);const auto* r=legacy.revocations.find(id);const auto* a=legacy.activations.find(id);if(Integer(c!=nullptr)+Integer(r!=nullptr)+Integer(a!=nullptr)!=1)return {false,{},common_failure("DiscipleControlOriginalSourceRequired",id).error};
    if(c)return {true,{c->source.request_id,c->source.predecessor_actor_id,"family_legacy_accept",c->source.tick,c->source.revision},{}};
    if(r)return {true,{r->source.request_id,r->source.predecessor_actor_id,"family_legacy_revoke",r->source.tick,r->source.revision},{}};
    const auto* consent=legacy.consents.find(a->consent_fact_id);if(!consent)return {false,{},common_failure("DiscipleControlOriginalSourceRequired",id).error};return {true,{consent->source.request_id,consent->source.predecessor_actor_id,"family_legacy_activate",a->tick,std::nullopt},{}};
}
Status matches(const FamilyLegacyState& legacy,const StableId& id,const DiscipleControlCommitBinding& binding,const CommitResult& result){
    auto original=source(legacy,id);if(!original.ok)return {false,{},original.error};const auto* effect=result.details?std::get_if<FamilyEffect>(&result.details->data):nullptr;
    if(!result.ok||!effect||result.result_id!=binding.result_id||result.kind!=binding.kind||result.revision!=binding.revision||result.tick!=original.value.tick||effect->fact_id!=std::optional<StableId>{id})return common_failure("DiscipleControlActualResultMismatch",id);
    return validate_family_legacy_receipt(legacy,result.kind,binding.actor_id,result.tick,*effect);
}
// 全旧表逐字段不变后才得到新增事实；不能把旧未绑定来源趁新提交追认。
Outcome<std::vector<StableId>> added(const FamilyLegacyState& before,const FamilyLegacyState& after,FamilyLegacyVersion version){try{
    if(version!=FamilyLegacyVersion::original1&&version!=FamilyLegacyVersion::biological2&&version!=FamilyLegacyVersion::collateral3)return {false,{},common_failure("UnknownLegacySourceVersion").error};auto encode=[version](const FamilyLegacyState& s){return version==FamilyLegacyVersion::collateral3?encode_family_legacy_v3(s):version==FamilyLegacyVersion::biological2?encode_family_legacy_v2(s):encode_family_legacy(s);};
    auto trimmed=after;std::vector<StableId> found;
    auto strip=[&](const auto& old,auto& next){for(const auto& row:old.rows())if(!next.find(row.first))return false;std::vector<StableId> remove;for(const auto& row:next.rows())if(!old.find(row.first)){remove.push_back(row.first);found.push_back(row.first);}for(const auto& id:remove)next.erase(id);return true;};
    if(!strip(before.consents,trimmed.consents)||!strip(before.revocations,trimmed.revocations)||!strip(before.activations,trimmed.activations)||!compare_boundary(encode(before),encode(trimmed)).exact())return {false,{},common_failure("DiscipleControlPriorSourceChanged").error};return {true,std::move(found),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
std::vector<StableId> selected(const CommonStore& c,const FamilyLegacyState& legacy,const DiscipleControlCatalog& catalog){std::vector<StableId> out;for(const auto& h:c.state().facts.rows()){if(h.second.domain!="family_legacy")continue;auto s=source(legacy,h.first);if(s.ok&&catalog.requests.find(s.value.request))out.push_back(h.first);}return out;}
}
Status validate_disciple_control_result_bindings(const CommonStore& c,const FamilyLegacyState& legacy,const DiscipleControlCatalog& catalog,const DiscipleControlState& state){
    auto valid=validate_disciple_control_shape(state);if(!valid.ok)return valid;if(!catalog.enabled)return state.commits.empty()?common_success():common_failure("DiscipleControlProfileRequired");
    std::set<StableId> expected;auto include=[&](const auto& table)->Status{for(const auto& row:table.rows()){auto s=source(legacy,row.first);if(!s.ok)return {false,{},s.error};if(catalog.requests.find(s.value.request))expected.insert(row.first);}return common_success();};valid=include(legacy.consents);if(!valid.ok)return valid;valid=include(legacy.revocations);if(!valid.ok)return valid;valid=include(legacy.activations);if(!valid.ok)return valid;
    const auto actual=selected(c,legacy,catalog);if(state.commits.size()!=expected.size()||state.commits.order()!=actual)return common_failure("DiscipleControlCommitCoverageMismatch");
    std::set<StableId> results;std::set<Integer> revisions;std::set<std::pair<StableId,Integer>> requests;Integer previous_revision=0,previous_result=0,previous_tick=0;
    for(const auto& row:state.commits.rows()){
        const auto& b=row.second;auto s=source(legacy,row.first);if(!s.ok)return {false,{},s.error};const auto& original=s.value;const auto* h=c.state().facts.find(row.first);const auto* sequence=c.state().request_sequences.find(b.actor_id);auto source_number=serial(row.first,"family_legacy"),result_number=serial(b.result_id,"result");
        const auto wanted=original.kind=="family_legacy_accept"?"family_legacy_control_consent":original.kind=="family_legacy_revoke"?"family_legacy_control_revocation":"family_legacy_control_activated";
        if(!expected.count(row.first)||!h||h->id!=row.first||h->domain!="family_legacy"||h->kind!=wanted||source_number<1||source_number<=previous_result||result_number<=source_number||result_number>=c.state().next_instance_id||b.actor_id!=original.actor||b.kind!=original.kind||original.tick<previous_tick||original.tick>c.tick()||b.revision<=previous_revision||b.revision>c.revision()||(original.source_revision&&*original.source_revision!=b.revision-1)||!sequence||b.request_seq<1||b.request_seq>*sequence||!results.insert(b.result_id).second||!revisions.insert(b.revision).second||!requests.emplace(b.actor_id,b.request_seq).second)return common_failure("InvalidDiscipleControlCommit",row.first);
        previous_revision=b.revision;previous_result=result_number;previous_tick=original.tick;
        for(const auto& history:c.state().history)if(history.result_id==b.result_id&&(history.actor_id!=b.actor_id||history.kind!=b.kind||history.tick!=original.tick))return common_failure("DiscipleControlHistoryMismatch",row.first);
        const auto key=b.actor_id+":"+std::to_string(b.request_seq);if(const auto* cached=c.state().results.find(key)){if(!*cached)return common_failure("DiscipleControlActualResultRequired",row.first);valid=matches(legacy,row.first,b,**cached);if(!valid.ok)return valid;}
        for(const auto& cached:c.state().results.rows())if(cached.second&&(cached.second->result_id==b.result_id||cached.second->revision==b.revision)&&cached.first!=key)return common_failure("DiscipleControlCacheIdentityMismatch",row.first);
    }
    // 缓存反向覆盖：一个旧来源不能再挂到不同请求；旧未选请求仍由原 legacy 回执门处理。
    for(const auto& row:c.state().results.rows())if(row.second&&row.second->details){const auto* e=std::get_if<FamilyEffect>(&row.second->details->data);if(!e||!e->fact_id)continue;auto s=source(legacy,*e->fact_id);if(!s.ok||!catalog.requests.find(s.value.request))continue;const auto* b=state.commits.find(*e->fact_id);if(!b||row.first!=b->actor_id+":"+std::to_string(b->request_seq))return common_failure("DiscipleControlCachedSourceReused",row.first);valid=matches(legacy,*e->fact_id,*b,*row.second);if(!valid.ok)return valid;}
    return common_success();
}
Status bind_disciple_control_result_in_candidate(CommonStore& c,const FamilyLegacyState& before,const FamilyLegacyState& after,const DiscipleControlCatalog& catalog,const DiscipleControlState& before_bindings,DiscipleControlState& state,const StableId& actor,Integer request,const CommitResult& result,FamilyLegacyVersion source_version){
    auto old_shape=validate_disciple_control_shape(before_bindings);if(!old_shape.ok)return old_shape;auto new_shape=validate_disciple_control_shape(state);if(!new_shape.ok)return new_shape;if(!compare_boundary(encode_disciple_control_state(before_bindings),encode_disciple_control_state(state)).exact())return common_failure("DiscipleControlPriorBindingChanged");
    auto valid=validate_disciple_control_result_bindings(c,before,catalog,state);if(!valid.ok)return valid;auto difference=added(before,after,source_version);if(!difference.ok)return {false,{},difference.error};std::vector<StableId> chosen;for(const auto& id:difference.value){auto s=source(after,id);if(!s.ok)return {false,{},s.error};if(catalog.enabled&&catalog.requests.find(s.value.request))chosen.push_back(id);}if(chosen.empty())return validate_disciple_control_result_bindings(c,after,catalog,state);if(chosen.size()!=1||difference.value.size()!=1)return common_failure("DiscipleControlOneDecisionRequired");
    const auto* cached=c.state().results.find(actor+":"+std::to_string(request));const auto* sequence=c.state().request_sequences.find(actor);if(!cached||cached->get()!=&result||!sequence||*sequence!=request||result.revision!=c.revision()||result.tick!=c.tick()||c.state().history.empty())return common_failure("DiscipleControlActualResultRequired");const auto& history=c.state().history.back();if(history.actor_id!=actor||history.result_id!=result.result_id||history.kind!=result.kind||history.tick!=result.tick)return common_failure("DiscipleControlActualHistoryRequired");
    const auto& id=chosen.front();DiscipleControlCommitBinding binding{result.result_id,actor,result.kind,request,result.revision,{}};valid=matches(after,id,binding,result);if(!valid.ok)return valid;if(!state.commits.insert(id,binding))return common_failure("DiscipleControlSourceAlreadyBound",id);valid=validate_disciple_control_result_bindings(c,after,catalog,state);if(!valid.ok)state.commits.erase(id);return valid;
}
Outcome<StableId> read_disciple_control_source_for_result(const CommonStore& c,const FamilyLegacyState& legacy,const DiscipleControlCatalog& catalog,const DiscipleControlState& state,const CommitResult& result){
    Integer found=0;for(const auto& row:c.state().results.rows())if(row.second.get()==&result)++found;if(found!=1)return {false,{},common_failure("DiscipleControlActualResultRequired").error};auto valid=validate_disciple_control_result_bindings(c,legacy,catalog,state);if(!valid.ok)return {false,{},valid.error};for(const auto& row:state.commits.rows())if(row.second.result_id==result.result_id){valid=matches(legacy,row.first,row.second,result);return valid.ok?Outcome<StableId>{true,row.first,{}}:Outcome<StableId>{false,{},valid.error};}return {false,{},common_failure("DiscipleControlResultSourceRequired").error};
}
}
