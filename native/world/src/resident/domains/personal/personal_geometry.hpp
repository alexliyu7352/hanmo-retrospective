// 个人战连续整数几何与有限可见导航；不消耗世界时间和随机，也不按ID争抢路权。
#pragma once
#include "personal_model.hpp"
namespace hanmo::resident {
    struct PersonalBody {
        PersonalPoint position;
        Integer radius=18;
    };
    struct PersonalMoves {
        OrderedTable<PersonalPoint> positions;
        std::vector<StableId> blocked;
    };
    double personal_distance(PersonalPoint,PersonalPoint);
    Integer personal_heading(PersonalPoint,PersonalPoint,Integer);
    bool personal_point_valid(const PersonalArena&,PersonalPoint,Integer radius=18);
    Status personal_can_move(const PersonalArena&,PersonalPoint,PersonalPoint,Integer,Integer radius=18,const std::vector<PersonalBody>& occupied={});
    Status personal_can_attack(const PersonalArena&,PersonalPoint,PersonalPoint,Integer,const PersonalWeapon&);
    bool personal_line_of_sight(const PersonalArena&,PersonalPoint,PersonalPoint);
    Outcome<PersonalMoves> personal_resolve_moves(const PersonalArena&,const OrderedTable<PersonalUnit>&,const OrderedTable<PersonalPlan>&);
    PersonalPoint personal_navigate(const PersonalArena&,PersonalPoint,Integer,const std::vector<PersonalBody>&,const std::vector<PersonalPoint>&,Integer);
    bool personal_at_exit(const PersonalArena&,Integer,PersonalPoint);
    PersonalPoint personal_withdraw_destination(const PersonalBattle&,const PersonalCatalog&,const StableId&,Integer);
    PersonalPoint personal_escort_destination(const PersonalBattle&,const PersonalCatalog&,const StableId&,Integer);
    StableId personal_guard_sector(PersonalPoint,Integer,PersonalPoint);
}
// 微步同步碰撞和视线均沿旧几何，不依赖画面帧率。
