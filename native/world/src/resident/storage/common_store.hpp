// 对唯一候选共同状态提供冻结CommonRead/Write接口；不拥有任何跨域状态副本。
#pragma once
#include "../model/common_operations.hpp"
#include "protected_market_resources.hpp"
#include "protected_freight_resources.hpp"
#include "resource_execution_draft.hpp"
#include "selected_lot_semantics.hpp"
#include "resident/kernel/action_time_read.hpp"
#include "carried_location_read.hpp"
#include "actor_condition_execution_draft.hpp"
#include "actor_location_execution_draft.hpp"
#include "actor_condition_savepoint_boundary.hpp"
#include "actor_selection_execution.hpp"
namespace hanmo::resident {
class CheckpointRetentionCensus;
// Same original weight rule for an immutable captured frame; never current inventory.
Outcome<Integer> read_inventory_weight(const InventoryView&,const CommonCatalog&);
// The same original balance/reservation rule over a captured immutable frame.
Outcome<Integer> read_wallet_spendable(const WalletView&);
// Borrow the existing Common lot-count algorithm through the read interface.
// An empty reservation selects unreserved lots; every nonempty ref is matched
// exactly. Owners and reservation eligibility remain the caller's actual scope.
Outcome<Integer> count_inventory_items(const CommonRead&,const StableId& inventory_id,
 const StableId& item_id,const std::vector<Owner>& owners,const ReservationRef& reservation={});
// Complete-owner projection for original escrow coverage. An empty owners list
// in the scoped API above still matches no owners; it never means wildcard.
Outcome<Integer> count_inventory_items_all_owners(const CommonRead&,const StableId& inventory_id,
 const StableId& item_id,const ReservationRef& reservation={});
// Original supply observations count physical stock, including every held
// lot of the specified actual owner. This is not spendable availability and
// cannot grant use of a reservation. It shares the same checked count engine.
Outcome<Integer> count_inventory_physical_items(const CommonRead&,const StableId& inventory_id,
 const StableId& item_id,const Owner& actual_owner);
// Actual captured inventory uses the same checked scoped count body.
Outcome<Integer> count_inventory_frame_items(const InventoryView&,const StableId& item_id,
 const std::vector<Owner>& owners,const ReservationRef& reservation);
// 动态owner和长期预留由同候选领域确认，未绑定时不得放行。
class OrganizationOwnerRead { public:virtual ~OrganizationOwnerRead()=default;virtual bool organization_exists(const StableId&) const=0; };
class HouseholdOwnerRead { public:virtual ~HouseholdOwnerRead()=default;virtual bool household_exists(const StableId&) const=0; };
class ReservationOwnerRead { public:virtual ~ReservationOwnerRead()=default;virtual bool reservation_exists(const ReservationRef&) const=0; };
// 仅实现叶创建的同栈捕获身份；不进入CommonState或任何保存访客。
enum class ReservationOwnerPhase { active,paused,settling,terminal };
struct ReservationOwnerView { ReservationRef reference;Integer owner_revision=0;ReservationOwnerPhase phase=ReservationOwnerPhase::terminal;bool may_retain_resources=false; };
class DomainActivityReservationOwnerRead {public:virtual ~DomainActivityReservationOwnerRead()=default;virtual Outcome<ReservationOwnerView> read_domain_activity_reservation_owner(const ReservationRef&)const=0;};
struct ResourceExecutionCaptureIdentity;
struct ActorConditionCaptureIdentity;
class CommonStore final : public CommonWrite {
public:
    explicit CommonStore(CommonState state,std::shared_ptr<const CommonCatalog> catalog);
    CommonStore(const CommonStore&);CommonStore& operator=(const CommonStore&);
    CommonStore(CommonStore&&) noexcept;CommonStore& operator=(CommonStore&&) noexcept;
    CommonState& state();const CommonState& state() const;
    // 捕获前在所属线程准备全部单人派生值；发布lease后members只借这些稳定值。
    Status prepare_checkpoint_read_cache();
    void retain_checkpoint_read_cache(CheckpointRetentionCensus&)const;
    const CheckpointMemoryCharge& checkpoint_copy_memory()const noexcept{return checkpoint_copy_memory_;}
    void bind_membership(const MembershipResolver*,bool field_battle_selected=false);
    Outcome<bool> field_battle_bound(const StableId&) const override;
    void bind_task_owners(const TaskOwnerRead*);
    void bind_household_owners(const HouseholdOwnerRead*);
    void bind_organization_owners(const OrganizationOwnerRead*);
    void bind_reservation_owners(const ReservationOwnerRead*);
    void bind_action_time_validation(const ActionTimeValidationRead*);
    void bind_carried_location_validation(const CarriedLocationValidationRead*);
    void bind_selected_lot_semantics(const SelectedLotSemanticRead*);
    const SelectedLotSemanticRead* selected_lot_semantics() const { return selected_lot_semantics_; }
    bool valid_resource_owner(const Owner&) const;
    void bind_domain_activity_reservation_owners(const DomainActivityReservationOwnerRead*);
    // 只接受本候选服务，缺失时已选定市场配置保持拒绝资源写入。
    void bind_protected_market_resources(ProtectedMarketResourceObserver*);
    const ItemDefinition* item_definition(const StableId&) const;
    bool reservation_exists(const ReservationRef&) const;
    const ActionCoreView* action(const ReservationRef& ref) const { return ref.kind==ReservationRef::Kind::action?action(ref.id):nullptr; }
    void bind_family_identity(const FamilyIdentityRead*);
    // 领域直接改party/army后显式使共享读版本失效，不保存重复成员索引。
    void invalidate(const ResourceKey&);
    // NPC普通拒绝的局部原子边界只记录受控primitive触及的值；不支持嵌套或直接state写入。
    Outcome<Integer> begin_savepoint();
    Status commit_savepoint(Integer token);
    Status rollback_savepoint(Integer token);
    Status apply_role(const StableId&,const RoleRecord&);
    // 动态身份与继任只写共同表，全部受当前候选和savepoint控制。
    Status create_actor(const ActorRecord&,Integer request_sequence=0);
    Status set_player_actor(const StableId&);
    Status replace_fact_header(const FactDraft&);
    Status apply_party(const StableId&,const std::optional<PartyRecord>&);
    const ActorCoreView* actor(const StableId&) const override;const ActionCoreView* action(const StableId&) const override;
    const WalletView* wallet(const StableId&) const override;const InventoryView* inventory(const StableId&) const override;
    const std::vector<StableId>* members(const StableId&) const override;
    const std::vector<StableId>& actor_order() const override;const std::vector<StableId>& action_order() const override;
    Outcome<Integer> inventory_weight(const StableId&) const override;Outcome<Integer> spendable(const StableId&) const override;
    ReadVersion read_version(const ResourceKey&) const override;Integer tick() const override;Integer revision() const override;
    Status apply_resources(const ResourceRecipe&) override;Status apply_actor(const ActorMutation&) override;Status apply_action(const ActionMutation&) override;
    Status append_fact(const FactDraft&) override;Outcome<StableId> next_id(const StableId&) override;
    Status append_fact_borrowed(std::string_view id,std::string_view kind,std::string_view domain) override;
    Outcome<ProducedStableId> next_owned_id(std::string_view prefix) override;
    Outcome<Integer> random_bounded(const StableId&,Integer,Integer) override;
    Outcome<Location> resolved_location(const StableId&) const;
    Outcome<Integer> count(const StableId&,const StableId&,const std::vector<Owner>&,const ReservationRef& reservation={}) const;
    Status validate_common() const;
    // 仅供一个未发布候选观察私人钱包毛支出；不把收入或预留变化计作抵扣。
    Status begin_wallet_debit_observation(const StableId&);
    Integer observed_wallet_debits() const;
    void clear_wallet_debit_observation();
    // 显式同栈捕获原recipe执行；不开新observer，不写持久段，未启用者保原资源路径。
    Status begin_resource_execution_capture();
    const std::vector<ResourceExecutionDraft>& resource_execution_drafts() const;
    // 持有值防止结束后重开或候选替换复用旧身份；没有活动捕获时为空。
    std::shared_ptr<const ResourceExecutionCaptureIdentity> resource_execution_capture_identity() const;
    // 保存点内不能取走草稿；失败原语无行，回滚裁回开始该保存点时的精确前界。
    Outcome<std::vector<ResourceExecutionDraft>> finish_resource_execution_capture();
    // 显式携货保护捕获只持同栈纯读服务，必须在原保存点外开启/结束；副本不继承。
    Status begin_protected_freight_resources(const ProtectedFreightResourceRead&);
    const std::vector<ProtectedFreightResourceDraft>& protected_freight_resource_drafts() const;
    std::shared_ptr<const ProtectedFreightCaptureIdentity> protected_freight_capture_identity() const;
    Outcome<std::vector<ProtectedFreightResourceDraft>> finish_protected_freight_resources();
    // 只观察原apply_actor/create_actor实际成功写回；未启用时完全保原路径。
    Status begin_actor_location_capture();
    const std::vector<ActorLocationExecutionDraft>& actor_location_execution_drafts() const;
    std::shared_ptr<const ActorLocationCaptureIdentity> actor_location_capture_identity() const;
    Outcome<std::vector<ActorLocationExecutionDraft>> finish_actor_location_capture();
    Status begin_actor_condition_capture();
    const std::vector<ActorConditionExecutionDraft>& actor_condition_execution_drafts() const;
    std::shared_ptr<const ActorConditionCaptureIdentity> actor_condition_capture_identity() const;
    // 只观察原保存点生命周期；即使rollback将值全还原，旧边界仍必须失效。
    Outcome<ActorConditionSavepointBoundary> actor_condition_savepoint_boundary() const;
    Status validate_actor_condition_savepoint_boundary(const ActorConditionSavepointBoundary&) const;
    // 原保存点内不能消费；返回序仅代表本次实际执行，不是业务头/完成时间。
    Outcome<std::vector<ActorConditionExecutionDraft>> finish_actor_condition_capture();
    // 显式一次提交捕获；开启后未知选择变化也拒绝，静态保护位缺捕获仍拒绝。
    Status begin_actor_selection_capture();
    const std::vector<ActorSelectionExecutionDraft>& actor_selection_execution_drafts() const;
    std::shared_ptr<const ActorSelectionCaptureIdentity> actor_selection_capture_identity() const;
    // 原命令scope固定本人、单一选择字段和实际请求值；不授予Growth业务权限。
    Outcome<ActorSelectionWriteHandle> begin_actor_selection_write(const StableId&,ActorSelectionCommand,const StableId&);
    // 必须已有恰一真实写回；保存点内可绑定领域来源，外层回滚还须撤回该领域候选。
    Outcome<ActorSelectionExecutionDraft> finish_actor_selection_write(const ActorSelectionWriteHandle&);
    // 未写scope只可显式放弃；已写必须绑定或随整候选丢弃，不能静默擦实录。
    Status abandon_actor_selection_write(const ActorSelectionWriteHandle&);
    // 保存点或命令scope未闭不能取走草稿；结束重开不复用捕获身份。
    Outcome<std::vector<ActorSelectionExecutionDraft>> finish_actor_selection_capture();
private:
    CheckpointMemoryCharge checkpoint_copy_memory_; // first: outlives every copied business field
    struct WalletDebitObservation { StableId wallet_id; Integer total=0; };
    std::optional<WalletDebitObservation> wallet_debit_;
    // 捕获只属于当前提交栈；copy/move都不让另一个候选继承启用或未绑定的来源草稿。
    struct ResourceExecutionCapture {
        bool active=false,in_recipe=false;
        std::vector<ResourceExecutionDraft> drafts;
        std::shared_ptr<const ResourceExecutionCaptureIdentity> identity;
        ResourceExecutionCapture()=default;
        ResourceExecutionCapture(ResourceExecutionCapture&& from) noexcept {from.clear();}
        ResourceExecutionCapture& operator=(ResourceExecutionCapture&& from) noexcept {clear();from.clear();return *this;}
        void clear(){active=false;in_recipe=false;drafts.clear();identity.reset();}
    } resource_execution_capture_;
    // 与资源捕获独立共存，copy/move均不转移旧栈身份或草稿。
    // 原SP只需还原已发布行前缀；pure prepare不允许另外修改任何领域状态。
    struct ProtectedFreightCapture {
        const ProtectedFreightResourceRead* read=nullptr;
        std::shared_ptr<const ProtectedFreightCaptureIdentity> identity;
        std::vector<ProtectedFreightResourceDraft> drafts;
        ProtectedFreightCapture()=default;
        ProtectedFreightCapture(ProtectedFreightCapture&& from) noexcept {from.clear();}
        ProtectedFreightCapture& operator=(ProtectedFreightCapture&& from) noexcept {clear();from.clear();return *this;}
        void clear(){read=nullptr;identity.reset();drafts.clear();}
    } protected_freight_capture_;
    struct ActorLocationCapture {
        bool active=false;
        std::vector<ActorLocationExecutionDraft> drafts;
        std::shared_ptr<const ActorLocationCaptureIdentity> identity;
        ActorLocationCapture()=default;
        ActorLocationCapture(ActorLocationCapture&& from) noexcept {from.clear();}
        ActorLocationCapture& operator=(ActorLocationCapture&& from) noexcept {clear();from.clear();return *this;}
        void clear(){active=false;drafts.clear();identity.reset();}
    } actor_location_capture_;
    struct ActorConditionCapture {
        bool active=false;
        std::vector<ActorConditionExecutionDraft> drafts;
        std::shared_ptr<const ActorConditionCaptureIdentity> identity;
        ActorConditionCapture()=default;
        ActorConditionCapture(ActorConditionCapture&& from) noexcept {from.clear();}
        ActorConditionCapture& operator=(ActorConditionCapture&& from) noexcept {clear();from.clear();return *this;}
        void clear(){active=false;drafts.clear();identity.reset();}
    } actor_condition_capture_;
    struct ActorSelectionScope {
        std::shared_ptr<const ActorSelectionScopeIdentity> identity;
        StableId actor_id;
        ActorSelectionCommand command=ActorSelectionCommand::equip_weapon;
        ActorCombat before;
        WorldIdentity world_before;
        Integer next_instance_before=0;
        ActorSelectionValues expected;
        std::optional<std::size_t> applied_index;
    };
    // copy/move不转移原栈许可；同址结束重开也有独立不可复用身份。
    struct ActorSelectionCapture {
        bool active=false;
        std::shared_ptr<const ActorSelectionCaptureIdentity> identity;
        std::optional<ActorSelectionScope> scope;
        std::vector<ActorSelectionExecutionDraft> drafts;
        ActorSelectionCapture()=default;
        ActorSelectionCapture(ActorSelectionCapture&& from) noexcept {from.clear();}
        ActorSelectionCapture& operator=(ActorSelectionCapture&& from) noexcept {clear();from.clear();return *this;}
        void clear(){active=false;identity.reset();scope.reset();drafts.clear();}
    } actor_selection_capture_;
    // 全部原字段校验后、任何人物写前核命令scope；失败不发布人物或草稿。
    Outcome<std::optional<ActorSelectionExecutionDraft>> prepare_actor_selection_write(const ActorMutation&,const ActorRecord&) const;
    // 原人物写成功后同步记录，保存点负责两者共同撤回。
    void record_actor_selection_write(ActorSelectionExecutionDraft);
    // 默认World移动也不得携带指向原候选的市场借用；其他既有成员移动语义保持。
    struct ProtectedMarketBinding {
        ProtectedMarketResourceObserver* observer=nullptr;
        ProtectedMarketBinding()=default;
        ProtectedMarketBinding(ProtectedMarketBinding&& from) noexcept {from.observer=nullptr;}
        ProtectedMarketBinding& operator=(ProtectedMarketBinding&& from) noexcept {observer=nullptr;from.observer=nullptr;return *this;}
    };
    struct SavepointData;std::shared_ptr<SavepointData> savepoint_;Integer savepoint_sequence_=0;
    CommonState state_;std::shared_ptr<const CommonCatalog> catalog_;const MembershipResolver* membership_=nullptr;bool field_battle_selected_=false;const TaskOwnerRead* task_owners_=nullptr;const FamilyIdentityRead* family_identity_=nullptr;const HouseholdOwnerRead* household_owners_=nullptr;const OrganizationOwnerRead* organization_owners_=nullptr;const ReservationOwnerRead* reservation_owners_=nullptr;ProtectedMarketBinding protected_market_;
    // Original borrowed candidate adapters never survive a candidate copy/move.
    const ActionTimeValidationRead* action_time_validation_=nullptr;
    const CarriedLocationValidationRead* carried_location_validation_=nullptr;
    const SelectedLotSemanticRead* selected_lot_semantics_=nullptr;
    const DomainActivityReservationOwnerRead* domain_activity_reservation_owners_=nullptr;
    // 费用先于实际缓存字段构造、后于其销毁；clone不会复制sealed资格。
    mutable CheckpointMemoryCharge solitary_memory_;
    mutable CheckpointHashMap<StableId,std::vector<StableId>> solitary_;
    bool checkpoint_read_cache_frozen_=false;
    CheckpointMap<std::pair<int,StableId>,std::uint64_t> versions_;
    void clear_checkpoint_read_cache()noexcept;
    void changed(ResourceKey::Kind,const StableId&);
};
} // clone清空跨域借用与缓存，由WorldBundle在同candidate重新绑定。
