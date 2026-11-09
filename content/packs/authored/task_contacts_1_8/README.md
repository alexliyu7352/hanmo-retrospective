# 青川私信原物返还 1.8

原 task_contacts 1.7 的六个场景保持旧模块选择。只新增 contacts::scenario.qingchuan_letter_returns，显式选择 original_task_assets_v1，复用原 offer.letter、真实学师委托、原收件人和文书交接/返报v2。

真实路径：在学师本人岗位获知私信，原受领完成后，未交收件人时回原学师办理取消。原封信由原任务产权批次返还原发信人私人容器，私人其他货物不交出。没有初态Task、假收据或知情事实。

selection：content/packs/selections/task-letter-returns-1.8.0.json，依赖 hanmo.local_society 1.11。本私信场景没有Task2资金历史，因此货物凭据不单独授予继产历史免责；这与1.2采购Task2组合明确分开。

compose与实际World/UI各有独立状态，不把作者结构有效写成运行通过。
