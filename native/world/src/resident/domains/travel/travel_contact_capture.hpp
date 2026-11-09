// 原普通路遇的同栈观察值；原Encounter写成和保留先验由中央/Interaction唯一负责。
#pragma once
#include "travel_plan_capture.hpp"
#include "../../indexes/contact_model.hpp"
namespace hanmo::resident {
struct TravelContactTruncationDraft {
    Integer source_tick=0;
    StableId original_encounter_id;
    bool interactive=false;
    ContactHit original_hit;
    std::array<TravelOriginalPlanSnapshot,2> before,after;
};
struct TravelEncounterResumeDraft {
    Integer source_tick=0;
    StableId original_encounter_id;
    bool original_resume_paused=false;
    TravelOriginalPlanSnapshot before,after;
};
}
