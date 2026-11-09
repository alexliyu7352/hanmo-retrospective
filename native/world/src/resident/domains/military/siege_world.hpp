#include "resident/domains/military/siege_model.hpp"
#include "resident/domains/military/military_command_contract.hpp"
// 既有城防世界的typed协调接缝；阶段返回事实、节点会战和返程请求，由中央在同一候选内办理。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "occupation_time.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <iterator>
#include <new>
namespace hanmo::resident {
struct SiegeWorldEvent { StableId siege_id;SiegeEvent event; };
struct SiegeWorldBattleRequest { StableId siege_id;SiegeBattleRequest request; };
// 原热结果独占拥有；普通字符串/内层vector在复制前按锁定STL fresh-copy收费。
// 两外层容器按实际allocate收费，不能从元素数推测几何capacity。
struct SiegeWorldStep {
    CheckpointMemoryCharge copied_fields;
    CheckpointVector<SiegeWorldEvent> events;
    CheckpointVector<SiegeWorldBattleRequest> battle_requests;
    SiegeWorldStep()=default;
    SiegeWorldStep(const SiegeWorldStep&)=delete;
    SiegeWorldStep& operator=(const SiegeWorldStep&)=delete;
    SiegeWorldStep(SiegeWorldStep&&)noexcept=default;
    SiegeWorldStep& operator=(SiegeWorldStep&& other)noexcept{if(this!=&other){this->~SiegeWorldStep();::new(static_cast<void*>(this))SiegeWorldStep(std::move(other));}return *this;}
};
// 按原首个siege与每个siege内事件序借用；不另建分组表或复制事件深值。
class SiegeWorldEventRange {
public:
    class Iterator {
    public:
        using iterator_category=std::forward_iterator_tag;using value_type=SiegeEvent;using difference_type=std::ptrdiff_t;using pointer=const SiegeEvent*;using reference=const SiegeEvent&;
        reference operator*()const{return rows_->at(index_).event;}
        pointer operator->()const{return &rows_->at(index_).event;}
        Iterator& operator++(){++index_;seek();return *this;}
        Iterator operator++(int){auto old=*this;++*this;return old;}
        bool operator==(const Iterator& other)const noexcept{return rows_==other.rows_&&index_==other.index_;}
        bool operator!=(const Iterator& other)const noexcept{return !(*this==other);}
    private:
        friend class SiegeWorldEventRange;
        Iterator(const CheckpointVector<SiegeWorldEvent>& rows,const StableId& id,std::size_t index):rows_(&rows),id_(&id),index_(index){seek();}
        void seek(){while(index_<rows_->size()&&(*rows_)[index_].siege_id!=*id_)++index_;}
        const CheckpointVector<SiegeWorldEvent>* rows_;const StableId* id_;std::size_t index_;
    };
    SiegeWorldEventRange(const SiegeWorldStep& step,const StableId& id)noexcept:step_(&step),id_(&id){}
    Iterator begin()const{return Iterator(step_->events,*id_,0);}
    Iterator end()const{return Iterator(step_->events,*id_,step_->events.size());}
private:const SiegeWorldStep* step_;const StableId* id_;
};
struct SiegeDepartureDestination { StableId army_id,campaign_id,garrison_id,destination_settlement_id;Integer cost=0,weight=0,money=0;bool preferred=false; };
struct SiegeDepartureRoom { StableId garrison_id;Integer weight=0,money=0; };
struct SiegeDeparturePlan { std::vector<StableId> army_ids;std::vector<SiegeDepartureDestination> destinations;Integer search_states=0;StableId army_id,reason,limit,feasibility;Integer maximum=0; };
StableId siege_for_actor(const MilitaryState&,const StableId& actor);
Outcome<SiegeObservation> observe_siege_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const SiegeRecord&,Integer tick=-1);
// 只用于热命令或已完成全World验证的页面；冷早校验不得经此入口读取晚人物资格。
Outcome<SiegeObservation> observe_siege_orders_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const SiegeRecord&);
std::vector<StableId> siege_departure_defenders(const SiegeRecord&);
Outcome<SiegeDeparturePlan> allocate_siege_departure(const std::vector<std::pair<StableId,std::vector<SiegeDepartureDestination>>>&,const std::vector<SiegeDepartureRoom>&,Integer search_budget=8192);
Outcome<SiegeDeparturePlan> preview_siege_departure(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const SiegeRecord&);
Outcome<MilitaryQuote> preview_siege_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&);
Outcome<MilitaryEffect> apply_siege_world(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& actor,const MilitaryCommand&);
Status preflight_siege_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Outcome<SiegeWorldStep> step_siege_world(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer finish_tick);
Outcome<StableId> create_siege_battle(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& siege,const SiegeBattleRequest&,Integer tick);
Status finish_siege_battles(const CommonRead&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const std::vector<StableId>& battle_ids,Integer tick);
Outcome<MilitaryEffect> finish_siege_events(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& siege,const std::vector<SiegeEvent>&,Integer tick,const std::optional<MilitaryOccupationExecution>& origin={});
// 同一原业务函数的借用range入口，供一个私有calendar candidate即时消费。
Outcome<MilitaryEffect> finish_siege_events(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& siege,const SiegeWorldEventRange&,Integer tick,const std::optional<MilitaryOccupationExecution>& origin={});
// 原子交接后才办理围城终局；失败必须丢弃中央完整候选，不能只重试占领。
Outcome<MilitaryEffect> finish_siege_integration(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& transfer_id,Integer tick);
Status refresh_siege_plans(const CommonRead&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
Outcome<MilitaryEffect> terminate_siege_world(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& siege,const StableId& reason,Integer tick);
Status validate_siege_world(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
}
