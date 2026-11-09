// 原控制来源五个纯DTO；从真实读口头机械拆出，布局/语义不变。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct AuthoredProfileController {
  StableId scenario_id, actor_id;
};
struct CreatedProfileController {
  StableId creation_fact_id, actor_id;
};
struct SuccessionProfileController {
  StableId succession_fact_id, actor_id;
};
struct LegacyProfileController {
  StableId activation_fact_id, consent_fact_id, actor_id;
};
struct ContinuationProfileController {
  StableId activation_id, pending_id, actor_id;
  Integer control_epoch = 0;
};
using ProfileControllerSource =
    std::variant<AuthoredProfileController, CreatedProfileController,
                 SuccessionProfileController, LegacyProfileController,
                 ContinuationProfileController>;
} // namespace hanmo::resident
