# A2 R：原生 Storage 物理选代小片

基线为 `eb375420173c97fa6ce3da708572002d1980ec86`。本片只修改 `game/storage/native_storage.gd`、新增 `game/tests/test_storage_slot_inspection.gd` 及本说明/局部回执；没有修改 Codec、Strict、StorageService、Main 或 Kernel。

## 接口与安全边界

`NativeStorage.inspect_slot(slot)` 只读真实 A/B 银行，调用正式 `Codec.inspect_metadata`。成功返回原 `text`、`metadata`、`generation`、`snapshot_generation`、`suffix` 与 `recovered`；不会检查已安装内容、当前引擎/规则或解正文。

- 按最高合法物理 generation 选择，与银行字母无关
- 同 generation、相同原 text 可固定选 A；同 generation、不同原 text（包括仅空白不同）报 `GenerationConflict`，不猜赢家
- 只有 Codec 明确 `physical_corruption:true` 可跳过坏银行，选择另一真实有效代并标记 recovered。`corrupt_generations` 每项只有 `{suffix,code}`，不猜坏封装的 generation
- 任一未知 future、资源限制或其他非物理错误立即停止，即使另一银行可读也不能降代。超限银行只检查文件长度，不整份读取
- 合法封装的 save_id 与请求槽名不符报 `SlotIdentityMismatch`，不能用旧银行掩盖放错槽的文件
- 全部已存在银行均明确物理坏才报 `CorruptSlot`；全不存在才报 `NotFound`。坏封装或未来格式不附猜出的 generation；列表沿旧调用约定缺 generation 时显示 0
- 槽命名改为完整终锚，`manual-1\n`/CRLF、路径及别名不能成为另一文件入口

`get_slot(slot)` 保留旧公共调用，先 inspect，只 decode 已选中的最高物理代。语义失败返回原错误，不转为 CorruptSlot，也不再尝试另一代。这样原 `put` 的“验证旧最高代再写另一银行”继续阻止覆盖语义不可读的最新代；get 语义失败不会被误当作空槽。

`list_slots()` 只走 inspect，保留 `slot/ok/generation/code`，成功项另附 metadata/recovered；缺包或不兼容内容仍显示选中代的身份。StorageService 的路由及 UI 尚未接新的完整恢复顺序。

## 实际局部验证

固定 Godot `4.7.2.stable.official.ed1daf0bf`，通过 `tools.build.run_checked` 执行：

- 合同骨架阶段 101 断言、34 预期失败；核心实现后 102/102；补超大小银行和坏代诊断字段后 `test_storage_slot_inspection.gd` **111/111**
- 相邻 `test_storage.gd` **86/86**，含旧双代、实际 v1 decode 与跨进程恢复；保留原极端指数的三条 Warning，无 SCRIPT ERROR/ERROR
- `python -m pytest -q tests/test_storage_profiles.py` **1 passed**（8.18 秒）
- 新源码/函数中文职责说明检查通过，`git diff --check` 通过

新测试创建进程专属 profile 和真实 A/B 文件，覆盖两种银行顺序、单银行、空槽、物理坏代、未知 future opaque、TooDeep、超大小稀疏文件、相同 generation 冲突、SlotIdentityMismatch、最新引擎/规则不兼容、list 元信息，以及 get 语义失败后 put 不覆盖。成功、失败和拒绝写入前后的所有原银行文件 SHA 保持一致；测试只清理自己创建的 profile。

缺包用例让旧 v1 代在真实已验证 alpha 目录上可 decode，再把新 v2 物理代选中并交真实 Resolver.resolve(metadata, [], registry)，得到 MissingPack；不降到旧代，银行字节保持不变。v2 正文是明确合成的 opaque 负例，不是历史原档，也不是完整恢复证明。

原历史 v1 fixture 与之前元信息/Resolver 合同保留不变。原始红绿输出与相邻门回执在独立工区 `verification/a2-resolver/`，清理工作树前须保全。

当前仍为 `M3_FULL_RESTORE=NOT_RUN; V2_SAVE_WRITE=NOT_RUN; UI_CONTINUE=NOT_RUN`。没有运行全量、长跨度或 Web 实际验收。
