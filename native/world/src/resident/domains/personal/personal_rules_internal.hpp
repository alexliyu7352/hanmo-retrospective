// 个人战叶之间的只读协作接口；所有函数只消费类型化冻结战场和目录。
#pragma once
#include "personal_geometry.hpp"
namespace hanmo::resident::personal_detail {
    template<class T> std::vector<StableId> ids(const OrderedTable<T>& table){
        auto result=table.order();
        std::sort(result.begin(),result.end());
        return result;
    }
    template<class T> Outcome<T> fail(const StableId& code){
        return {false,{},common_failure(code).error};
    }
    inline Integer value(const OrderedTable<Integer>& table,const StableId& id,Integer fallback=0){
        auto p=table.find(id);
        return p?*p:fallback;
    }
    inline bool enemy(const PersonalUnit& a,const PersonalUnit& b){
        return a.side!=b.side&&b.hp>0&&!b.withdrawn_round;
    }
    inline bool low_hp(const PersonalUnit& u){
        return !u.protected_actor&&u.hp>0&&u.hp*100<=u.initial_hp*25;
    }
    inline bool recovering(const PersonalUnit& u,Integer round){
        return round<u.recovery_until;
    }
    inline bool status(const PersonalUnit& u,const StableId& id,Integer round){
        auto p=u.statuses.find(id);
        return p&&*p>=round;
    }
    inline Integer mastered(Integer value,Integer mastery){
        return 10000+(value-10000)*mastery/100;
    }
    inline Integer scale(Integer value,Integer bp){
        return value*bp/10000;
    }
    const PersonalWeapon* weapon(const PersonalUnit&,const PersonalCatalog&);
    Outcome<const PersonalSkill*> skill(const PersonalUnit&,const PersonalCatalog&,const StableId&);
    const PersonalSkill* heart(const PersonalUnit&,const PersonalCatalog&);
    PersonalWeapon effective_weapon(const PersonalWeapon&,const PersonalSkill*);
    Integer move_distance(const PersonalUnit&,const PersonalCatalog&,Integer,const PersonalSkill* skill=nullptr);
    Status battle_error(const PersonalBattle&,const PersonalCatalog&);
    Status withdrawal_valid(const PersonalBattle&,const PersonalCatalog&,const StableId&);
    Status armor_valid(const PersonalUnit&,const PersonalCatalog&);
    Integer hit_chance(const PersonalUnit&,const PersonalUnit&,const PersonalWeapon&,const PersonalSkill*);
    Integer attack_power(const PersonalUnit&,const PersonalUnit&,const PersonalWeapon&,const PersonalSkill*,const PersonalCatalog&,const PersonalObjective&);
    Integer mitigate(Integer,const PersonalUnit&,const PersonalCatalog&,Integer,Integer);
    Integer damage(const PersonalUnit&,const PersonalUnit&,const PersonalWeapon&,const PersonalSkill*,const PersonalCatalog&,Integer,Integer,const PersonalObjective&);
    Integer guard_multiplier(const PersonalUnit&,const PersonalUnit&,Integer);
    Outcome<std::vector<StableId>> select_targets(const PersonalBattle&,const PersonalCatalog&,const StableId&,const StableId&,const PersonalSkill*,const PersonalWeapon&);
    PersonalPlan plan(PersonalPoint,Integer,const StableId&,const StableId& target={},const StableId& skill={});
    BoundaryValue encode_plan(const PersonalPlan&);
    BoundaryValue encode_event(const PersonalEvent&);
    BoundaryValue encode_arena(const PersonalArena&);
    BoundaryValue encode_skill(const PersonalSkill&);
    StableId action_key(const PersonalPlan&,PersonalPoint);
    void finish(PersonalBattle&,const PersonalCatalog&,const StableId&);
}
