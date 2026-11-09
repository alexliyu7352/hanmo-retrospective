// 每次同步只读持同栈origin/context；不缓存World引用或在克隆间保存服务指针。
#include "growth_personal_mastery_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {namespace {
Outcome<GrowthPersonalMasteryObservation> read(const WorldBundle& w,const StableId& actor,Integer tick,const StableId& entry,bool historical){
    if(!w.catalog||!w.catalog->common||!w.catalog->family||!w.catalog->family->enabled||!w.catalog->growth||!w.catalog->growth->cultivation||!w.catalog->personal||!w.catalog->personal->enabled||!w.catalog->personal_mastery_initial)return {false,{},common_failure("GrowthMasteryInitialCatalogRequired",actor).error};
    const GrowthNavalHistoryContext origins{*w.catalog->common,*w.catalog->family,w.family,w.family_birth_enabled()?&w.family_birth:nullptr};
    const GrowthPersonalMasteryContext context{origins,*w.catalog->personal_mastery_initial,*w.catalog->growth,*w.catalog->personal};
    return historical?read_growth_personal_mastery_before_entry(w.common,context,w.growth,actor,tick,entry):read_growth_personal_mastery(w.common,context,w.growth,actor);
}
}
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery_world(const WorldBundle& w,const StableId& actor){return read(w,actor,w.common.tick(),{},false);}
Outcome<GrowthPersonalMasteryObservation> read_growth_personal_mastery_before_world(const WorldBundle& w,const StableId& actor,Integer tick,const StableId& entry){return read(w,actor,tick,entry,true);}
}
