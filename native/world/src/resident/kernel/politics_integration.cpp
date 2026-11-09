// 政治具名命令进入中央候选前后均复验；报价、事实和缓存回执在同一业务叶闭合。
#include "politics_integration.hpp"
#include "command_types.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// 读票必须比领域 lease 后销毁；不把工厂栈上的 context 指针交给调用方。
struct PoliticalRuntimeReadAccess {
    std::unique_ptr<PhaseCurrentReadScope> phase;
    std::shared_ptr<const PoliticalExactReadLease> lease;
    std::optional<PoliticalOrganizationRuntimeReadContext> runtime;
};
struct PoliticalRuntimeWriteAccess {
    std::unique_ptr<PhaseCurrentReadScope> phase;
    std::shared_ptr<PoliticalExactWriteLease> lease;
    const PoliticalOrganizationRuntimeWriteContext* runtime=nullptr;
};
// 只给真实完整模式开读阶段；已有领域阶段继续使用原票与原实际时刻。
Outcome<std::unique_ptr<PhaseCurrentReadScope>> political_read_phase(
    const WorldBundle& world,const WorldMoment& at) {
    if(world.has_active_read_phase())return {true,{},{}};
    return world.enter_phase_read_context("political_business","organization",{at,at},at);
}
// 入口上下文的各根必须来自该世界，不能用同名组织接入另一个候选。
Status political_context_world(const PoliticsReadContext& context,const WorldBundle& world) {
    if(!world.catalog||&context.common!=&world.common||
       &context.politics!=&world.politics||&context.organization!=&world.organization||
       &context.economy!=&world.local_economy||&context.social!=&world.social||
       &context.activity!=static_cast<const ActivityRead*>(&world.services)||
       &context.common_catalog!=world.catalog->common.get()||
       &context.organization_catalog!=world.catalog->organization.get()||
       &context.politics_catalog!=world.catalog->politics.get())
        return common_failure("PoliticalBusinessWorldMismatch",{},ErrorKind::internal);
    return common_success();
}
// 旧模式不建立完整图；完整模式绝不以空 runtime 或虚拟资格来源降级。
Outcome<PoliticalRuntimeReadAccess> political_runtime(const PoliticsReadContext& context) {
    PoliticalRuntimeReadAccess out;
    if(!context.organization_catalog.authority_lifecycle_enabled&&
       !context.organization_catalog.exact_fact_clock_enabled)return {true,std::move(out),{}};
    if(!context.original_world)return {false,{},common_failure("PoliticalRuntimeRequired",{},ErrorKind::unsupported_capability).error};
    const auto& world=*context.original_world;
    auto checked=political_context_world(context,world);if(!checked.ok)return {false,{},checked.error};
    auto at=world.actual_phase_moment();if(!at.ok)return {false,{},at.error};
    auto phase=political_read_phase(world,at.value);if(!phase.ok)return {false,{},phase.error};
    out.phase=std::move(phase.value);
    auto lease=bind_original_political_exact_read(world,at.value,native_political_exact_world_factory());
    if(!lease.ok)return {false,{},lease.error};out.lease=std::move(lease.value);
    const auto binding=out.lease->bindings();const auto& original=binding.organization;
    out.runtime.emplace(PoliticalOrganizationRuntimeReadContext{
        original.runtime,original.clock,original.office_sources.stamp,
        original.office_sources.transitions,binding.qualifications});
    return {true,std::move(out),{}};
}
// writer 与其只读预检共享同一实际 lease，不重新获取另一张身份票。
Outcome<PoliticalRuntimeWriteAccess> political_runtime(PoliticsWriteContext& context) {
    PoliticalRuntimeWriteAccess out;
    if(!context.organization_catalog.authority_lifecycle_enabled&&
       !context.organization_catalog.exact_fact_clock_enabled)return {true,std::move(out),{}};
    if(!context.candidate_world)return {false,{},common_failure("PoliticalRuntimeRequired",{},ErrorKind::unsupported_capability).error};
    auto& world=*context.candidate_world;
    auto checked=political_context_world(context.read(),world);if(!checked.ok)return {false,{},checked.error};
    auto at=world.actual_phase_moment();if(!at.ok)return {false,{},at.error};
    auto phase=political_read_phase(world,at.value);if(!phase.ok)return {false,{},phase.error};
    out.phase=std::move(phase.value);
    auto lease=bind_original_political_exact_write(world,at.value,native_political_exact_world_factory());
    if(!lease.ok)return {false,{},lease.error};out.lease=std::move(lease.value);
    out.runtime=&out.lease->bindings().politics_runtime;
    return {true,std::move(out),{}};
}
// 成功、拒绝与异常都结束实际写票，下一 consumer 必须重取。
struct PoliticalWriterBoundary {
    WorldBundle* world;
    ~PoliticalWriterBoundary(){if(world)world->note_write_boundary();}
};
// 两份签发身份必须逐字段一致，不能只比同一个组织名称。
bool authority_equal(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch&&a.actor_id==b.actor_id;}
// 报价冻结完整原选择；同revision也不能换候选、换政策或将拒绝改成接受。
bool command_equal(const PoliticsCommand& a,const PoliticsCommand& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);if constexpr(std::is_same_v<T,PoliticalAppointCommand>)return authority_equal(x.authority,y.authority)&&x.jurisdiction_id==y.jurisdiction_id&&x.target_actor_id==y.target_actor_id&&x.policy_id==y.policy_id&&x.expected_jurisdiction_revision==y.expected_jurisdiction_revision&&x.expected_office_epoch==y.expected_office_epoch;else if constexpr(std::is_same_v<T,PoliticalDismissCommand>)return authority_equal(x.authority,y.authority)&&x.jurisdiction_id==y.jurisdiction_id&&x.expected_jurisdiction_revision==y.expected_jurisdiction_revision&&x.expected_office_epoch==y.expected_office_epoch;else if constexpr(std::is_same_v<T,PoliticalWithdrawCommand>)return authority_equal(x.authority,y.authority)&&x.appointment_id==y.appointment_id&&x.expected_revision==y.expected_revision;else{const bool same=x.actor_id==y.actor_id&&x.appointment_id==y.appointment_id&&x.expected_revision==y.expected_revision&&x.source_fact_id==y.source_fact_id;if constexpr(std::is_same_v<T,PoliticalAnswerAppointmentCommand>)return same&&x.accept==y.accept;return same;}},a);}
bool identity_equal(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 读取版本只记录实际参与人物及指定公库，不包含钱包余额或第二份资源状态。
void add_version(const CommonStore& c,std::vector<ReadVersion>& out,const ResourceKey& key){for(const auto& r:out)if(r.resource.kind==key.kind&&r.resource.id==key.id)return;out.push_back(c.read_version(key));}
// 组织变权事实必须唯一、同tick且由这条政治结果直接引起。
const OrganizationFact* office_transfer(const OrganizationState& org,const PoliticsFact& source){const OrganizationFact* found=nullptr;for(const auto& row:org.facts.rows()){const auto& f=row.second;if(f.kind!="organization_office_changed"||f.cause_fact_ids!=std::vector<StableId>{source.id})continue;if(found||f.organization_id!=source.organization_id||f.office_id!=source.office_id||f.tick!=source.tick)return nullptr;found=&f;}return found;}
// 所有业务事实都必须落在该命令的全局分配区间，不能交换同tick的合法回执。
Integer ordinal(const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return 0;const auto digits=id.substr(at+1);Integer value=0;const auto result=std::from_chars(digits.data(),digits.data()+digits.size(),value);return result.ec==std::errc{}&&result.ptr==digits.data()+digits.size()&&value>0&&std::to_string(value)==digits?value:0;}
// 新实例保留原全局分配顺序；冷恢复不接受悬空或倒退的计数器。
bool instance_id(const StableId& id,const StableId& prefix,Integer next){if(id.rfind(prefix+":",0)!=0)return false;const auto digits=id.substr(prefix.size()+1);Integer value=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&value<next&&std::to_string(value)==digits;}
}
PoliticsReadContext PoliticsWriteContext::read()const{
    PoliticsReadContext out{common,common_catalog,politics,politics_catalog,organization,organization_catalog,economy,social,activity,navigation,view_index,dismissal_preview};
    out.original_world=candidate_world;return out;
}
bool is_politics_command_kind(const StableId& kind){return kind=="political_appoint"||kind=="political_answer_appointment"||kind=="political_arrive"||kind=="political_withdraw_appointment"||kind=="political_dismiss";}
// 只有已获取的真实 lease 调用本叶；不把借入 runtime 暴露成公开上下文旁路。
static Outcome<PoliticsQuote> preview_political_business_bound(const PoliticsReadContext& w,const StableId& actor,const PoliticsCommand& command,const PoliticalOrganizationRuntimeReadContext* runtime){auto valid=preview_politics_command(w.common,w.politics,w.politics_catalog,w.organization,w.organization_catalog,w.economy,w.activity,actor,command,runtime);if(valid.ok&&w.dismissal_preview)if(const auto* dismiss=std::get_if<PoliticalDismissCommand>(&command))valid=w.dismissal_preview(actor,*dismiss);if(!valid.ok)return {false,{},valid.error};PoliticsQuote quote;quote.identity=w.common.state().identity;quote.actor_id=actor;quote.command=command;add_version(w.common,quote.resource_versions,{ResourceKey::Kind::actor,actor});StableId jurisdiction;
    auto source=std::visit([&](const auto& c)->Status{using T=std::decay_t<decltype(c)>;if constexpr(std::is_same_v<T,PoliticalAppointCommand>){jurisdiction=c.jurisdiction_id;add_version(w.common,quote.resource_versions,{ResourceKey::Kind::actor,c.target_actor_id});}else if constexpr(std::is_same_v<T,PoliticalDismissCommand>){jurisdiction=c.jurisdiction_id;const auto* j=w.politics.jurisdictions.find(jurisdiction);const auto* office=w.organization.offices.find(j->office_id);if(office->holder_actor_id)add_version(w.common,quote.resource_versions,{ResourceKey::Kind::actor,*office->holder_actor_id});}else{const auto* p=w.politics.appointments.find(c.appointment_id);jurisdiction=p->jurisdiction_id;add_version(w.common,quote.resource_versions,{ResourceKey::Kind::actor,p->actor_id});if constexpr(!std::is_same_v<T,PoliticalWithdrawCommand>){auto known=read_political_fact_known(w.common,w.politics,w.social,actor,c.source_fact_id,runtime?&runtime->clock:nullptr);if(!known.ok)return {false,{},known.error};if(!known.value)return common_failure("PoliticalAppointmentNotKnown",c.source_fact_id);}}return common_success();},command);if(!source.ok)return {false,{},source.error};const auto* j=w.politics.jurisdictions.find(jurisdiction);add_version(w.common,quote.resource_versions,{ResourceKey::Kind::wallet,j->public_wallet_id});for(const auto& id:j->public_inventory_ids)add_version(w.common,quote.resource_versions,{ResourceKey::Kind::inventory,id});return {true,std::move(quote),{}};}
