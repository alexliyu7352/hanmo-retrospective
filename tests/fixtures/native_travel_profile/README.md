# native-travel 开发入口代表门

生产代表脚本：`game/tests/test_native_travel_profile.gd`。输入由共用开发 profile 准备器生成：`native-travel-stage.json` / `HM-NATIVE-TRAVEL-STAGE/1`。本目录不准备第二个完整工程、不复制 runner、不包含人物/作者规则变体。

## 运行前提

- 固定 Godot `4.7.2.stable.official.ed1daf0bf`；检查真实 major/minor/patch/status/build 和完整40位 hash，不能把 build 字段误当引擎 hash。
- 中央只准备一个实际输出，完成正常图形导入与完整来源复核。代表脚本只检查扩展已自动发现，不调用 load_extension 或 editor import。
- 显式只开 travel、关闭 growth，项目 GD 源与 manifest 一致；原生库及六键 travel_build_expected 使用已发表审查记录。
- 使用该输出自己的 storage profile、custom user dir 和可写 XDG；不使用玩家默认存储，也不覆盖新证据路径。

外层 verify 调用：

`godot --headless --path "$GAME" --script res://tests/test_native_travel_profile.gd -- --manifest="$MANIFEST" --output="$NEW_JSON"`

## 实际覆盖

1. 实例化正式 `main.tscn`，验证 Main._ready 的模式配置；再由 `Main.start_content` 完成原内容/表现准入和 Kernel 替换，验证配置没有丢失。
2. 未开局、开局未推进、普通 preview/成长查询均没有旅行提案或 Bridge 创建；growth 工作始终为零。
3. 通过公共 `Kernel.submit` 提交 wait，再 advance 一半日，原作者 walker 必须实际出发。缺 walker/计划/实际出发明确失败，不补内容或放宽规则。
4. 下一次真实 advance 必须取得位置进度并支付本人旅费；出发半日不提前扣下一拍费用。两次成功请求重放均不再次运行 provider 或改状态。
5. 从该实际已用 Bridge 读 audit，核共享构建字段及未安装工件 runtime。audit 的原 profile 不改；旅行输出的 native_travel_schedule 六键由正式 provider 的成功调用验证。
6. 全程核 manifest、catalog、Main/Kernel/Backend/provider/扩展/库输入摘要未变，并记录独立存储目录。

Main 的 `_perform("wait")` 会安排 deferred 自动推进，本代表门故意不调用它。这里核真实 Main 配置/换 Kernel 与公共 Kernel 行为，不能声称已经复验 GUI 按钮或自动操作体验。

## 无资源微门

`HANMO_GODOT_EDITOR=/absolute/fixed/Godot python -m unittest tests.test_native_travel_profile_runtime -v`

微门只复制代表GD、`check_contract.gd` 和空 project，设置独立可写 XDG，不导入库或完整资源。实际固定 Engine 下32个输入合同断言已通过（含manifest JSON整数与原生ABI整数的精确比较）；它只证明脚本可解析及模式/来源/build/storage/Engine形状拒绝，不能替代上面的真实Main代表运行。未提供固定引擎时 Python 测试会明确跳过。

原旅行等价/原子性231、缺库14、legacy77证据保持冻结，不由此脚本重复。A/B性能、GUI、保存/迁移和全世界原生权威不在本门。

## 实际ABI类型纠正

首次正式代表门62/2原证据保留：Godot JSON将manifest的64/32解析为TYPE_FLOAT，真实Bridge.audit返回TYPE_INT。小型只读真实库探针已确认两端数值完全相等；代表门仅对pointer_bits/real_t_bits接受有限、精确整数值的manifest数字，实际Bridge仍必须TYPE_INT。错误数值、字符串、bool、小数、NaN/Inf及原生float都拒绝，其余build字段仍同型同值。新增微门先复现32/3，再修至32/0；正式stage同步/复验由中央单独进行。
