// 共同读写落同一typed候选；资源recipe先暂存涉及容器，成功后按原序更新，不扫描复制整世界。
#include "common_store.hpp"
#include "common_capture_read_set.hpp"
#include "common_command_publication.hpp"
#include "original_inventory_mutation.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <new>
#include <type_traits>
#include <variant>
#include <charconv>
#include <algorithm>
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include <stdexcept>
#include "../model/common_rng.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
// 每次实际begin单独分配，旧边界持有期间绝不复用其地址；没有永久身份含义。
// The original tracked shared factory owns each identity object and control block.
// Capture vectors, copied row payloads and SavepointData are separate owners.
struct ResourceExecutionCaptureIdentity {};
// 私有观察状态的所有键及嵌套字符串均走原checkpoint allocator，不借调用者临时值。
struct CommonCaptureReadSetProof::State {
    using Text=std::basic_string<char,std::char_traits<char>,CheckpointAllocator<char>>;
    // 只复制原资源键；版本数相等不能撤销已经观察到的写入。
    struct Resource {
        ResourceKey::Kind kind;
        Text id;
        explicit Resource(const ResourceKey& key):kind(key.kind),id(key.id.data(),key.id.size()){}
    };
    // 复制原真实事实头及字段序；领域payload仍只由原所属域与上层输入校验负责。
    struct Source {
        Text id,kind,domain;
        CheckpointVector<Text> field_order;
        explicit Source(const FactRecord& value)
            :id(value.id.data(),value.id.size()),kind(value.kind.data(),value.kind.size()),
             domain(value.domain.data(),value.domain.size()) {
            field_order.reserve(value.field_order.size());
            for(const auto& field:value.field_order)field_order.emplace_back(field.data(),field.size());
        }
        // 只借当前真实头比较，不把数值或字符串相同当成连续身份的替代证明。
        bool matches(const FactRecord& value)const noexcept {
            if(std::string_view(id.data(),id.size())!=value.id||
               std::string_view(kind.data(),kind.size())!=value.kind||
               std::string_view(domain.data(),domain.size())!=value.domain||
               field_order.size()!=value.field_order.size())return false;
            for(std::size_t n=0;n<field_order.size();++n)
                if(std::string_view(field_order[n].data(),field_order[n].size())!=value.field_order[n])return false;
            return true;
        }
    };
    const CommonStore* common=nullptr;
    std::shared_ptr<const ActorConditionCaptureIdentity> capture;
    CheckpointVector<Resource> resources;
    CheckpointVector<Source> sources;
    bool actor_dirty=false,action_dirty=false,resource_dirty=false,source_dirty=false;
    // 工厂已核来源存在；构造仅持有完整复制，异常时尚未注册到活动捕获。
    State(const CommonStore& value,std::shared_ptr<const ActorConditionCaptureIdentity> identity,
          const std::vector<ResourceKey>& keys,const std::vector<SourceRef>& headers)
        :common(&value),capture(std::move(identity)) {
        resources.reserve(keys.size());
        for(const auto& key:keys)resources.emplace_back(key);
        sources.reserve(headers.size());
        for(const auto& header:headers)sources.emplace_back(*value.state().facts.find(header.id));
    }
    // 原写路径只扫描已有键并置粘滞位，不分配、不移除观察者，也不重新捕获。
    void changed(ResourceKey::Kind kind,std::string_view id)noexcept {
        for(const auto& key:resources)if(key.kind==kind&&std::string_view(key.id.data(),key.id.size())==id){
            if(kind==ResourceKey::Kind::actor)actor_dirty=true;
            else if(kind==ResourceKey::Kind::action)action_dirty=true;
            else resource_dirty=true;
            return;
        }
    }
    // 事实ID全局唯一；旧头替换成其他schema也必须使原来源观察失效。
    void changed_source(std::string_view id)noexcept {
        for(const auto& source:sources)if(std::string_view(source.id.data(),source.id.size())==id){source_dirty=true;return;}
    }
};
// 原人物捕获拥有独立弱注册表；结束重开不能复用旧证明，注册表本身也受实际分配计费。
struct ActorConditionCaptureIdentity {
    // 发布标记只属于这次完整命令捕获；Common复制/移动及捕获结束均不继承。
    struct CommandPublication {
        const CommonStore *common = nullptr;
        Integer base_revision = 0;
        bool revision_reserved = false;
    };
    mutable std::optional<CommandPublication> command_publication;
    mutable CheckpointVector<std::weak_ptr<CommonCaptureReadSetProof::State>> read_sets;
    // 清走已释放证明的弱项；每个新证明追加独立状态，绝不覆盖旧观察状态。
    void observe(const std::shared_ptr<CommonCaptureReadSetProof::State>& state)const {
        read_sets.erase(std::remove_if(read_sets.begin(),read_sets.end(),
            [](const auto& weak){return weak.expired();}),read_sets.end());
        read_sets.emplace_back(state);
    }
    // 通知只锁定已有弱引用；原Common写入和保存点恢复均不产生新的读集分配。
    void changed(ResourceKey::Kind kind,std::string_view id)const noexcept {
        for(const auto& weak:read_sets)if(auto state=weak.lock())state->changed(kind,id);
    }
    // 事实替换、删除与恢复共用相同失效入口，保持各证明的独立粘滞状态。
    void changed_source(std::string_view id)const noexcept {
        for(const auto& weak:read_sets)if(auto state=weak.lock())state->changed_source(id);
    }
};
// 只记录原候选和基准版本，不提前改世界，也不把普通/Opening捕获当命令提交。
Status begin_complete_command_publication(CommonStore &common) {
    const auto capture = common.actor_condition_capture_identity();
    if (!capture || capture->command_publication)
        return common_failure("CompleteCommandPublicationCaptureRequired", {},
                              ErrorKind::internal);
    capture->command_publication.emplace(
        ActorConditionCaptureIdentity::CommandPublication{
            &common, common.revision(), false});
    return common_success();
}
// 原封口写边界内仅首次递增；重复封口须仍在原Common及同一预留版本，不能修补漂移。
Status reserve_complete_command_publication_revision(CommonStore &common) {
    const auto capture = common.actor_condition_capture_identity();
    if (!capture || !capture->command_publication ||
        capture->command_publication->common != &common)
        return common_failure("CompleteCommandPublicationCaptureRequired", {},
                              ErrorKind::internal);
    auto &publication = *capture->command_publication;
    auto &revision = common.state().identity.revision;
    if (publication.revision_reserved) {
        if (publication.base_revision == std::numeric_limits<Integer>::max() ||
            revision != publication.base_revision + 1)
            return common_failure("CompleteCommandPublicationRevisionMismatch",
                                  {}, ErrorKind::internal);
        return common_success();
    }
    if (revision != publication.base_revision)
        return common_failure("CompleteCommandPublicationRevisionMismatch", {},
                              ErrorKind::internal);
    if (revision == std::numeric_limits<Integer>::max())
        return common_failure("WorldRevisionOverflow");
    ++revision;
    publication.revision_reserved = true;
    return common_success();
}
struct ActorLocationCaptureIdentity {};
struct ActorLocationWriteIdentity {};
// 同一捕获和每个已发布recipe分别有不可复制的实际分配身份。
struct ProtectedFreightCaptureIdentity {};
struct ProtectedFreightRecipeIdentity {};
// 局部撤销记录只含实际被primitive触及的对象，按逆序恢复原行位置及读版本。
struct CommonStore::SavepointData {
    struct ActorBefore {StableId id;ActorRecord value;};
    struct ActorCreated {StableId id;};
    struct PlayerBefore {StableId id;};
    struct PartyBefore {StableId id;std::optional<PartyRecord> value;std::size_t position=0;};
    struct FactBefore {StableId id;FactRecord value;};
    struct ActionBefore {StableId id;std::optional<ActionCoreView> value;std::size_t position=0;std::optional<FieldOrder> order;std::size_t order_position=0;};
    struct WalletBefore {StableId id;std::optional<WalletView> value;};
    struct InventoryBefore {StableId id;std::optional<InventoryView> value;};
    struct FactAdded {StableId id;};
    struct RoleBefore {StableId id;RoleRecord value;};
    struct VersionBefore {std::pair<int,StableId> key;std::optional<std::uint64_t> value;};
    struct NextBefore {Integer value;};
    struct RngBefore {StableId id;RngStream value;};
    using Undo=std::variant<ActorBefore,ActorCreated,PlayerBefore,PartyBefore,FactBefore,ActionBefore,WalletBefore,InventoryBefore,FactAdded,RoleBefore,VersionBefore,NextBefore,RngBefore>;
    Integer token=0;std::vector<Undo> undo;std::optional<WalletDebitObservation> debit_before;std::optional<std::size_t> resource_drafts_before,actor_location_drafts_before,actor_condition_drafts_before,actor_selection_drafts_before,freight_drafts_before;
    // 保存点可跨真实finish，必须还原既有行完成位，单截断新增行不足以恢复原边界。
    std::vector<bool> actor_selection_completed_before;
};
namespace {
// 有符号运算显式检查溢出，不依赖C++未定义行为。
bool add_checked(Integer a,Integer b,Integer& out) {if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return false;out=a+b;return true;}
bool matching_owner(const Owner& owner,const std::vector<Owner>& owners) {return std::any_of(owners.begin(),owners.end(),[&](const Owner& o){return owner_equal(owner,o);});}
Outcome<Integer> count_lots(const InventoryView& inv,const StableId& item,const std::vector<Owner>* owners,const ReservationRef* reservation,const Owner* sole_owner=nullptr) {Integer n=0;for(auto& l:inv.lots)if(l.item_id==item&&(!reservation||l.reserved_by==*reservation)&&(!owners||matching_owner(l.owner,*owners))&&(!sole_owner||owner_equal(l.owner,*sole_owner)))if(l.quantity<0||!add_checked(n,l.quantity,n))return {false,0,common_failure("ProjectionOverflow").error};return {true,n,{}};}
Outcome<Integer> count_lots(const InventoryView& inv,const StableId& item,const std::vector<Owner>& owners,const ReservationRef& reservation){return count_lots(inv,item,&owners,&reservation);}
// 局部证明失败只给具名原因；上层保留它自己的原始业务错误与回滚回执。
Status read_set_failure(const char* code,const char* reason){
    auto failed=common_failure(code);failed.error.reason=reason;return failed;
}
}
// 唯一构造入口拥有已分配观察状态，不暴露其键或置脏位给调用者。
CommonCaptureReadSetProof::CommonCaptureReadSetProof(std::shared_ptr<State> state)noexcept
    :state_(std::move(state)){}
