// Original JSON report/knowledge rows. No BoundaryValue tree or world clone.
#pragma once
#include "social_model.hpp"
namespace hanmo::resident {
Outcome<OrderedTable<Report>> decode_dynamic_social_reports_json(const BoundaryValue&);
Outcome<OrderedTable<OrderedTable<KnownReport>>> decode_dynamic_social_known_json(const BoundaryValue&);
}
