// 零时关系决定一事件一请求；实际缓存裁剪不改变永久来源，也不采用同 tick latest 回猜。
#include "apprenticeship_receipts.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <map>
#include <set>
namespace hanmo::resident {namespace {
Integer serial(const StableId& id,const char* kind){const auto prefix=StableId{kind}+":";if(id.rfind(prefix,0)!=0)return -1;const auto digits=id.substr(prefix.size());Integer n=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits?n:-1;}
struct Source {StableId actor,kind;Integer tick=0,revision=0;};
// 只允许本域三种零时本人业务，不能把师承账接到 Advance 或其他人的请求上。
Outcome<Source> source(const FamilyApprenticeshipState& state,const StableId& id){
    const auto* p=state.proposals.find(id);const auto* r=state.replies.find(id);const auto* e=state.exits.find(id);if(Integer(p!=nullptr)+Integer(r!=nullptr)+Integer(e!=nullptr)!=1)return {false,{},common_failure("ApprenticeshipCommitSourceRequired",id).error};
    if(p)return {true,{p->source.listener_actor_id,"family_apprenticeship_receive",p->source.tick,p->source.revision},{}};
    if(r)return {true,{r->actor_id,"family_apprenticeship_reply",r->tick,r->revision},{}};
    return {true,{e->actor_id,"family_apprenticeship_exit",e->tick,e->revision},{}};
}
// 既有 FamilyEffect 热/冷均持完全相同字段，不夹入控制变更或另一份领域效果。
Status result_matches(const FamilyApprenticeshipState& state,const StableId& id,const ApprenticeshipCommitBinding& binding,const CommitResult& result){
    auto original=source(state,id);if(!original.ok)return {false,{},original.error};
    if(!result.ok||!result.details||result.result_id!=binding.result_id||result.kind!=binding.kind||result.revision!=binding.revision||result.tick!=original.value.tick)return common_failure("ApprenticeshipActualResultMismatch",id);
    const auto* effect=std::get_if<FamilyEffect>(&result.details->data);if(!effect)return common_failure("ApprenticeshipFamilyEffectRequired",id);
    return validate_apprenticeship_effect(state,id,result.kind,binding.actor_id,result.tick,*effect);
}
// 比较旧表逐字段，包括 field_order 与旧绑定；新增来源不能掩盖删除或重写旧事实。
Outcome<std::vector<StableId>> added(const FamilyApprenticeshipState& before,const FamilyApprenticeshipState& after){
    if(before.commits.order()!=after.commits.order())return {false,{},common_failure("ApprenticeshipPriorBindingChanged").error};
    auto left=after;std::vector<StableId> out;
    auto strip=[&](const auto& old,auto& next){for(const auto& id:next.order())if(!old.find(id))out.push_back(id);for(const auto& row:old.rows())if(!next.find(row.first))return false;std::vector<StableId> remove;for(const auto& row:next.rows())if(!old.find(row.first))remove.push_back(row.first);for(const auto& id:remove)next.erase(id);return true;};
    if(!strip(before.proposals,left.proposals)||!strip(before.replies,left.replies)||!strip(before.exits,left.exits))return {false,{},common_failure("ApprenticeshipPriorSourceChanged").error};
    if(!compare_boundary(encode_family_apprenticeship(before),encode_family_apprenticeship(left)).exact())return {false,{},common_failure("ApprenticeshipPriorSourceChanged").error};
    return {true,std::move(out),{}};
}
}
Status validate_apprenticeship_effect(const FamilyApprenticeshipState& state,const StableId& id,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect& effect){
    auto original=source(state,id);if(!original.ok)return {false,{},original.error};const auto& s=original.value;
    const std::set<StableId> fields(effect.field_order.begin(),effect.field_order.end());
    if(s.actor!=actor||s.kind!=kind||s.tick!=tick||effect.success!=std::optional<bool>{true}||effect.actor_id!=std::optional<StableId>{actor}||effect.player_actor_id!=effect.actor_id||effect.fact_id!=std::optional<StableId>{id}||effect.code||effect.heir_actor_id||effect.already_completed||(!effect.field_order.empty()&&(effect.field_order.size()!=fields.size()||fields!=std::set<StableId>{"ok","success","actor_id","player_actor_id","fact_id"})))return common_failure("ApprenticeshipEffectSourceMismatch",id);return common_success();
}
Status validate_apprenticeship_result_bindings(const CommonStore& c,const FamilyApprenticeshipState& state){
    auto valid=validate_family_apprenticeship_shape(state);if(!valid.ok)return valid;
    if(state.commits.size()!=state.proposals.size()+state.replies.size()+state.exits.size())return common_failure("ApprenticeshipCommitBindingRequired");
    std::set<StableId> results;std::set<Integer> revisions;std::set<std::pair<StableId,Integer>> requests;std::vector<StableId> actual_order;
    for(const auto& header:c.state().facts.rows())if(state.commits.find(header.first))actual_order.push_back(header.first);
    if(actual_order!=state.commits.order())return common_failure("ApprenticeshipCommitOrderMismatch");
    for(const auto& row:state.commits.rows()){
        const auto& b=row.second;auto original=source(state,row.first);if(!original.ok)return {false,{},original.error};const auto& s=original.value;const auto* sequence=c.state().request_sequences.find(b.actor_id);const auto source_id=serial(row.first,"apprenticeship"),result_id=serial(b.result_id,"result");
        if(source_id<1||result_id<=source_id||result_id>=c.state().next_instance_id||s.actor!=b.actor_id||s.kind!=b.kind||s.tick<0||s.tick>c.tick()||b.revision<1||b.revision>c.revision()||s.revision!=b.revision-1||!sequence||b.request_seq<1||b.request_seq>*sequence||!results.insert(b.result_id).second||!revisions.insert(b.revision).second||!requests.emplace(b.actor_id,b.request_seq).second)return common_failure("InvalidApprenticeshipCommitBinding",row.first);
        for(const auto& h:c.state().history)if(h.result_id==b.result_id&&(h.actor_id!=b.actor_id||h.kind!=b.kind||h.tick!=s.tick))return common_failure("ApprenticeshipCommitHistoryMismatch",row.first);
        const auto key=b.actor_id+":"+std::to_string(b.request_seq);if(const auto* cached=c.state().results.find(key)){if(!*cached)return common_failure("ApprenticeshipActualResultRequired",row.first);valid=result_matches(state,row.first,b,**cached);if(!valid.ok)return valid;}
        for(const auto& cached:c.state().results.rows())if(cached.second&&(cached.second->result_id==b.result_id||cached.second->revision==b.revision)&&cached.first!=key)return common_failure("ApprenticeshipCommitCacheMismatch",row.first);
    }
    return common_success();
}
Status bind_apprenticeship_result_in_candidate(CommonStore& c,const FamilyApprenticeshipState& before,FamilyApprenticeshipState& after,const StableId& actor,Integer sequence,const CommitResult& result){
    auto valid=validate_apprenticeship_result_bindings(c,before);if(!valid.ok)return valid;valid=validate_family_apprenticeship_shape(after);if(!valid.ok)return valid;auto difference=added(before,after);if(!difference.ok)return {false,{},difference.error};if(difference.value.empty())return validate_apprenticeship_result_bindings(c,after);if(difference.value.size()!=1)return common_failure("ApprenticeshipOnePersonalDecisionRequired");
    const auto* cached=c.state().results.find(actor+":"+std::to_string(sequence));const auto* current_sequence=c.state().request_sequences.find(actor);
    if(!cached||cached->get()!=&result||!current_sequence||*current_sequence!=sequence||result.revision!=c.revision()||result.tick!=c.tick()||c.state().history.empty())return common_failure("ApprenticeshipActualResultRequired",result.result_id);
    const auto& history=c.state().history.back();if(history.actor_id!=actor||history.result_id!=result.result_id||history.kind!=result.kind||history.tick!=result.tick)return common_failure("ApprenticeshipActualHistoryRequired",result.result_id);
    const auto& id=difference.value.front();ApprenticeshipCommitBinding binding{result.result_id,actor,result.kind,sequence,result.revision,{}};valid=result_matches(after,id,binding,result);if(!valid.ok)return valid;
    if(!after.commits.insert(id,binding))return common_failure("ApprenticeshipSourceAlreadyBound",id);valid=validate_apprenticeship_result_bindings(c,after);if(!valid.ok)after.commits.erase(id);return valid;
}
Outcome<StableId> read_apprenticeship_source_for_result(const CommonStore& c,const FamilyApprenticeshipState& state,const CommitResult& result){
    Integer found=0;for(const auto& cached:c.state().results.rows())if(cached.second.get()==&result)++found;if(found!=1)return {false,{},common_failure("ApprenticeshipActualResultRequired",result.result_id).error};auto valid=validate_apprenticeship_result_bindings(c,state);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:state.commits.rows())if(row.second.result_id==result.result_id){valid=result_matches(state,row.first,row.second,result);return valid.ok?Outcome<StableId>{true,row.first,{}}:Outcome<StableId>{false,{},valid.error};}
    return {false,{},common_failure("ApprenticeshipResultSourceRequired",result.result_id).error};
}
}