CommonCaptureReadSetProof::~CommonCaptureReadSetProof()=default;
// 同一活捕获中只登记一次完整读集；重复/无效键在任何注册或复制前拒绝。
Outcome<std::unique_ptr<CommonCaptureReadSetProof>> CommonCaptureReadSetProof::capture(
    const CommonStore& common,const std::vector<ResourceKey>& resources,
    const std::vector<SourceRef>& sources){
    auto identity=common.actor_condition_capture_identity();
    if(!identity)return {false,{},read_set_failure("CommonCaptureReadSetCaptureRequired","pending_start_capture_lifetime_changed").error};
    if(resources.empty()&&sources.empty())return {false,{},common_failure("InvalidCommonCaptureReadSet").error};
    for(std::size_t n=0;n<resources.size();++n){
        const auto& key=resources[n];
        if(key.id.empty()||int(key.kind)<int(ResourceKey::Kind::actor)||int(key.kind)>int(ResourceKey::Kind::army))
            return {false,{},common_failure("InvalidCommonCaptureReadSet",key.id).error};
        for(std::size_t j=0;j<n;++j)if(resources[j].kind==key.kind&&resources[j].id==key.id)
            return {false,{},common_failure("DuplicateCommonCaptureReadSetKey",key.id).error};
    }
    for(std::size_t n=0;n<sources.size();++n){
        const auto& source=sources[n];
        for(std::size_t j=0;j<n;++j)if(sources[j].id==source.id)
            return {false,{},common_failure("DuplicateCommonCaptureReadSetSource",source.id).error};
        const auto* actual=common.state().facts.find(source.id);
        if(!valid_source_ref(source)||!actual||actual->id!=source.id||actual->kind!=source.kind||actual->domain!=source.domain)
            return {false,{},read_set_failure("CommonCaptureReadSetSourceRequired","pending_start_business_source_changed").error};
    }
    auto state=checkpoint_make_shared<State>(common,identity,resources,sources);
    auto proof=std::unique_ptr<CommonCaptureReadSetProof>(new CommonCaptureReadSetProof(state));
    identity->observe(state);
    return {true,std::move(proof),{}};
}
// 相同最终值不覆盖历史写入；只有无关资源的保存点开始、提交或恢复可以通过。
Status CommonCaptureReadSetProof::validate(const CommonStore& common)const{
    if(!state_||state_->common!=&common||!state_->capture||state_->capture!=common.actor_condition_capture_identity())
        return read_set_failure("CommonCaptureReadSetChanged","pending_start_capture_lifetime_changed");
    if(state_->action_dirty)return read_set_failure("CommonCaptureReadSetChanged","pending_start_action_lifetime_changed");
    if(state_->actor_dirty)return read_set_failure("CommonCaptureReadSetChanged","pending_start_actor_written");
    if(state_->source_dirty)return read_set_failure("CommonCaptureReadSetChanged","pending_start_business_source_changed");
    if(state_->resource_dirty)return read_set_failure("CommonCaptureReadSetChanged","pending_start_resource_written");
    for(const auto& source:state_->sources){
        const auto& rows=common.state().facts.rows();
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return std::string_view(source.id.data(),source.id.size())==row.first;});
        if(found==rows.end()||!source.matches(found->second)){
            state_->source_dirty=true;
            return read_set_failure("CommonCaptureReadSetChanged","pending_start_business_source_changed");
        }
    }
    return common_success();
}
// 原头写入者只广播已存在的捕获；无捕获时严格保留原路径且不制造新身份。
void invalidate_common_capture_source(const CommonStore& common,std::string_view source_id)noexcept{
    if(auto identity=common.actor_condition_capture_identity())identity->changed_source(source_id);
}
Status apply_original_action_resource_release_in_candidate(CommonWrite& common,const StableId& action_id){
 static_assert(std::is_nothrow_move_constructible_v<ReleaseActionResources>,"original release input transfer must not copy payload");
 auto input_memory=CheckpointMemoryCharge(0);
 ResourceRecipe recipe;recipe.operations.reserve(1);
 recipe.operations.emplace_back(std::in_place_type<ReleaseActionResources>,ReleaseActionResources{action_id});
 return common.apply_resources(recipe);
}
CommonStore::CommonStore(CommonState state,std::shared_ptr<const CommonCatalog> catalog):state_(std::move(state)),catalog_(std::move(catalog)){}
CommonStore::CommonStore(const CommonStore& o):checkpoint_copy_memory_([&]{return CheckpointMemoryCharge(0);}()),savepoint_sequence_(o.savepoint_sequence_),state_(o.state_),catalog_(o.catalog_),field_battle_selected_(o.field_battle_selected_),versions_(o.versions_){}
CommonStore& CommonStore::operator=(const CommonStore& o){if(this!=&o){CommonStore copy(o);copy.savepoint_sequence_=std::max(savepoint_sequence_,o.savepoint_sequence_);*this=std::move(copy);}return *this;}
// 移动保留原共同保存点与所有既有成员语义，仅丢弃当前栈临时捕获及其撤销标记。
CommonStore::CommonStore(CommonStore&& o) noexcept
    :checkpoint_copy_memory_(std::move(o.checkpoint_copy_memory_)),wallet_debit_(std::move(o.wallet_debit_)),resource_execution_capture_(std::move(o.resource_execution_capture_)),protected_freight_capture_(std::move(o.protected_freight_capture_)),actor_location_capture_(std::move(o.actor_location_capture_)),actor_condition_capture_(std::move(o.actor_condition_capture_)),actor_selection_capture_(std::move(o.actor_selection_capture_)),
     savepoint_(std::move(o.savepoint_)),savepoint_sequence_(o.savepoint_sequence_),state_(std::move(o.state_)),
     catalog_(std::move(o.catalog_)),membership_(o.membership_),field_battle_selected_(o.field_battle_selected_),task_owners_(o.task_owners_),family_identity_(o.family_identity_),
     household_owners_(o.household_owners_),organization_owners_(o.organization_owners_),reservation_owners_(o.reservation_owners_),
     protected_market_(std::move(o.protected_market_)),solitary_memory_(std::move(o.solitary_memory_)),solitary_(std::move(o.solitary_)),checkpoint_read_cache_frozen_(o.checkpoint_read_cache_frozen_),versions_(std::move(o.versions_)) {
    o.action_time_validation_=nullptr;o.carried_location_validation_=nullptr;
    o.selected_lot_semantics_=nullptr;o.domain_activity_reservation_owners_=nullptr;
    if(savepoint_){savepoint_->resource_drafts_before.reset();savepoint_->freight_drafts_before.reset();savepoint_->actor_location_drafts_before.reset();savepoint_->actor_condition_drafts_before.reset();savepoint_->actor_selection_drafts_before.reset();savepoint_->actor_selection_completed_before.clear();}
}
CommonStore& CommonStore::operator=(CommonStore&& o) noexcept {
    if(this!=&o){this->~CommonStore();new(this)CommonStore(std::move(o));}return *this;
}
// 查询缓存既不持权威状态，也不能在后台lease存在时继续增长未计费的保留图。
void CommonStore::clear_checkpoint_read_cache()noexcept{solitary_.clear();solitary_memory_={};checkpoint_read_cache_frozen_=false;}
Status CommonStore::prepare_checkpoint_read_cache(){
    if(savepoint_||wallet_debit_||resource_execution_capture_.active||protected_freight_capture_.read||actor_location_capture_.active||actor_condition_capture_.active||actor_selection_capture_.active)return common_failure("CheckpointRequiresStableBoundary",{},ErrorKind::internal);
    // 稳定边界的旧捕获已结束，真正释放其空/陈旧capacity；不把非空原运行scope抹去。
    if(resource_execution_capture_.identity||resource_execution_capture_.in_recipe||protected_freight_capture_.identity||actor_location_capture_.identity||actor_condition_capture_.identity||actor_selection_capture_.identity||actor_selection_capture_.scope)return common_failure("CheckpointRequiresStableBoundary",{},ErrorKind::internal);
    decltype(resource_execution_capture_.drafts)().swap(resource_execution_capture_.drafts);
    decltype(protected_freight_capture_.drafts)().swap(protected_freight_capture_.drafts);
    decltype(actor_location_capture_.drafts)().swap(actor_location_capture_.drafts);
    decltype(actor_condition_capture_.drafts)().swap(actor_condition_capture_.drafts);
    decltype(actor_selection_capture_.drafts)().swap(actor_selection_capture_.drafts);
    if(checkpoint_read_cache_frozen_)return common_success();
    CheckpointMemoryCharge charge;CheckpointHashMap<StableId,std::vector<StableId>> next;next.reserve(state_.actors.size());
    for(const auto& row:state_.actors.rows()){const auto& id=row.first;(charge).grow(0);std::vector<StableId> one;one.reserve(1);one.emplace_back(id);next.emplace(StableId(id),std::move(one));}
    solitary_.swap(next);solitary_memory_.swap(charge);checkpoint_read_cache_frozen_=true;return common_success();
}
// 保留实际 allocator owner 和同 job 生命周期；不再逐字符串计量。
void CommonStore::retain_checkpoint_read_cache(CheckpointRetentionCensus& census)const{
    census.owner(versions_.get_allocator().allocation_owner());
    census.owner(solitary_.get_allocator().allocation_owner());
    census.resource(solitary_memory_.resource());
}
CommonState& CommonStore::state(){return state_;}const CommonState& CommonStore::state()const{return state_;}
void CommonStore::bind_membership(const MembershipResolver* p,bool selected){membership_=p;field_battle_selected_=selected;clear_checkpoint_read_cache();}
Outcome<bool> CommonStore::field_battle_bound(const StableId& actor)const{
    if(field_battle_selected_&&(!membership_||!membership_->field_battle_binding_ready()))
        return {false,false,common_failure("FieldBattleBindingReaderRequired",actor,ErrorKind::internal).error};
    return membership_?membership_->field_battle_bound(actor):Outcome<bool>{true,false,{}};
}
void CommonStore::bind_task_owners(const TaskOwnerRead* source){task_owners_=source;}
void CommonStore::bind_household_owners(const HouseholdOwnerRead* source){household_owners_=source;}
void CommonStore::bind_organization_owners(const OrganizationOwnerRead* source){organization_owners_=source;}
void CommonStore::bind_reservation_owners(const ReservationOwnerRead* source){reservation_owners_=source;}
void CommonStore::bind_action_time_validation(const ActionTimeValidationRead* value){action_time_validation_=value;}
void CommonStore::bind_carried_location_validation(const CarriedLocationValidationRead* value){carried_location_validation_=value;}
void CommonStore::bind_domain_activity_reservation_owners(const DomainActivityReservationOwnerRead* read){domain_activity_reservation_owners_=read;}
void CommonStore::bind_selected_lot_semantics(const SelectedLotSemanticRead* read){selected_lot_semantics_=read;}
bool CommonStore::valid_resource_owner(const Owner& o)const{if(o.extra||o.id.empty()||!catalog_)return false;if(o.kind==Owner::Kind::actor)return actor(o.id)!=nullptr;if(o.kind==Owner::Kind::organization)return state_.metadata.ruleset_id==dynamic_ruleset_id?organization_owners_&&organization_owners_->organization_exists(o.id):std::find(catalog_->organization_ids.begin(),catalog_->organization_ids.end(),o.id)!=catalog_->organization_ids.end();if(o.kind==Owner::Kind::task)return task_owners_&&task_owners_->task_exists(o.id);if(o.kind==Owner::Kind::household)return state_.metadata.ruleset_id==dynamic_ruleset_id&&household_owners_&&household_owners_->household_exists(o.id);return false;}
const ItemDefinition* CommonStore::item_definition(const StableId& id)const{return catalog_?catalog_->items.find(id):nullptr;}
bool CommonStore::reservation_exists(const ReservationRef& ref)const{if(ref.id.empty())return false;
if(ref.kind==ReservationRef::Kind::domain_activity){if(!catalog_||!catalog_->domain_activity_reservations_enabled||state_.metadata.ruleset_id!=dynamic_ruleset_id||ref.domain_id.empty()||ref.activity_kind.empty()||!domain_activity_reservation_owners_)return false;auto owner=domain_activity_reservation_owners_->read_domain_activity_reservation_owner(ref);return owner.ok&&owner.value.reference==ref&&owner.value.owner_revision>0&&owner.value.phase!=ReservationOwnerPhase::terminal&&owner.value.may_retain_resources;}
if(!ref.domain_id.empty()||!ref.activity_kind.empty())return false;if(ref.kind==ReservationRef::Kind::action)return action(ref.id)!=nullptr;return state_.metadata.ruleset_id==dynamic_ruleset_id&&(ref.kind==ReservationRef::Kind::contract||ref.kind==ReservationRef::Kind::org_activity)&&reservation_owners_&&reservation_owners_->reservation_exists(ref);}

