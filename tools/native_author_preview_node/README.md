# 原生作者试玩真实流程窄门

`run_node.py` 通过当前 `authoring_worker.py` 的 HM-AUTHOR/1 行协议打开青川1.8的声明文件副本，改测试副本章名，保存、compose、preview_prepare。请求完整保留 document/revision/source guard。按原面板先 headless import，再以固定4.7.2编辑器运行真实 Main；只触发当前可见按钮的原信号。

`--expect-missing-backend` 用于原失败，要求保存与试玩准备成功后，真正选本人确认得到实际 `UnsupportedNativeCapability`（Bridge类不存在）。修后提供 `--native-stage`，同一路径进入沈蘅原貌、执行一次可见本地移动、返回标题并正常退出。这个窄门不执行历史沈蘅交易、拜师和切磋链，不写正式作者包或旧银行。

所有输出要求全新目录；作者原字节、当前预览输入、编辑器身份和原协议均保留。作用范围仅为 headless 控件信号，不代表GUI鼠标或像素验收。

`--expect-preview-error CODE` 只认可worker实际返回的指定拒绝，并检查目标不存在，绝不启动Main。`--legacy-gd` 用已有非dynamic_world作者包检查正常标题入口仍装载GD世界并退出；不复制旧军事/交易测试。

示例参数：

```sh
source <本地路径>
python3 tools/native_author_preview_node/run_node.py \
  --author-pack /path/to/content/packs/authored/local_society_1_8 \
  --output /new/isolated/output \
  --editor "$GODOT_BIN" \
  --native-stage /verified/native-resident-stage/game
```

原红前稀疏补齐仅四个已追踪blob，逐项核对2578175索引：

- game/assets/audio/ambience.wav：88f4a3526307951c95dc258349b73cfc99d38850，既有只读stage
- game/assets/audio/paper.wav：400721661c8b467f41794999859b45fa3a6cef11，既有只读stage
- game/assets/audio/LICENSE.json：1981891d7b7774daff026a1a614da40185a7fe34，已有Git对象
- game/character_studio/src/studio_session.gd：1dd32474b4bf158d96cd426ae60b395bc3db1dcf，既有只读stage

首次driver漏掉面板既有导入步骤，原始输出 `native-author-original-red-01` 因HMStrictJSON类缓存未生成而停止，不算原产品失败证据。随后补齐同面板导入，保留首份失败且使用新目录。

`native-author-original-red-02` 已经完成真实选本人并得到 `内容尚不能运行：UnsupportedNativeCapability`，Bridge=false、started=false、world={}。此原件的driver最初按计划预期MissingNativeBackend，断言因此失败；另Godot正常导入产生 `.uid`，原清单对比错误地将新增视为输入变更。原始报告不改写。后续driver按观察到的真实错误码断言，原输入逐文件核摘要，新增引擎文件单独列明。

## 2026-10-08冻结8d6773真实结果

- `native-author-green-01`：五次真实worker请求全部成功。Main正常选沈蘅原貌，Bridge真实注册，单次原 `move_local` 回执成功，revision 0→1，tick维持0；本人未变，返回标题确认后正常离开，进程0。
- `native-author-legacy-gd-01`：既有军事预览作者fixture同协议保存、compose与preview，无dynamic_world，正常标题启动仍是GD世界（Bridge=false、native节点0），正常退出0。不执行旧军事规则链。
- `native-author-no-stage`：实际协议拒绝MissingPreviewNativeRuntime，目标未发布，未启动Main。
- `native-author-changed-stage`：仅在独立stage副本给描述符追加注释，实际协议拒绝PreviewNativeIdentityMismatch，目标未发布，未启动Main。原stage五份身份文件末尾重新核摘要未变。

四项原件见 `evidence/verification-summary.json` 的精确清单。原作者文件和已发布preview全部输入逐文件核对未变；Godot新增UID单列，不混同源码变更。SO165复用，不重编。所有真实过程仅限headless控件信号，GUI仍未验收。
