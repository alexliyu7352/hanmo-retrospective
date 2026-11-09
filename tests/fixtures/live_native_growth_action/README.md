# 实际成长页按需原生行动门

这个小工程直接使用指定 Git commit 的全部 `core/ui/storage` GDScript、模块与能力描述，复用既有正式 prepared catalog 和已冻结的 `f1c907d` Bridge。不会重新编译 C++、改作者数值、重放 20 份原离线游戏来源链，或把 headless 控件检查写成 GUI 验收。

## 准备

`prepare_fixture.py --prepared-game <原tiny game> --catalog <正式catalog.hmcatalog.json> --source-commit <完整40位Git commit> --library <正式991c库> --output <全新目录>`。

固定来源是既有 companions `af38ee3f5bed09f35c36003b4e33c2cfc8bffa74f9ff5bd891f21f1ce5c54ebd`、`base::scenario.alpha_life`、原玩家 `base::actor.player`。所有原作者、锁及受信文件逐字复制；仅安装索引选择原来的该条记录。可加 `--enabled` 写真实 Main 开发配置。未加时默认关闭。

固定 Godot `4.7.2.stable.official.ed1daf0bf`。新工程先正常建立全局类／tiny资源索引。headless 准备时可暂把扩展描述移出资源根，完成一次 `--headless --editor --import --quit` 后放回；这一步不是扩展冷导入证明。GUI 位使用独立正常桌面路径。正式库 SHA256 必须是 `991c5e09d20b1deb07b15741d839c32259ee888a1af7411d4fe342f9a65546f2`，描述仅声明 4.7.2 Linux debug、不可重载。

## 运行与边界

- 旧源 `91587fed507d7915d35a1e7ac9dc2cfdd5374340`：脚本参数 `--record-oracle=<新绝对路径>`。只有成功的六行实际旧输出才能首次记录。
- 新源：`--native --oracle=<旧oracle绝对路径>`，使用同 catalog 字节核旧 summary、真实 Main 成长控件及 preview。实际收费学艺后修炼两半日，点击原按钮推进一半日。
- 必需错页案例：玩家 wait revision 1 已读，恢复同内容 NPC wait revision 1／玩家 idle。原 advance 仍成功，故原继续按钮必须由新行动输入消失，不能以 preview 兜底。
- `--native --source-contract-only`：未序列化 Factory 字典经原 Schema 合法、原 strict project 仍拒 StringName 键、新 live adapter 应成功。禁止测试先 JSON 往返清洗世界。
- `--missing-extension`：新进程不自动加载扩展，缺扩展失败缓存；测试明确加载后仍保持该代失败，显式 invalidate 后才尝试一次。
- 旧 ownership 仅用 `--ownership-only --content=<同catalog> --scenario=base::scenario.alpha_life` 定点核三权威容器、四私有字段、七 adapter 字段和无永久 Kernel／Callable 环；不重跑原事务长门。

记录真实 project 总时间（包含一次完整 Schema）、encode、install、query 的计数和微秒。重复读取可增加 reads/cache_hits，因此不拿整个 diagnostics 字典相等代替工作次数检查。Schema 静态尾 validator 包装保持原结果，并证明一次构建只有一次全校验。

开放 actor extra float 由原 Schema 明确接纳后用于不可表示反例；失败显示 unavailable 并被缓存，不能读旧 DTO 或返回假 idle。真实 query 的深层 BOM 字符还覆盖 Bridge 拒绝。单字段修改 player 会违反旧继任合同，专测保留 `InvalidSuccessionPlayer` 和失败不失效；没有伪造成年继任来源，合法单字段切 player 未验。该限制不削弱上述真实同 revision 换世界错页门。
