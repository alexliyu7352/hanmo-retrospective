// 自愿偿租和明确复业只绑定真正本次提交；自动日历应计不伪造玩家请求。
#include "private_shop_daily_rent_cached_receipt.hpp"
#include "private_shop_daily_rent_codec.hpp"
#include "resident/kernel/command_types.hpp"
#include <charconv>
#include <set>
#include <map>
namespace hanmo::resident {namespace {
struct Source{StableId id,first,last,actor,command;Integer tick=0;};
std::optional<Integer> serial(const StableId& id,Integer next){auto at=id.rfind(':');if(at==StableId::npos)return {};auto digits=id.substr(at+1);Integer n=0;auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);if(p.ec!=std::errc{}||p.ptr!=digits.data()+digits.size()||n<1||n>=next||std::to_string(n)!=digits)return {};return n;}
// 一次复业可能连带新期自动实付，完整源区间直到原 LE 付款，仍只有一次本人决定。
std::vector<Source> sources(const PrivateShopDailyRentState& s){
 std::vector<Source> out;
 for(const auto& row:s.payments.rows()){const auto& p=row.second;if(p.funding_kind!="automatic_shop")out.push_back({row.first,row.first,p.local_payment_id,p.actor_id,"private_shop_pay_rent_arrear",p.tick});}
 for(const auto& row:s.resumptions.rows()){const auto& r=row.second;StableId last=row.first;const auto* a=s.assessments.find(r.assessment_fact_id);if(a&&a->resume_fact_id==r.id){const auto* p=s.payments.find(a->automatic_payment_fact_id);last=p?p->local_payment_id:StableId{};}out.push_back({row.first,row.first,last,r.actor_id,"private_shop_resume_rent",r.tick});}
 return out;
}
Status interval(const CommonStore& c,const StableId& key,const CommitResult& result,const Source& source,const std::vector<Source>& all){
    const auto split=key.rfind(':');if(split==StableId::npos)return common_failure("PrivateShopRentCachedKeyMismatch",key);const auto actor=key.substr(0,split),digits=key.substr(split+1);Integer request=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* seq=c.state().request_sequences.find(actor);if(!c.actor(actor)||!seq||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>2147483647||request>*seq||std::to_string(request)!=digits)return common_failure("PrivateShopRentCachedKeyMismatch",key);
    const HistoryRecord* previous=nullptr;const HistoryRecord* lower=nullptr;const HistoryRecord* selected=nullptr;for(const auto& h:c.state().history){if(h.result_id==result.result_id){if(selected||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("PrivateShopRentCachedHistoryMismatch",key);selected=&h;lower=previous;}previous=&h;}
    if(!selected||result.tick<0||result.tick>c.tick()||source.tick!=result.tick||source.actor!=actor||source.command!=result.kind)return common_failure("PrivateShopRentCachedHistoryMismatch",key);
    auto n=c.state().next_instance_id;auto first=serial(source.first,n),last=serial(source.last,n),from=lower?serial(lower->result_id,n):std::optional<Integer>{0},to=serial(result.result_id,n);if(result.result_id.rfind("result:",0)!=0||(lower&&lower->result_id.rfind("result:",0)!=0)||!first||!last||!from||!to||*from>=*first||*first>*last||*last>=*to)return common_failure("PrivateShopRentCachedAllocationMismatch",key);
    for(const auto& s:all){auto id=serial(s.last,n);if(s.actor==actor&&s.command==result.kind&&id&&*id>*last&&*id<*to)return common_failure("PrivateShopRentCachedAllocationMismatch",key);}return common_success();
}
}
bool private_shop_daily_rent_command_kind(const StableId& kind){return kind=="private_shop_pay_rent_arrear"||kind=="private_shop_resume_rent";}
bool private_shop_daily_rent_fact_id(const StableId& id){for(const auto* p:{private_shop_rent_assessed_kind,private_shop_rent_paid_kind,private_shop_rent_resumed_kind})if(id.rfind(StableId{p}+":",0)==0)return true;return false;}
Status validate_private_shop_daily_rent_cached_results(const CommonStore& c,const PrivateShopDailyRentState* state,ProjectionWriter* projection){
    auto layout=validate_private_shop_daily_rent_layout(c,state);if(!layout.ok)return layout;for(const auto& h:c.state().history)if(private_shop_daily_rent_command_kind(h.kind)&&!state)return common_failure("PrivateShopDailyRentProfileRequired",h.result_id);const auto known=state?sources(*state):std::vector<Source>{};std::set<StableId> used;
    if(state){
        if(state->player_results.size()!=known.size())return common_failure("PrivateShopRentResultCoverageMismatch");std::set<StableId> result_ids;std::set<std::pair<StableId,Integer>> requests;auto chronological=known;std::sort(chronological.begin(),chronological.end(),[&](const Source& a,const Source& b){return serial(a.first,c.state().next_instance_id).value_or(0)<serial(b.first,c.state().next_instance_id).value_or(0);});Integer prior_result=0,prior_revision=0;std::map<StableId,Integer> last_requests;
        for(const auto& source:chronological){const auto* b=state->player_results.find(source.id);const auto* seq=b?c.state().request_sequences.find(b->actor_id):nullptr;auto first=serial(source.first,c.state().next_instance_id),last=serial(source.last,c.state().next_instance_id);auto result=b?serial(b->result_id,c.state().next_instance_id):std::optional<Integer>{};if(!b||b->id!=source.id||b->actor_id!=source.actor||b->command_kind!=source.command||b->tick!=source.tick||b->tick<0||b->tick>c.tick()||!seq||b->request_seq<1||b->request_seq>2147483647||b->request_seq>*seq||b->revision<1||b->revision>c.revision()||b->result_id.rfind("result:",0)!=0||!first||!last||!result||*first>*last||*first<=prior_result||*last>=*result||b->revision<=prior_revision||b->request_seq<=last_requests[b->actor_id]||!result_ids.insert(b->result_id).second||!requests.insert({b->actor_id,b->request_seq}).second)return common_failure("InvalidPrivateShopRentResultBinding",source.id);
            prior_result=*result;prior_revision=b->revision;last_requests[b->actor_id]=b->request_seq;
            for(const auto& h:c.state().history)if(h.result_id==b->result_id&&(h.actor_id!=b->actor_id||h.kind!=b->command_kind||h.tick!=b->tick))return common_failure("PrivateShopRentBoundHistoryMismatch",source.id);
            const auto key=b->actor_id+":"+std::to_string(b->request_seq);const auto* cached=c.state().results.find(key);if(cached&&(!*cached||(*cached)->result_id!=b->result_id||(*cached)->kind!=b->command_kind||(*cached)->tick!=b->tick||(*cached)->revision!=b->revision))return common_failure("PrivateShopRentBoundCacheMismatch",source.id);
            for(const auto& entry:c.state().results.rows())if(entry.second&&entry.second->result_id==b->result_id&&entry.first!=key)return common_failure("PrivateShopRentBoundCacheMismatch",source.id);
        }
    }
    for(const auto& row:c.state().results.rows())if(row.second){const auto& r=*row.second;const auto* effect=r.details?std::get_if<InteractionEffect>(&r.details->data):nullptr;const auto* started=r.details?std::get_if<ActionStarted>(&r.details->data):nullptr;bool tagged=effect&&private_shop_daily_rent_fact_id(effect->settlement_result);if(started&&private_shop_daily_rent_fact_id(started->action_id))tagged=true;const bool command=private_shop_daily_rent_command_kind(r.kind);if(!command&&!tagged)continue;
        if(!state||!command||!r.ok||!effect||!private_shop_daily_rent_fact_id(effect->settlement_result)||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return common_failure("InvalidPrivateShopRentReceipt",row.first);
        const Source* source=nullptr;for(const auto& s:known)if(s.id==effect->settlement_result){if(source)return common_failure("DuplicatePrivateShopRentCachedSource",s.id);source=&s;}if(!source||!used.insert(source->id).second)return common_failure("InvalidPrivateShopRentCachedSource",effect->settlement_result);
        const auto* binding=state->player_results.find(source->id);if(!binding||binding->result_id!=r.result_id||binding->command_kind!=r.kind||binding->revision!=r.revision||binding->tick!=r.tick||row.first!=binding->actor_id+":"+std::to_string(binding->request_seq))return common_failure("PrivateShopRentBoundCacheMismatch",source->id);
        const auto* head=c.state().facts.find(source->id);if(!head)return common_failure("PrivateShopRentCachedFactMissing",source->id);if(projection){auto payload=validate_private_shop_daily_rent_fact_payload(*projection,c,*state,*head);if(!payload.ok)return payload;}else{auto payload=encode_private_shop_daily_rent_fact_payload(c,*state,*head);if(!payload.ok)return {false,{},payload.error};}auto valid=interval(c,row.first,r,*source,known);if(!valid.ok)return valid;
    }return common_success();
}
// 永久提交身份不能证明当时受控者；在独立控制链通过后以原业务头逐一读取，不借今日metadata。
Status validate_private_shop_daily_rent_player_history(const CommonStore& c,const PrivateShopDailyRentState* state,const PrivateShopRentHistoricalPlayerRead& read,ProjectionWriter* projection){
    auto valid=validate_private_shop_daily_rent_cached_results(c,state,projection);if(!valid.ok||!state)return valid;
    for(const auto& source:sources(*state)){
        const auto* head=c.state().facts.find(source.id);const auto* expected=source.command=="private_shop_pay_rent_arrear"?private_shop_rent_paid_kind:private_shop_rent_resumed_kind;
        if(!head||head->id!=source.id||head->kind!=expected||head->domain!=private_shop_daily_rent_module||source.tick<0||source.tick>c.tick())return common_failure("PrivateShopRentPlayerBoundaryMismatch",source.id);
        if(!read)return common_failure("PrivateShopRentPlayerHistoryUnavailable",source.id);
        auto actor=read(source.tick,source.id);if(!actor.ok)return {false,{},actor.error};if(actor.value!=source.actor)return common_failure("PrivateShopRentHistoricalPlayerMismatch",source.id);
    }return common_success();
}
Outcome<std::vector<StableId>> private_shop_daily_rent_new_command_sources(const PrivateShopDailyRentState& before,const PrivateShopDailyRentState& after){
    auto fail=[](const StableId& id)->Outcome<std::vector<StableId>>{return {false,{},common_failure("PrivateShopRentPriorSourceChanged",id).error};};if(before.version!=1||after.version!=1||before.last_step_tick>after.last_step_tick)return fail("version");
    auto unchanged=[](const auto& left,const auto& right)->StableId{for(const auto& row:left.rows()){const auto* next=right.find(row.first);if(!next||!compare_boundary(dynamic_economy_codec::encode(row.second),dynamic_economy_codec::encode(*next)).exact())return row.first;}return {};};
    for(const auto& id:{unchanged(before.assessments,after.assessments),unchanged(before.payments,after.payments),unchanged(before.resumptions,after.resumptions),unchanged(before.player_results,after.player_results)})if(!id.empty())return fail(id);
    const auto prior=sources(before),current=sources(after);if(before.player_results.size()!=prior.size()||after.player_results.size()!=before.player_results.size())return fail("player_results");std::set<StableId> old;for(const auto& source:prior){if(!before.player_results.find(source.id))return fail(source.id);old.insert(source.id);}std::vector<StableId> out;for(const auto& source:current)if(!old.count(source.id)){if(after.player_results.find(source.id))return fail(source.id);out.push_back(source.id);}return {true,std::move(out),{}};
}
Status bind_private_shop_daily_rent_result_in_candidate(CommonStore& c,PrivateShopDailyRentState& state,const std::vector<StableId>& ids,const StableId& actor,Integer sequence,const CommitResult& result){
    if(ids.empty()){if(private_shop_daily_rent_command_kind(result.kind))return common_failure("PrivateShopRentNewCommandSourceRequired");return validate_private_shop_daily_rent_cached_results(c,&state);}
    const auto* cached=c.state().results.find(actor+":"+std::to_string(sequence));const auto* seq=c.state().request_sequences.find(actor);if(ids.size()!=1||!cached||cached->get()!=&result||!seq||*seq!=sequence||result.tick!=c.tick()||result.revision!=c.revision()||c.state().history.empty())return common_failure("PrivateShopRentActualResultRequired",result.result_id);const auto& h=c.state().history.back();if(h.result_id!=result.result_id||h.actor_id!=actor||h.kind!=result.kind||h.tick!=result.tick)return common_failure("PrivateShopRentActualHistoryRequired",result.result_id);
    const auto known=sources(state);const Source* exact=nullptr;for(const auto& source:known)if(source.id==ids.front())exact=&source;if(!exact||exact->actor!=actor||exact->command!=result.kind||exact->tick!=result.tick||state.player_results.find(exact->id))return common_failure("PrivateShopRentNewCommandSourceRequired",ids.front());
    for(const auto& source:known)if(source.id!=exact->id&&!state.player_results.find(source.id))return common_failure("PrivateShopRentUnboundSource",source.id);
    auto previous=state.player_results;if(!state.player_results.insert(exact->id,{exact->id,actor,result.kind,result.result_id,sequence,result.revision,result.tick}))return common_failure("PrivateShopRentDuplicateResultBinding",exact->id);auto valid=validate_private_shop_daily_rent_cached_results(c,&state);if(!valid.ok)state.player_results=std::move(previous);return valid;
}

}
