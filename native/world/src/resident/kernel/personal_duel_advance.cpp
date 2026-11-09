// 只在真实原advance成功之后制作pending条件读凭据，不生成永久事实或第二次时钟推进。
#include "personal_duel_advance.hpp"
#include "world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 原事实头保完整顺序与字段存在序，不能以相同末ID代替整段来源。
bool same_heads(const OrderedTable<FactRecord>& a,const OrderedTable<FactRecord>& b){
    if(a.order()!=b.order())return false;
    for(const auto& row:a.rows()){const auto* value=b.find(row.first);if(!value||row.second.id!=value->id||row.second.kind!=value->kind||row.second.domain!=value->domain||row.second.field_order!=value->field_order)return false;}
    return true;
}
// 原随机流也须停在实际advance尾；pending纯读不能偷抽任何旧流。
bool same_random(const OrderedTable<RngStream>& a,const OrderedTable<RngStream>& b){
    if(a.order()!=b.order())return false;
    for(const auto& row:a.rows()){const auto* value=b.find(row.first);if(!value||row.second.seed!=value->seed||row.second.state!=value->state||row.second.field_order!=value->field_order)return false;}
    return true;
}
// 原共同primitive版本只作当前纯读失效护栏，不是人物、物批或业务历史授权。
std::vector<ReadVersion> current_versions(const CommonStore& c){
    std::vector<ReadVersion> result;const auto& s=c.state();
    const auto add=[&](ResourceKey::Kind kind,const std::vector<StableId>& ids){for(const auto& id:ids)result.push_back(c.read_version({kind,id}));};
    add(ResourceKey::Kind::actor,s.actors.order());add(ResourceKey::Kind::action,s.actions.order());add(ResourceKey::Kind::party,s.parties.order());add(ResourceKey::Kind::wallet,s.wallets.order());add(ResourceKey::Kind::inventory,s.inventories.order());return result;
}
// 同值重写也会推进原版本；回滚后版本恢复则另由保存点边界拒绝。
bool same_versions(const std::vector<ReadVersion>& a,const std::vector<ReadVersion>& b){
    if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].resource.kind!=b[i].resource.kind||a[i].resource.id!=b[i].resource.id||a[i].version!=b[i].version)return false;return true;
}
}
Outcome<std::unique_ptr<GrowthConditionDuelAdvanceProof>> advance_world_for_personal_condition(WorldBundle& w,const std::vector<StableId>& hold,AdvanceDetails& details){
    const auto start=w.common.state().identity;const auto capture=w.common.actor_condition_capture_identity();
    if(!capture)return {false,{},common_failure("PersonalDuelAdvanceConditionCaptureRequired").error};
    if(start.tick==std::numeric_limits<Integer>::max())return {false,{},common_failure("WorldTickOverflow").error};
    auto elapsed=advance_world(w,hold);if(!elapsed.ok)return {false,{},elapsed.error};
    const auto& end=w.common.state().identity;
    if(end.world_id!=start.world_id||end.execution_epoch!=start.execution_epoch||end.revision!=start.revision||end.tick!=start.tick+1||w.common.actor_condition_capture_identity()!=capture)return {false,{},common_failure("PersonalDuelAdvanceCompletedBoundaryMismatch").error};
    auto savepoint=w.common.actor_condition_savepoint_boundary();if(!savepoint.ok)return {false,{},savepoint.error};
    // 局部具名类只能在本函数真实成功路径构造；对外仅给S4原抽象纯读接口。
    class SuccessfulAdvanceProof final:public GrowthConditionDuelAdvanceProof {
    public:
        SuccessfulAdvanceProof(const CommonStore& c,Integer from,ActorConditionSavepointBoundary boundary)
          :common_(&c),capture_(c.actor_condition_capture_identity()),savepoint_(std::move(boundary)),from_(from),identity_(c.state().identity),next_(c.state().next_instance_id),player_(c.state().metadata.player_actor_id),facts_(c.state().facts),random_(c.state().rng),versions_(current_versions(c)),draft_count_(c.actor_condition_execution_drafts().size()){}
        SuccessfulAdvanceProof(const SuccessfulAdvanceProof&)=delete;
        SuccessfulAdvanceProof& operator=(const SuccessfulAdvanceProof&)=delete;
        // 验证可重复，只要原区间没有写入或回滚；任何一次失败后凭据永久失效。
        Status validate_duel_advance(const CommonStore& c,Integer from,Integer to,const std::shared_ptr<const ActorConditionCaptureIdentity>& capture)const override{
            const auto& id=c.state().identity;
            if(used_||invalid_||common_!=&c||!capture||capture!=capture_||capture!=c.actor_condition_capture_identity()||from!=from_||to!=identity_.tick||id.world_id!=identity_.world_id||id.execution_epoch!=identity_.execution_epoch||id.revision!=identity_.revision||id.tick!=identity_.tick||c.state().next_instance_id!=next_||c.state().metadata.player_actor_id!=player_||c.actor_condition_execution_drafts().size()!=draft_count_||!c.validate_actor_condition_savepoint_boundary(savepoint_).ok||!same_heads(facts_,c.state().facts)||!same_random(random_,c.state().rng)||!same_versions(versions_,current_versions(c))){invalid_=true;return common_failure("PersonalDuelAdvanceProofExpired");}
            return common_success();
        }
        // 只有完整域重放已经成功的调用者才消费；第二次或失效后不能恢复。
        Status consume_duel_advance(const CommonStore& c,Integer from,Integer to,const std::shared_ptr<const ActorConditionCaptureIdentity>& capture)override{
            auto status=validate_duel_advance(c,from,to,capture);if(status.ok)used_=true;return status;
        }
    private:
        const CommonStore* common_;
        std::shared_ptr<const ActorConditionCaptureIdentity> capture_;
        ActorConditionSavepointBoundary savepoint_;
        Integer from_;
        WorldIdentity identity_;
        Integer next_;
        StableId player_;
        OrderedTable<FactRecord> facts_;
        OrderedTable<RngStream> random_;
        std::vector<ReadVersion> versions_;
        std::size_t draft_count_=0;
        bool used_=false;
        mutable bool invalid_=false;
    };
    std::unique_ptr<GrowthConditionDuelAdvanceProof> proof=std::make_unique<SuccessfulAdvanceProof>(w.common,start.tick,std::move(savepoint.value));
    details=std::move(elapsed.value);return {true,std::move(proof),{}};
}
}
