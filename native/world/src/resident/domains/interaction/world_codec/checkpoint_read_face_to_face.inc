// 独立 Face3 段只恢复原 Interaction 拥有根，重复、缺选和版本不符均拒绝。
else if (section.domain == "face_to_face_contacts") {
  auto selected = selected_native_face_to_face_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value != 3 || section.codec_version != 3 ||
      i->face_to_face_contacts)
    return {false,
            {},
            common_failure("CheckpointFaceToFaceSelectionMismatch").error};
  i->face_to_face_contacts.emplace();
  r = visit_face_to_face_contact_sources_checkpoint(reader,
                                                    *i->face_to_face_contacts);
  if (r.ok && i->face_to_face_contacts->version != selected.value)
    r = common_failure("FaceToFaceSelectedRootMismatch");
}