void CommonStore::bind_family_identity(const FamilyIdentityRead* source){family_identity_=source;}
void CommonStore::invalidate(const ResourceKey& key){changed(key.kind,key.id);}
const ActorCoreView* CommonStore::actor(const StableId& id)const{return state_.actors.find(id);}
const ActionCoreView* CommonStore::action(const StableId& id)const{return state_.actions.find(id);}
const WalletView* CommonStore::wallet(const StableId& id)const{return state_.wallets.find(id);}
const InventoryView* CommonStore::inventory(const StableId& id)const{return state_.inventories.find(id);}
const std::vector<StableId>* CommonStore::members(const StableId& id)const{if(!actor(id))return nullptr;if(membership_)if(auto* p=membership_->active_army_members(id))return p;for(auto& row:state_.parties.rows())if(std::find(row.second.members.begin(),row.second.members.end(),id)!=row.second.members.end())return &row.second.members;auto p=solitary_.find(id);if(p==solitary_.end()){
    if(checkpoint_read_cache_frozen_)throw std::logic_error("CheckpointFrozenWorldChanged");
    (solitary_memory_).grow(0);
    std::vector<StableId> one;one.reserve(1);one.emplace_back(id);p=solitary_.emplace(StableId(id),std::move(one)).first;
}return &p->second;}
const std::vector<StableId>& CommonStore::actor_order()const{return state_.actors.order();}const std::vector<StableId>& CommonStore::action_order()const{return state_.actions.order();}
namespace {
Outcome<Integer> wallet_spendable_from_frame(const WalletView& frame,const StableId& original_id){Integer total=0;for(const auto& r:frame.reserved)if(r.amount<0||!add_checked(total,r.amount,total))return {false,0,common_failure("ProjectionOverflow",original_id).error};Integer n;if(!add_checked(frame.balance,-total,n))return {false,0,common_failure("ProjectionOverflow",original_id).error};return {true,n,{}};}
}
Outcome<Integer> read_wallet_spendable(const WalletView& frame){return wallet_spendable_from_frame(frame,frame.id);}
Outcome<Integer> CommonStore::spendable(const StableId& id)const{const auto* frame=wallet(id);if(!frame)return {false,0,common_failure("UnknownWallet",id).error};return wallet_spendable_from_frame(*frame,id);}
namespace {
Outcome<Integer> inventory_weight_from_frame(const InventoryView& inventory,const CommonCatalog* catalog,const StableId& original_id){
 Integer total=0;for(const auto& lot:inventory.lots){const auto* item=catalog?catalog->items.find(lot.item_id):nullptr;if(!item)return {false,0,common_failure("UnknownItem",lot.item_id).error};
  if(lot.quantity<0||item->weight<0||(item->weight&&lot.quantity>INT64_MAX/item->weight)||!add_checked(total,lot.quantity*item->weight,total))return {false,0,common_failure("ProjectionOverflow",original_id).error};}
 return {true,total,{}};
}
}
Outcome<Integer> read_inventory_weight(const InventoryView& frame,const CommonCatalog& catalog){return inventory_weight_from_frame(frame,&catalog,frame.id);}
Outcome<Integer> CommonStore::inventory_weight(const StableId& id)const{const auto* frame=inventory(id);if(!frame)return {false,0,common_failure("UnknownInventory",id).error};return inventory_weight_from_frame(*frame,catalog_.get(),id);}
Outcome<Integer> count_inventory_frame_items(const InventoryView& frame,const StableId& item,const std::vector<Owner>& owners,const ReservationRef& reservation){return count_lots(frame,item,owners,reservation);}
Outcome<Integer> count_inventory_items(const CommonRead& common,const StableId& id,const StableId& item,const std::vector<Owner>& owners,const ReservationRef& reservation){auto* inv=common.inventory(id);if(!inv)return {false,0,common_failure("UnknownInventory",id).error};return count_lots(*inv,item,owners,reservation);}
Outcome<Integer> count_inventory_items_all_owners(const CommonRead& common,const StableId& id,const StableId& item,const ReservationRef& reservation){auto* inv=common.inventory(id);if(!inv)return {false,0,common_failure("UnknownInventory",id).error};return count_lots(*inv,item,nullptr,&reservation);}
Outcome<Integer> count_inventory_physical_items(const CommonRead& common,const StableId& id,const StableId& item,const Owner& owner){auto* inv=common.inventory(id);if(!inv)return {false,0,common_failure("UnknownInventory",id).error};return count_lots(*inv,item,nullptr,nullptr,&owner);}
Outcome<Integer> CommonStore::count(const StableId& id,const StableId& item,const std::vector<Owner>& owners,const ReservationRef& reservation)const{return count_inventory_items(*this,id,item,owners,reservation);}
ReadVersion CommonStore::read_version(const ResourceKey& k)const{auto i=versions_.find({int(k.kind),k.id});return {k,i==versions_.end()?0:i->second};}
Integer CommonStore::tick()const{return state_.identity.tick;}Integer CommonStore::revision()const{return state_.identity.revision;}
// 每次原资源写入先置脏匹配读集；旧版本缓存和保存点语义保持不变。
void CommonStore::changed(ResourceKey::Kind k,const StableId& id){if(auto identity=actor_condition_capture_identity())identity->changed(k,id);auto key=std::make_pair(int(k),id);if(savepoint_){auto it=versions_.find(key);savepoint_->undo.emplace_back(SavepointData::VersionBefore{key,it==versions_.end()?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{it->second}});}++versions_[key];clear_checkpoint_read_cache();}
namespace {
CheckpointMemoryCharge reserve_produced_stable_id(const ProducedStableIdBuildView& value){
    return CheckpointMemoryCharge(0);
}
}
ProducedStableId::ProducedStableId(const ProducedStableIdBuildView& value)
    :copied_fields_(reserve_produced_stable_id(value)),value_(checkpoint_produced_stable_id_size(value),'\0'){
    auto out=std::copy(value.prefix.begin(),value.prefix.end(),value_.begin());
    *out++=':';std::copy(value.decimal.begin(),value.decimal.end(),out);
}
Outcome<ProducedStableId> CommonStore::next_owned_id(std::string_view prefix){
    if(prefix.empty()||state_.next_instance_id<1||state_.next_instance_id==INT64_MAX)return {false,{},common_failure("InstanceIdOverflow").error};
    // The sole original counter and undo sequence. Its older undo-vector
    // allocation is an independent frontier, not paid by this output lease.
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::NextBefore{state_.next_instance_id});
    const auto serial=state_.next_instance_id++;
    char digits[20];const auto converted=std::to_chars(digits,digits+sizeof(digits),serial);
    if(converted.ec!=std::errc{})throw std::logic_error("InstanceIdDecimalOverflow");
    return {true,ProducedStableId{ProducedStableIdBuildView{prefix,std::string_view(digits,static_cast<std::size_t>(converted.ptr-digits))}}, {}};
}
Outcome<StableId> CommonStore::next_id(const StableId& prefix){
    auto produced=next_owned_id(prefix);if(!produced.ok)return {false,{},std::move(produced.error)};
    // Explicit legacy ABI only: its returned bare string cannot carry the
    // lease. Callers remain uncovered after this return until migrated.
    return {true,std::move(produced.value.value_),{}};
}
// 仅固定引擎的有界整数入口已由真实952向量核准；不将int64端点静默窄化为Godot int32。
Outcome<Integer> CommonStore::random_bounded(const StableId& stream,Integer from,Integer to){
    auto* origin=state_.rng.find(stream);if(!origin){auto error=common_failure("UnknownRandomStream",stream,ErrorKind::internal).error;error.domain="rng";error.command="random_bounded";error.source_id=stream;return {false,0,std::move(error)};}
    if(state_.metadata.rng_backend!="godot:4.7.2-stable (official)"){auto error=common_failure("UnsupportedNativeCapability","rng_backend",ErrorKind::unsupported_capability).error;error.domain="rng";error.command="random_bounded";error.source_id=stream;return {false,0,std::move(error)};}
    auto draw=godot_pcg32_range(origin->state,from,to);if(!draw.ok){draw.error.domain="rng";draw.error.command="random_bounded";draw.error.source_id=stream;return {false,0,std::move(draw.error)};}
    if(draw.value.raw_draws){if(savepoint_)savepoint_->undo.emplace_back(SavepointData::RngBefore{stream,*origin});origin->state=draw.value.next_state;}
    return {true,draw.value.value,{}};
}