// 预览拥有本次原 lease，结果只保原报价，不留下任何来源指针。
Outcome<PoliticsQuote> preview_political_business(const PoliticsReadContext& w,const StableId& actor,const PoliticsCommand& command){
    auto sources=political_runtime(w);if(!sources.ok)return {false,{},sources.error};
    return preview_political_business_bound(w,actor,command,sources.value.runtime?&*sources.value.runtime:nullptr);
}
Outcome<PoliticsEffects> apply_political_business(PoliticsWriteContext& w,const StableId& actor,const PoliticsCommand& command,const PoliticsQuote& quote){if(quote.actor_id!=actor||quote.duration!=0||!identity_equal(quote.identity,w.common.state().identity)||!command_equal(quote.command,command))return {false,{},common_failure("StalePoliticsQuote").error};auto sources=political_runtime(w);if(!sources.ok)return {false,{},sources.error};auto read=w.read();std::optional<PoliticalOrganizationRuntimeReadContext> runtime;if(sources.value.runtime)runtime.emplace(sources.value.runtime->read());auto current=preview_political_business_bound(read,actor,command,runtime?&*runtime:nullptr);if(!current.ok)return {false,{},current.error};if(current.value.resource_versions.size()!=quote.resource_versions.size())return {false,{},common_failure("StalePoliticsQuote").error};for(std::size_t n=0;n<quote.resource_versions.size();++n){const auto& a=quote.resource_versions[n];const auto& b=current.value.resource_versions[n];if(a.resource.kind!=b.resource.kind||a.resource.id!=b.resource.id||a.version!=b.version)return {false,{},common_failure("StalePoliticsQuote").error};}PoliticalWriterBoundary boundary{w.candidate_world};if(const auto* dismiss=std::get_if<PoliticalDismissCommand>(&command);dismiss&&w.dismissal_preview){if(!w.dismissal_apply)return {false,{},common_failure("PoliticalDismissalSourceWriterRequired").error};return w.dismissal_apply(actor,*dismiss);}return apply_politics_command_in_transaction(w.common,w.politics,w.politics_catalog,w.organization,w.organization_catalog,w.economy,w.activity,actor,command,sources.value.runtime);}
Outcome<BoundaryValue> query_political_business(const PoliticsReadContext& w,const StableId& actor,const StableId& place){auto sources=political_runtime(w);if(!sources.ok)return {false,{},sources.error};return query_politics_entries(w.common,w.common_catalog,w.politics,w.politics_catalog,w.organization,w.organization_catalog,w.economy,w.social,w.activity,actor,place,w.navigation,w.view_index,w.dismissal_preview,sources.value.runtime?&*sources.value.runtime:nullptr);}
Status bootstrap_political_business(PoliticsWriteContext& w){if(!w.politics.jurisdictions.empty()||!w.politics.appointments.empty()||!w.politics.facts.empty())return common_failure("PoliticsAlreadyInitialized");auto initial=make_initial_politics(w.common,w.common_catalog,w.politics_catalog,w.organization,w.organization_catalog);if(!initial.ok)return {false,{},initial.error};w.politics=std::move(initial.value);return common_success();}
Outcome<PoliticsEffects> finish_political_business(PoliticsWriteContext& w){return reconcile_politics(w.common,w.politics,w.politics_catalog,w.organization,w.organization_catalog,w.economy);}
Status validate_politics_receipt_source(const CommonStore& common,const PoliticsState& state,const OrganizationState& org,const StableId& actor,const StableId& kind,Integer tick,const PoliticsEffects& effects){auto bad=[](){return common_failure("PoliticalReceiptSourceMismatch");};if(!is_politics_command_kind(kind)||effects.fact_ids.empty()||effects.receipt_ids!=effects.fact_ids)return bad();const auto* first=state.facts.find(effects.fact_ids.front());if(!first||first->tick!=tick||first->decision_actor_id!=actor||!common.state().facts.find(first->id))return bad();std::vector<StableId> expected{first->id};if(kind=="political_dismiss"){if(first->kind!="political_office_dismissed"||!effects.appointment_id.empty())return bad();const auto* transfer=office_transfer(org,*first);if(!transfer||!transfer->actor_id.empty())return bad();expected.push_back(transfer->id);}else{const auto* appointment=state.appointments.find(effects.appointment_id);if(!appointment||first->appointment_id!=appointment->id)return bad();if(kind=="political_appoint"){if(first->kind!="political_appointment_offered"||appointment->issuer_actor_id!=actor||appointment->offered_fact_id!=first->id)return bad();}else if(kind=="political_answer_appointment"){if((first->kind!="political_appointment_accepted"&&first->kind!="political_appointment_declined")||appointment->actor_id!=actor)return bad();}else if(kind=="political_arrive"){if(first->kind!="political_appointment_arrived"||appointment->actor_id!=actor)return bad();const auto* transfer=office_transfer(org,*first);if(!transfer||transfer->actor_id!=actor)return bad();expected.push_back(transfer->id);}else if(first->kind!="political_appointment_withdrawn")return bad();}if(effects.fact_ids!=expected)return bad();for(const auto& id:expected)if(!common.state().facts.find(id))return bad();return common_success();}
// 缓存只是现有世界历史的投影；字段形状相同但来源不符的回执必须拒绝。
Status validate_political_business(const PoliticsReadContext& w,bool capability,const PoliticsReceiptReader& receipts){return validate_political_business(w,capability,receipts,nullptr);}
Status validate_political_business(const PoliticsReadContext& w,bool capability,const PoliticsReceiptReader& receipts,const PoliticalTerritoryRead* territories){if(capability!=w.politics_catalog.enabled)return common_failure("PoliticalProfileMismatch");auto state=validate_politics(w.common,w.common_catalog,w.politics,w.politics_catalog,w.organization,w.organization_catalog,territories);if(!state.ok)return state;return validate_political_business_receipts(w,capability,receipts);}
// 不接受调用者自报正文已验；调用入口独立且仍逐条反读原缓存、事实、历史和分配区间。
Status validate_political_business_receipts(const PoliticsReadContext& w,bool capability,const PoliticsReceiptReader& receipts){if(capability!=w.politics_catalog.enabled)return common_failure("PoliticalProfileMismatch");for(const auto& row:w.politics.appointments.rows())if(!instance_id(row.first,"appointment",w.common.state().next_instance_id))return common_failure("InstanceCounterCollision",row.first);for(const auto& row:w.politics.facts.rows())if(!instance_id(row.first,"politics",w.common.state().next_instance_id))return common_failure("InstanceCounterCollision",row.first);std::set<StableId> seen;
    for(const auto& row:w.common.state().results.rows()){if(!row.second)continue;const auto& receipt=*row.second;const auto* effect=receipts?receipts(receipt):nullptr;const bool political=is_politics_command_kind(receipt.kind);if(!effect){if(political)return common_failure("PoliticalReceiptSourceMismatch",row.first);continue;}if(!capability||!political)return common_failure("PoliticalReceiptSourceMismatch",row.first);const auto split=row.first.rfind(':');if(split==StableId::npos)return common_failure("PoliticalReceiptSourceMismatch",row.first);const auto actor=row.first.substr(0,split),digits=row.first.substr(split+1);Integer request=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* sequence=w.common.state().request_sequences.find(actor);if(!sequence||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>*sequence||std::to_string(request)!=digits)return common_failure("PoliticalReceiptSourceMismatch",row.first);const HistoryRecord* history=nullptr;const HistoryRecord* previous=nullptr;const HistoryRecord* before=nullptr;for(const auto& item:w.common.state().history){if(item.result_id==receipt.result_id){if(history)return common_failure("PoliticalReceiptHistoryMismatch",row.first);history=&item;before=previous;}previous=&item;}if(!history||history->actor_id!=actor||history->kind!=receipt.kind||history->tick!=receipt.tick)return common_failure("PoliticalReceiptHistoryMismatch",row.first);auto source=validate_politics_receipt_source(w.common,w.politics,w.organization,actor,receipt.kind,receipt.tick,*effect);if(!source.ok)return source;const auto upper=ordinal(receipt.result_id),lower=before?ordinal(before->result_id):0;if(upper<=lower||(before&&lower==0))return common_failure("PoliticalReceiptHistoryMismatch",row.first);for(const auto& id:effect->fact_ids)if(ordinal(id)<=lower||ordinal(id)>=upper)return common_failure("PoliticalReceiptAllocationMismatch",row.first);if(!seen.insert(effect->fact_ids.front()).second)return common_failure("DuplicatePoliticalCachedReceipt",row.first);}
    return common_success();
}
} // 幂等request_seq与最终整候选发布仍使用中央原协议；本叶不另建命令缓存。
