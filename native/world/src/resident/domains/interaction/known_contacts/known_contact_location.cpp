// 同一人物有多条来源时只选真实最后一次亲历；同tick按原fact顺序，不拿今日岗位或位置刷新。
#include "known_contact_location.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_time2.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry.hpp"
#include <limits>
#include <charconv>
#include <map>
namespace hanmo::resident { namespace {
struct Entry {KnownContactLocation value;std::size_t position=0;};
struct Cutoff {std::size_t position=0;Integer serial=0,tick=0;std::optional<WorldMoment> exact_at;};
std::optional<Integer> serial(const StableId& id,Integer next){auto colon=id.find(':');if(colon==StableId::npos)return {};const auto value=id.substr(colon+1);Integer n=0;auto parsed=std::from_chars(value.data(),value.data()+value.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||n<1||n>=next||std::to_string(n)!=value)return {};return n;}
Outcome<std::vector<Entry>> collect(const KnownContactLocationAccess& read,const StableId& observer,const Cutoff* cutoff=nullptr,const DynamicStepInput* phase=nullptr,const StableId* only_subject=nullptr){
    const auto& common=read.common;
    std::optional<WorldMoment> actual_now;if(read.actual_clock){if(phase)return {false,{},common_failure("MixedKnownContactClock",{},ErrorKind::internal).error};auto m=read.actual_clock->read_current_moment();if(!m.ok)return {false,{},m.error};actual_now=std::move(m.value);}
    if(phase&&(phase->from_tick!=common.tick()||common.tick()==INT64_MAX||phase->to_tick!=common.tick()+1))return {false,{},common_failure("InvalidKnownContactPhase").error};
    if(!common.actor(observer))return {false,{},common_failure("UnknownContactObserver").error};
    if(read.appointments){const auto& a=*read.appointments;if(a.state.version==2){const auto*x=read.exact_appointments;if(!actual_now||!x)return {false,{},common_failure("KnownContactAppointmentTime2OwnerRequired",{},ErrorKind::unsupported_capability).error};if(&x->common!=&common||&x->state!=&a.state||&x->catalog!=&a.catalog||&x->interaction!=&a.interaction||&x->definitions!=&a.interaction_catalog)return {false,{},common_failure("KnownContactAppointmentCandidateMismatch",{},ErrorKind::internal).error};}else{if(read.exact_appointments)return {false,{},common_failure("KnownContactAppointmentSelectionMismatch",{},ErrorKind::internal).error};auto valid=validate_personal_appointments(common,a.interaction,a.interaction_catalog,a.catalog,a.state,phase);if(!valid.ok)return {false,{},valid.error};valid=validate_personal_appointment_presence(common,read.common_catalog,a.state,a.death_origins,phase);if(!valid.ok)return {false,{},valid.error};}}
    if(read.messages){const auto& m=*read.messages;auto valid=validate_task_message_handoffs(common,m.tasks,m.catalog,read.common_catalog,m.state,phase);if(!valid.ok)return {false,{},valid.error};}
    std::map<StableId,Entry> latest;std::size_t position=0;
    // 只有typed来源中的两位实际参与者能认领这次接触；原发信人不自动看见远方收件人。
    const auto remember=[&](const StableId& first,const StableId& second,const StableId& place,const StableId& fact,const StableId& kind,Integer tick)->Status{
        if(observer!=first&&observer!=second)return common_success();const auto& subject=observer==first?second:first;if(only_subject&&subject!=*only_subject)return common_success();
        if(subject==observer||!common.actor(subject)||!read.common_catalog.places.find(place)||tick<0||tick>(actual_now?actual_now->half_day_tick:phase?phase->to_tick:common.tick()))return common_failure("InvalidKnownContactSource",fact);
        if(cutoff){auto source_serial=serial(fact,common.state().next_instance_id);if(!source_serial||((*source_serial<cutoff->serial)!=(position<cutoff->position)))return common_failure("KnownContactBoundaryOrderMismatch",fact);if(position>=cutoff->position)return common_success();if(tick>cutoff->tick)return common_failure("KnownContactBoundaryTickMismatch",fact);}
        if(read.exact_appointments&&(kind==personal_appointment_accepted_kind||kind==personal_appointment_cancelled_kind||kind==personal_appointment_started_kind||kind==personal_appointment_finished_kind)){auto m=read_personal_appointment_source_moment(*read.exact_appointments,{personal_appointment_module,kind,fact},2);if(!m.ok)return {false,{},m.error};if(m.value.half_day_tick!=tick)return common_failure("KnownContactOriginalTimeProjectionMismatch",fact,ErrorKind::internal);auto cmp=compare_world_moments(m.value,cutoff&&cutoff->exact_at?*cutoff->exact_at:*actual_now);if(!cmp.ok)return {false,{},cmp.error};if(cmp.value>0)return common_failure("KnownContactOriginalTimeOrderMismatch",fact,ErrorKind::internal);}
        auto found=latest.find(subject);if(found!=latest.end()&&(found->second.position>=position||found->second.value.observed_tick>tick))return common_failure("KnownContactChronologyMismatch",fact);
        latest[subject]={{subject,place,fact,kind,tick,tick},position};return common_success();
    };
    for(const auto& row:common.state().facts.rows()){
        ++position;Status valid=common_success();const auto& h=row.second;
        if(read.appointments&&h.domain==personal_appointment_module){
            const auto& a=read.appointments->state;
            if(const auto* x=a.accepted.find(row.first))valid=remember(x->contact.actor_id,x->contact.provider_actor_id,x->contact.place_id,row.first,h.kind,x->contact.tick);
            else if(const auto* x=a.cancelled.find(row.first))valid=remember(x->contact.actor_id,x->contact.provider_actor_id,x->contact.place_id,row.first,h.kind,x->contact.tick);
            else if(const auto* x=a.started.find(row.first))valid=remember(x->contact.actor_id,x->contact.provider_actor_id,x->contact.place_id,row.first,h.kind,x->contact.tick);
            // 中断只证明旧行动结束，不能据其失败时刻宣称两人仍同地；成功才有新的亲历。
            else if(const auto* x=a.finished.find(row.first);x&&x->success){const auto* start=a.started.find(x->start_fact_id);if(!start)return {false,{},common_failure("MissingKnownContactStart",row.first).error};valid=remember(start->contact.actor_id,start->contact.provider_actor_id,start->contact.place_id,row.first,h.kind,x->tick);}
        }else if(read.messages&&h.kind==task_message_handoff_kind){
            // 完整原段已验；只读取这份实交的两参与者，避免phase期再调用仅current的旧知情口。
            const auto* r=read.messages->state.receipts.find(row.first);if(!r)return {false,{},common_failure("MissingKnownContactHandoff",row.first).error};valid=remember(r->carrier_actor_id,r->recipient_actor_id,r->place_id,r->fact_id,h.kind,r->tick);
        }else if(read.messages&&h.kind==task_message_report_kind){
            const auto* r=read.messages->state.reports.find(row.first);if(!r)return {false,{},common_failure("MissingKnownContactReport",row.first).error};valid=remember(r->carrier_actor_id,r->report_actor_id,r->place_id,r->fact_id,h.kind,r->tick);
        }
        if(!valid.ok)return {false,{},valid.error};
    }
    std::vector<Entry> out;for(auto& row:latest)out.push_back(std::move(row.second));std::sort(out.begin(),out.end(),[](const Entry& a,const Entry& b){return a.position>b.position;});return {true,std::move(out),{}};
}
}
Outcome<KnownContactLocations> read_known_contact_locations(const KnownContactLocationAccess& read,const StableId& observer,Integer cursor,Integer limit){
    if(cursor<0||limit<1||limit>50||cursor>std::numeric_limits<Integer>::max()-limit)return {false,{},common_failure("InvalidKnownContactCursor").error};auto entries=collect(read,observer);if(!entries.ok)return {false,{},entries.error};KnownContactLocations result{read.common.state().identity,observer,cursor,{},{}};const auto end=std::min(entries.value.size(),static_cast<std::size_t>(cursor+limit));for(auto i=static_cast<std::size_t>(cursor);i<end;++i)result.rows.push_back(std::move(entries.value[i].value));if(end<entries.value.size())result.next_cursor=cursor+limit;return {true,std::move(result),{}};
}
Outcome<std::optional<KnownContactLocation>> read_known_contact_location(const KnownContactLocationAccess& read,const StableId& observer,const StableId& subject){
    auto entries=collect(read,observer);if(!entries.ok)return {false,{},entries.error};for(auto& row:entries.value)if(row.value.subject_actor_id==subject)return {true,std::move(row.value),{}};return {true,std::nullopt,{}};
}
// 开工前的本人最后亲见用于回答内容，完成时刻只说明询问者何时听见，不改变旧观察时刻。
Outcome<std::optional<KnownContactLocation>> read_known_contact_location_before_inquiry(const KnownContactLocationAccess& read,const StableId& observer,const StableId& subject,Integer tick,const StableId& fact,const DynamicStepInput* phase){
    if(read.actual_clock)return {false,{},common_failure("KnownContactExactInquiryCutRequired",fact,ErrorKind::unsupported_capability).error};
    const auto& common=read.common;const auto* header=common.state().facts.find(fact);auto number=serial(fact,common.state().next_instance_id);
    if(!header||header->id!=fact||header->kind!="contact_inquiry_started"||header->domain!="contact_inquiries"||fact.rfind("contact_inquiry_started:",0)!=0||!number||tick<0||tick>common.tick())return {false,{},common_failure("KnownContactInquiryBoundaryRequired",fact).error};
    std::size_t position=0;for(const auto& row:common.state().facts.rows()){++position;if(row.first==fact)break;}Cutoff cutoff{position,*number,tick};auto entries=collect(read,observer,&cutoff,phase);if(!entries.ok)return {false,{},entries.error};for(auto& row:entries.value)if(row.value.subject_actor_id==subject)return {true,std::move(row.value),{}};return {true,std::nullopt,{}};
}
Outcome<std::optional<KnownContactLocation>> read_known_contact_location_before_inquiry2(const KnownContactLocationAccess&read,const ContactInquiryState&state,const ContactInquiryCatalog&catalog,const StableId&id){
 if(state.version!=2||!read.actual_clock)return {false,{},common_failure("KnownContactExactInquiryCutRequired",id,ErrorKind::unsupported_capability).error};
 const auto*s=state.started.find(id);const auto*h=read.common.state().facts.find(id);const auto key=S3EventTimeKey(S3ContactInquiryTable::started,id,S3EventRole::started);const auto*e=state.temporal_events.find(key.value());auto number=serial(id,read.common.state().next_instance_id);
 if(!s||s->fact_id!=id||!h||h->id!=id||h->domain!=contact_inquiry_module||h->kind!=contact_inquiry_started_kind||!e||!same_s3_event_record(e->original_record_ref,S3ContactInquiryTable::started,id)||e->event_role!=S3EventRole::started||!number)return {false,{},common_failure("KnownContactActualInquirySourceMissing",id,ErrorKind::internal).error};
 auto exact=read_s3_exact_event(*e);if(!exact.ok)return {false,{},exact.error};const auto*started=std::get_if<S3ActionStartedSource>(&exact.value->actual_source);auto now=read.actual_clock->read_current_moment();if(!now.ok)return {false,{},now.error};auto cmp=compare_world_moments(exact.value->occurred_at,now.value);if(!cmp.ok)return {false,{},cmp.error};if(!started||started->action_id!=s->action_id||started->original_start_result_ref.domain!=h->domain||started->original_start_result_ref.kind!=h->kind||started->original_start_result_ref.id!=id||s->contact.tick!=exact.value->occurred_at.half_day_tick||cmp.value>0)return {false,{},common_failure("KnownContactActualInquiryCutMismatch",id,ErrorKind::internal).error};
 // Policy owns the selected subject, so callers cannot ask an arbitrary other topic.
 const auto*p=contact_inquiry_policy(catalog,s->policy_id);if(catalog.version!=2||!p||p->provider_actor_id!=s->contact.provider_actor_id)return {false,{},common_failure("KnownContactInquiryPolicyRequired",id,ErrorKind::internal).error};
 std::size_t position=0;bool found=false;for(const auto&row:read.common.state().facts.rows()){++position;if(row.first==id){found=true;break;}}if(!found)return {false,{},common_failure("KnownContactInquiryHeaderOrderMissing",id,ErrorKind::internal).error};
 Cutoff cutoff{position,*number,s->contact.tick,exact.value->occurred_at};auto entries=collect(read,s->contact.provider_actor_id,&cutoff,nullptr,&p->subject_actor_id);if(!entries.ok)return {false,{},entries.error};
 for(auto&row:entries.value)if(row.value.subject_actor_id==p->subject_actor_id)return {true,std::move(row.value),{}};return {true,std::nullopt,{}};
}

}