Outcome<Location> CommonStore::resolved_location(const StableId& id)const{auto bound=field_battle_bound(id);if(!bound.ok)return {false,{},bound.error};if(bound.value)return {false,{},common_failure("FieldBattleLocationRequired",id).error};auto* a=actor(id);if(!a)return {false,{},common_failure("UnknownActor",id).error};const ActorCoreView* leader=nullptr;if(auto* p=std::get_if<FollowingLocation>(&a->location)){auto* party=state_.parties.find(p->party_id);if(!party||std::find(party->members.begin(),party->members.end(),id)==party->members.end())return {false,{},common_failure("DanglingParty",id).error};leader=actor(party->leader_id);}else if(std::holds_alternative<InArmyLocation>(a->location)){if(!membership_)return {false,{},common_failure("UnboundArmyMembership",id,ErrorKind::internal).error};auto* p=membership_->army_leader(id);leader=p?actor(*p):nullptr;}else return {true,a->location,{}};if(!leader||std::holds_alternative<FollowingLocation>(leader->location)||std::holds_alternative<InArmyLocation>(leader->location))return {false,{},common_failure("NestedParty",id).error};return {true,leader->location,{}};}
Status CommonStore::apply_actor(const ActorMutation& m){
    auto* a=state_.actors.find(m.actor_id);if(!a)return common_failure("UnknownActor",m.actor_id);
    if(m.location_mode!=ActorMutation::LocationMode::replace&&m.location_mode!=ActorMutation::LocationMode::wilderness_point&&m.location_mode!=ActorMutation::LocationMode::route_progress&&m.location_mode!=ActorMutation::LocationMode::route_contact)return common_failure("InvalidLocationMutation",m.actor_id,ErrorKind::internal);
    if(m.location_mode==ActorMutation::LocationMode::wilderness_point){auto* before=std::get_if<WildernessLocation>(&a->location);auto* after=m.location?std::get_if<WildernessLocation>(&*m.location):nullptr;if(!before||!after||before->map_id!=after->map_id||before->origin!=after->origin)return common_failure("InvalidLocationMutation",m.actor_id,ErrorKind::internal);}
    if(m.location_mode==ActorMutation::LocationMode::route_progress){auto* before=std::get_if<OnRouteLocation>(&a->location);auto* after=m.location?std::get_if<OnRouteLocation>(&*m.location):nullptr;if(!before||!after||before->edge_id!=after->edge_id||before->origin!=after->origin)return common_failure("InvalidLocationMutation",m.actor_id,ErrorKind::internal);}
    if(m.location_mode==ActorMutation::LocationMode::route_contact&&(!m.location||!std::holds_alternative<OnRouteLocation>(*m.location)))return common_failure("InvalidLocationMutation",m.actor_id,ErrorKind::internal);
    if(m.location)if(const auto* carried=std::get_if<CarriedLocation>(&*m.location)){if(!catalog_||!catalog_->carrier_location_enabled)return common_failure("CarrierLocationProfileRequired",m.actor_id,ErrorKind::unsupported_capability);if(carried->carrier_id.empty()||(carried->carrier_kind!=CarriedLocation::CarrierKind::transport_unit&&carried->carrier_kind!=CarriedLocation::CarrierKind::vessel))return common_failure("InvalidActorCarriedLocation",m.actor_id);}
    // 成长写集沿原共同范围先拒绝坏值，拒绝时不能先写hp、位置或其它人物字段。
    if(m.skills){const auto& q=*m.skills;for(auto n:{q.archery,q.cavalry,q.commerce,q.debate,q.infantry,q.martial,q.negotiation,q.rites,q.tactics,q.transport})if(n<0||n>1200)return common_failure("InvalidSkill",m.actor_id);}
    if(m.combat)for(const auto& learned:m.combat->learned.rows())if(learned.second<0||learned.second>100)return common_failure("InvalidLearnedSkill",m.actor_id);
    auto selection=prepare_actor_selection_write(m,*a);if(!selection.ok)return {false,{},selection.error};
    // 先过原全部字段校验再持前像，坏skills/combat不能留下半写或捕获。
    std::optional<ActorLocationExecutionDraft> location_capture;
    if(actor_location_capture_.active&&m.location){
        if(actor_location_capture_.drafts.size()>=static_cast<std::size_t>(INT64_MAX))return common_failure("ActorLocationCaptureOverflow");
        location_capture=ActorLocationExecutionDraft{ActorLocationExecutionDraft::Kind::mutation,m.actor_id,static_cast<Integer>(actor_location_capture_.drafts.size()+1),a->location,{},checkpoint_make_shared<ActorLocationWriteIdentity>()};
    }
    std::optional<ActorConditionExecutionDraft> captured;
    if(actor_condition_capture_.active&&(m.hp||m.fatigue||m.injury||m.alive||m.available)){
        if(actor_condition_capture_.drafts.size()>=static_cast<std::size_t>(INT64_MAX))return common_failure("ActorConditionCaptureOverflow");
        captured=ActorConditionExecutionDraft{state_.identity,static_cast<Integer>(actor_condition_capture_.drafts.size()+1),ActorConditionExecutionDraft::Kind::mutation,m.actor_id,ActorConditionExecutionValues{a->hp,a->fatigue,a->injury,a->alive,a->available},{},m.hp.has_value(),m.fatigue.has_value(),m.injury.has_value(),m.alive.has_value(),state_.next_instance_id,state_.next_instance_id,m.available.has_value()};
    }
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::ActorBefore{m.actor_id,*a});
    // 原道路半步原位写progress；相遇锚点替换字典时保留原接触构造顺序。
    if(m.location){if(m.location_mode==ActorMutation::LocationMode::wilderness_point)std::get<WildernessLocation>(a->location).point=std::get<WildernessLocation>(*m.location).point;else if(m.location_mode==ActorMutation::LocationMode::route_progress)std::get<OnRouteLocation>(a->location).progress=std::get<OnRouteLocation>(*m.location).progress;else{a->location=*m.location;a->location_order.clear();if(m.location_mode==ActorMutation::LocationMode::route_contact)a->location_order={"kind","edge_id","origin","progress"};}}
    if(m.action_id)a->action_id=*m.action_id;if(m.alive)a->alive=*m.alive;if(m.available)a->available=*m.available;if(m.hp)a->hp=*m.hp;if(m.fatigue)a->fatigue=*m.fatigue;if(m.merit)a->merit=*m.merit;if(m.injury)a->injury=*m.injury;
    if(m.skills)a->skills=*m.skills;
    // 原成长/装备逐字段更新combat；已有键序不重排，仅可选护具键按首次写入追加。
    if(m.combat){
        if(!a->combat_order.empty()){
            if(!a->combat.armor_id&&m.combat->armor_id)a->combat_order.push_back("armor_id");
            else if(a->combat.armor_id&&!m.combat->armor_id)a->combat_order.erase(std::remove(a->combat_order.begin(),a->combat_order.end(),"armor_id"),a->combat_order.end());
        }
        a->combat=*m.combat;
    }
    changed(ResourceKey::Kind::actor,m.actor_id);if(location_capture){location_capture->after=a->location;actor_location_capture_.drafts.push_back(std::move(*location_capture));}if(captured){captured->after={a->hp,a->fatigue,a->injury,a->alive,a->available};actor_condition_capture_.drafts.push_back(std::move(*captured));}if(selection.value)record_actor_selection_write(std::move(*selection.value));return common_success();
}
Status CommonStore::apply_action(const ActionMutation& m){auto* a=state_.actions.find(m.action.id);if(m.kind==ActionMutation::Kind::insert&&a)return common_failure("DuplicateActionId",m.action.id);if(m.kind!=ActionMutation::Kind::insert&&!a)return common_failure("UnknownAction",m.action.id);if(savepoint_){auto position=std::find(state_.actions.order().begin(),state_.actions.order().end(),m.action.id)-state_.actions.order().begin();auto* keys=state_.action_field_order.find(m.action.id);savepoint_->undo.emplace_back(SavepointData::ActionBefore{m.action.id,a?std::optional<ActionCoreView>{*a}:std::optional<ActionCoreView>{},static_cast<std::size_t>(position),keys?std::optional<FieldOrder>{*keys}:std::optional<FieldOrder>{},static_cast<std::size_t>(std::find(state_.action_field_order.order().begin(),state_.action_field_order.order().end(),m.action.id)-state_.action_field_order.order().begin())});}if(m.kind==ActionMutation::Kind::insert)state_.actions.insert(m.action.id,m.action);else if(m.kind==ActionMutation::Kind::erase){state_.actions.erase(m.action.id);state_.action_field_order.erase(m.action.id);}else{if(m.remaining_ticks)a->remaining_ticks=*m.remaining_ticks;if(m.action_kind)a->kind=*m.action_kind;if(m.participants)a->participants=*m.participants;}changed(ResourceKey::Kind::action,m.action.id);return common_success();}

