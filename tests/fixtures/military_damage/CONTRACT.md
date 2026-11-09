# 当前野战伤害纯叶与显式候选乘区

正式对照为 `05_duel_and_warfare.md` §5.7–5.8，疲劳档为 `03_people_growth_cards.md` §3.3。本片只解决当前字段可实现的伤害乘区；完整统率、兵种技能、训练、部队疲劳攻守势公式未实现，不默认填50/0或借将领疲劳冒充部队疲劳。

## A：兼容旧行为

`military_round.gd` 保留目标/射界/战法合法性、RNG、协防、伤亡分类；只将数学投影委托 `domains/military/military_damage.gd`。

旧内容缺省模型 `{}` 或 `{kind: legacy_v1}` 严格沿原算法：ceil人数×attack → 每阶段最低1及逐次整数截断（士气、战法、弓贴身、defense+guard、佯退）→目标上限；原命中近似与实际随机余数取整继续由既有流程消费。没有全局默改内容或旧RNG。

测试先在真实旧 `27e5bc0` 字节捕获14个有限病例的预览/完整结算，提取后逐字段及RNG比较。捕获JSON仅本地，非历史黄金存档、非提交内容；测试也不复制另一套旧公式当oracle。

## B：有限显式新模型

每个 battlefield 可由中央schema启用 `damage_model`：

- `kind: legacy_base_factors_v1`
- `side_bp` / `rear_bp`：10000–18000，100整数倍，rear不得低于side
- `forest_ranged_bp` / `hill_melee_bp`：4000–10000，100整数倍
- `min_total_bp: 4000` / `max_total_bp: 18000` 固定

新模型必须全部字段齐备且无额外键。未知模型、错类型、越界/非100整数倍、缺字段都拒绝；不是任意表达式/脚本。正式候选测试采用侧11000、背12000、林8000、丘9000。

数学顺序：

1. 用现 `ceil(N×troop.attack/100)` 和原 `defense+guard` 形成基础，仍最低1
2. 士气 `5000+50×morale`、战法power、贴身50%、佯退110%、侧后、适用地形六个因子以整数有理数约分合成
3. 在合成比值上夹40%–180%，再对基础减员乘算，最终一次向下取整/最低1/目标有效人数上限

新模型不再用侧后增加命中；仅在远射林地/近战丘陵实际应用地形伤害因子时，该cover不再同时扣命中。近战林地、远射丘陵及其他作者cover和其余原命中项保持。普通defend原guard只在基础内；BA02付费固守差异未定，未另加80%固守或造重复卡。原伤亡比例、士气后果和随机余数消费仍保留，不能称完整公式/平衡已完成。

纯接口 `quote(attacker, defender, context, model={})`：

- attacker：fit_count、attack、accuracy、morale、inspired、suppressed、cavalry_in_forest
- defender：fit_count、defense、guard、cover、exposed、terrain
- context：facing、height_advantage、ranged、close_combat、power_bp、accuracy_bonus、incoming_damage_bp
- 返回原 `{ok, hit_chance, potential, flank}`；失败 `{ok:false, code, path}`

完整公式以后需真实 `combat_stats` 的统率/派生兵种技能/训练/部队疲劳与来源快照。真实training/fatigue生产链尚缺，刷新按可信开战快照方向另由中央冻结。模型与模块/存档格式登记、作者编译由中央负责。本片不直接改共享Schema。

## 局部证据范围

固定4.7.2：A旧基线对齐44项；B同门78项含上述44与34个新模型/坏参数/实际Round/保存计划重放断言。B首次1红是夹具把距1步兵近战误当远射林地，改为真实距2弓射后全通过，生产数学不改，首红日志保留。未跑GUI、全量、整套旧战法或完整v2链。

## C：可信城墙来源的独立第七因子

`quote` 追加第五个可选参数 `source_factors={}`，仅允许 `{siege_wall_bp: int}`，范围10000–30000（沿既有wall_defense_bp，允许非整百），缺省10000。该参数不是作者damage_model或存档字段；中央桥须先用FortificationContext的真实source和实际攻击单位调用 `factor(context, unit_id, ranged)`，再传其factor_bp。墙/名单/守方资格归来源叶，不在伤害叶重建第二套判定。

只有显式B模型且Round已经确认的远射消费该来源；legacy及非远射保持中性。其作用加入同一有理乘积、40%–180%夹界和最终取整，不借战法power_bp，不在潜在减员结果后再乘。未知来源字段、坏类型/范围拒绝，不能静默吞掉。

七因子中的任意整数墙值会扩大分母；夹界比较用等价2/5和9/5，最终用13个二进制位精确乘除避免基础×大分子溢出。基础人数/攻击上限保证基础≤4096，约分后本次固定七因子分子及比较、余数步骤均处于int64内；未来新增因子必须重新证明边界。测试含直接乘积14284300422390103017超过int64的合法例，以及Python Fraction独立参考1939的非整百例。

C本地窄门18项通过，随后同源A+B78项再次通过（C改变候选乘除实现，故核中性回放必要）。来源真实守方/非守方、有效墙/毁墙/相邻/多墙判定由S独立门负责，不能把此纯投影数学夹具冒称完整攻城或中央桥已验。
