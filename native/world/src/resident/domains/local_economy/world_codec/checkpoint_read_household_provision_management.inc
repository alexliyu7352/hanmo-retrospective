// 地方经济检查点恢复分派：沿原 else-if 链读取 household_provision_management,
// production_disruptions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "household_provision_management") r =
    visit_household_provision_management_checkpoint(reader,
                                                    *household_management);
else if (section.domain == "production_disruptions") r =
    visit_production_disruption_checkpoint(reader, *disruption);