#include "family_known_contact.hpp"
namespace hanmo::resident {
namespace {
// 此新增消费支只接受原 KQ contact_receipt；任意 Report/Origin 不降为接触。
Status family_contact_read(const FamilyKnownContactReadContext1 &context,
                           const KnowledgeSourceRef &basis,
                           const ReadStamp &stamp, bool historical) {
  const auto &common = context.contacts.common;
  if ((!historical && stamp.mode() != ReadMode::committed_current &&
       stamp.mode() != ReadMode::phase_current) ||
      (historical && stamp.mode() != ReadMode::committed_current &&
       stamp.mode() != ReadMode::phase_current &&
       stamp.mode() != ReadMode::committed_history))
    return common_failure("FamilyKnownContactReadStampRequired",
                          basis.source_id);
  auto valid = context.authority.validate(
      stamp, common.state().identity, stamp.mode(), common, context.versions);
  if (!valid.ok)
    return valid;
  if (basis.kind != KnowledgeSourceRef::Kind::contact_receipt ||
      basis.source_id.empty() || basis.source_subject)
    return common_failure("FamilyKnownContactReceiptRequired", basis.source_id,
                          ErrorKind::unsupported_capability);
  if (!context.contacts.actual_clock)
    return common_failure("FamilyKnownContactExactClockRequired",
                          basis.source_id, ErrorKind::unsupported_capability);
  return common_success();
}
} // namespace
Outcome<bool> read_family_known_contact_target1(
    const FamilyKnownContactReadContext1 &context, const StableId &observer,
    const StableId &target, const KnowledgeSourceRef &basis,
    const ReadStamp &stamp) {
  auto valid = family_contact_read(context, basis, stamp, false);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto known = read_known_contact_location(context.contacts, observer, target);
  if (!known.ok)
    return {false, {}, known.error};
  return {
      true, known.value && known.value->source_fact_id == basis.source_id, {}};
}
Status validate_family_known_contact_target_before_fact1(
    const FamilyKnownContactReadContext1 &context, const FactRecord &actual_a,
    const FamilyPersonalExpression2 &expression, const StableId &target,
    const KnowledgeSourceRef &basis, const ReadStamp &stamp) {
  auto valid = family_contact_read(context, basis, stamp, true);
  if (!valid.ok)
    return valid;
  const auto &common = context.contacts.common;
  if (common.state().facts.find(actual_a.id) != &actual_a ||
      actual_a.domain != "family_collateral_legacy" ||
      (actual_a.kind != "family_collateral_proposed" &&
       actual_a.kind != "family_collateral_replied" &&
       actual_a.kind != "family_collateral_recognized") ||
      expression.source_schema != 2 || expression.actor_id.empty() ||
      target.empty() || expression.actor_id == target)
    return common_failure("FamilyKnownContactActualExpressionRequired",
                          actual_a.id);
  auto now = context.contacts.actual_clock->read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto order = compare_world_moments(expression.expressed_at, now.value);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0)
    return common_failure("FamilyKnownContactFutureExpression", actual_a.id);
  const auto number = serial(actual_a.id, common.state().next_instance_id);
  if (!number)
    return common_failure("FamilyKnownContactActualAllocationRequired",
                          actual_a.id);
  std::size_t position = 0;
  for (const auto &row : common.state().facts.rows()) {
    ++position;
    if (row.first == actual_a.id)
      break;
  }
  // 沿已有 collect 的 Common 次序与原全局分配校验，未新增 ID 排序或持久 cut
  // 槽。
  const Cutoff cutoff{position, *number, expression.expressed_at.half_day_tick,
                      expression.expressed_at};
  auto known =
      collect(context.contacts, expression.actor_id, &cutoff, nullptr, &target);
  if (!known.ok)
    return {false, {}, known.error};
  for (const auto &entry : known.value)
    if (entry.value.subject_actor_id == target &&
        entry.value.source_fact_id == basis.source_id)
      return common_success();
  return common_failure("FamilyKnownContactNotKnownBeforeExpression",
                        basis.source_id);
}
} // namespace hanmo::resident