Status CommonStore::append_fact(const FactDraft& f){return append_fact_borrowed(f.id,f.kind,f.domain);}
Status CommonStore::append_fact_borrowed(std::string_view id,std::string_view kind,std::string_view domain){
    if(id.empty()||kind.empty()||domain.empty())return common_failure("InvalidFact");
    // Compare the original stored keys without constructing an owning lookup
    // key. Diagnostic ownership remains its separate original frontier.
    for(const auto& row:state_.facts.rows())if(std::string_view(row.first)==id)return common_failure("DuplicateFactId",row.first);
    // This is the same original append source, not a second header ledger.
    // Every fresh local string is reserved before construction; field_order
    // is actually empty. The existing table owns its independent target copy.

    auto memory=CheckpointMemoryCharge(0);
    FactRecord record{StableId(id),StableId(kind),StableId(domain),{}};
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::FactAdded{record.id});
    invalidate_common_capture_source(*this,id);
    state_.facts.insert(record.id,record);
    return common_success();
}
// 创建先核ID与专属容器，领域再证明原型/创建事实；失败不留下半个人物或请求序号。
Status CommonStore::create_actor(const ActorRecord& value,Integer sequence){
    if(value.id.empty()||sequence<0||sequence>2147483647)return common_failure("InvalidDynamicActor",value.id);
    if(state_.actors.find(value.id)||state_.request_sequences.find(value.id))return common_failure("ActorIdentityCollision",value.id);
    auto* wallet=state_.wallets.find(value.wallet_id);auto* inventory=state_.inventories.find(value.inventory_id);
    const Owner expected{Owner::Kind::actor,value.id,{},{}};
    if(!wallet||!inventory||!owner_equal(wallet->owner,expected)||!owner_equal(inventory->owner,expected))return common_failure("ActorAssetsMissing",value.id);
    // 真实动态创建在原资产/身份门之后记录；原初始种子不借当前人物倒填。
    if(actor_location_capture_.active&&actor_location_capture_.drafts.size()>=static_cast<std::size_t>(INT64_MAX))return common_failure("ActorLocationCaptureOverflow");
    if(actor_condition_capture_.active&&actor_condition_capture_.drafts.size()>=static_cast<std::size_t>(INT64_MAX))return common_failure("ActorConditionCaptureOverflow");
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::ActorCreated{value.id});
    state_.actors.insert(value.id,value);state_.request_sequences.insert(value.id,sequence);changed(ResourceKey::Kind::actor,value.id);if(actor_location_capture_.active)actor_location_capture_.drafts.push_back({ActorLocationExecutionDraft::Kind::insertion,value.id,static_cast<Integer>(actor_location_capture_.drafts.size()+1),std::nullopt,value.location,checkpoint_make_shared<ActorLocationWriteIdentity>()});if(actor_condition_capture_.active)actor_condition_capture_.drafts.push_back({state_.identity,static_cast<Integer>(actor_condition_capture_.drafts.size()+1),ActorConditionExecutionDraft::Kind::creation,value.id,std::nullopt,{value.hp,value.fatigue,value.injury,value.alive,value.available},true,true,true,true,state_.next_instance_id,state_.next_instance_id,true});return common_success();
}
// 玩家切换不自行分配epoch或复制人物；原家庭规则负责继任条件和全域事实。
Status CommonStore::set_player_actor(const StableId& id){
    if(!state_.actors.find(id))return common_failure("InvalidPlayerActor",id);
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::PlayerBefore{state_.metadata.player_actor_id});
    state_.metadata.player_actor_id=id;return common_success();
}
// 收养事实沿原ID原位置转终态，payload仍归家庭域且全局注册必须匹配。
Status CommonStore::replace_fact_header(const FactDraft& value){
    auto* prior=state_.facts.find(value.id);if(!prior)return common_failure("UnknownFact",value.id);
    if(value.kind.empty()||value.domain.empty())return common_failure("InvalidFact",value.id);
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::FactBefore{value.id,*prior});
    invalidate_common_capture_source(*this,value.id);
    prior->kind=value.kind;prior->domain=value.domain;return common_success();
}
// 同行变更保原表位置，并使所有受影响成员与party读版本失效；外层仍统一发布。
Status CommonStore::apply_party(const StableId& id,const std::optional<PartyRecord>& value){
    auto* before=state_.parties.find(id);if(id.empty()||(value&&value->id!=id)||(!value&&!before))return common_failure("InvalidParty",id);
    std::set<StableId> affected;if(before)affected.insert(before->members.begin(),before->members.end());if(value){for(const auto& actor:value->members){if(!this->actor(actor))return common_failure("UnknownActor",actor);affected.insert(actor);}}
    if(savepoint_)savepoint_->undo.emplace_back(SavepointData::PartyBefore{id,before?std::optional<PartyRecord>{*before}:std::nullopt,static_cast<std::size_t>(std::find(state_.parties.order().begin(),state_.parties.order().end(),id)-state_.parties.order().begin())});
    if(value)state_.parties.set(id,*value);else state_.parties.erase(id);changed(ResourceKey::Kind::party,id);for(const auto& actor:affected)changed(ResourceKey::Kind::actor,actor);return common_success();
}
// 观察器只由共同候选提交入口建立，复制世界时不继承，领域不能另建支付账本。
Status CommonStore::begin_wallet_debit_observation(const StableId& id){if(wallet_debit_||savepoint_)return common_failure("RunSpendObservationBusy",{},ErrorKind::internal);if(!wallet(id))return common_failure("UnknownWallet",id);wallet_debit_=WalletDebitObservation{id,0};return common_success();}
Integer CommonStore::observed_wallet_debits()const{return wallet_debit_?wallet_debit_->total:0;}
void CommonStore::clear_wallet_debit_observation(){wallet_debit_.reset();}
// 捕获不生成事实或改变任何业务许可；必须在同一个原提交栈完整消费后才可作永久来源。
Status CommonStore::begin_resource_execution_capture(){if(resource_execution_capture_.active||resource_execution_capture_.in_recipe||savepoint_)return common_failure("ResourceExecutionCaptureBusy",{},ErrorKind::internal);resource_execution_capture_.identity=checkpoint_make_shared<ResourceExecutionCaptureIdentity>();resource_execution_capture_.active=true;resource_execution_capture_.drafts.clear();return common_success();}
const std::vector<ResourceExecutionDraft>& CommonStore::resource_execution_drafts()const{return resource_execution_capture_.drafts;}
// 原生命周期结束、copy/assignment或move均清身份，不能借同地址重启继承旧覆盖。
std::shared_ptr<const ResourceExecutionCaptureIdentity> CommonStore::resource_execution_capture_identity()const{return resource_execution_capture_.active?resource_execution_capture_.identity:nullptr;}
Outcome<std::vector<ResourceExecutionDraft>> CommonStore::finish_resource_execution_capture(){if(!resource_execution_capture_.active||resource_execution_capture_.in_recipe||savepoint_)return {false,{},common_failure("ResourceExecutionCaptureBoundary",{},ErrorKind::internal).error};auto result=std::move(resource_execution_capture_.drafts);resource_execution_capture_.clear();return {true,std::move(result),{}};}
// 同栈显式捕获不发事实/消耗ID/RNG，不担任跨域业务观察器。
Status CommonStore::begin_actor_location_capture(){if(actor_location_capture_.active||savepoint_)return common_failure("ActorLocationCaptureBusy",{},ErrorKind::internal);actor_location_capture_.identity=checkpoint_make_shared<ActorLocationCaptureIdentity>();actor_location_capture_.active=true;actor_location_capture_.drafts.clear();return common_success();}
const std::vector<ActorLocationExecutionDraft>& CommonStore::actor_location_execution_drafts()const{return actor_location_capture_.drafts;}
std::shared_ptr<const ActorLocationCaptureIdentity> CommonStore::actor_location_capture_identity()const{return actor_location_capture_.active?actor_location_capture_.identity:nullptr;}
Outcome<std::vector<ActorLocationExecutionDraft>> CommonStore::finish_actor_location_capture(){if(!actor_location_capture_.active||savepoint_)return {false,{},common_failure("ActorLocationCaptureBoundary",{},ErrorKind::internal).error};auto out=std::move(actor_location_capture_.drafts);actor_location_capture_.clear();return {true,std::move(out),{}};}
Status CommonStore::begin_actor_condition_capture(){if(actor_condition_capture_.active||savepoint_)return common_failure("ActorConditionCaptureBusy",{},ErrorKind::internal);actor_condition_capture_.identity=checkpoint_make_shared<ActorConditionCaptureIdentity>();actor_condition_capture_.active=true;actor_condition_capture_.drafts.clear();return common_success();}
const std::vector<ActorConditionExecutionDraft>& CommonStore::actor_condition_execution_drafts()const{return actor_condition_capture_.drafts;}
std::shared_ptr<const ActorConditionCaptureIdentity> CommonStore::actor_condition_capture_identity()const{return actor_condition_capture_.active?actor_condition_capture_.identity:nullptr;}
Outcome<std::vector<ActorConditionExecutionDraft>> CommonStore::finish_actor_condition_capture(){if(!actor_condition_capture_.active||savepoint_)return {false,{},common_failure("ActorConditionCaptureBoundary",{},ErrorKind::internal).error};auto out=std::move(actor_condition_capture_.drafts);actor_condition_capture_.clear();return {true,std::move(out),{}};}
// 携货服务只属于当前捕获，原目录未选时也不允许借此开启新保护路径。
Status CommonStore::begin_protected_freight_resources(const ProtectedFreightResourceRead& read){
    if(!catalog_||!catalog_->private_shop_freight_protection_enabled)return common_failure("ProtectedFreightProfileRequired");
    if(protected_freight_capture_.read||savepoint_||resource_execution_capture_.in_recipe)return common_failure("ProtectedFreightCaptureBusy");
    protected_freight_capture_.identity=checkpoint_make_shared<ProtectedFreightCaptureIdentity>();protected_freight_capture_.read=&read;return common_success();
}
const std::vector<ProtectedFreightResourceDraft>& CommonStore::protected_freight_resource_drafts()const{return protected_freight_capture_.drafts;}
std::shared_ptr<const ProtectedFreightCaptureIdentity> CommonStore::protected_freight_capture_identity()const{return protected_freight_capture_.identity;}
// 只有外层保存集已结束才能交出临时行；原市场回调或prepare中不能取走捕获。
Outcome<std::vector<ProtectedFreightResourceDraft>> CommonStore::finish_protected_freight_resources(){
    if(!protected_freight_capture_.read||savepoint_||resource_execution_capture_.in_recipe)return {false,{},common_failure("ProtectedFreightCaptureBoundary").error};
    auto rows=std::move(protected_freight_capture_.drafts);protected_freight_capture_.clear();return {true,std::move(rows),{}};
}
// 静态选择随catalog保留；复制候选没有旧服务指针，未重绑时不能静默退回无保护原语。
void CommonStore::bind_protected_market_resources(ProtectedMarketResourceObserver* observer){protected_market_.observer=observer;}
Status CommonStore::apply_resources(const ResourceRecipe& recipe){
    for(auto& v:recipe.expected_versions)if(read_version(v.resource).version!=v.version)return common_failure("StaleResourceRecipe",v.resource.id);
    // 只向Common自身临时草稿写入；观察器的规则拒绝仍发生在任何资源或草稿发布之前。
    if((resource_execution_capture_.active||protected_freight_capture_.read)&&resource_execution_capture_.in_recipe)return common_failure("ResourceExecutionCaptureReentry",{},ErrorKind::internal);
    struct CaptureGuard {bool* active;bool before;~CaptureGuard(){*active=before;}} capture_guard{&resource_execution_capture_.in_recipe,resource_execution_capture_.in_recipe};
    resource_execution_capture_.in_recipe=true;
    const bool protect_freight=catalog_&&catalog_->private_shop_freight_protection_enabled;
    if(protect_freight&&!protected_freight_capture_.read)return common_failure("ProtectedFreightGuardRequired",{},ErrorKind::internal);
    std::optional<ResourceExecutionDraft> execution;
    if(resource_execution_capture_.active||protect_freight){execution.emplace();execution->identity=state_.identity;execution->recipe=recipe;execution->next_instance_before=state_.next_instance_id;}
    const bool protect_markets=catalog_&&!catalog_->protected_market_inventory_ids.empty();
    if(protect_markets){if(!protected_market_.observer)return common_failure("ProtectedMarketGuardRequired",{},ErrorKind::internal);auto inspected=protected_market_.observer->inspect_market_recipe(*this,recipe);if(!inspected.ok)return inspected;}
    OrderedTable<WalletView> wallets;OrderedTable<InventoryView> inventories;Integer next=state_.next_instance_id,observed_debit=0;
    ResourceOperationExecutionDraft* operation_execution=nullptr;
    auto trace_wallet=[&](const StableId& id){if(!operation_execution)return;for(const auto& row:operation_execution->wallets)if(row.wallet_id==id)return;const auto* original=wallets.find(id);if(!original)original=state_.wallets.find(id);ResourceWalletExecutionDraft row;row.wallet_id=id;if(original)row.before=*original;operation_execution->wallets.push_back(std::move(row));};
    auto trace_inventory=[&](const StableId& id){if(!operation_execution)return;for(const auto& row:operation_execution->inventories)if(row.inventory_id==id)return;const auto* original=inventories.find(id);if(!original)original=state_.inventories.find(id);ResourceInventoryExecutionDraft row;row.inventory_id=id;if(original)row.before=*original;operation_execution->inventories.push_back(std::move(row));};
    auto flow=[&](ResourceLotFlowDraft::Kind kind,const StableId& source_inventory,const StableId& source_lot,const StableId& destination_inventory,const StableId& destination_lot,const StableId& item,Integer quantity){if(operation_execution)operation_execution->lot_flows.push_back({kind,source_inventory,source_lot,destination_inventory,destination_lot,item,quantity});};
    auto wallet_stage=[&](const StableId& id)->WalletView*{trace_wallet(id);if(auto* p=wallets.find(id))return p;auto* p=state_.wallets.find(id);if(!p)return nullptr;wallets.insert(id,*p);return wallets.find(id);};
    auto inventory_stage=[&](const StableId& id)->InventoryView*{trace_inventory(id);if(auto* p=inventories.find(id))return p;auto* p=state_.inventories.find(id);if(!p)return nullptr;inventories.insert(id,*p);return inventories.find(id);};
    // 原flow仍进入同一operation草稿；共享叶只同步借用参数，不持有第二份捕获。
    const OriginalResourceFlowOutput original_flow{&operation_execution,[](void* opaque,ResourceLotFlowDraft::Kind kind,const StableId& source_inventory,const StableId& source_lot,const StableId& destination_inventory,const StableId& destination_lot,const StableId& item,Integer quantity){auto* current=*static_cast<ResourceOperationExecutionDraft**>(opaque);if(current)current->lot_flows.push_back({kind,source_inventory,source_lot,destination_inventory,destination_lot,item,quantity});}};
    auto add=[&](InventoryView& inv,const StableId& item,Integer q,const Owner& owner,const ReservationRef& reserved,StableId source_inventory={},StableId source_lot={},ResourceLotFlowDraft::Kind kind=ResourceLotFlowDraft::Kind::add)->Status{return apply_original_plain_lot_add(inv,catalog_.get(),item,q,owner,reserved,next,original_flow,source_inventory,source_lot,kind);};
    std::size_t operation_index=0;
    for(const auto& op:recipe.operations){if(execution){execution->operations.emplace_back();operation_execution=&execution->operations.back();operation_execution->operation_index=operation_index;operation_execution->next_instance_before=next;}++operation_index;Status s=std::visit([&](const auto& value)->Status{using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,CreateWallet>){if(state_.wallets.find(value.wallet.id)||wallets.find(value.wallet.id))return common_failure("DuplicateWalletId",value.wallet.id);trace_wallet(value.wallet.id);wallets.insert(value.wallet.id,value.wallet);return common_success();}
        else if constexpr(std::is_same_v<T,CreateInventory>){if(state_.inventories.find(value.inventory.id)||inventories.find(value.inventory.id))return common_failure("DuplicateInventoryId",value.inventory.id);trace_inventory(value.inventory.id);inventories.insert(value.inventory.id,value.inventory);return common_success();}
        else if constexpr(std::is_same_v<T,SetWalletOwner>){auto* w=wallet_stage(value.wallet_id);if(!w)return common_failure("UnknownWallet",value.wallet_id);w->owner=value.owner;return common_success();}
        else if constexpr(std::is_same_v<T,WalletDelta>){auto* w=wallet_stage(value.wallet_id);if(!w)return common_failure("UnknownWallet",value.wallet_id);Integer balance;if(!add_checked(w->balance,value.delta,balance))return common_failure("MoneyOverflow");if(value.require_spendable&&value.delta<0){Integer reserved=0;for(auto& r:w->reserved)if(!add_checked(reserved,r.amount,reserved))return common_failure("MoneyOverflow");if(balance<reserved)return common_failure("InsufficientFunds");}if(wallet_debit_&&wallet_debit_->wallet_id==value.wallet_id&&value.delta<0){if(value.delta==INT64_MIN||!add_checked(observed_debit,-value.delta,observed_debit))return common_failure("RunMoneyOverflow");}w->balance=balance;return common_success();}
        else if constexpr(std::is_same_v<T,SetReservation>){auto* w=wallet_stage(value.wallet_id);if(!w)return common_failure("UnknownWallet",value.wallet_id);auto i=std::find_if(w->reserved.begin(),w->reserved.end(),[&](const Reservation& r){return r.action_id==value.action_id;});if(value.erase){if(i!=w->reserved.end())w->reserved.erase(i);}else {if(value.amount<0)return common_failure("InvalidReservation");if(i==w->reserved.end())w->reserved.push_back({value.action_id,value.amount});else i->amount=value.amount;}return common_success();}
        else if constexpr(std::is_same_v<T,ReleaseActionResources>||std::is_same_v<T,ReleaseReservationResources>){const ReservationRef reference=[&](){if constexpr(std::is_same_v<T,ReleaseActionResources>)return ReservationRef{value.action_id};else return value.reservation;}();for(auto& row:state_.wallets.rows())if(std::any_of(row.second.reserved.begin(),row.second.reserved.end(),[&](const Reservation& r){return r.action_id==reference;}))wallet_stage(row.first);for(auto& row:state_.inventories.rows())if(std::any_of(row.second.lots.begin(),row.second.lots.end(),[&](const LotView& l){return l.reserved_by==reference;}))inventory_stage(row.first);for(auto& row:wallets.rows()){if(std::any_of(row.second.reserved.begin(),row.second.reserved.end(),[&](const Reservation& r){return r.action_id==reference;}))trace_wallet(row.first);auto& rs=row.second.reserved;rs.erase(std::remove_if(rs.begin(),rs.end(),[&](const Reservation& r){return r.action_id==reference;}),rs.end());}for(auto& row:inventories.rows()){if(std::any_of(row.second.lots.begin(),row.second.lots.end(),[&](const LotView& l){return l.reserved_by==reference;}))trace_inventory(row.first);bool changed=false;for(auto& l:row.second.lots)if(l.reserved_by==reference){l.reserved_by.clear();changed=true;}if(changed)++row.second.revision;}return common_success();}
        // 整编先核整批身份、产权、无预留和承载；失败只丢弃recipe暂存，不能拆批或分配新ID。
        else if constexpr(std::is_same_v<T,MoveIntegrationLot>){
            if(value.source_id==value.destination_id)return common_failure("SameContainer");
            if(value.expected_quantity<=0)return common_failure("InvalidQuantity",value.lot_id);
            auto public_owner=[&](const Owner& owner){return !owner.extra&&owner.kind==Owner::Kind::organization&&!owner.id.empty()&&catalog_&&(state_.metadata.ruleset_id==dynamic_ruleset_id?organization_owners_&&organization_owners_->organization_exists(owner.id):std::find(catalog_->organization_ids.begin(),catalog_->organization_ids.end(),owner.id)!=catalog_->organization_ids.end());};
            if(!public_owner(value.expected_owner)||!public_owner(value.new_owner))return common_failure("InvalidIntegrationOwner",value.lot_id);
            if(!inventory_stage(value.source_id)||!inventory_stage(value.destination_id))return common_failure("UnknownInventory");
            auto* source=inventories.find(value.source_id);auto* destination=inventories.find(value.destination_id);
            if(!owner_equal(source->owner,value.expected_owner)||!owner_equal(destination->owner,value.new_owner))return common_failure("IntegrationContainerOwnership",value.lot_id);
            auto lot=std::find_if(source->lots.begin(),source->lots.end(),[&](const LotView& row){return row.id==value.lot_id;});
            if(lot==source->lots.end())return common_failure("UnknownLot",value.lot_id);
            if(lot->quantity!=value.expected_quantity||!owner_equal(lot->owner,value.expected_owner))return common_failure("StaleIntegrationLot",value.lot_id);
            if(!lot->reserved_by.empty()||lot->reserved_by.kind!=ReservationRef::Kind::action)return common_failure("ReservedIntegrationLot",value.lot_id);
            if(std::any_of(destination->lots.begin(),destination->lots.end(),[&](const LotView& row){return row.id==value.lot_id;}))return common_failure("DuplicateLotId",value.lot_id);
            if(source->revision==INT64_MAX||destination->revision==INT64_MAX)return common_failure("ProjectionOverflow",value.lot_id);
            Integer weight=0;
            auto add_weight=[&](const LotView& row)->Status{auto* item=catalog_->items.find(row.item_id);if(!item)return common_failure("UnknownItem",row.item_id);if(row.quantity<0||item->weight<0||(item->weight&&row.quantity>INT64_MAX/item->weight)||!add_checked(weight,row.quantity*item->weight,weight))return common_failure("ProjectionOverflow",row.id);return common_success();};
            for(const auto& row:destination->lots){auto checked=add_weight(row);if(!checked.ok)return checked;}
            auto checked=add_weight(*lot);if(!checked.ok)return checked;
            if(weight>destination->capacity)return common_failure("OverCapacity",destination->id);
            LotView moved=*lot;moved.owner.kind=value.new_owner.kind;moved.owner.id=value.new_owner.id;
            flow(ResourceLotFlowDraft::Kind::integration_move,source->id,lot->id,destination->id,lot->id,lot->item_id,lot->quantity);destination->lots.push_back(std::move(moved));source->lots.erase(lot);++source->revision;++destination->revision;
            return common_success();
        }
        // 精确选择只执行给定ID，不替换缺货批；所有变化仍位于本recipe暂存。
        else if constexpr(std::is_same_v<T,ReserveSelectedLots>||std::is_same_v<T,TransferSelectedLots>||std::is_same_v<T,ConsumeSelectedLots>){
            constexpr bool reserve=std::is_same_v<T,ReserveSelectedLots>,transfer=std::is_same_v<T,TransferSelectedLots>;
            if(!catalog_||!catalog_->selected_resource_operations_enabled)return common_failure("SelectedResourceCapabilityRequired",{},ErrorKind::unsupported_capability);
            const auto& source_id=[&]()->const StableId&{if constexpr(transfer)return value.source_id;else return value.inventory_id;}();
            const auto& selections=value.selections;
            auto guarded=[&](const StableId& id){return std::any_of(recipe.expected_versions.begin(),recipe.expected_versions.end(),[&](const ReadVersion& v){return v.resource.kind==ResourceKey::Kind::inventory&&v.resource.id==id;});};
            if(!guarded(source_id))return common_failure("SelectedResourceReadVersionRequired",source_id);
            if constexpr(transfer){if(source_id==value.destination_id)return common_failure("SameContainer");if(!guarded(value.destination_id))return common_failure("SelectedResourceReadVersionRequired",value.destination_id);if(value.new_owner&&!valid_resource_owner(*value.new_owner))return common_failure("InvalidOwner",value.new_owner->id);}
            if(selections.empty()||value.owners.empty())return common_failure("EmptyCargoSelection",source_id);
            for(std::size_t n=0;n<value.owners.size();++n){if(!valid_resource_owner(value.owners[n]))return common_failure("InvalidOwner",value.owners[n].id);for(std::size_t j=0;j<n;++j)if(owner_equal(value.owners[n],value.owners[j]))return common_failure("DuplicateSelectionOwner",value.owners[n].id);}
            if constexpr(reserve){if(value.reservation.empty()||!reservation_exists(value.reservation))return common_failure("ReservationOwnerMissing",value.reservation.id);}
            else if((value.reservation.empty()&&value.reservation.kind!=ReservationRef::Kind::action)||(!value.reservation.empty()&&!reservation_exists(value.reservation)))return common_failure("ReservationOwnerMissing",value.reservation.id);
            if(!inventory_stage(source_id))return common_failure("UnknownInventory",source_id);
            if constexpr(transfer){if(!inventory_stage(value.destination_id))return common_failure("UnknownInventory",value.destination_id);}
            // stage插入可使OrderedTable底层地址移动，全部容器取址须在插入结束之后。
            auto* source=inventories.find(source_id);InventoryView* destination=nullptr;
            if constexpr(transfer)destination=inventories.find(value.destination_id);
            std::set<StableId> selected_ids;Integer splits=0;std::vector<LotView> selected;
            selected.reserve(selections.size());
            auto occurrences=[&](const StableId& id){Integer count=0;for(const auto& row:state_.inventories.rows()){const auto* current=inventories.find(row.first);if(!current)current=&row.second;for(const auto& lot:current->lots)if(lot.id==id)++count;}for(const auto& row:inventories.rows())if(!state_.inventories.find(row.first))for(const auto& lot:row.second.lots)if(lot.id==id)++count;return count;};
            for(const auto& selection:selections){
                if(selection.inventory_id!=source_id||selection.lot_id.empty()||selection.quantity<=0||!selected_ids.insert(selection.lot_id).second)return common_failure("InvalidCargoSelection",selection.lot_id);
                const auto at=std::find_if(source->lots.begin(),source->lots.end(),[&](const LotView& lot){return lot.id==selection.lot_id;});
                if(at==source->lots.end())return common_failure("UnknownLot",selection.lot_id);
                const auto& lot=*at;if(lot.quantity<selection.quantity||!matching_owner(lot.owner,value.owners)||!valid_resource_owner(lot.owner)||occurrences(lot.id)!=1)return common_failure("StaleSelectedLot",lot.id);
                const ReservationRef expected=reserve?ReservationRef{}:value.reservation;
                if(lot.reserved_by!=expected)return common_failure("SelectedLotReservationMismatch",lot.id);
                if(!catalog_->items.find(lot.item_id))return common_failure("UnknownItem",lot.item_id);
                if(lot.extra||lot.owner.extra||(lot.item_instance_id&&lot.lot_properties_ref))return common_failure("UnsupportedSelectedLotMetadata",lot.id,ErrorKind::unsupported_capability);
                if((lot.item_instance_id&&!catalog_->item_instances_enabled)||(lot.lot_properties_ref&&!catalog_->lot_properties_enabled))return common_failure("LotReferenceCapabilityRequired",lot.id,ErrorKind::unsupported_capability);
                if(lot.item_instance_id&&(lot.quantity!=1||selection.quantity!=1))return common_failure("UniqueInstanceSplit",lot.id);
                if(lot.item_instance_id||lot.lot_properties_ref){if(!selected_lot_semantics_)return common_failure("LotSemanticReaderRequired",lot.id,ErrorKind::unsupported_capability);auto checked=selected_lot_semantics_->validate_selected_use(*this,source_id,lot,selection.quantity,reserve?SelectedLotUse::reserve:(transfer?SelectedLotUse::transfer:SelectedLotUse::consume));if(!checked.ok)return checked;}
                if(selection.quantity<lot.quantity&& (reserve||transfer))++splits;
                selected.push_back(lot);
            }
            if constexpr(!reserve&&!transfer)return apply_original_selected_consume_mutation(*source,selected,selections,next,original_flow);
            else {
            if(selections.size()>static_cast<std::size_t>(INT64_MAX)||splits>INT64_MAX-1||source->revision<0||source->revision>INT64_MAX-(reserve?1+splits:1)||next<1||next>INT64_MAX-splits)return common_failure("ProjectionOverflow",source_id);
            if constexpr(transfer){
                const auto count=static_cast<Integer>(selections.size());if(destination->revision<0||destination->revision>INT64_MAX-count)return common_failure("ProjectionOverflow",destination->id);
                Integer weight=0;auto weigh=[&](const LotView& lot,Integer quantity)->Status{const auto* item=catalog_->items.find(lot.item_id);if(!item||item->weight<0||quantity<0||(item->weight&&quantity>INT64_MAX/item->weight)||!add_checked(weight,quantity*item->weight,weight))return common_failure("ProjectionOverflow",lot.id);return common_success();};
                for(const auto& lot:destination->lots){auto checked=weigh(lot,lot.quantity);if(!checked.ok)return checked;}
                for(std::size_t n=0;n<selected.size();++n){if(std::any_of(destination->lots.begin(),destination->lots.end(),[&](const LotView& lot){return lot.id==selected[n].id;}))return common_failure("DuplicateLotId",selected[n].id);auto checked=weigh(selected[n],selections[n].quantity);if(!checked.ok)return checked;}
                if(weight>destination->capacity)return common_failure("OverCapacity",destination->id);
            }
            for(std::size_t n=0;n<selected.size();++n){
                const auto& before=selected[n];const auto quantity=selections[n].quantity;const bool whole=quantity==before.quantity;
                auto at=std::find_if(source->lots.begin(),source->lots.end(),[&](const LotView& lot){return lot.id==before.id;});
                if constexpr(reserve){
                    if(whole){at->reserved_by=value.reservation;flow(ResourceLotFlowDraft::Kind::selected_reserve_whole,source_id,before.id,source_id,before.id,before.item_id,quantity);}
                    else {LotView reserved=before;reserved.id="lot:"+std::to_string(next++);if(occurrences(reserved.id))return common_failure("DuplicateLotId",reserved.id);reserved.quantity=quantity;reserved.reserved_by=value.reservation;at->quantity-=quantity;flow(ResourceLotFlowDraft::Kind::selected_reserve_split,source_id,before.id,source_id,reserved.id,before.item_id,quantity);source->lots.push_back(std::move(reserved));}
                }else if constexpr(transfer){
                    LotView moved=before;moved.quantity=quantity;moved.reserved_by.clear();if(value.new_owner)moved.owner=*value.new_owner;
                    if(whole)source->lots.erase(at);else{moved.id="lot:"+std::to_string(next++);if(occurrences(moved.id))return common_failure("DuplicateLotId",moved.id);at->quantity-=quantity;}
                    flow(whole?ResourceLotFlowDraft::Kind::selected_transfer_whole:ResourceLotFlowDraft::Kind::selected_transfer_split,source_id,before.id,destination->id,moved.id,before.item_id,quantity);destination->lots.push_back(std::move(moved));
                }
            }
            source->revision+=reserve?1+splits:1;if constexpr(transfer)destination->revision+=static_cast<Integer>(selected.size());return common_success();
            }
        }
        else if constexpr(std::is_same_v<T,TransferLots>){if(value.source_id==value.destination_id)return common_failure("SameContainer");if(value.quantity<0)return common_failure("InvalidQuantity");if(!inventory_stage(value.source_id)||!inventory_stage(value.destination_id))return common_failure("UnknownInventory");auto* source=inventories.find(value.source_id);auto* destination=inventories.find(value.destination_id);auto available=count_lots(*source,value.item_id,value.owners,value.reservation);if(!available.ok)return {false,{},available.error};if(available.value<value.quantity)return common_failure("InsufficientGoods");Integer remaining=value.quantity;for(auto& owner:value.owners){for(auto& lot:source->lots){if(lot.item_id!=value.item_id||!owner_equal(lot.owner,owner)||lot.reserved_by!=value.reservation)continue;Integer used=std::min(remaining,lot.quantity);if(used&&(lot.item_instance_id||lot.lot_properties_ref))return common_failure("SelectedResourceOperationRequired",lot.id,ErrorKind::unsupported_capability);lot.quantity-=used;auto a=add(*destination,value.item_id,used,value.new_owner?*value.new_owner:owner,{},source->id,lot.id,ResourceLotFlowDraft::Kind::transfer);if(!a.ok)return a;remaining-=used;if(!remaining)break;}if(!remaining)break;}source->lots.erase(std::remove_if(source->lots.begin(),source->lots.end(),[](const LotView& l){return l.quantity<=0;}),source->lots.end());++source->revision;return common_success();}
        else {auto* inv=inventory_stage(value.inventory_id);if(!inv)return common_failure("UnknownInventory",value.inventory_id);
            if constexpr(std::is_same_v<T,AddLot>){
                if(value.item_instance_id||value.lot_properties_ref){
                    if(value.quantity<=0||value.item_instance_id&&value.lot_properties_ref)return common_failure("LotReferenceConflict",value.item_id);
                    if(!catalog_||(value.item_instance_id&&!catalog_->item_instances_enabled)||(value.lot_properties_ref&&!catalog_->lot_properties_enabled)||!selected_lot_semantics_)return common_failure("LotReferenceCapabilityRequired",value.item_id,ErrorKind::unsupported_capability);
                    LotView proposed;proposed.item_id=value.item_id;proposed.quantity=value.quantity;proposed.owner=value.owner;proposed.reserved_by=value.reservation;proposed.item_instance_id=value.item_instance_id;proposed.lot_properties_ref=value.lot_properties_ref;
                    auto checked=selected_lot_semantics_->validate_materialized_lot(*this,inv->id,proposed);if(!checked.ok)return checked;
                    Integer weight=0;for(const auto& lot:inv->lots){const auto* item=catalog_->items.find(lot.item_id);if(!item||item->weight<0||lot.quantity<0||(item->weight&&lot.quantity>INT64_MAX/item->weight)||!add_checked(weight,lot.quantity*item->weight,weight))return common_failure("ProjectionOverflow",inv->id);}
                    const auto* item=catalog_->items.find(value.item_id);if(!item||item->weight<0||(item->weight&&value.quantity>INT64_MAX/item->weight)||!add_checked(weight,value.quantity*item->weight,weight))return common_failure("ProjectionOverflow",inv->id);if(weight>inv->capacity)return common_failure("OverCapacity",inv->id);
                    checked=add(*inv,value.item_id,value.quantity,value.owner,value.reservation);if(!checked.ok)return checked;inv->lots.back().item_instance_id=value.item_instance_id;inv->lots.back().lot_properties_ref=value.lot_properties_ref;return common_success();
                }
                return add(*inv,value.item_id,value.quantity,value.owner,value.reservation);
            }
            else if constexpr(std::is_same_v<T,SetInventoryOwner>){inv->owner=value.owner;return common_success();}
            else if constexpr(std::is_same_v<T,SetLotOwner>){auto lot=std::find_if(inv->lots.begin(),inv->lots.end(),[&](const LotView& row){return row.id==value.lot_id;});if(lot==inv->lots.end())return common_failure("UnknownLot",value.lot_id);if(value.bump_revision&&inv->revision==INT64_MAX)return common_failure("ProjectionOverflow",value.inventory_id);lot->owner=value.owner;if(value.bump_revision)++inv->revision;return common_success();}
            else if constexpr(std::is_same_v<T,AdjustLots>){for(auto& delta:value.lots){auto i=std::find_if(inv->lots.begin(),inv->lots.end(),[&](const LotView& l){return l.id==delta.lot_id;});if(i==inv->lots.end())return common_failure("UnknownLot",delta.lot_id);if(delta.delta&&(i->item_instance_id||i->lot_properties_ref))return common_failure("SelectedResourceOperationRequired",i->id,ErrorKind::unsupported_capability);Integer q;if(!add_checked(i->quantity,delta.delta,q)||q<0)return common_failure("InvalidQuantity",delta.lot_id);flow(ResourceLotFlowDraft::Kind::adjust,inv->id,i->id,inv->id,i->id,i->item_id,delta.delta);i->quantity=q;}inv->lots.erase(std::remove_if(inv->lots.begin(),inv->lots.end(),[](const LotView& l){return l.quantity==0;}),inv->lots.end());if(value.bump_revision)++inv->revision;return common_success();}
            else if constexpr(std::is_same_v<T,SetInventoryCapacity>){inv->capacity=value.capacity;return common_success();}
            else if constexpr(std::is_same_v<T,ReserveLots>){if(value.quantity<0)return common_failure("InvalidQuantity");auto available=count_lots(*inv,value.item_id,value.owners,{});if(!available.ok)return {false,{},available.error};if(available.value<value.quantity)return common_failure("InsufficientGoods");Integer remaining=value.quantity;for(auto& owner:value.owners){const auto size=inv->lots.size();for(std::size_t i=0;i<size;++i){auto& lot=inv->lots[i];if(lot.item_id!=value.item_id||!owner_equal(lot.owner,owner)||!lot.reserved_by.empty())continue;Integer used=std::min(remaining,lot.quantity);if(used&&(lot.item_instance_id||lot.lot_properties_ref))return common_failure("SelectedResourceOperationRequired",lot.id,ErrorKind::unsupported_capability);if(used==lot.quantity){flow(ResourceLotFlowDraft::Kind::reserve_whole,inv->id,lot.id,inv->id,lot.id,lot.item_id,used);lot.reserved_by=value.action_id;}else {lot.quantity-=used;auto a=add(*inv,value.item_id,used,owner,value.action_id,inv->id,lot.id,ResourceLotFlowDraft::Kind::reserve_split);if(!a.ok)return a;}remaining-=used;if(!remaining)break;}if(!remaining)break;}++inv->revision;return common_success();}
            else {if(value.quantity<0)return common_failure("InvalidQuantity");auto available=count_lots(*inv,value.item_id,value.owners,value.reservation);if(!available.ok)return {false,{},available.error};if(available.value<value.quantity)return common_failure("InsufficientGoods");if(value.quantity==0)return common_success();Integer remaining=value.quantity;for(auto& lot:inv->lots){if(lot.item_id!=value.item_id||!matching_owner(lot.owner,value.owners)||lot.reserved_by!=value.reservation)continue;Integer used=std::min(remaining,lot.quantity);if(used&&(lot.item_instance_id||lot.lot_properties_ref))return common_failure("SelectedResourceOperationRequired",lot.id,ErrorKind::unsupported_capability);if(used)flow(ResourceLotFlowDraft::Kind::consume,inv->id,lot.id,{},{},lot.item_id,used);lot.quantity-=used;remaining-=used;if(!remaining)break;}inv->lots.erase(std::remove_if(inv->lots.begin(),inv->lots.end(),[](const LotView& l){return l.quantity<=0;}),inv->lots.end());++inv->revision;return common_success();}
        }
    },op);if(!s.ok)return s;if(operation_execution){for(auto& row:operation_execution->wallets){const auto* after=wallets.find(row.wallet_id);if(!after)return common_failure("ResourceExecutionWalletMissing",row.wallet_id,ErrorKind::internal);row.after=*after;}for(auto& row:operation_execution->inventories){const auto* after=inventories.find(row.inventory_id);if(!after)return common_failure("ResourceExecutionInventoryMissing",row.inventory_id,ErrorKind::internal);row.after=*after;}operation_execution->next_instance_after=next;}}
    Integer next_observed=0;if(wallet_debit_&&!add_checked(wallet_debit_->total,observed_debit,next_observed))return common_failure("RunMoneyOverflow");
    // 先纯准备携货结果；在原市场接受之前完成新行的分配，随后不再调用可拒绝的携货代码。
    std::optional<ProtectedFreightResourceDraft> freight;
    if(protect_freight){
        execution->next_instance_after=next;
        auto prepared=protected_freight_capture_.read->prepare_freight_recipe(*this,*execution);
        if(!prepared.ok)return {false,{},std::move(prepared.error)};
        if(!prepared.value)return common_failure("ProtectedFreightPreparedMissing",{},ErrorKind::internal);
        freight.emplace();freight->capture=protected_freight_capture_.identity;
        freight->identity=checkpoint_make_shared<ProtectedFreightRecipeIdentity>();
        freight->recipe_index=protected_freight_capture_.drafts.size();
        freight->execution=*execution;freight->prepared=std::move(prepared.value);
        protected_freight_capture_.drafts.reserve(protected_freight_capture_.drafts.size()+1);
        if(resource_execution_capture_.active)resource_execution_capture_.drafts.reserve(resource_execution_capture_.drafts.size()+1);
    }
    // 此点之后没有可返回的规则拒绝：观察失败时wallet/inventory/ID及毛支出都尚未发布。
    if(protect_markets){auto captured=protected_market_.observer->capture_market_recipe(*this,recipe,inventories,next);if(!captured.ok)return captured;}
    for(auto& row:wallets.rows()){if(savepoint_){auto* old=state_.wallets.find(row.first);savepoint_->undo.emplace_back(SavepointData::WalletBefore{row.first,old?std::optional<WalletView>{*old}:std::optional<WalletView>{}});}state_.wallets.set(row.first,std::move(row.second));changed(ResourceKey::Kind::wallet,row.first);}for(auto& row:inventories.rows()){if(savepoint_){auto* old=state_.inventories.find(row.first);savepoint_->undo.emplace_back(SavepointData::InventoryBefore{row.first,old?std::optional<InventoryView>{*old}:std::optional<InventoryView>{}});}state_.inventories.set(row.first,std::move(row.second));changed(ResourceKey::Kind::inventory,row.first);}if(savepoint_&&state_.next_instance_id!=next)savepoint_->undo.emplace_back(SavepointData::NextBefore{state_.next_instance_id});state_.next_instance_id=next;if(wallet_debit_)wallet_debit_->total=next_observed;if(execution&&resource_execution_capture_.active){execution->next_instance_after=next;resource_execution_capture_.drafts.push_back(std::move(*execution));}if(freight)protected_freight_capture_.drafts.push_back(std::move(*freight));return common_success();
}
// savepoint不复制共同表；只有后续实际mutation在上述入口加入逆操作。
Outcome<Integer> CommonStore::begin_savepoint(){if(savepoint_)return {false,0,common_failure("NestedCommonSavepoint",{},ErrorKind::internal).error};if(savepoint_sequence_==INT64_MAX)return {false,0,common_failure("CommonSavepointOverflow",{},ErrorKind::internal).error};auto data=std::make_shared<SavepointData>();data->token=++savepoint_sequence_;data->debit_before=wallet_debit_;if(protected_freight_capture_.read)data->freight_drafts_before=protected_freight_capture_.drafts.size();if(resource_execution_capture_.active)data->resource_drafts_before=resource_execution_capture_.drafts.size();if(actor_location_capture_.active)data->actor_location_drafts_before=actor_location_capture_.drafts.size();if(actor_condition_capture_.active)data->actor_condition_drafts_before=actor_condition_capture_.drafts.size();if(actor_selection_capture_.active){data->actor_selection_drafts_before=actor_selection_capture_.drafts.size();for(const auto& row:actor_selection_capture_.drafts)data->actor_selection_completed_before.push_back(row.scope_completed);}savepoint_=std::move(data);return {true,savepoint_->token,{}};}
// 保存点序只由实际begin推进，token存在性区分同一保存点结束，均不进入持久字节。
Outcome<ActorConditionSavepointBoundary> CommonStore::actor_condition_savepoint_boundary() const {
    auto capture=actor_condition_capture_identity();if(!capture)return {false,{},common_failure("ActorConditionCaptureRequired").error};
    ActorConditionSavepointBoundary read;read.common_=this;read.capture_=std::move(capture);read.issued_sequence_=savepoint_sequence_;
    if(savepoint_)read.active_token_=savepoint_->token;
    return {true,std::move(read),{}};
}
// 不把无写保存点或全值相等回滚当成同一连续读取区间；调用者另核真正人物/业务来源。
Status CommonStore::validate_actor_condition_savepoint_boundary(const ActorConditionSavepointBoundary& read) const {
    const auto active=savepoint_?std::optional<Integer>{savepoint_->token}:std::nullopt;
    if(read.common_!=this||!read.capture_||read.capture_!=actor_condition_capture_identity()||read.issued_sequence_!=savepoint_sequence_||read.active_token_!=active)
        return common_failure("ActorConditionSavepointBoundaryExpired");
    return common_success();
}
Status CommonStore::commit_savepoint(Integer token){if(!savepoint_||savepoint_->token!=token)return common_failure("StaleCommonSavepoint",{},ErrorKind::internal);savepoint_.reset();return common_success();}
// 逆向恢复前也通知读集；保存点内刚建立的证明不能因数值被还原而复活。
Status CommonStore::rollback_savepoint(Integer token){if(!savepoint_||savepoint_->token!=token)return common_failure("StaleCommonSavepoint",{},ErrorKind::internal);if(savepoint_->resource_drafts_before&&(!resource_execution_capture_.active||resource_execution_capture_.drafts.size()<*savepoint_->resource_drafts_before))return common_failure("ResourceExecutionUndoBoundary",{},ErrorKind::internal);if(savepoint_->actor_location_drafts_before&&(!actor_location_capture_.active||actor_location_capture_.drafts.size()<*savepoint_->actor_location_drafts_before))return common_failure("ActorLocationUndoBoundary",{},ErrorKind::internal);if(savepoint_->actor_condition_drafts_before&&(!actor_condition_capture_.active||actor_condition_capture_.drafts.size()<*savepoint_->actor_condition_drafts_before))return common_failure("ActorConditionUndoBoundary",{},ErrorKind::internal);if(savepoint_->actor_selection_drafts_before&&(!actor_selection_capture_.active||actor_selection_capture_.drafts.size()<*savepoint_->actor_selection_drafts_before||savepoint_->actor_selection_completed_before.size()!=*savepoint_->actor_selection_drafts_before))return common_failure("ActorSelectionUndoBoundary",{},ErrorKind::internal);if(savepoint_->freight_drafts_before&&(!protected_freight_capture_.read||protected_freight_capture_.drafts.size()<*savepoint_->freight_drafts_before))return common_failure("ProtectedFreightUndoBoundary",{},ErrorKind::internal);auto saved=std::move(savepoint_);for(auto it=saved->undo.rbegin();it!=saved->undo.rend();++it){auto status=std::visit([&](auto& u)->Status{using T=std::decay_t<decltype(u)>;if constexpr(std::is_same_v<T,SavepointData::ActorBefore>)state_.actors.set(u.id,std::move(u.value));else if constexpr(std::is_same_v<T,SavepointData::ActorCreated>){state_.actors.erase(u.id);state_.request_sequences.erase(u.id);}else if constexpr(std::is_same_v<T,SavepointData::PlayerBefore>)state_.metadata.player_actor_id=u.id;else if constexpr(std::is_same_v<T,SavepointData::FactBefore>){invalidate_common_capture_source(*this,u.id);state_.facts.set(u.id,std::move(u.value));}else if constexpr(std::is_same_v<T,SavepointData::PartyBefore>){state_.parties.erase(u.id);if(u.value&&!state_.parties.insert_at(u.position,u.id,std::move(*u.value)))return common_failure("InvalidCommonUndoPosition",u.id,ErrorKind::internal);}else if constexpr(std::is_same_v<T,SavepointData::ActionBefore>){state_.actions.erase(u.id);if(u.value&&!state_.actions.insert_at(u.position,u.id,std::move(*u.value)))return common_failure("InvalidCommonUndoPosition",u.id,ErrorKind::internal);state_.action_field_order.erase(u.id);if(u.order&&!state_.action_field_order.insert_at(u.order_position,u.id,std::move(*u.order)))return common_failure("InvalidCommonUndoOrder",u.id,ErrorKind::internal);}else if constexpr(std::is_same_v<T,SavepointData::WalletBefore>){if(u.value)state_.wallets.set(u.id,std::move(*u.value));else state_.wallets.erase(u.id);}else if constexpr(std::is_same_v<T,SavepointData::InventoryBefore>){if(u.value)state_.inventories.set(u.id,std::move(*u.value));else state_.inventories.erase(u.id);}else if constexpr(std::is_same_v<T,SavepointData::FactAdded>){invalidate_common_capture_source(*this,u.id);state_.facts.erase(u.id);}else if constexpr(std::is_same_v<T,SavepointData::RoleBefore>)state_.roles.set(u.id,std::move(u.value));else if constexpr(std::is_same_v<T,SavepointData::VersionBefore>){if(auto identity=actor_condition_capture_identity())identity->changed(static_cast<ResourceKey::Kind>(u.key.first),u.key.second);if(u.value)versions_[u.key]=*u.value;else versions_.erase(u.key);}else if constexpr(std::is_same_v<T,SavepointData::RngBefore>)state_.rng.set(u.id,std::move(u.value));else state_.next_instance_id=u.value;return common_success();},*it);if(!status.ok)return status;}wallet_debit_=std::move(saved->debit_before);if(saved->freight_drafts_before)protected_freight_capture_.drafts.resize(*saved->freight_drafts_before);if(saved->resource_drafts_before)resource_execution_capture_.drafts.resize(*saved->resource_drafts_before);if(saved->actor_location_drafts_before)actor_location_capture_.drafts.resize(*saved->actor_location_drafts_before);if(saved->actor_condition_drafts_before)actor_condition_capture_.drafts.resize(*saved->actor_condition_drafts_before);if(saved->actor_selection_drafts_before){actor_selection_capture_.drafts.resize(*saved->actor_selection_drafts_before);for(std::size_t n=0;n<*saved->actor_selection_drafts_before;++n)actor_selection_capture_.drafts[n].scope_completed=saved->actor_selection_completed_before[n];}actor_selection_capture_.scope.reset();clear_checkpoint_read_cache();return common_success();}
Status CommonStore::apply_role(const StableId& id,const RoleRecord& value){auto* role=state_.roles.find(id);if(!role)return common_failure("UnknownRole",id);if(savepoint_)savepoint_->undo.emplace_back(SavepointData::RoleBefore{id,*role});const auto previous_actor_id=role->actor_id;*role=value;if(previous_actor_id!=value.actor_id)changed(ResourceKey::Kind::actor,previous_actor_id);changed(ResourceKey::Kind::actor,value.actor_id);return common_success();}

} // 本地recipe失败不改变候选；大事务失败由中央丢弃整个WorldBundle。
