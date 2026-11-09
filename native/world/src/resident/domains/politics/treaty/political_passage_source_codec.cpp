#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_passage_source.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
namespace hanmo::resident {namespace {
Status source(CodecVisitor& c,SourceRef& r){c.field("domain",r.domain);c.field("kind",r.kind);c.field("id",r.id);if(c.status().ok&&!valid_source_ref(r))return c.reject("InvalidPoliticalPassageSourceRef");return c.status();}
Status identity(CodecVisitor& c,WorldIdentity& v){
    StableId hex;constexpr char digits[]="0123456789abcdef";
    if(!c.reading())for(const auto b:v.world_id){hex.push_back(digits[b>>4]);hex.push_back(digits[b&15]);}
    c.field("world_id",hex);if(!c.status().ok)return c.status();if(hex.size()!=2*v.world_id.size())return c.reject("InvalidPoliticalPassageWorldId");
    if(c.reading())for(std::size_t i=0;i<v.world_id.size();++i){auto digit=[](char a){return a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:-1;};const auto hi=digit(hex[2*i]),lo=digit(hex[2*i+1]);if(hi<0||lo<0)return c.reject("InvalidPoliticalPassageWorldId");v.world_id[i]=static_cast<std::uint8_t>(16*hi+lo);}
    c.field("execution_epoch",v.execution_epoch);c.field("revision",v.revision);return c.field("tick",v.tick);
}
Status decision(CodecVisitor& c,PoliticalPassageDecision& d){
    if(!c.reading()&&d.read_stamp)return c.reject("PersistentPassageReadStampForbidden");
    c.object("identity",[&](auto& x){return identity(x,d.identity);});
    visit_passage_boundary_ref(c,"boundary",d.boundary);
    Integer subject=static_cast<Integer>(d.subject.kind);c.field("subject_kind",subject);if(subject<0||subject>2)return c.reject("InvalidPoliticalPassageSubject");if(c.reading())d.subject.kind=static_cast<SubjectKind>(subject);
    c.field("subject_id",d.subject.id);c.field("mode",d.mode);c.field("entry_tick",d.entry_tick);
    Integer admission=static_cast<Integer>(d.admission);c.field("admission",admission);if(admission<0||admission>2)return c.reject("InvalidPoliticalPassageAdmission");if(c.reading())d.admission=static_cast<PassageAdmission>(admission);
    c.optional("toll",d.toll,[](auto& x,auto& toll){x.field("currency_id",toll.currency_id);x.field("amount",toll.amount);x.field("payee_wallet_id",toll.payee_wallet_id);return x.object("tariff_source_ref",[&](auto& y){return source(y,toll.tariff_source_ref);});});
    for(auto item:{std::make_pair("authority_basis_refs",&d.authority_basis_refs),std::make_pair("treaty_term_refs",&d.treaty_term_refs),std::make_pair("local_delivery_source_refs",&d.local_delivery_source_refs)})c.sequence(item.first,*item.second,[](auto& x,auto& r){return source(x,r);});
    c.object("admission_source",[&](auto& x){return source(x,d.admission_source);});
    visit_passage_requester(c,"requester",d.requester);c.field("source_schema_version",d.source_schema_version);c.field("visible_reason_code",d.visible_reason_code);
    if(c.reading())d.read_stamp.reset();return c.status();
}
Status record(CodecVisitor& c,PoliticalPassageDecisionSource& r){
    c.field("id",r.id);visit_movement_execution_ref(c,"execution",r.execution);visit_spatial_carrier_ref(c,"carrier",r.carrier);
    c.field("entry_ordinal",r.entry_ordinal);c.field("composition_revision",r.composition_revision);c.field("manifest_revision",r.manifest_revision);
    c.object("occurred_at",[&](auto& x){return visit_world_moment2(x,r.occurred_at);});return c.object("decision",[&](auto& x){return decision(x,r.decision);});
}
}
Status visit_political_passage_sources(CodecVisitor& c,PoliticalPassageDecisionState& state){
    using Row=typename OrderedTable<PoliticalPassageDecisionSource>::Row;
    auto emit=[](CodecVisitor& x,Row& row){x.field("key",row.first);auto status=x.object("record",[&](auto& y){return record(y,row.second);});if(status.ok&&(row.first.empty()||row.first!=row.second.id))return x.reject("PoliticalPassageSourceKeyMismatch");return status;};
    if(!c.reading())return c.sequence("adopted_decisions",state.decisions.rows(),emit);
    std::vector<Row> rows;c.sequence("adopted_decisions",rows,emit);if(!c.status().ok)return c.status();
    if(!state.decisions.adopt_checkpoint_rows(std::move(rows)))return c.reject("DuplicatePoliticalPassageSource");return c.status();
}
}

// source2 复用已定义的唯一字段表；不以新模式重读旧 source1。
#include "political_passage_codec_fields.hpp"
namespace hanmo::resident { namespace {
// 原行含可抛 WorldMoment 值，迁移仅在未发布候选内进行；失败整体弃候选。
struct PassageDecodedTransfer2 {
 template<class Destination>Status operator()(Destination& out,std::pair<StableId,PoliticalPassageDecisionSource>& row)const {
  out.emplace_back(std::move(row));return common_success();
 }
};
template<class Archive>Status selected_sources2(Archive& c,PoliticalPassageDecisionState& state){
 auto fields=[](auto& a,auto& row){return political_passage_codec_fields::record(a,row,2);};
 if constexpr(std::is_same_v<Archive,CodecVisitor>)
  return common_checkpoint::table(c,"adopted_decisions",state.decisions,fields,PassageDecodedTransfer2{});
 else if constexpr(std::is_same_v<Archive,ActorFoundationJsonVisitor>)
  return c.table("adopted_decisions",state.decisions,fields,PassageDecodedTransfer2{});
 else return c.table("adopted_decisions",state.decisions,fields);
}
}
Status visit_political_passage_sources2(CodecVisitor& c,PoliticalPassageDecisionState& state){return selected_sources2(c,state);}
Outcome<BoundaryValue> encode_political_passage_sources2(const PoliticalPassageDecisionState& state){
 BoundaryValue out;BoundaryProjectionWriter writer(out);
 auto valid=emit_projection_record<ActorFoundationProjectionArchive>(writer,"",state,
   [](auto& a,auto& v){return selected_sources2(a,v);});
 if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};
}
Outcome<PoliticalPassageDecisionState> decode_political_passage_sources2(const BoundaryValue& input){
 PoliticalPassageDecisionState out;ActorFoundationJsonVisitor reader(input);
 auto valid=selected_sources2(reader,out);if(valid.ok)valid=reader.finish();
 return {valid.ok,std::move(out),valid.error};
}
}
