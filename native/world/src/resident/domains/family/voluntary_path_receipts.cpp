#include "voluntary_path_receipts.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {namespace {
Integer serial(const StableId& id,const StableId& prefix){const auto lead=prefix+":";if(id.rfind(lead,0)!=0)return -1;const auto digits=id.substr(lead.size());Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits?n:-1;}
const SuccessionConsentFact* plan(const FamilyState& f,const StableId& id){const auto* p=f.facts.find(id);return p?std::get_if<SuccessionConsentFact>(p):nullptr;}
struct Source {StableId actor,successor,kind,head_kind,domain,effect_fact;Integer tick=0,revision=0;bool completed=false,modern=false;};
Outcome<Source> source(const FamilyState& f,const VoluntaryPathState& s,const StableId& id,const StableId& command={}){
    Source out;out.effect_fact=id;
    if(const auto* p=plan(f,id)){
        out.actor=p->agreement.personal.predecessor_actor_id;out.successor=p->agreement.personal.successor_actor_id;
        out.tick=p->agreement.accepted_tick;out.revision=p->agreement.accepted_revision;out.modern=s.consents.find(id)!=nullptr;
        out.kind=out.modern?"family_voluntary_path_accept":"family_agree_succession";out.head_kind="family_succession_consent";out.domain="family";
    }else if(const auto* value=f.facts.find(id);value&&std::get_if<VoluntarySuccessionFact>(value)){
        const auto& done=std::get<VoluntarySuccessionFact>(*value);const auto* p=plan(f,done.consent_fact_id);const auto* revision=s.completion_revisions.find(id);
        if(!p||!revision)return {false,{},common_failure("VoluntaryPathCompletionSourceRequired",id).error};
        out.actor=p->agreement.personal.predecessor_actor_id;out.successor=p->agreement.personal.successor_actor_id;out.tick=done.tick;out.revision=*revision;
        out.modern=s.consents.find(done.consent_fact_id)!=nullptr;out.completed=true;out.kind=out.modern?"family_voluntary_path_continue":"family_continue_as";out.head_kind="family_voluntary_succession";out.domain="family";
    }else if(const auto* end=s.cancellations.find(id)){
        out.actor=end->actor_id;out.tick=end->tick;out.revision=end->revision;out.modern=true;out.head_kind=voluntary_path_cancelled_kind;out.domain=voluntary_path_module;
        out.kind=end->withdrawal?"family_voluntary_path_withdraw":"family_voluntary_path_cancel";
        if(command=="family_cancel_succession"&&!end->withdrawal){out.kind=command;out.effect_fact=end->plan_id;out.modern=false;}
    }else return {false,{},common_failure("VoluntaryPathResultSourceRequired",id).error};
    if(!command.empty()&&command!=out.kind)return {false,{},common_failure("VoluntaryPathCommandSourceMismatch",id).error};
    return {true,std::move(out),{}};
}
Status matches(const FamilyState& f,const VoluntaryPathState& s,const StableId& id,const VoluntaryPathCommitBinding& b,const CommitResult& r){
    auto read=source(f,s,id,b.kind);if(!read.ok)return {false,{},read.error};const auto& p=read.value;
    const auto* effect=r.details?std::get_if<FamilyEffect>(&r.details->data):nullptr;
    if(!r.ok||!effect||r.result_id!=b.result_id||r.kind!=b.kind||r.revision!=b.revision||r.tick!=p.tick||b.actor_id!=p.actor||
       effect->success!=std::optional<bool>{true}||effect->fact_id!=std::optional<StableId>{p.effect_fact}||effect->code||effect->already_completed||
       effect->player_actor_id!=std::optional<StableId>{p.completed?p.successor:p.actor}||
       effect->actor_id!=(p.modern?std::optional<StableId>{p.actor}:std::nullopt)||
       effect->heir_actor_id!=(p.successor.empty()?std::nullopt:std::optional<StableId>{p.successor}))return common_failure("VoluntaryPathActualResultMismatch",id);
    std::set<StableId> keys{"ok","success","fact_id","player_actor_id"};if(p.modern)keys.insert("actor_id");if(!p.successor.empty())keys.insert("heir_actor_id");
    if(!effect->field_order.empty()&&(effect->field_order.size()!=keys.size()||std::set<StableId>(effect->field_order.begin(),effect->field_order.end())!=keys))return common_failure("VoluntaryPathEffectShapeMismatch",id);
    return common_success();
}
std::vector<StableId> source_ids(const FamilyState& f,const VoluntaryPathState& s){std::vector<StableId> ids;for(const auto& row:f.facts.rows())if(std::holds_alternative<SuccessionConsentFact>(row.second)||std::holds_alternative<VoluntarySuccessionFact>(row.second))ids.push_back(row.first);for(const auto& id:s.cancellations.order())ids.push_back(id);return ids;}
BoundaryValue family_sources(const FamilyState& f){BoundaryValue::Dict rows;for(const auto& row:f.facts.rows())if(const auto* p=std::get_if<SuccessionConsentFact>(&row.second))rows.emplace_back(row.first,encode_succession_consent_fact(*p));else if(const auto* d=std::get_if<VoluntarySuccessionFact>(&row.second))rows.emplace_back(row.first,encode_voluntary_succession_fact(*d));return BoundaryValue{std::move(rows)};}
Outcome<std::vector<StableId>> added(const FamilyState& before_f,const VoluntaryPathState& before,const FamilyState& after_f,const VoluntaryPathState& after){try{
    const auto original=source_ids(before_f,before),current=source_ids(after_f,after);std::set<StableId> old(original.begin(),original.end());std::vector<StableId> ids;
    for(const auto& id:original)if(std::find(current.begin(),current.end(),id)==current.end())return {false,{},common_failure("VoluntaryPathPriorSourceChanged").error};
    for(const auto& id:current)if(!old.count(id))ids.push_back(id);
    auto trimmed_f=after_f;auto trimmed=after;
    for(const auto& id:ids){StableId changed_plan;
        if(const auto* value=after_f.facts.find(id)){if(const auto* d=std::get_if<VoluntarySuccessionFact>(value))changed_plan=d->consent_fact_id;trimmed_f.facts.erase(id);trimmed.consents.erase(id);trimmed.completion_revisions.erase(id);}
        if(const auto* end=after.cancellations.find(id)){changed_plan=end->plan_id;trimmed.cancellations.erase(id);}
        if(!changed_plan.empty()){const auto* original_plan=plan(before_f,changed_plan);auto* value=trimmed_f.facts.find(changed_plan);auto* current_plan=value?std::get_if<SuccessionConsentFact>(value):nullptr;
            if(!original_plan||original_plan->conclusion||!current_plan||!current_plan->conclusion)return {false,{},common_failure("VoluntaryPathPriorPlanRequired").error};
            current_plan->conclusion=original_plan->conclusion;current_plan->field_order=original_plan->field_order;}
    }
    if(!compare_boundary(family_sources(before_f),family_sources(trimmed_f)).exact()||!compare_boundary(encode_voluntary_path_state(before),encode_voluntary_path_state(trimmed)).exact())
        return {false,{},common_failure("VoluntaryPathPriorSourceChanged").error};return {true,std::move(ids),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
Status validate_voluntary_path_result_bindings(const CommonStore& c,const FamilyState& family,const VoluntaryPathState& s){
    auto valid=validate_voluntary_path_state_shape(s);if(!valid.ok)return valid;const auto sources=source_ids(family,s);
    if(sources.size()!=s.commits.size())return common_failure("VoluntaryPathCommitCoverageMismatch");
    std::set<StableId> covered(sources.begin(),sources.end()),results;std::set<Integer> revisions;std::set<std::pair<StableId,Integer>> requests;
    std::size_t completion_count=0;for(const auto& row:family.facts.rows())if(std::holds_alternative<VoluntarySuccessionFact>(row.second))++completion_count;
    if(completion_count!=s.completion_revisions.size())return common_failure("VoluntaryPathCompletionCoverageMismatch");
    Integer last_result=0,last_revision=0,last_tick=-1;const auto& order=c.state().facts.order();auto last=order.begin();bool seen=false;
    for(const auto& row:s.commits.rows()){
        if(!covered.erase(row.first))return common_failure("VoluntaryPathCommitCoverageMismatch",row.first);const auto& b=row.second;
        auto original=source(family,s,row.first,b.kind);if(!original.ok)return {false,{},original.error};const auto& p=original.value;
        const auto* h=c.state().facts.find(row.first);const auto n=serial(row.first,p.domain=="family"?"family":"voluntary_path"),r=serial(b.result_id,"result");
        const auto position=std::find(order.begin(),order.end(),row.first);const auto* sequence=c.state().request_sequences.find(b.actor_id);
        if(!h||h->id!=row.first||h->kind!=p.head_kind||h->domain!=p.domain||position==order.end()||(seen&&position<=last)||n<=last_result||r<=n||r>=c.state().next_instance_id||
           b.actor_id!=p.actor||p.tick<last_tick||p.tick>c.tick()||p.revision!=b.revision-1||b.revision<=last_revision||b.revision>c.revision()||
           !sequence||b.request_seq<1||b.request_seq>*sequence||!results.insert(b.result_id).second||!revisions.insert(b.revision).second||!requests.emplace(b.actor_id,b.request_seq).second)
            return common_failure("InvalidVoluntaryPathCommit",row.first);
        last=position;seen=true;last_result=r;last_revision=b.revision;last_tick=p.tick;
        const auto key=b.actor_id+":"+std::to_string(b.request_seq);
        for(const auto& history:c.state().history)if(history.result_id==b.result_id&&(history.actor_id!=b.actor_id||history.kind!=b.kind||history.tick!=p.tick))return common_failure("VoluntaryPathHistoryMismatch",row.first);
        if(const auto* cached=c.state().results.find(key)){if(!*cached)return common_failure("VoluntaryPathActualResultRequired");valid=matches(family,s,row.first,b,**cached);if(!valid.ok)return valid;}
        for(const auto& cached:c.state().results.rows())if(cached.second&&(cached.second->result_id==b.result_id||cached.second->revision==b.revision)&&cached.first!=key)return common_failure("VoluntaryPathCacheIdentityMismatch",row.first);
    }
    for(const auto& cached:c.state().results.rows())if(cached.second&&cached.second->ok&&cached.second->details){
        const auto& kind=cached.second->kind;
        if(kind!="family_agree_succession"&&kind!="family_continue_as"&&kind!="family_cancel_succession"&&kind.rfind("family_voluntary_path_",0)!=0)continue;
        const VoluntaryPathCommitBinding* binding=nullptr;StableId id;for(const auto& row:s.commits.rows())if(row.second.result_id==cached.second->result_id){if(binding)return common_failure("VoluntaryPathCachedSourceReused");binding=&row.second;id=row.first;}
        if(!binding||cached.first!=binding->actor_id+":"+std::to_string(binding->request_seq))return common_failure("VoluntaryPathCachedSourceMissing");
        valid=matches(family,s,id,*binding,*cached.second);if(!valid.ok)return valid;
    }
    return covered.empty()?common_success():common_failure("VoluntaryPathCommitCoverageMismatch");
}
Status bind_voluntary_path_result_in_candidate(CommonStore& c,const WorldIdentity& identity,const FamilyState& before_f,
    const VoluntaryPathState& before,FamilyState& after_f,VoluntaryPathState& after,const StableId& actor,Integer sequence,const CommitResult& result){
    auto difference=added(before_f,before,after_f,after);if(!difference.ok)return {false,{},difference.error};
    if(difference.value.empty())return validate_voluntary_path_result_bindings(c,after_f,after);
    if(difference.value.size()!=1)return common_failure("OneVoluntaryPathSourcePerRequest");
    // 只有真正新增本域来源才要求本次是零时本人结果；其他合法Advance不被本模块改协议。
    const auto* cached=c.state().results.find(actor+":"+std::to_string(sequence));const auto* requested=c.state().request_sequences.find(actor);
    if(!cached||cached->get()!=&result||!requested||*requested!=sequence||!result.ok||result.revision!=c.revision()||result.tick!=c.tick()||
       identity.world_id!=c.state().identity.world_id||identity.execution_epoch!=c.state().identity.execution_epoch||identity.tick!=result.tick||
       identity.revision<0||identity.revision>=std::numeric_limits<Integer>::max()||identity.revision+1!=result.revision||c.state().history.empty())return common_failure("VoluntaryPathActualResultRequired");
    const auto& h=c.state().history.back();if(h.result_id!=result.result_id||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("VoluntaryPathActualHistoryRequired");
    const auto id=difference.value.front();
    auto observed=source(after_f,after,id,result.kind);if(!observed.ok)return {false,{},observed.error};
    if(observed.value.revision!=identity.revision||observed.value.tick!=identity.tick||observed.value.actor!=actor)return common_failure("VoluntaryPathOriginalSubmitMismatch",id);
    VoluntaryPathCommitBinding binding{result.result_id,actor,result.kind,sequence,result.revision,{}};
    auto valid=matches(after_f,after,id,binding,result);if(!valid.ok)return valid;
    if(!after.commits.insert(id,binding))return common_failure("VoluntaryPathAlreadyBound",id);
    valid=validate_voluntary_path_result_bindings(c,after_f,after);if(!valid.ok)after.commits.erase(id);return valid;
}
Outcome<StableId> read_voluntary_path_source_for_result(const CommonStore& c,const FamilyState& f,const VoluntaryPathState& s,const CommitResult& result){
    Integer exact=0;for(const auto& row:c.state().results.rows())if(row.second.get()==&result)++exact;
    if(exact!=1)return {false,{},common_failure("VoluntaryPathActualResultRequired").error};
    auto valid=validate_voluntary_path_result_bindings(c,f,s);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:s.commits.rows())if(row.second.result_id==result.result_id){valid=matches(f,s,row.first,row.second,result);return valid.ok?Outcome<StableId>{true,row.first,{}}:Outcome<StableId>{false,{},valid.error};}
    return {false,{},common_failure("VoluntaryPathResultSourceRequired").error};
}
}
