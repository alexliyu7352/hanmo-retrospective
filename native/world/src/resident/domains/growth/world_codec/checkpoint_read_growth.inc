// 成长检查点恢复分派：沿原 else-if 链读取
// growth；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "growth") r =
    catalog.exact_time && exact_complete_selected(*catalog.exact_time)
        ? (catalog.growth->complete_capability_application_chain
               ? visit_growth_trade_grant_checkpoint(reader, *growth,
                                                          *catalog.growth)
           : catalog.growth->complete_exact_rest_mouth_sources
               ? visit_growth_exact_rest_mouth_checkpoint(reader, *growth,
                                                          *catalog.growth)
           : catalog.growth->complete_exact_command_cuts
               ? visit_growth_exact_command_cut_checkpoint(reader, *growth,
                                                           *catalog.growth)
               : visit_growth_exact_period_checkpoint(reader, *growth,
                                                      *catalog.growth))
    : catalog.exact_time
        ? visit_growth_fixed_service_checkpoint(reader, *growth,
                                                catalog.regional_life,
                                                catalog.growth->cards.enabled)
        : visit_growth_checkpoint(reader, *growth, catalog.regional_life,
                                  catalog.growth->cards.enabled);
