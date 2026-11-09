// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto i = decode_native_interaction_seed(source, catalog);
if (!i.ok)
  return {false, {}, i.error};
if (catalog.interaction->visit_messages) {
  auto decoded =
      decode_visit_messages(field(source, "visit_messages", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.visit_messages = std::move(decoded.value);
} else if (source.find("visit_messages"))
  fail("UnexpectedVisitMessageDomain", "visit_messages");
if (catalog.director && catalog.director->visit_reception) {
  auto decoded = decode_visit_reception_claims(
      field(source, "visit_reception_claims", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.visit_reception_claims = std::move(decoded.value);
} else if (source.find("visit_reception_claims"))
  fail("UnexpectedVisitReceptionClaims", "visit_reception_claims");
if (catalog.personal_appointments) {
  auto decoded = decode_personal_appointments(
      field(source, "personal_appointments", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.personal_appointments = std::move(decoded.value);
} else if (source.find("personal_appointments"))
  fail("UnexpectedPersonalAppointmentDomain", "personal_appointments");
if (catalog.contact_inquiries) {
  auto decoded =
      decode_contact_inquiries(field(source, "contact_inquiries", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.contact_inquiries = std::move(decoded.value);
} else if (source.find("contact_inquiries"))
  fail("UnexpectedContactInquiryDomain", "contact_inquiries");
if (catalog.visit_message_replies) {
  auto decoded =
      decode_visit_replies(field(source, "visit_message_replies", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.visit_message_replies = std::move(decoded.value);
} else if (source.find("visit_message_replies"))
  fail("UnexpectedVisitReplyDomain", "visit_message_replies");
if (catalog.born_personal_dialogues) {
  auto decoded = decode_born_personal_dialogues(
      field(source, "born_personal_dialogues", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.born_personal_dialogues = std::move(decoded.value);
} else if (source.find("born_personal_dialogues"))
  fail("UnexpectedBornPersonalDialogueDomain", "born_personal_dialogues");
if (catalog.visit_appointment_invitations) {
  auto a = decode_visit_appointments(
      field(source, "visit_appointment_invitations", "state"));
  if (!a.ok)
    return {false, {}, a.error};
  auto m = decode_visit_appointment_meetings(
      field(source, "visit_appointment_meetings", "state"));
  if (!m.ok)
    return {false, {}, m.error};
  i.value.visit_appointment_invitations = std::move(a.value);
  i.value.visit_appointment_meetings = std::move(m.value);
} else if (source.find("visit_appointment_invitations") ||
           source.find("visit_appointment_meetings"))
  fail("UnexpectedVisitAppointmentDomain", "state");
if (catalog.visit_message_receipts_enabled) {
  auto decoded = decode_visit_message_receipts(
      field(source, visit_message_receipt_module, "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.visit_message_receipts = std::move(decoded.value);
} else if (source.find(visit_message_receipt_module))
  fail("UnexpectedVisitMessageReceiptDomain", visit_message_receipt_module);
if (catalog.known_personal_dialogues) {
  auto decoded = decode_known_personal_dialogues(
      field(source, "known_personal_dialogues", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  i.value.known_personal_dialogues = std::move(decoded.value);
} else if (source.find("known_personal_dialogues"))
  fail("UnexpectedKnownPersonalDialogueDomain", "known_personal_dialogues");
auto face_selected = selected_native_face_to_face_version(catalog.identity);
if (!face_selected.ok)
  return {false, {}, std::move(face_selected.error)};
if (face_selected.value) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::face_to_face_contacts);
  auto face = decode_face_to_face_contact_sources(
      field(source, "face_to_face_contacts", "state"));
  if (!face.ok)
    return {false, {}, std::move(face.error)};
  if (face.value.version != face_selected.value)
    fail("FaceToFaceSelectedRootMismatch", "face_to_face_contacts");
  i.value.face_to_face_contacts = std::move(face.value);
} else if (source.find("face_to_face_contacts"))
  fail("UnexpectedFaceToFaceSources", "face_to_face_contacts");
if (catalog.actor_social_occupancy_selected) {
  auto occupied = decode_actor_social_occupancy(
      field(source, "actor_social_occupancy", "state"));
  if (!occupied.ok)
    return {false, {}, std::move(occupied.error)};
  auto version =
      selected_native_actor_social_occupancy_version(catalog.identity);
  if (!version.ok)
    return {false, {}, std::move(version.error)};
  if (occupied.value.version != version.value)
    fail("ActorSocialOccupancyModuleMismatch", "actor_social_occupancy");
  i.value.social_occupancy = std::move(occupied.value);
} else if (source.find("actor_social_occupancy"))
  fail("UnexpectedActorSocialOccupancy", "actor_social_occupancy");
result.interaction =
    std::make_shared<const InteractionState>(std::move(i.value));
