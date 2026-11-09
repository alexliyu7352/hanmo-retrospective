#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "social_dynamic_json_read.hpp"
#include "social_projection.hpp"
// 动态社会冷编码逐字段持久化来源与投递证据，旧社会checkpoint由原visitor独立保存。
#include "social_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/conditions/domain_codec_util.hpp"
namespace hanmo::resident {
namespace {
// 枚举冷格式闭合，未知状态不被强制转换成合法默认。
template<class E>E enum_value(const BoundaryValue& o,const char* key,Integer maximum){auto n=boundary::get_int(o,key,key);if(n<0||n>maximum)boundary::fail("InvalidDynamicEnum",key);return static_cast<E>(n);}
template<class E>Status visit_enum(CodecVisitor& c,const char* key,E& value,Integer maximum){Integer n=static_cast<Integer>(value);c.field(key,n);if(n<0||n>maximum)return c.reject("InvalidDynamicEnum",key);if(c.reading())value=static_cast<E>(n);return c.status();}

SubjectRef read_subject(const BoundaryValue& v){boundary::fields(v,{"kind","id"},{},"subject");return {enum_value<SubjectKind>(v,"kind",2),boundary::get_string(v,"id","subject")};}
Status visit_subject(CodecVisitor& c,SubjectRef& s){visit_enum(c,"kind",s.kind,2);return c.field("id",s.id);}

SupplyObservation read_supply(const BoundaryValue& v){using namespace boundary;fields(v,{"source_key","organization_id","site_id","inventory_id","item_id","source_fact_id","target_amount","physical_amount","source_revision"},{},"supply");return {get_string(v,"source_key","supply"),get_string(v,"organization_id","supply"),get_string(v,"site_id","supply"),get_string(v,"inventory_id","supply"),get_string(v,"item_id","supply"),get_string(v,"source_fact_id","supply"),get_int(v,"target_amount","supply"),get_int(v,"physical_amount","supply"),get_int(v,"source_revision","supply")};}
Status visit_supply(CodecVisitor& c,SupplyObservation& v){c.field("source_key",v.source_key);c.field("organization_id",v.organization_id);c.field("site_id",v.site_id);c.field("inventory_id",v.inventory_id);c.field("item_id",v.item_id);c.field("source_fact_id",v.source_fact_id);c.field("target_amount",v.target_amount);c.field("physical_amount",v.physical_amount);return c.field("source_revision",v.source_revision);}

ObligationObservation read_obligation(const BoundaryValue& v){using namespace boundary;fields(v,{"obligation_id","principal_id","beneficiary_id","source_fact_id","due_tick","due_amount","paid_amount","outstanding_amount","arrear_id"},{},"obligation");return {get_string(v,"obligation_id","obligation"),get_string(v,"principal_id","obligation"),get_string(v,"beneficiary_id","obligation"),get_string(v,"source_fact_id","obligation"),get_int(v,"due_tick","obligation"),get_int(v,"due_amount","obligation"),get_int(v,"paid_amount","obligation"),get_int(v,"outstanding_amount","obligation"),get_string(v,"arrear_id","obligation")};}
Status visit_obligation(CodecVisitor& c,ObligationObservation& v){c.field("obligation_id",v.obligation_id);c.field("principal_id",v.principal_id);c.field("beneficiary_id",v.beneficiary_id);c.field("source_fact_id",v.source_fact_id);c.field("due_tick",v.due_tick);c.field("due_amount",v.due_amount);c.field("paid_amount",v.paid_amount);c.field("outstanding_amount",v.outstanding_amount);return c.field("arrear_id",v.arrear_id);}

DynamicFactView read_fact(const BoundaryValue& v){using namespace boundary;fields(v,{"id","kind","rule_version","tick","cause_fact_ids","affair_ids","participants","subject_id","outcome","publicly_visible"},{"supply","obligation"},"observation");if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<DynamicFactView>();else if(auto* budget=ScopedCheckpointBoundaryDecode::current())budget->default_values<DynamicFactView>();DynamicFactView f;f.id=get_string(v,"id","observation");f.kind=get_string(v,"kind","observation");f.rule_version=get_string(v,"rule_version","observation");f.tick=get_int(v,"tick","observation");f.cause_fact_ids=strings(field(v,"cause_fact_ids","observation"),"cause_fact_ids");f.affair_ids=strings(field(v,"affair_ids","observation"),"affair_ids");f.subject_id=get_string(v,"subject_id","observation");f.outcome=get_string(v,"outcome","observation");f.publicly_visible=get_bool(v,"publicly_visible","observation");domain_codec::dec_list(field(v,"participants","observation"),f.participants,"participants",[](const BoundaryValue& p,FactParticipant& out,const StableId&){fields(p,{"subject","role"},{},"participant");out.subject=read_subject(field(p,"subject","participant"));out.role=get_string(p,"role","participant");});if(auto* s=v.find("supply"))f.supply=read_supply(*s);if(auto* o=v.find("obligation"))f.obligation=read_obligation(*o);return f;}
Status visit_fact(CodecVisitor& c,DynamicFactView& f){c.field("id",f.id);c.field("kind",f.kind);c.field("rule_version",f.rule_version);c.field("tick",f.tick);common_checkpoint::strings(c,"cause_fact_ids",f.cause_fact_ids);common_checkpoint::strings(c,"affair_ids",f.affair_ids);c.sequence("participants",f.participants,[](CodecVisitor& v,FactParticipant& p){v.object("subject",[&](CodecVisitor& x){return visit_subject(x,p.subject);});return v.field("role",p.role);});c.optional("supply",f.supply,visit_supply);c.optional("obligation",f.obligation,visit_obligation);c.field("subject_id",f.subject_id);c.field("outcome",f.outcome);return c.field("publicly_visible",f.publicly_visible);}
}
// These are the original JSON field readers, shared by legacy and Time2.
// Input objects are borrowed; only the typed destination is constructed.
Outcome<OrderedTable<Report>> decode_dynamic_social_reports_json(const BoundaryValue&value){try{
 using namespace boundary;OrderedTable<Report> out;std::vector<std::pair<StableId,Report>> rows;
 domain_codec::dec_pairs(value,rows,"reports",[](const BoundaryValue&v,Report&r,const StableId&path){
  fields(v,{"id","fact_id","payload_kind","origin_subject","observation_tick","visibility_scope","reliability_kind","observation"},{"valid_until_tick"},path);
  r.id=get_string(v,"id",path);r.fact_id=get_string(v,"fact_id",path);r.payload_kind=get_string(v,"payload_kind",path);
  r.origin_subject=read_subject(field(v,"origin_subject",path));r.observation_tick=get_int(v,"observation_tick",path);
  r.visibility_scope=enum_value<ReportVisibility>(v,"visibility_scope",2);r.reliability_kind=enum_value<ReportReliability>(v,"reliability_kind",2);
  r.observation=read_fact(field(v,"observation",path));if(auto* until=v.find("valid_until_tick"))r.valid_until_tick=integer(*until,path);
 });
 if(!out.adopt_checkpoint_rows(std::move(rows)))fail("DuplicateReport","reports");return {true,std::move(out),{}};
 }catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<OrderedTable<OrderedTable<KnownReport>>> decode_dynamic_social_known_json(const BoundaryValue&value){try{
 using namespace boundary;OrderedTable<OrderedTable<KnownReport>> out;
 std::vector<std::pair<StableId,OrderedTable<KnownReport>>> receivers;
 domain_codec::dec_pairs(value,receivers,"known_reports",[](const BoundaryValue&raw,OrderedTable<KnownReport>&table,const StableId&){
  std::vector<std::pair<StableId,KnownReport>> rows;
  domain_codec::dec_pairs(raw,rows,"known",[](const BoundaryValue&v,KnownReport&k,const StableId&path){
   fields(v,{"receiver","report_id","learned_tick","source_subject"},{"source_message_id"},path);
   k.receiver=read_subject(field(v,"receiver",path));k.report_id=get_string(v,"report_id",path);k.learned_tick=get_int(v,"learned_tick",path);
   k.source_subject=read_subject(field(v,"source_subject",path));if(auto* m=v.find("source_message_id"))k.source_message_id=string(*m,path);
  });
  if(!table.adopt_checkpoint_rows(std::move(rows)))fail("DuplicateKnownReport","known");
 });
 if(!out.adopt_checkpoint_rows(std::move(receivers)))fail("DuplicateReportReceiver","known_reports");return {true,std::move(out),{}};
 }catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<DynamicSocialState> decode_dynamic_social(const BoundaryValue& value){try{
 using namespace boundary;fields(value,{"version","last_collected_tick","reports","pending_messages","known_reports"},{},"dynamic_social");
 if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<DynamicSocialState>();else if(auto* budget=ScopedCheckpointBoundaryDecode::current())budget->default_values<DynamicSocialState>();
 DynamicSocialState s;s.version=get_int(value,"version","dynamic_social");if(s.version!=1&&s.version!=2)fail("InvalidDynamicSocialVersion","dynamic_social");s.last_collected_tick=get_int(value,"last_collected_tick","dynamic_social");
 auto reports=decode_dynamic_social_reports_json(field(value,"reports","dynamic_social"));if(!reports.ok)return {false,{},reports.error};s.reports=std::move(reports.value);
 std::vector<std::pair<StableId,PendingMessage>> messages;
 const auto message_path=checkpoint_decode_boundary_text("pending_messages");
 domain_codec::dec_pairs(field(value,"pending_messages","dynamic_social"),messages,message_path,[&](const BoundaryValue&v,PendingMessage&m,const StableId&path){
  fields(v,{"id","report_id","sender","receiver","channel","created_tick","due_tick","status"},s.version==2?std::initializer_list<const char*>{"local_place_id"}:std::initializer_list<const char*>{},path);
  m.id=get_string(v,"id",path);m.report_id=get_string(v,"report_id",path);m.sender=read_subject(field(v,"sender",path));m.receiver=read_subject(field(v,"receiver",path));
  m.channel=get_string(v,"channel",path);m.created_tick=get_int(v,"created_tick",path);m.due_tick=get_int(v,"due_tick",path);m.status=enum_value<MessageStatus>(v,"status",2);
  if(auto* place=v.find("local_place_id"))m.local_place_id=string(*place,path);
 });
 if(!s.pending_messages.adopt_checkpoint_rows(std::move(messages)))fail("DuplicateMessage",message_path);
 auto known=decode_dynamic_social_known_json(field(value,"known_reports","dynamic_social"));if(!known.ok)return {false,{},known.error};s.known_reports=std::move(known.value);return {true,std::move(s),{}};
 }catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
namespace {
Status social_subject(ProjectionWriter& w,const SubjectRef& s,bool strict){const auto kind=static_cast<Integer>(s.kind);if(strict&&(kind<0||kind>2))return w.reject("InvalidDynamicEnum","kind");w.field("kind",kind);return w.field("id",s.id);}
Status social_fact(ProjectionWriter& w,const DynamicFactView& f,bool strict){
    w.field("id",f.id);w.field("kind",f.kind);w.field("rule_version",f.rule_version);w.field("tick",f.tick);
    auto strings=[](auto& out,const char* key,const auto& values){return out.sequence(key,values,[](auto& c,const auto& id){return c.field("",id);});};strings(w,"cause_fact_ids",f.cause_fact_ids);strings(w,"affair_ids",f.affair_ids);
    w.sequence("participants",f.participants,[&](auto& out,const auto& p){return out.object("",[&](auto& c){c.object("subject",[&](auto& x){return social_subject(x,p.subject,strict);});return c.field("role",p.role);});});
    w.field("subject_id",f.subject_id);w.field("outcome",f.outcome);w.field("publicly_visible",f.publicly_visible);
    if(f.supply)w.object("supply",[&](auto& c){const auto& v=*f.supply;c.field("source_key",v.source_key);c.field("organization_id",v.organization_id);c.field("site_id",v.site_id);c.field("inventory_id",v.inventory_id);c.field("item_id",v.item_id);c.field("source_fact_id",v.source_fact_id);c.field("target_amount",v.target_amount);c.field("physical_amount",v.physical_amount);return c.field("source_revision",v.source_revision);});
    if(f.obligation)w.object("obligation",[&](auto& c){const auto& v=*f.obligation;c.field("obligation_id",v.obligation_id);c.field("principal_id",v.principal_id);c.field("beneficiary_id",v.beneficiary_id);c.field("source_fact_id",v.source_fact_id);c.field("due_tick",v.due_tick);c.field("due_amount",v.due_amount);c.field("paid_amount",v.paid_amount);c.field("outstanding_amount",v.outstanding_amount);return c.field("arrear_id",v.arrear_id);});return w.status();
}
}
Status emit_dynamic_social_reports_fields(ProjectionWriter& w,const DynamicSocialState& s,bool strict){
    for(const auto& row:s.reports.rows()){w.object(row.first,[&](auto& c){const auto& r=row.second;const auto visibility=static_cast<Integer>(r.visibility_scope),reliability=static_cast<Integer>(r.reliability_kind);if(strict&&(visibility<0||visibility>2))return c.reject("InvalidDynamicEnum","visibility_scope");if(strict&&(reliability<0||reliability>2))return c.reject("InvalidDynamicEnum","reliability_kind");c.field("id",r.id);c.field("fact_id",r.fact_id);c.field("payload_kind",r.payload_kind);c.object("origin_subject",[&](auto& x){return social_subject(x,r.origin_subject,strict);});c.field("observation_tick",r.observation_tick);c.field("visibility_scope",visibility);c.field("reliability_kind",reliability);c.object("observation",[&](auto& x){return social_fact(x,r.observation,strict);});if(r.valid_until_tick)c.field("valid_until_tick",*r.valid_until_tick);return c.status();});if(!w.status().ok)break;}return w.status();
}
Status emit_dynamic_social_known_fields(ProjectionWriter& w,const DynamicSocialState& s,bool strict){
    for(const auto& row:s.known_reports.rows()){w.object(row.first,[&](auto& c){for(const auto& entry:row.second.rows()){c.object(entry.first,[&](auto& x){const auto& k=entry.second;x.object("receiver",[&](auto& y){return social_subject(y,k.receiver,strict);});x.field("report_id",k.report_id);x.field("learned_tick",k.learned_tick);x.object("source_subject",[&](auto& y){return social_subject(y,k.source_subject,strict);});if(k.source_message_id)x.field("source_message_id",*k.source_message_id);return x.status();});if(!c.status().ok)break;}return c.status();});if(!w.status().ok)break;}return w.status();
}
Status emit_dynamic_social_fields(ProjectionWriter& out,const DynamicSocialState& s){
    out.field("version",s.version);out.field("last_collected_tick",s.last_collected_tick);out.object("reports",[&](auto& w){return emit_dynamic_social_reports_fields(w,s);});
    out.object("pending_messages",[&](auto& w){for(const auto& row:s.pending_messages.rows()){w.object(row.first,[&](auto& c){const auto& m=row.second;c.field("id",m.id);c.field("report_id",m.report_id);c.object("sender",[&](auto& x){return social_subject(x,m.sender,false);});c.object("receiver",[&](auto& x){return social_subject(x,m.receiver,false);});c.field("channel",m.channel);c.field("created_tick",m.created_tick);c.field("due_tick",m.due_tick);c.field("status",static_cast<Integer>(m.status));if(m.local_place_id){if(s.version!=2)return c.reject("RegionalLifeUnavailable",row.first);c.field("local_place_id",*m.local_place_id);}return c.status();});if(!w.status().ok)break;}return w.status();});
    out.object("known_reports",[&](auto& w){return emit_dynamic_social_known_fields(w,s);});return out.status();
}
BoundaryValue encode_dynamic_social(const DynamicSocialState& s){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_dynamic_social_fields(out,s);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
Status visit_dynamic_social_checkpoint(CodecVisitor& c,DynamicSocialState& s){c.field("version",s.version);c.field("last_collected_tick",s.last_collected_tick);common_checkpoint::table(c,"reports",s.reports,[](CodecVisitor& v,Report& r){v.field("id",r.id);v.field("fact_id",r.fact_id);v.field("payload_kind",r.payload_kind);v.object("origin_subject",[&](CodecVisitor& x){return visit_subject(x,r.origin_subject);});v.field("observation_tick",r.observation_tick);visit_enum(v,"visibility_scope",r.visibility_scope,2);v.optional("valid_until_tick",r.valid_until_tick,[](CodecVisitor& x,Integer& n){return x.field("value",n);});visit_enum(v,"reliability_kind",r.reliability_kind,2);return v.object("observation",[&](CodecVisitor& x){return visit_fact(x,r.observation);});});common_checkpoint::table(c,"pending_messages",s.pending_messages,[&s](CodecVisitor& v,PendingMessage& m){v.field("id",m.id);v.field("report_id",m.report_id);v.object("sender",[&](CodecVisitor& x){return visit_subject(x,m.sender);});v.object("receiver",[&](CodecVisitor& x){return visit_subject(x,m.receiver);});v.field("channel",m.channel);v.field("created_tick",m.created_tick);v.field("due_tick",m.due_tick);visit_enum(v,"status",m.status,2);if(s.version==2)v.optional("local_place_id",m.local_place_id,[](CodecVisitor& x,StableId& place){return x.field("value",place);});else if(m.local_place_id)return v.reject("RegionalLifeUnavailable");return v.status();});return common_checkpoint::table(c,"known_reports",s.known_reports,[](CodecVisitor& v,OrderedTable<KnownReport>& t){return common_checkpoint::table(v,"known",t,[](CodecVisitor& x,KnownReport& k){x.object("receiver",[&](CodecVisitor& y){return visit_subject(y,k.receiver);});x.field("report_id",k.report_id);x.optional("source_message_id",k.source_message_id,[](CodecVisitor& y,StableId& id){return y.field("value",id);});x.field("learned_tick",k.learned_tick);return x.object("source_subject",[&](CodecVisitor& y){return visit_subject(y,k.source_subject);});});});}
} // 冷载荷经严格字段检查；恢复后的跨域来源与权限仍由同候选validator核验。
