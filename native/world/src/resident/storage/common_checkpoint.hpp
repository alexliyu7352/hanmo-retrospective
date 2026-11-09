// 共同检查点仅遍历typed字段；回执payload由中央唯一codec注入，不能原样藏一个JSON树。
#pragma once
#include "common_codec.hpp"
#include "../checkpoint/codec_visitor.hpp"
#include "../checkpoint/projection_writer.hpp"
namespace hanmo::resident {
class CommonResultCodec {
public:
    virtual ~CommonResultCodec()=default;
    virtual Outcome<std::shared_ptr<const CommitResult>> decode(const BoundaryValue&) const=0;
    virtual BoundaryValue encode(const CommitResult&) const=0;
    // 原插件可继续旧encode；完整native路径必须由其真实writer覆盖此口。
    virtual Status emit(ProjectionWriter& out,const StableId&,const CommitResult&)const{return out.reject("ResultProjectionCodecRequired");}
    virtual Status visit(CodecVisitor&,std::shared_ptr<const CommitResult>&) const=0;
};
Status visit_common_checkpoint(CodecVisitor&,CommonState&,const CommonResultCodec* results=nullptr);
namespace common_checkpoint {
// The row wire belongs to the original caller. This single staged sequence/
// adoption leg is also used by dynamic {key,record} rows, which must not acquire
// Common's distinct object("entry") wrapper.
template<class A,class T,class Row> Status table_rows(
    A& c,std::string_view name,OrderedTable<T>& value,std::string_view duplicate_code,Row row){
    if(!c.reading())return c.sequence(name,value.rows(),row);
    std::vector<std::pair<StableId,T>> rows;auto result=c.sequence(name,rows,row);
    if(result.ok&&!value.adopt_checkpoint_rows(std::move(rows)))return c.reject(duplicate_code,name);
    return result;
}
template<class A,class T,class Row,class Transfer> Status table_rows(
    A& c,std::string_view name,OrderedTable<T>& value,std::string_view duplicate_code,Row row,Transfer&& transfer){
    (void)duplicate_code; // The finite fallible adoption reports its original failure.
    if(!c.reading())return c.sequence(name,value.rows(),row);
    CheckpointVector<std::pair<StableId,T>> rows;auto result=c.sequence(name,rows,row);
    if(!result.ok)return result;
    auto adopted=value.adopt_checkpoint_rows(std::move(rows),std::forward<Transfer>(transfer));
    return adopted.ok?c.status():adopted;
}
// 有序表只重建索引，不以排序替代权威序；duplicate key是恢复错误。
template<class T,class F,class Decode> Status table_fields(
    CodecVisitor& c,std::string_view name,OrderedTable<T>& value,F visit,Decode decode){
    auto row=[&](CodecVisitor& v,auto& entry){return v.object("entry",[&](CodecVisitor& e){
        e.field("key",entry.first);
        if constexpr(std::is_invocable_r_v<Status,F&,CodecVisitor&,const StableId&,T&>)
            return visit(e,static_cast<const StableId&>(entry.first),entry.second);
        else return visit(e,entry.second);
    });};

    return decode(row);
}
template<class T,class F> Status table(CodecVisitor& c,std::string_view name,OrderedTable<T>& value,F visit){
    // Existing ordinary decoded storage retains its original outer decoder
    // reservation; adopting the whole vector performs no element transfer.
    return table_fields(c,name,value,visit,[&](auto& row){
        return table_rows(c,name,value,"DuplicateCheckpointKey",row);
    });
}
template<class T,class F,class Transfer> Status table(
    CodecVisitor& c,std::string_view name,OrderedTable<T>& value,F visit,Transfer&& transfer){
    // Finite original owners explicitly supply their fallible exact-row move.
    // The sole table adoption reserves the new ordinary shell while the
    // original staged allocator block and decoded values are still retained.
    return table_fields(c,name,value,visit,[&](auto& row){
        return table_rows(c,name,value,"DuplicateCheckpointKey",row,std::forward<Transfer>(transfer));
    });
}
inline Status strings(CodecVisitor& c,std::string_view name,std::vector<StableId>& v){return c.sequence(name,v,[](CodecVisitor& x,StableId& value){return x.field("value",value);});}
inline constexpr std::uint32_t typed_resource_references=1;
inline constexpr std::uint32_t named_membership_receipts=2;
// 只有显式skills_extended/1的共同codec才写六项块；旧格式不多写存在位。
inline constexpr std::uint32_t extended_actor_skills=4;
inline constexpr std::uint32_t politics_receipts=8;
inline constexpr std::uint32_t political_fiscal_receipts=16;
inline constexpr std::uint32_t production_configuration_receipts=32;
inline constexpr std::uint32_t household_provision_configuration_receipts=4096;
// 仅Growth显式教学格式读写永久邀学来源；旧格式没有额外存在位。
inline constexpr std::uint32_t teaching_request_sources=64;
// 显式普通双边授课永久来源；旧Growth3..18无存在位。
inline constexpr std::uint32_t study_teaching_request_sources=512;
Status reservation(CodecVisitor&,ReservationRef&,const StableId& legacy_name);
Status owner(CodecVisitor&,Owner&);
Status location(CodecVisitor&,Location&);
Status lot(CodecVisitor&,LotView&);
Status wallet(CodecVisitor&,WalletView&);
Status inventory(CodecVisitor&,InventoryView&);
} // owner/位置属于共同schema，领域checkpoint调用同一实现。
} // writer/reader只接触尚未发布的typed对象，成功后仍需所有领域校验。
