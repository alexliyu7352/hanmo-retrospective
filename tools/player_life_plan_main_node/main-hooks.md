# 中央独占接线

C 已拥有并修改 screen_composer.gd、study_screen.gd、family_screen.gd；中央只需接 Main/backend/page registry，勿重复拷贝 C 正文。

- Main 新增 `_player_plan_open: bool=false`。`_player_plan_enabled()` 只在当前情景 `player_plan_profile=="known_life_v1"` 且 resident 模式返回 true，不改 global capabilities。
- `_open_player_plan()` 保留原手记的停许可/稳定页逻辑，关闭地图、成长、技艺、家事、营务与课程等 UI 标志，再置 `_player_plan_open=true` 并 `_render()`。`_close_player_plan()` 只清 UI 标志并重绘。
- `_show_journal` 保留原文本。新 profile 增加 `dialog.add_button("安排已知生活",true,"player_plan")`，custom_action 回调关闭该 dialog 并调用 `_open_player_plan()`；旧 profile 不显示。打开手记本身无计划命令。
- Main 新局、标题、替换世界与物理恢复清 `_player_plan_open`。新的其他页面入口也关闭该标志。`player_plan_open/close` 走正常手动导航停许可。
- 当前 plan 页准备 `kernel.demand_view("player_plan")`，在 active study 等早返回之前。其它来源 add preview 沿已有 demand preview 队列，不同步调用 worker，不新增第二 transport。
- `_begin_player_plan(max_additional_ticks,max_gross_spend,source,scene_epoch)` 由控件传整数和冻结 UI 身份；先核世界、场景、busy、storage，再使用原 finite continuation driver 的新 plan run。每次启动明确本人批准的新预算，恢复后不自动运行。
- UI 与 A/B 冻结 command: player_plan_add {kind,params,authorized,terms?}; remove {step_id}; reorder {step_ids}; clear/pause {}; authorize {step_id,authorized,terms?}。原生 preview(add未授权) 应返回 terms，以便本人勾选的 add 带回完全相同条款。
- `player_plan` 页包含 enabled、steps、budget、commitments、conflicts。未启用返回 enabled:false。steps 为原 typed kind/params/terms 与稳定 step_id；commitments 的 task 行给 source_id、place_id、label、due_tick。课程的预计完成不是法定期限。

首节点固定中央唯一4.7.2 stage，C不创建第二stage、不运行原生或GUI。driver将在该绑定冻结之后才运行。
