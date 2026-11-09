// 玩家计划检查点恢复分派：沿原 else-if 链读取
// player_plan；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "player_plan") r =
    visit_player_plan_checkpoint(reader, *plan);
