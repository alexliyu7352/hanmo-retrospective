// 页上接受、拒绝和离职均调用原业务报价；只表达本人已可见的合同与当前会面邀约。
#include "task_employment_view.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
#include <set>
namespace hanmo::resident { namespace {
using V=BoundaryValue;using A=V::Array;using D=V::Dict;
constexpr Integer page_size=16;
// 不接受另一世界、旧恢复epoch或旧半日的私产来源。
bool same(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 身份编码与原生有限页保持同一格式，前端只借此使旧按钮失效。
V identity(const WorldIdentity& source){StableId id;constexpr char hex[]="0123456789abcdef";for(auto byte:source.world_id){id+=hex[byte>>4];id+=hex[byte&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{source.execution_epoch}},{"revision",V{source.revision}},{"tick",V{source.tick}}});}
// 条款为已冻结分数工资，不用当下产量或私钱包余额推测报酬。
StableId terms(const StableId& job,Integer numerator,Integer denominator,Integer ticks,Integer cap){const StableId name=(job=="farm"||job=="labor")?"耕作":job=="guard"?"守备":job=="manager"?"管事":job;return name+" · 每实际到岗半日 "+std::to_string(numerator)+(denominator==1?"":("/"+std::to_string(denominator)))+" 钱；接受后期限 "+std::to_string(ticks)+" 个半日，按实际到岗结薪，缺勤不延长；工资预约上限 "+std::to_string(cap)+" 钱。";}
// 原状态只转文案；未结工资不因终止文案被说成已付清。
StableId status_text(const StableId& status){if(status=="offered")return "待本人决定";if(status=="active")return "约定履行中";if(status=="decline")return "本人已拒绝";if(status=="withdraw")return "雇主已撤回";if(status=="leave")return "本人已离职";if(status=="dismiss")return "雇主已辞退";if(status=="completed")return "服务期已结束";if(status=="expired")return "合同已到期";if(status=="employer_dead"||status=="worker_dead")return "原雇用关系已终止";return "原合同记录";}
// 只编码固定三类参数，不把快照中的钱包、工资或事实ID传回成可修改输入。
std::pair<StableId,V> command_value(const EstateEmploymentCommand& command){if(const auto* offer=std::get_if<EstateEmploymentOffer>(&command))return {"estate_employment_offer",boundary::obj({{"policy_id",V{offer->policy_id}},{"worker_actor_id",V{offer->worker_actor_id}}})};if(const auto* request=std::get_if<EstateEmploymentRequestOffer>(&command))return {"estate_employment_request_offer",boundary::obj({{"policy_id",V{request->policy_id}}})};const auto& decision=std::get<EstateEmploymentDecision>(command);return {"estate_employment_decide",boundary::obj({{"contract_id",V{decision.contract_id}},{"decision",V{decision.decision}}})};}
// 成功报价还须回显本人、原命令与完整来源；拒绝保留原码，不生成假成功按钮。
Outcome<V> action(const CommonStore& common,const StableId& actor,const TaskEmploymentPreview& preview,
    const EstateEmploymentCommand& request,const StableId& text,const StableId& summary){
    const auto command=command_value(request);auto quoted=preview(actor,request);
    if(quoted.ok){const auto echoed=command_value(quoted.value.command);if(!same(quoted.value.source_identity,common.state().identity)||quoted.value.actor_id!=actor||echoed.first!=command.first||!compare_boundary(echoed.second,command.second).exact())return {false,{},common_failure("EmploymentPreviewSourceMismatch").error};}
    return {true,boundary::obj({{"command",V{command.first}},{"params",command.second},{"text",V{text}},{"summary",V{summary}},{"available",V{quoted.ok}},{"code",V{quoted.ok?StableId{}:quoted.error.original.code}}}),{}};
}
}
Outcome<BoundaryValue> query_task_employment(const CommonStore& common,const CommonCatalog& catalog,
    const InteractionState& interaction,const EstateEmploymentRead& read,const TaskEmploymentPreview& preview,
    Integer cursor,Integer policy_cursor){
    const auto failed=[](const StableId& code)->Outcome<V>{return {false,{},common_failure(code,"task_employment").error};};
    const auto& observer=common.state().metadata.player_actor_id;
    if(!common.actor(observer))return failed("UnknownPlayer");if(!preview)return failed("EmploymentPreviewUnavailable");
    if(cursor<0||policy_cursor<0||cursor>std::numeric_limits<Integer>::max()-page_size)return failed("InvalidEmploymentCursor");
    auto ids=read.estate_employment_contract_ids(observer,cursor,page_size);if(!ids.ok)return {false,{},ids.error};if(ids.value.size()>static_cast<std::size_t>(page_size))return failed("EmploymentPageLimit");
    const InteractionSession* meeting=nullptr;for(const auto& session:interaction.interactions)if(session.actor_id==observer){if(meeting)return failed("DuplicateInteraction");meeting=&session;}
    A rows,offers;std::set<StableId> seen;CommonPresentation names(common.state(),catalog);
    for(const auto& id:ids.value){
        if(id.empty()||!seen.insert(id).second)return failed("InvalidEmploymentSourceIndex");
        auto source=read.estate_employment_source(observer,id);if(!source.ok)return {false,{},source.error};if(!source.value)return failed("MissingEmploymentSource");const auto& value=*source.value;const auto& frozen=value.terms;
        const bool employer=frozen.employer_actor_id==observer,worker=frozen.worker_actor_id==observer;
        if(!same(value.source_identity,common.state().identity)||value.observer_actor_id!=observer||value.contract_id!=id||value.observed_tick!=common.tick()||employer==worker
            ||frozen.wage_numerator<0||frozen.wage_denominator<=0||frozen.service_ticks<=0||frozen.reservation_cap<0||value.served_ticks<0||value.served_ticks>frozen.service_ticks
            ||value.earned_cash<0||value.paid_cash<0||value.paid_cash>value.earned_cash||value.reserved_cash<0)return failed("EmploymentViewSourceMismatch");
        auto other=names.display_name(employer?frozen.worker_actor_id:frozen.employer_actor_id);if(!other.ok)return {false,{},other.error};const auto text=terms(frozen.job,frozen.wage_numerator,frozen.wage_denominator,frozen.service_ticks,frozen.reservation_cap);
        A actions;
        std::vector<std::pair<StableId,StableId>> decisions;
        if(value.status=="offered")decisions=worker?std::vector<std::pair<StableId,StableId>>{{"accept","本人接受"},{"decline","本人拒绝"}}:std::vector<std::pair<StableId,StableId>>{{"withdraw","撤回邀约"}};
        if(value.status=="offered"&&employer&&meeting&&meeting->phase=="say"&&meeting->context.provider_actor_id==frozen.worker_actor_id)decisions.emplace_back("request_reply","询问对方本人答复");
        else if(value.status=="active")decisions=worker?std::vector<std::pair<StableId,StableId>>{{"leave","本人提出离职"}}:std::vector<std::pair<StableId,StableId>>{{"dismiss","终止这份雇用"}};
        for(const auto& decision:decisions){const auto summary=(employer?"受雇人：":"雇主：")+other.value+"。"+text+" 已赚未付款仍由原工资账保留，离职或辞退不消除此债。";auto button=action(common,observer,preview,EstateEmploymentDecision{id,decision.first},decision.second,summary);if(!button.ok)return {false,{},button.error};actions.push_back(std::move(button.value));}
        rows.push_back(boundary::obj({{"id",V{id}},{"kind",V{StableId{"contract"}}},{"display_name",V{(employer?"雇用 ":"受雇于 ")+other.value}},{"status",V{value.status}},{"status_text",V{status_text(value.status)}},{"terms_text",V{text}},
            {"progress_text",V{"已到岗 "+std::to_string(value.served_ticks)+" 次（最多 "+std::to_string(frozen.service_ticks)+" 次）；已赚 "+std::to_string(value.earned_cash)+" 钱，实付 "+std::to_string(value.paid_cash)+" 钱，未付 "+std::to_string(value.earned_cash-value.paid_cash)+" 钱；当前预约 "+std::to_string(value.reserved_cash)+" 钱。"}},
            {"served_ticks",V{value.served_ticks}},{"earned_cash",V{value.earned_cash}},{"paid_cash",V{value.paid_cash}},{"reserved_cash",V{value.reserved_cash}},{"actions",V{std::move(actions)}}}));
    }
    bool more_policies=false;
    if(meeting&&(meeting->phase=="say"||meeting->phase=="choice"||meeting->phase=="action")){
        auto policies=read.estate_employment_policies(observer,meeting->context.provider_actor_id);if(!policies.ok)return {false,{},policies.error};
        if(policies.value.size()>50||static_cast<std::size_t>(policy_cursor)>policies.value.size())return failed("InvalidEmploymentPolicyCursor");
        std::set<StableId> policy_ids;for(const auto& value:policies.value)if(value.policy_id.empty()||!policy_ids.insert(value.policy_id).second||!same(value.source_identity,common.state().identity)||value.observer_actor_id!=observer||value.provider_actor_id!=meeting->context.provider_actor_id||value.wage_numerator<0||value.wage_denominator<=0||value.service_ticks<=0||value.offer_valid_ticks<=0||value.reservation_cap<0||value.employer_actor_id!=(value.provider_offer?value.provider_actor_id:observer)||value.worker_actor_id!=(value.provider_offer?observer:value.provider_actor_id)||value.employer_actor_id==value.worker_actor_id)return failed("EmploymentPolicySourceMismatch");
        const auto end=std::min<std::size_t>(policies.value.size(),static_cast<std::size_t>(policy_cursor)+page_size);more_policies=end<policies.value.size();
        for(auto offset=static_cast<std::size_t>(policy_cursor);offset<end;++offset){const auto& value=policies.value[offset];auto name=names.display_name(value.provider_actor_id);if(!name.ok)return {false,{},name.error};const auto text=terms(value.job,value.wage_numerator,value.wage_denominator,value.service_ticks,value.reservation_cap);const EstateEmploymentCommand request=value.provider_offer?EstateEmploymentCommand{EstateEmploymentRequestOffer{value.policy_id}}:EstateEmploymentCommand{EstateEmploymentOffer{value.policy_id,value.worker_actor_id}};auto button=action(common,observer,preview,request,value.provider_offer?"向雇主询问这份工作":"向本人提出雇用邀约",(value.provider_offer?"雇主：":"受雇人：")+name.value+"。"+text+" 发出时保留约定工资资金；须受雇人自己接受才开始服务。邀约有效 "+std::to_string(value.offer_valid_ticks)+" 个半日。");if(!button.ok)return {false,{},button.error};offers.push_back(boundary::obj({{"id",V{value.policy_id}},{"kind",V{StableId{"policy"}}},{"display_name",V{value.provider_offer?"询问 "+name.value+" 的工作邀约":"向 "+name.value+" 提出雇用"}},{"status_text",V{StableId{"当前当面条款，尚未成立合同"}}},{"terms_text",V{text}},{"actions",V{A{std::move(button.value)}}}}));}
    }else if(policy_cursor!=0)return failed("InvalidEmploymentPolicyCursor");
    D page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{observer}},{"cursor",V{cursor}},{"policy_cursor",V{policy_cursor}},{"rows",V{std::move(rows)}},{"offers",V{std::move(offers)}}};
    if(ids.value.size()==static_cast<std::size_t>(page_size))page.emplace_back("next_cursor",V{cursor+page_size});if(more_policies)page.emplace_back("next_policy_cursor",V{policy_cursor+page_size});return {true,V{std::move(page)},{}};
}
}
