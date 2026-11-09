#pragma once
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
enum class ReportDisclosureBasisKind { public_report,original_participant,original_business_permission };
struct ReportDisclosurePermission {ReportDisclosureBasisKind kind=ReportDisclosureBasisKind::public_report;MomentOwnerSource original_source;};
}
