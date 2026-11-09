// 原GDScript几何的逐项原生实现；保留相切、整数截断和稳定图节点顺序。
#include "personal_geometry.hpp"
#include <cmath>
#include <limits>
#include <set>
namespace hanmo::resident {
    namespace {
    constexpr double epsilon=1e-7,pi=3.1415926535897932384626433832795;
    // 场地只收正尺寸矩形和明确阻挡位，非法配置不能被视为可通行。
    bool valid(const PersonalArena& a){
        if(a.width<=0||a.height<=0)return false;
        for(auto& r:a.obstacles)if(r.width<=0||r.height<=0)return false;
        return true;
    }
    bool inside(const PersonalArena& a,PersonalPoint p){
        return p.x>=0&&p.y>=0&&p.x<=a.width&&p.y<=a.height;
    }
    double squared(PersonalPoint a,PersonalPoint b){
        double x=double(a.x)-b.x,y=double(a.y)-b.y;
        return x*x+y*y;
    }
    // 零长路径退化为点；最近点投影只用于浮点距离，不回写规则坐标。
    double segment_distance(PersonalPoint p,PersonalPoint a,PersonalPoint b){
        double x=double(b.x)-a.x,y=double(b.y)-a.y,n=x*x+y*y;
        if(n==0)return squared(p,a);
        double t=std::clamp(((double(p.x)-a.x)*x+(double(p.y)-a.y)*y)/n,0.0,1.0);
        x=double(p.x)-a.x-t*x;
        y=double(p.y)-a.y-t*y;
        return x*x+y*y;
    }
    bool collision(PersonalPoint a0,PersonalPoint a1,PersonalPoint b0,PersonalPoint b1,Integer r){
        double d=segment_distance({0,0},{a0.x-b0.x,a0.y-b0.y},{a1.x-b1.x,a1.y-b1.y});
        return r==0?d<=epsilon:d<double(r)*r-epsilon;
    }
    // 闭合矩形裁剪保留视线触边即阻挡的原语义。
    bool segment_rect(PersonalPoint a,PersonalPoint b,const PersonalObstacle& r){
        double enter=0,leave=1;
        for(int axis=0;axis<2;++axis){
            Integer low=axis?r.y:r.x,high=low+(axis?r.height:r.width),from=axis?a.y:a.x;
            double delta=double(axis?b.y:b.x)-from;
            if(delta==0){
                if(from<low||from>high)return false;
                continue;
            }
            double first=(low-double(from))/delta,last=(high-double(from))/delta;
            enter=std::max(enter,std::min(first,last));
            leave=std::min(leave,std::max(first,last));
            if(enter>leave)return false;
        }
        return true;
    }
    bool body_rect(PersonalPoint a,PersonalPoint b,Integer radius,const PersonalObstacle& r){
        if(segment_rect(a,b,r))return true;
        if(radius==0)return false;
        double d=std::numeric_limits<double>::infinity();
        for(auto p:{a,b})d=std::min(d,squared(p,{std::clamp(p.x,r.x,r.x+r.width),std::clamp(p.y,r.y,r.y+r.height)}));
        for(auto p:{PersonalPoint{r.x,r.y},PersonalPoint{r.x+r.width,r.y},PersonalPoint{r.x,r.y+r.height},PersonalPoint{r.x+r.width,r.y+r.height}})d=std::min(d,segment_distance(p,a,b));
        return d<double(radius)*radius-epsilon;
    }
    PersonalPoint radial(PersonalPoint p,double r,double angle){
        return {Integer(std::round(p.x+std::cos(angle)*r)),Integer(std::round(p.y+std::sin(angle)*r))};
    }
    std::vector<PersonalBody> occupied(const PersonalBattle& b,const StableId& actor,bool sort){
        auto ids=b.units.order();
        if(sort)std::sort(ids.begin(),ids.end());
        std::vector<PersonalBody> result;
        for(auto& id:ids){
            auto& u=*b.units.find(id);
            if(id!=actor&&!u.withdrawn_round)result.push_back({u.position,u.radius});
        }
        return result;
    }
    void gate_goals(std::vector<PersonalPoint>& goals,const PersonalExit& gate){
        goals.push_back(gate.position);
        for(double r:{std::max(0.0,gate.radius-1.0),gate.radius*0.5})for(int i=0;i<16;++i)goals.push_back(radial(gate.position,r,2*pi*i/16.0));
    }
    }
    // 旧导航距离经Godot Vector2的binary32长度，几何扫掠仍使用上方binary64平方距离。
    double personal_distance(PersonalPoint a,PersonalPoint b){
        float x=static_cast<float>(double(a.x)-b.x),y=static_cast<float>(double(a.y)-b.y);
        return std::sqrt(x*x+y*y);
    }
    Integer personal_heading(PersonalPoint a,PersonalPoint b,Integer fallback){
        if(a==b)return fallback;
        auto n=Integer(std::round(std::atan2(double(b.y)-a.y,double(b.x)-a.x)*180.0/pi));
        return (n%360+360)%360;
    }
    bool personal_point_valid(const PersonalArena& a,PersonalPoint p,Integer radius){
        if(!valid(a)||radius<0||p.x<radius||p.y<radius||p.x>a.width-radius||p.y>a.height-radius)return false;
        for(auto& r:a.obstacles)if(r.blocks_movement&&body_rect(p,p,radius,r))return false;
        return true;
    }
    Status personal_can_move(const PersonalArena& a,PersonalPoint from,PersonalPoint to,Integer maximum,Integer radius,const std::vector<PersonalBody>& bodies){
        if(!valid(a))return common_failure("InvalidArena");
        if(radius<0||maximum<0)return common_failure("InvalidMovement");
        if(!personal_point_valid(a,from,radius))return common_failure("InvalidOrigin");
        if(!personal_point_valid(a,to,radius))return common_failure("InvalidDestination");
        if(squared(from,to)>double(maximum)*maximum)return common_failure("MoveTooFar");
        for(auto& r:a.obstacles)if(r.blocks_movement&&body_rect(from,to,radius,r))return common_failure("MovementBlocked");
        for(auto& body:bodies){
            if(body.radius<0)return common_failure("InvalidOccupant");
            if(collision(from,to,body.position,body.position,radius+body.radius))return common_failure("BodyBlocked");
        }
        return common_success();
    }
    bool personal_line_of_sight(const PersonalArena& a,PersonalPoint from,PersonalPoint to){
        if(!valid(a)||!inside(a,from)||!inside(a,to))return false;
        for(auto& r:a.obstacles)if(r.blocks_sight&&segment_rect(from,to,r))return false;
        return true;
    }
    Status personal_can_attack(const PersonalArena& a,PersonalPoint from,PersonalPoint to,Integer heading,const PersonalWeapon& w){
        if(!valid(a))return common_failure("InvalidArena");
        if(!inside(a,from)||!inside(a,to))return common_failure("InvalidPoint");
        if(w.minimum_range<0||w.maximum_range<w.minimum_range||w.arc_degrees<0||w.arc_degrees>360)return common_failure("InvalidWeapon");
        double d=squared(from,to);
        if(d<double(w.minimum_range)*w.minimum_range)return common_failure("InsideMinimumRange");
        if(d>double(w.maximum_range)*w.maximum_range)return common_failure("OutOfRange");
        if(d==0)return common_failure("CoincidentTarget");
        double angle=std::atan2(double(to.y)-from.y,double(to.x)-from.x)*180/pi;
        double diff=std::fabs(std::fmod(std::fmod(angle-heading+180,360)+360,360)-180);
        if(diff>w.arc_degrees/2.0+epsilon)return common_failure("WrongFacing");
        if(!personal_line_of_sight(a,from,to))return common_failure("LineBlocked");
        return common_success();
    }
    // 所有身体同一微步提案，新增阻挡反复收敛；不能抢占或在中途互穿。
    Outcome<PersonalMoves> personal_resolve_moves(const PersonalArena& a,const OrderedTable<PersonalUnit>& units,const OrderedTable<PersonalPlan>& plans){
        PersonalMoves r;
        auto ids=units.order();
        ids.erase(std::remove_if(ids.begin(),ids.end(),[&](auto& id){
            return units.find(id)->withdrawn_round.has_value();
        }),ids.end());
        std::sort(ids.begin(),ids.end());
        for(auto& id:ids)r.positions.insert(id,units.find(id)->position);
        auto fail=[&](const StableId& code){
            return Outcome<PersonalMoves>{false,{},common_failure(code).error};
        };
        if(!valid(a))return fail("InvalidArena");
        for(auto& id:ids)if(!personal_point_valid(a,*r.positions.find(id),units.find(id)->radius))return fail("InvalidUnitPosition");
        for(std::size_t i=0;i<ids.size();++i)for(std::size_t j=i+1;j<ids.size();++j){
            auto& u=*units.find(ids[i]);
            auto& v=*units.find(ids[j]);
            if(collision(u.position,u.position,v.position,v.position,u.radius+v.radius))return fail("OverlappingUnits");
        }
        for(auto& row:plans.rows())if(!r.positions.find(row.first))return fail("UnknownMoveActor");
        auto origins=r.positions;
        Integer steps=0;
        OrderedTable<PersonalPoint> targets;
        std::set<StableId> blocked;
        for(auto& id:ids)if(auto p=plans.find(id)){
            if(!inside(a,p->destination)){
                blocked.insert(id);
                continue;
            }
            targets.insert(id,p->destination);
            auto o=*origins.find(id);
            steps=std::max(steps,std::max(std::abs(p->destination.x-o.x),std::abs(p->destination.y-o.y)));
        }
        for(Integer step=1;step<=steps;++step){
            auto next=r.positions;
            for(auto& id:ids){
                auto t=targets.find(id);
                if(!t||blocked.count(id))continue;
                auto o=*origins.find(id);
                PersonalPoint to{o.x+(t->x-o.x)*step/steps,o.y+(t->y-o.y)*step/steps};
                if(personal_can_move(a,*r.positions.find(id),to,2,units.find(id)->radius).ok)next.set(id,to);
                else blocked.insert(id);
            }
            bool changed=true;
            while(changed){
                changed=false;
                std::set<StableId> newly;
                for(std::size_t i=0;i<ids.size();++i)for(std::size_t j=i+1;j<ids.size();++j){
                    auto& x=ids[i];
                    auto& y=ids[j];
                    if(!collision(*r.positions.find(x),*next.find(x),*r.positions.find(y),*next.find(y),units.find(x)->radius+units.find(y)->radius))continue;
                    if(*next.find(x)!=*r.positions.find(x))newly.insert(x);
                    if(*next.find(y)!=*r.positions.find(y))newly.insert(y);
                }
                for(auto& id:newly){
                    next.set(id,*r.positions.find(id));
                    blocked.insert(id);
                    changed=true;
                }
            }
            r.positions=std::move(next);
        }
        r.blocked.assign(blocked.begin(),blocked.end());
        return {true,std::move(r),{}};
    }
    // 有限可见图使用原插入顺序Dijkstra；超预算或无路保持原位。
    PersonalPoint personal_navigate(const PersonalArena& a,PersonalPoint origin,Integer radius,const std::vector<PersonalBody>& bodies,const std::vector<PersonalPoint>& goals,Integer maximum){
        if(maximum<=0)return origin;
        std::vector<PersonalPoint> nodes{origin};
        std::set<std::size_t> targets;
        auto append=[&](PersonalPoint p){
            auto f=std::find(nodes.begin(),nodes.end(),p);
            if(f!=nodes.end())return std::size_t(f-nodes.begin());
            nodes.push_back(p);
            return nodes.size()-1;
        };
        auto free=[&](PersonalPoint p){
            return personal_can_move(a,p,p,0,radius,bodies).ok;
        };
        for(auto p:goals)if(free(p))targets.insert(append(p));
        if(targets.empty())return origin;
        for(auto& r:a.obstacles)if(r.blocks_movement){
            Integer m=radius+1;
            for(auto p:{PersonalPoint{r.x-m,r.y-m},PersonalPoint{r.x+r.width+m,r.y-m},PersonalPoint{r.x-m,r.y+r.height+m},PersonalPoint{r.x+r.width+m,r.y+r.height+m}})if(free(p))append(p);
        }
        for(auto& body:bodies){
            double clearance=(radius+body.radius+2.0)/std::cos(pi/8);
            for(int i=0;i<8;++i){
                auto p=radial(body.position,clearance,2*pi*i/8);
                if(free(p))append(p);
            }
        }
        if(nodes.size()>192)return origin;
        std::vector<double> distances(nodes.size(),INFINITY);
        std::vector<std::size_t> previous(nodes.size(),0);
        std::vector<bool> closed(nodes.size(),false);
        distances[0]=0;
        for(std::size_t iteration=0;iteration<nodes.size();++iteration){
            std::size_t selected=nodes.size();
            double cheapest=INFINITY;
            for(std::size_t i=0;i<nodes.size();++i)if(!closed[i]&&distances[i]<cheapest){
                selected=i;
                cheapest=distances[i];
            }
            if(selected==nodes.size())break;
            if(targets.count(selected)){
                auto first=selected;
                while(previous[first]!=0)first=previous[first];
                auto target=nodes[first];
                double length=personal_distance(origin,target);
                if(length>maximum){
                    double f=std::max<Integer>(0,maximum-1)/length;
                    target={Integer(std::round(origin.x+(target.x-origin.x)*f)),Integer(std::round(origin.y+(target.y-origin.y)*f))};
                }
                return personal_can_move(a,origin,target,maximum,radius,bodies).ok?target:origin;
            }
            closed[selected]=true;
            for(std::size_t i=0;i<nodes.size();++i){
                if(closed[i]||i==selected)continue;
                double length=personal_distance(nodes[selected],nodes[i]),cost=cheapest+length;
                if(cost>=distances[i])continue;
                if(personal_can_move(a,nodes[selected],nodes[i],Integer(std::ceil(length)),radius,bodies).ok){
                    distances[i]=cost;
                    previous[i]=selected;
                }
            }
        }
        return origin;
    }
    bool personal_at_exit(const PersonalArena& a,Integer side,PersonalPoint position){
        for(auto& gate:a.exits)if(gate.side==side&&personal_distance(position,gate.position)<=gate.radius)return true;
        return false;
    }
    PersonalPoint personal_withdraw_destination(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,Integer maximum){
        auto& u=*b.units.find(actor);
        auto& a=*c.arenas.find(b.battlefield_id);
        if(personal_at_exit(a,u.side,u.position))return u.position;
        std::vector<PersonalPoint> goals;
        for(auto& g:a.exits)if(g.side==u.side)gate_goals(goals,g);
        return personal_navigate(a,u.position,u.radius,occupied(b,actor,true),goals,maximum);
    }
    PersonalPoint personal_escort_destination(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,Integer maximum){
        if(c.objectives.find(b.objective_id)->escort_model=="exit_v1")return personal_withdraw_destination(b,c,actor,maximum);
        auto& u=*b.units.find(actor);
        auto& leader=*b.units.find(b.controller_id);
        auto& a=*c.arenas.find(b.battlefield_id);
        if(maximum<=0)return u.position;
        std::vector<PersonalPoint> goals;
        for(auto& gate:a.exits)if(gate.side==u.side&&personal_distance(leader.position,gate.position)<=gate.radius){
            if(personal_distance(u.position,gate.position)<=gate.radius)return u.position;
            gate_goals(goals,gate);
        }
        if(goals.empty()){
            auto spacing=u.radius+leader.radius+4;
            if(personal_distance(u.position,leader.position)<=spacing)return u.position;
            for(int i=0;i<16;++i)goals.push_back(radial(leader.position,double(spacing),2*pi*i/16));
        }
        return personal_navigate(a,u.position,u.radius,occupied(b,actor,true),goals,maximum);
    }
    StableId personal_guard_sector(PersonalPoint target,Integer heading,PersonalPoint attacker){
        if(heading<0||heading>359||target==attacker)return "invalid";
        double x=double(attacker.x)-target.x,y=double(attacker.y)-target.y,dot=std::cos(heading*pi/180)*x+std::sin(heading*pi/180)*y,tolerance=std::max(1.0,std::abs(x)+std::abs(y))*1e-10;
        if(std::abs(dot)<=tolerance)return "side";
        return dot>0?"front":"back";
    }
}
