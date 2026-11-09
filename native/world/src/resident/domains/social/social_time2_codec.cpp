#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "social_time2_version.hpp"
#include "social_dynamic_json_read.hpp"
#include "social_projection_limits.hpp"
#include "social_projection.hpp"
// V§12.1固定十根与Social期间两根；旧报告/知识保持原codec字节，新附项严格typed。
#include "social_time2_codec.hpp"
#include "propagation.hpp"
#include "propagation_disposition_codec_fields.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <stdexcept>
#include <type_traits>
namespace hanmo::resident {
namespace {
// 首错保持原类别；异常仅在此冷边界内部传播，不把技术故障包装成业务拒绝。
struct CodecFailure { RuleError error; };
template<class T> T take(Outcome<T> value){if(!value.ok)throw CodecFailure{std::move(value.error)};return std::move(value.value);}
// 所有公开入口捕获可传播的分配错误；原BoundaryValue复制叶的独立故障保证不由此替代。
template<class T,class F> Outcome<T> guarded(F fn){try{return {true,fn(),{}};}catch(const CodecFailure& e){return {false,{},e.error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}catch(const std::bad_alloc&){RuleError e;e.original={"OutOfMemory","social_time2_codec"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"SizeExhausted","social_time2_codec"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
// 手工BoundaryValue也须完整检查重复键和有界树，不依赖JSON parser已做过验证。
void inspect_json(const BoundaryValue& value,std::size_t depth,std::size_t& nodes,std::size_t& bytes){if(depth>=CodecVisitor::max_depth)boundary::fail("CheckpointTooDeep","social_time2_codec");if(++nodes>2000000u)boundary::fail("CheckpointTooLarge","social_time2_codec");auto charge=[&](std::size_t n){if(n>CodecVisitor::max_bytes-bytes)boundary::fail("CheckpointTooLarge","social_time2_codec");bytes+=n;};charge(1);if(auto* s=value.as<StableId>())charge(s->size());else if(auto* d=value.as<BoundaryValue::Dict>()){if(d->size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge","social_time2_codec");for(std::size_t i=0;i<d->size();++i){const auto& row=(*d)[i];charge(row.first.size());for(std::size_t j=0;j<i;++j)if((*d)[j].first==row.first)boundary::fail("DuplicateSocialTime2Key",row.first);inspect_json(row.second,depth+1,nodes,bytes);}}else if(auto* a=value.as<BoundaryValue::Array>()){if(a->size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge","social_time2_codec");for(const auto& row:*a)inspect_json(row,depth+1,nodes,bytes);}}
// 包装预算仅属于本次调用，不改变世界或保存新的预算字段。
void inspect_json(const BoundaryValue& value){std::size_t nodes=0,bytes=0;inspect_json(value,0,nodes,bytes);}
// 新字段严格按声明序；JSON表保持原字典形状，可选附项用显式null。
// The same original finite Social rows cross the one shared staged-table
// adoption. A fallible exact-moment move retains the source decode owner until
// it completes; failure discards this entire unpublished candidate.
struct SocialDecodedRowTransfer {
 template<class Destination,class T>Status operator()(Destination& destination,std::pair<StableId,T>& source)const{
  static_assert(std::is_same_v<T,PendingMessage>||std::is_same_v<T,SocialSourceTimeBinding>||
   std::is_same_v<T,SocialMessageExecutionBinding>||std::is_same_v<T,SocialPropagationDecision>||
   std::is_same_v<T,SocialPropagationCooldown>||std::is_same_v<T,SocialPeriodOpening>||
   std::is_same_v<T,SocialPeriodClosure>,"Original finite Social row transfer required");
  destination.emplace_back(std::move(source));return common_success();
 }
};
class JsonCodec {
 std::optional<ActorFoundationJsonVisitor> owner_;
 ActorFoundationJsonVisitor* original_=nullptr;
 explicit JsonCodec(ActorFoundationJsonVisitor& source,bool scalar=false):original_(&source){
  if(source.reading()&&!scalar)source.ordered_fields();
 }
public:
 explicit JsonCodec(const BoundaryValue& value,bool scalar=false)
  :owner_(std::in_place,value,scalar),original_(&*owner_){if(!scalar)original_->ordered_fields();}
 JsonCodec(const BoundaryValue& value,std::size_t first,std::size_t last)
  :owner_(std::in_place,value),original_(&*owner_){original_->ordered_fields(first,last);}
 explicit JsonCodec(BoundaryValue& value,bool scalar=false)
  :owner_(std::in_place,value,scalar),original_(&*owner_){}
 bool reading()const{return original_->reading();}
 Status status()const{return original_->status();}
 Status reject(std::string_view code,std::string_view path="social_time2_codec"){return original_->reject(code,path);}
 Status finish(){return original_->finish();}
 template<class T>Status prepare_default_values(std::uint64_t count=1){return original_->template prepare_default_values<T>(count);}
 template<class T>Status field(std::string_view name,T& value){return original_->field(name,value);}
 template<class F>Status object(std::string_view name,F fn){
  return original_->object(name,[&](auto& child){JsonCodec adapter(child);return fn(adapter);});
 }
 template<class T,class Allocator,class F>Status sequence(std::string_view name,std::vector<T,Allocator>& values,F fn){
  constexpr bool scalar=std::is_same_v<T,StableId>;
  return original_->sequence(name,values,[&](auto& child,T& row){JsonCodec adapter(child,scalar);return fn(adapter,row);},scalar);
 }
 template<class T,class F>Status optional(std::string_view name,std::optional<T>& value,F fn,bool omitted=false){
  constexpr bool scalar=std::is_same_v<T,StableId>;
  return original_->optional(name,value,[&](auto& child,T& row){JsonCodec adapter(child,scalar);return fn(adapter,row);},scalar,omitted);
 }
 template<class T,class F>Status table(std::string_view name,OrderedTable<T>& rows,F fn){
  return original_->table(name,rows,[&](auto& child,T& row){JsonCodec adapter(child);return fn(adapter,row);},SocialDecodedRowTransfer{});
 }
};
// Projection archive preserves this codec's scalar/null/dictionary shapes and omitted legacy slot.
class SocialProjectionArchive {
    ProjectionWriter& out_;bool scalar_=false;std::string_view scalar_name_;
    template<class T,class F>Status item(std::string_view name,T& value,F fn){if constexpr(std::is_same_v<T,StableId>){SocialProjectionArchive child(out_);child.scalar_=true;child.scalar_name_=name;return fn(child,value);}else return out_.object(name,[&](auto& out){SocialProjectionArchive child(out);return fn(child,value);});}
public:
    explicit SocialProjectionArchive(ProjectionWriter& out):out_(out){}
    bool reading()const{return false;}Status status()const{return out_.status();}Status finish()const{return status();}
    template<class T>Status prepare_default_values(std::uint64_t=1){return status();}
    Status reject(std::string_view code,std::string_view path="social_time2_codec"){return out_.reject(code,path);}
    template<class T>Status field(std::string_view name,T& v){return out_.field(scalar_?scalar_name_:std::string_view(name),v);}
    template<class F>Status object(std::string_view name,F fn){return out_.object(name,[&](auto& out){SocialProjectionArchive child(out);return fn(child);});}
    template<class T,class Alloc,class F>Status sequence(std::string_view name,std::vector<T,Alloc>& values,F fn){if(values.size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);return out_.sequence(name,values,[&](auto&,const auto& value){return item("",const_cast<T&>(value),fn);});}
    template<class T,class F>Status optional(std::string_view name,std::optional<T>& v,F fn,bool omitted=false){if(!v)return omitted?status():out_.null(name);return item(name,*v,fn);}
    template<class T,class F>Status table(std::string_view name,OrderedTable<T>& rows,F fn){if(rows.size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);return out_.object(name,[&](auto& out){for(const auto& row:rows.rows()){out.object(row.first,[&](auto& record){SocialProjectionArchive child(record);return fn(child,const_cast<T&>(row.second));});if(!out.status().ok)break;}return out.status();});}
    Status empty_table(std::string_view name){return out_.object(name,[](auto& out){return out.status();});}
};
// JSON表沿旧字典形状，binary沿原entry/key包装；两端都保同一插入序。
template<class C,class T,class F> Status table(C& c,const char* name,OrderedTable<T>& v,F fn){if(!c.status().ok)return c.status();if(!c.reading()){const auto& rows=v.rows();for(std::size_t i=0;i<rows.size();++i)for(std::size_t j=0;j<i;++j)if(rows[i].first==rows[j].first)return c.reject("DuplicateSocialTime2Key",name);}if constexpr(std::is_same_v<C,JsonCodec>||std::is_same_v<C,SocialProjectionArchive>)return c.table(name,v,fn);else return common_checkpoint::table(c,name,v,fn);}
// 旧base2的local_place_id继续省略式JSON与原binary Optional，不新增槽。
template<class C> Status local_place(C& c,std::optional<StableId>& v){auto fn=[](auto& x,auto& s){return x.field("value",s);};if constexpr(std::is_same_v<C,JsonCodec>||std::is_same_v<C,SocialProjectionArchive>)return c.optional("local_place_id",v,fn,true);else return c.optional("local_place_id",v,fn);}
// 有限tag不接受超界或负数，不猜默认联合分支。
template<class C> Status finite(C& c,const char* name,Integer& v,Integer maximum){c.field(name,v);if(c.status().ok&&(v<0||v>maximum))return c.reject("InvalidSocialTime2Tag",name);return c.status();}
// SubjectRef原kind/id顺序不变，未知枚举仍失败。
template<class C> Status subject(C& c,SubjectRef& v){Integer kind=static_cast<Integer>(v.kind);c.field("kind",kind);if(c.status().ok&&(kind<0||kind>2))return c.reject("InvalidDynamicEnum","kind");if(c.status().ok&&c.reading())v.kind=static_cast<SubjectKind>(kind);return c.field("id",v.id);}
// SourceRef只保存三字段；source_schema属于外围真实source合同。
template<class C> Status source(C& c,SourceRef& v){c.field("domain",v.domain);c.field("kind",v.kind);return c.field("id",v.id);}
// 原目录锁完整字段序，不能省略模块身份或artifact摘要。
template<class C> Status catalog(C& c,CatalogIdentity& v){c.field("content_id",v.content_id);c.field("content_version",v.content_version);c.field("content_digest",v.content_digest);c.field("ruleset_id",v.ruleset_id);c.field("schema_sha256",v.schema_sha256);c.optional("pack_set_digest",v.pack_set_digest,[](auto& x,auto& s){return x.field("value",s);});std::size_t index=0;c.sequence("module_versions",v.module_versions,[&](auto& x,auto& p){x.field("name",p.first);x.field("version",p.second);for(std::size_t n=0;n<index;++n)if(v.module_versions[n].first==p.first)return x.reject("DuplicateSocialTime2Module");++index;return x.status();});return c.field("artifact_sha256",v.artifact_sha256);}
// Moment只调用唯一精确数值叶，规范字符串和溢出错误直接传播。
template<class C> Status moment(C& c,WorldMoment& v){if(!c.status().ok)return c.status();if constexpr(std::is_same_v<std::remove_cv_t<std::remove_reference_t<C>>,CodecVisitor>){}if(!prepare_checkpoint_default_values<EncodedWorldMoment>(c).ok)return c.status();EncodedWorldMoment encoded;if(!c.reading())encoded=take(encode_world_moment(v));c.field("half_day_tick",encoded.half_day_tick);c.object("sub_half_day",[&](auto& x){x.field("numerator",encoded.sub_half_day.numerator);return x.field("denominator",encoded.sub_half_day.denominator);});if(c.status().ok&&c.reading())v=take(decode_world_moment(encoded));return c.status();}
// 知识键仍为原(receiver,report_id)，不是第二知识状态。
template<class C> Status knowledge_key(C& c,SocialKnowledgeKey& v){c.object("receiver",[&](auto& x){return subject(x,v.receiver);});return c.field("report_id",v.report_id);}
// 四种时间键按V声明顺序固定tag0..3。
template<class C> Status time_key(C& c,SocialTimeKey& v){Integer tag=static_cast<Integer>(v.index());finite(c,"tag",tag,3);if(!c.status().ok)return c.status();if(c.reading()){switch(tag){case 0:{if(!prepare_checkpoint_default_values<ReportObservationKey>(c).ok)return c.status();v=ReportObservationKey{};}break;case 1:{if(!prepare_checkpoint_default_values<MessageCreatedKey>(c).ok)return c.status();v=MessageCreatedKey{};}break;case 2:{if(!prepare_checkpoint_default_values<MessageDeliveredKey>(c).ok)return c.status();v=MessageDeliveredKey{};}break;default:{if(!prepare_checkpoint_default_values<SocialKnowledgeKey>(c).ok)return c.status();v=SocialKnowledgeKey{};}}}switch(tag){case 0:return c.field("report_id",std::get<ReportObservationKey>(v).report_id);case 1:return c.field("message_id",std::get<MessageCreatedKey>(v).message_id);case 2:return c.field("message_id",std::get<MessageDeliveredKey>(v).message_id);default:return knowledge_key(c,std::get<SocialKnowledgeKey>(v));}}
// 时间来源只编码真实所选联合；ExactSource不另复制它拥有的时刻。
template<class C> Status time_source(C& c,SocialTimeSource& v){Integer tag=static_cast<Integer>(v.index());finite(c,"tag",tag,2);if(!c.status().ok)return c.status();if(c.reading()){switch(tag){case 0:{if(!prepare_checkpoint_default_values<SocialLegacyBoundary>(c).ok)return c.status();v=SocialLegacyBoundary{};}break;case 1:{if(!prepare_checkpoint_default_values<SocialExactMoment>(c).ok)return c.status();v=SocialExactMoment{};}break;default:{if(!prepare_checkpoint_default_values<SocialExactSource>(c).ok)return c.status();v=SocialExactSource{};}}}if(tag==0){auto& x=std::get<SocialLegacyBoundary>(v);c.field("tick",x.tick);return c.object("original_catalog_identity",[&](auto& y){return catalog(y,x.original_catalog_identity);});}if(tag==1)return c.object("moment",[&](auto& x){return moment(x,std::get<SocialExactMoment>(v).moment);});auto& owner=std::get<SocialExactSource>(v).original_timed_source_ref;return c.object("original_timed_source_ref",[&](auto& x){x.object("source_ref",[&](auto& y){return source(y,owner.source_ref);});x.field("source_schema",owner.source_schema);if(x.status().ok&&owner.source_schema<=0)return x.reject("InvalidSocialSourceSchema");return x.status();});}
// 旧渠道或S3业务原源保tag0/1；profile准入不由codec暗中放宽。
template<class C> Status message_source(C& c,SocialMessageSource& v){Integer tag=static_cast<Integer>(v.index());finite(c,"tag",tag,1);if(!c.status().ok)return c.status();if(c.reading()){if(tag==0){if(!prepare_checkpoint_default_values<SocialLegacyChannel>(c).ok)return c.status();v=SocialLegacyChannel{};}else{if(!prepare_checkpoint_default_values<SocialS3BusinessLink>(c).ok)return c.status();v=SocialS3BusinessLink{};}}if(tag==0)return c.field("original_profile",std::get<SocialLegacyChannel>(v).original_profile);return c.object("original_business_fact_ref",[&](auto& x){return source(x,std::get<SocialS3BusinessLink>(v).original_business_fact_ref);});}
// 原receiver槽只出现被选中分支，不伪造机构actor或复制RoleAddress。
template<class C> Status receiver(C& c,PendingMessage& v){Integer tag=v.address_binding?1:0;finite(c,"tag",tag,1);if(!c.status().ok)return c.status();if(c.reading()){if(tag==1){if(!prepare_checkpoint_default_values<SocialS3AddressBinding>(c).ok||!prepare_checkpoint_default_values<SubjectRef>(c).ok)return c.status();v.address_binding.emplace();v.receiver=SubjectRef{};}else v.address_binding.reset();}if(tag==0)return subject(c,v.receiver);if(!c.reading()&&!v.receiver.id.empty())return c.reject("AmbiguousSocialReceiver");return c.object("original_business_fact_ref",[&](auto& x){return source(x,v.address_binding->original_business_fact_ref);});}
// 原消息字段保序；base1仍不准local_place_id，base2才保该可选槽。
template<class C> Status message(C& c,PendingMessage& v,Integer version){c.field("id",v.id);c.field("report_id",v.report_id);c.object("sender",[&](auto& x){return subject(x,v.sender);});c.object("receiver",[&](auto& x){return receiver(x,v);});c.field("channel",v.channel);c.field("created_tick",v.created_tick);c.field("due_tick",v.due_tick);Integer status=static_cast<Integer>(v.status);c.field("status",status);if(c.status().ok&&(status<0||status>2))return c.reject("InvalidDynamicEnum","status");if(c.status().ok&&c.reading())v.status=static_cast<MessageStatus>(status);if(social_regional_time2_version(version))local_place(c,v.local_place_id);else if(c.status().ok&&v.local_place_id)return c.reject("RegionalLifeUnavailable");return c.status();}
// 原有序字符串集合不排序；重复来源引用在本冷边界拒绝。
template<class C> Status ids(C& c,const char* name,std::vector<StableId>& values){std::size_t index=0;return c.sequence(name,values,[&](auto& x,auto& value){x.field("value",value);for(std::size_t n=0;n<index;++n)if(values[n]==value)return x.reject("DuplicateSocialSourceReference",name);++index;return x.status();});}
// V固定传播记录。S4快照字段只走原owner模板，不在此复制其规则。
template<class C>Status locked_policy(C& c,FixedLockedRuleInput& r){c.object("catalog_identity",[&](auto& x){return catalog(x,r.catalog_identity);});return c.field("rule_ref",r.rule_ref);}
template<class C>Status propagation_decision(C& c,SocialPropagationDecision& v,Integer version){
 c.object("subject",[&](auto& x){return subject(x,v.subject);});c.field("registration_epoch",v.registration_epoch);c.field("decision_seq",v.decision_seq);c.field("report_id",v.report_id);
 c.object("policy_ref",[&](auto& x){return locked_policy(x,v.policy_ref);});c.object("source_known_key",[&](auto& x){return knowledge_key(x,v.source_known_key);});
 c.sequence("source_relation_basis",v.source_relation_basis,[](auto& x,auto& r){return propagation_disposition_codec::relation(x,r);});
 c.sequence("disposition_policy_basis",v.disposition_policy_basis,[](auto& x,auto& r){return propagation_disposition_codec::known_policy(x,r);});
 c.field("evaluated_at",v.evaluated_at);c.sequence("considered_recipients",v.considered_recipients,[](auto& x,auto& r){return subject(x,r);});c.sequence("selected_recipients",v.selected_recipients,[](auto& x,auto& r){return subject(x,r);});
 Integer outcome=static_cast<Integer>(v.outcome);finite(c,"outcome",outcome,3);if(c.status().ok&&c.reading())v.outcome=static_cast<SocialPropagationOutcome>(outcome);
 ids(c,"actual_message_refs",v.actual_message_refs);c.field("next_review_tick",v.next_review_tick);c.sequence("causal_source_refs",v.causal_source_refs,[](auto& x,auto& r){return source(x,r);});
 if(!c.status().ok)return c.status();
 if(!social_propagation_cut_version(version)){if(v.actual_adoption)return c.reject("UnselectedPropagationAdoptionCut");return c.status();}
 if(c.reading()){if(!prepare_checkpoint_default_values<SocialPropagationAdoption>(c).ok)return c.status();v.actual_adoption.emplace();}if(!v.actual_adoption)return c.reject("PropagationActualAdoptionCutRequired");
 return c.object("actual_adoption",[&](auto& x){auto& a=*v.actual_adoption;x.object("adopted_at",[&](auto& y){return moment(y,a.adopted_at);});return x.object("original_prefix",[&](auto& y){return common_fact_prefix_fields::fields(y,a.original_prefix,[](auto& z,auto& r){return source(z,r);},"InvalidPropagationCommonPrefix");});});
}
template<class C>Status propagation_cooldown(C& c,SocialPropagationCooldown& v){c.object("subject",[&](auto& x){return subject(x,v.subject);});c.field("report_id",v.report_id);c.object("recipient",[&](auto& x){return subject(x,v.recipient);});c.object("policy_ref",[&](auto& x){return locked_policy(x,v.policy_ref);});c.field("last_actual_dispatch_tick",v.last_actual_dispatch_tick);c.field("next_eligible_tick",v.next_eligible_tick);return c.object("source_decision_ref",[&](auto& x){return source(x,v.source_decision_ref);});}
// 十根后五项的唯一typed遍历，源与实际执行完整保留。
template<class C> Status additions(C& c,SocialTime2State& v,Integer version){table(c,"source_time_bindings",v.source_time_bindings,[&](auto& x,auto& row){x.object("key",[&](auto& y){return time_key(y,row.key);});x.object("time_source",[&](auto& y){return time_source(y,row.time_source);});x.object("actual_source_ref",[&](auto& y){return source(y,row.actual_source_ref);});if(!x.status().ok)return x.status();if(social_knowledge_cut_version(version)&&std::holds_alternative<SocialKnowledgeKey>(row.key)){if(x.reading()){if(!prepare_checkpoint_default_values<SourceRef>(x).ok)return x.status();row.learned_after_common_fact.emplace();}if(!row.learned_after_common_fact)return x.reject("KnowledgeCommonCutRequired");return x.object("learned_after_common_fact",[&](auto& y){return source(y,*row.learned_after_common_fact);});}if(row.learned_after_common_fact)return x.reject("UnselectedKnowledgeCommonCut");return x.status();});table(c,"message_execution_bindings",v.message_execution_bindings,[](auto& x,auto& row){x.field("message_id",row.message_id);x.object("source_binding",[&](auto& y){return message_source(y,row.source_binding);});x.optional("actual_delivery_ref",row.actual_delivery_ref,[](auto& y,auto& ref){return source(y,ref);});x.optional("actual_dossier_access_ref",row.actual_dossier_access_ref,[](auto& y,auto& ref){return source(y,ref);});x.sequence("actual_knowledge_keys",row.actual_knowledge_keys,[](auto& y,auto& key){return knowledge_key(y,key);});return x.object("channel_rule_ref",[&](auto& y){y.object("catalog_identity",[&](auto& z){return catalog(z,row.channel_rule_ref.catalog_identity);});return y.field("rule_ref",row.channel_rule_ref.rule_ref);});});table(c,"propagation_decisions",v.propagation_decisions,[&](auto& x,auto& r){return propagation_decision(x,r,version);});table(c,"propagation_cooldowns",v.propagation_cooldowns,[](auto& x,auto& r){return propagation_cooldown(x,r);});return c.object("collection_cursor",[&](auto& x){x.optional("last_scanned_fact_id",v.collection_cursor.last_scanned_fact_id,[](auto& y,auto& id){return y.field("value",id);});return x.object("through_moment",[&](auto& y){return moment(y,v.collection_cursor.through_moment);});});}
// 期间源严格两根，集合顺序只来自原Opening/实际关闭消费，不在load重采。
template<class C> Status periods(C& c,SocialPeriodSources& v){table(c,"openings",v.openings,[](auto& x,auto& row){x.field("period_id",row.period_id);x.object("opened_at",[&](auto& y){return moment(y,row.opened_at);});x.field("through_tick",row.through_tick);ids(x,"due_message_ids",row.due_message_ids);x.object("catalog_identity",[&](auto& y){return catalog(y,row.catalog_identity);});x.field("provider_rule_id",row.provider_rule_id);return x.field("provider_rule_version",row.provider_rule_version);});return table(c,"closures",v.closures,[](auto& x,auto& row){x.field("period_id",row.period_id);x.object("closed_at",[&](auto& y){return moment(y,row.closed_at);});return ids(x,"delivered_message_ids",row.delivered_message_ids);});}
// 原1/2此叶逐字段保持；仅新receiver槽由后继替换。
template<class E>Status visit_enum(CodecVisitor& c,const char* key,E& value,Integer maximum){Integer n=static_cast<Integer>(value);c.field(key,n);if(n<0||n>maximum)return c.reject("InvalidDynamicEnum",key);if(c.reading())value=static_cast<E>(n);return c.status();}
// 原1/2此叶逐字段保持；仅新receiver槽由后继替换。
Status visit_subject(CodecVisitor& c,SubjectRef& s){visit_enum(c,"kind",s.kind,2);return c.field("id",s.id);}
// 原1/2此叶逐字段保持；仅新receiver槽由后继替换。
Status visit_supply(CodecVisitor& c,SupplyObservation& v){c.field("source_key",v.source_key);c.field("organization_id",v.organization_id);c.field("site_id",v.site_id);c.field("inventory_id",v.inventory_id);c.field("item_id",v.item_id);c.field("source_fact_id",v.source_fact_id);c.field("target_amount",v.target_amount);c.field("physical_amount",v.physical_amount);return c.field("source_revision",v.source_revision);}
// 原1/2此叶逐字段保持；仅新receiver槽由后继替换。
Status visit_obligation(CodecVisitor& c,ObligationObservation& v){c.field("obligation_id",v.obligation_id);c.field("principal_id",v.principal_id);c.field("beneficiary_id",v.beneficiary_id);c.field("source_fact_id",v.source_fact_id);c.field("due_tick",v.due_tick);c.field("due_amount",v.due_amount);c.field("paid_amount",v.paid_amount);c.field("outstanding_amount",v.outstanding_amount);return c.field("arrear_id",v.arrear_id);}
// 原1/2此叶逐字段保持；仅新receiver槽由后继替换。
Status visit_fact(CodecVisitor& c,DynamicFactView& f){c.field("id",f.id);c.field("kind",f.kind);c.field("rule_version",f.rule_version);c.field("tick",f.tick);common_checkpoint::strings(c,"cause_fact_ids",f.cause_fact_ids);common_checkpoint::strings(c,"affair_ids",f.affair_ids);c.sequence("participants",f.participants,[](CodecVisitor& v,FactParticipant& p){v.object("subject",[&](CodecVisitor& x){return visit_subject(x,p.subject);});return v.field("role",p.role);});c.optional("supply",f.supply,visit_supply);c.optional("obligation",f.obligation,visit_obligation);c.field("subject_id",f.subject_id);c.field("outcome",f.outcome);return c.field("publicly_visible",f.publicly_visible);}
// binary原Report与KnownReport完全沿原paired叶次序，只有receiver槽增加tag。
Status visit_dynamic(CodecVisitor& c,DynamicSocialState& s){c.field("version",s.version);if(c.status().ok&&!social_time2_version(s.version))return c.reject("InvalidDynamicSocialVersion");c.field("last_collected_tick",s.last_collected_tick);common_checkpoint::table(c,"reports",s.reports,[](CodecVisitor& v,Report& r){v.field("id",r.id);v.field("fact_id",r.fact_id);v.field("payload_kind",r.payload_kind);v.object("origin_subject",[&](CodecVisitor& x){return visit_subject(x,r.origin_subject);});v.field("observation_tick",r.observation_tick);visit_enum(v,"visibility_scope",r.visibility_scope,2);v.optional("valid_until_tick",r.valid_until_tick,[](CodecVisitor& x,Integer& n){return x.field("value",n);});visit_enum(v,"reliability_kind",r.reliability_kind,2);return v.object("observation",[&](CodecVisitor& x){return visit_fact(x,r.observation);});});common_checkpoint::table(c,"pending_messages",s.pending_messages,[&](auto& x,auto& row){return message(x,row,s.version);});common_checkpoint::table(c,"known_reports",s.known_reports,[](CodecVisitor& v,OrderedTable<KnownReport>& t){return common_checkpoint::table(v,"known",t,[](CodecVisitor& x,KnownReport& k){x.object("receiver",[&](CodecVisitor& y){return visit_subject(y,k.receiver);});x.field("report_id",k.report_id);x.optional("source_message_id",k.source_message_id,[](CodecVisitor& y,StableId& id){return y.field("value",id);});x.field("learned_tick",k.learned_tick);return x.object("source_subject",[&](CodecVisitor& y){return visit_subject(y,k.source_subject);});});});if(!c.status().ok)return c.status();if(c.reading()){if(!prepare_checkpoint_default_values<SocialTime2State>(c).ok)return c.status();s.time2.emplace();}if(!s.time2)return c.reject("MissingSocialTime2State");return additions(c,*s.time2,s.version);}
// root名字和固定顺序必须完整相等，不能同版接受少根状态。
void roots(const BoundaryValue& v,std::initializer_list<const char*> names){const auto& d=boundary::object(v,"dynamic_social");if(d.size()!=names.size())boundary::fail("InvalidSocialTime2RootShape","dynamic_social");std::size_t i=0;for(auto name:names)if(d[i++].first!=name)boundary::fail("SocialTime2FieldOrderMismatch",name);}
// 即使调用方直接改过rows，物理表键仍须唯一，不靠可重建index掩盖重复。
template<class T> void unique_keys(const OrderedTable<T>& table){const auto& rows=table.rows();for(std::size_t i=0;i<rows.size();++i)for(std::size_t j=0;j<i;++j)if(rows[i].first==rows[j].first)boundary::fail("DuplicateSocialTime2Key",rows[i].first);}
// 写时先拒不存在精确state及非空未选表，不能输出可误认成功的半段。
void writable(const DynamicSocialState& s){if(!social_time2_version(s.version))boundary::fail("InvalidDynamicSocialVersion","dynamic_social");if(!s.time2)boundary::fail("MissingSocialTime2State","dynamic_social");unique_keys(s.reports);unique_keys(s.pending_messages);unique_keys(s.known_reports);for(const auto& row:s.known_reports.rows())unique_keys(row.second);take(validate_social_propagation_shape(s));}
Status emit_exact_unchecked(ProjectionWriter& out,const DynamicSocialState& state){
    out.field("version",state.version);out.field("last_collected_tick",state.last_collected_tick);
    out.object("reports",[&](auto& w){return emit_dynamic_social_reports_fields(w,state,true);});
    SocialProjectionArchive archive(out);table(archive,"pending_messages",const_cast<OrderedTable<PendingMessage>&>(state.pending_messages),[&](auto& c,auto& row){return message(c,row,state.version);});
    out.object("known_reports",[&](auto& w){return emit_dynamic_social_known_fields(w,state,true);});
    if(out.status().ok)additions(archive,const_cast<SocialTime2State&>(*state.time2),state.version);return out.status();
}
Status emit_periods_unchecked(ProjectionWriter& out,const SocialPeriodSources& state){SocialProjectionArchive archive(out);return periods(archive,const_cast<SocialPeriodSources&>(state));}
template<class F>Status checked_projection(ProjectionWriter& out,F emit){social_projection_detail::WireLimitWriter limits;auto checked=limits.object("",[&](auto& w){return emit(w);});if(checked.ok)checked=limits.finish();if(!checked.ok)return checked;return emit(out);}
} // 内部schema辅助只服务本物理版本。
// JSON原Report/Known只经过原paired解码，新消息无需构造伪造legacy接收者。
Outcome<DynamicSocialState> decode_dynamic_social_time2(const BoundaryValue& value){return guarded<DynamicSocialState>([&]{
 inspect_json(value);roots(value,{"version","last_collected_tick","reports","pending_messages","known_reports","source_time_bindings","message_execution_bindings","propagation_decisions","propagation_cooldowns","collection_cursor"});
 if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<DynamicSocialState>();DynamicSocialState state;state.version=boundary::get_int(value,"version","dynamic_social");if(!social_time2_version(state.version))boundary::fail("InvalidDynamicSocialVersion","dynamic_social");state.last_collected_tick=boundary::get_int(value,"last_collected_tick","dynamic_social");
 const auto&root=*value.as<BoundaryValue::Dict>();state.reports=take(decode_dynamic_social_reports_json(root[2].second));
 {JsonCodec messages(value,std::size_t{3},std::size_t{4});take(table(messages,"pending_messages",state.pending_messages,[&](auto&c,auto&row){return message(c,row,state.version);}));take(messages.finish());}
 state.known_reports=take(decode_dynamic_social_known_json(root[4].second));if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<SocialTime2State>();state.time2.emplace();JsonCodec tail(value,std::size_t{5},root.size());take(additions(tail,*state.time2,state.version));take(tail.finish());return state;
});}
// JSON原Report/Known原字段序不动，其它根按固定十根顺序追加。
Status emit_dynamic_social_time2_fields(ProjectionWriter& out,const DynamicSocialState& state){return guarded<Done>([&]{writable(state);take(checked_projection(out,[&](auto& w){return emit_exact_unchecked(w,state);}));return Done{};});}
Outcome<BoundaryValue> encode_dynamic_social_time2(const DynamicSocialState& state){return guarded<BoundaryValue>([&]{BoundaryValue value;BoundaryProjectionWriter writer(value);take(writer.object("",[&](auto& out){return emit_dynamic_social_time2_fields(out,state);}));take(writer.finish());return value;});}
// 解码仅在完整本段成功后安装候选；外层仍必须finish并核全世界闭包。
Status visit_dynamic_social_time2_checkpoint(CodecVisitor& c,DynamicSocialState& state){if(!c.status().ok)return c.status();return guarded<Done>([&]{if(!c.reading()){writable(state);take(visit_dynamic(c,state));}else{take(prepare_checkpoint_default_values<DynamicSocialState>(c));DynamicSocialState candidate;take(visit_dynamic(c,candidate));state=std::move(candidate);}return Done{};});}
// 期间JSON严格两张OrderedTable，版本选择仍由外层物理段注册负责。
Outcome<SocialPeriodSources> decode_social_period_sources(const BoundaryValue& value){return guarded<SocialPeriodSources>([&]{inspect_json(value);roots(value,{"openings","closures"});JsonCodec c(value);take(c.prepare_default_values<SocialPeriodSources>());SocialPeriodSources state;take(periods(c,state));take(c.finish());return state;});}
// 编码保实际捕获集合，绝不排序、补投递或写入第二知识时间。
Status emit_social_period_sources_fields(ProjectionWriter& out,const SocialPeriodSources& state){return guarded<Done>([&]{take(checked_projection(out,[&](auto& w){return emit_periods_unchecked(w,state);}));return Done{};});}
Outcome<BoundaryValue> encode_social_period_sources(const SocialPeriodSources& state){return guarded<BoundaryValue>([&]{BoundaryValue value;BoundaryProjectionWriter writer(value);take(writer.object("",[&](auto& out){return emit_social_period_sources_fields(out,state);}));take(writer.finish());return value;});}
// binary成对访问同一期间声明；失败不安装部分表。
Status visit_social_period_sources_checkpoint(CodecVisitor& c,SocialPeriodSources& state){if(!c.status().ok)return c.status();return guarded<Done>([&]{if(!c.reading())take(periods(c,state));else{take(prepare_checkpoint_default_values<SocialPeriodSources>(c));SocialPeriodSources candidate;take(periods(c,candidate));state=std::move(candidate);}return Done{};});}
namespace {
Status selected_exact_social(Integer selected,Integer actual){return !social_time2_version(selected)?common_failure("ExactSocialPhysicalSelectionRequired",{},ErrorKind::unsupported_capability):selected!=actual?common_failure("ExactSocialPhysicalSelectionMismatch",{},ErrorKind::internal):common_success();}
}
Outcome<DynamicSocialState> decode_dynamic_social_time2(const BoundaryValue&value,Integer selected){
 auto allowed=selected_exact_social(selected,selected);if(!allowed.ok)return {false,{},allowed.error};auto decoded=decode_dynamic_social_time2(value);if(!decoded.ok)return decoded;auto same=selected_exact_social(selected,decoded.value.version);if(!same.ok)return {false,{},same.error};return decoded;
}
Outcome<BoundaryValue> encode_dynamic_social_time2(const DynamicSocialState&state,Integer selected){auto same=selected_exact_social(selected,state.version);return same.ok?encode_dynamic_social_time2(state):Outcome<BoundaryValue>{false,{},same.error};}
Status emit_dynamic_social_time2_fields(ProjectionWriter&out,const DynamicSocialState&state,Integer selected){auto same=selected_exact_social(selected,state.version);return same.ok?emit_dynamic_social_time2_fields(out,state):same;}
Status visit_dynamic_social_time2_checkpoint(CodecVisitor&visitor,DynamicSocialState&state,Integer selected){
 auto allowed=selected_exact_social(selected,selected);if(!allowed.ok)return allowed;if(!visitor.reading()){auto same=selected_exact_social(selected,state.version);return same.ok?visit_dynamic_social_time2_checkpoint(visitor,state):same;}
 DynamicSocialState candidate;auto read=visit_dynamic_social_time2_checkpoint(visitor,candidate);if(!read.ok)return read;auto same=selected_exact_social(selected,candidate.version);if(!same.ok)return same;state=std::move(candidate);return common_success();
}
} // 原旧codec独立保留，是否允许S3与宏观期间源由选定profile决定。
