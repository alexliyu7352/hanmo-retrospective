# 伤兵归营与北营医馆

显式作者源 `woundcare::scenario.northern_recovery`，依赖原 `hanmo.field_appointments 1.0.0`、`hanmo.inland_fleet 1.2.0`、`qingxi.campaign 1.2.0`。原军令、兵员、公库、编军与舟师初态来自同一已冻野外接任场景；本包不预造伤亡或回队事实。

新增原创医者何安在北营提供原 `service/rest`，二十钱、两半日，实际医馆履约每半日提供两点人物康复。人物康复启用 `personal_injury_recovery/1`，需要完整 S4 模块。军事恢复启用 `military_wounded_recovery/1`，沿 Military8 原战伤批、实际军粮与安全驻营累计，必须四个合格恢复日才按批次回队；人物康复不替代军事供给，也不授军职。

原钱粮没有为验证额外增补。缺粮、动员、行军、交战或围城会暂停军事恢复；不得通过等待时长补算合格日。具名军官不重复塞入兵卒名册；归营已退役的庄园亲兵由原退役规则处理。

状态：作者 SOURCE_ONLY。完整 S4/S2 配对未正式注册前不作正式 compose/Installed 准入或自然战后银行通过声明。
