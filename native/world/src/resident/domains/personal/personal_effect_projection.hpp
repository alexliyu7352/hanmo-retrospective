#pragma once
#include "personal_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {Status emit_personal_receipt_fields(ProjectionWriter&,const PersonalReceipt&);}
