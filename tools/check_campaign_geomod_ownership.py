"""Text-only installed L1S1 editor framing and scripted-blast owner check."""
from locate_campaign_geomod_surface import locate
from probe_campaign_geomod_brush import U32, parse_record, section_bytes


data, _ = section_bytes("L1S1.rfl")
count = U32.unpack_from(data)[0]
offset = 4
sidecars = []
for _ in range(count):
    brush = parse_record(data, offset, U32.unpack_from(data, offset)[0])
    if brush["opaque_bytes"]:
        sidecars.append((brush["uid"], brush["opaque_bytes"]))
    offset += brush["bytes"]
assert count == 160 and offset == len(data) == 649126
assert sidecars == [(8523, 960), (8524, 960), (8543, 960), (8544, 960), (21, 1056)]

positive = locate("L1S1.rfl", 28, (36.7616, 4.0061, 44.4250), 3)
assert positive["compiled_faces_in_room"] == 411
assert [(row["face"], row["owner_uid"], row["source_word"])
        for row in positive["nearest"]] == [(3766, 8755, 4395),
                                           (3764, 8755, 4396),
                                           (3765, 7778, 3328)]
negative = locate("L1S1.rfl", 33, (71.9004, 7.6876, 30.8536), 1)
assert (negative["nearest"][0]["owner_uid"], negative["nearest"][0]["source_word"]) == (8756, 4409)
print("PASS L1S1 brush framing, five opaque sidecars, and both scripted-blast surface owners")
