# 原静态采购货物收据 1.2

沿 task_takeover 1.1 原场景 task_handover::scenario.south_static_responsibility、原南门供应员和原采购合同，只显式增加 task_static_asset_profile=original_task_assets_v1。

真实路径：沿原菜单到南门，向供应员获知并受领静态采购，原任务钱包买一份粮。回原供应员报告，或在同一自然购货银行分支取消。原预算24、购货20、结酬8不变；粮食仅由原 TransferLots 搬入原南公库。同ID现金Task2与实货凭据分别可冷验，不改原主体或假造占领。

selection：content/packs/selections/task-static-assets-1.2.0.json，依赖原 qingxi.campaign 1.2 和 hanmo.political_takeover 1.0。旧1.1银行不升级版本或回填货物史。

本包是正常作者输入。compose结构门与实际World运行分别记录；未声明新GUI、政治长链或自然继产已通过。
