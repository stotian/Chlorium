"""Read only the requested public level from the local GD save cache."""
from pathlib import Path
import base64
import collections
import json
import zlib
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
target_id = "133175713"
save_dir = Path.home() / "AppData/Local/GeometryDash"


def inflate(text):
    if isinstance(text, str):
        text = text.encode()
    binary = base64.urlsafe_b64decode(text + b"=" * (-len(text) % 4))
    for mode in (31, 15, -15):
        try:
            return zlib.decompress(binary, mode)
        except zlib.error:
            pass
    raise ValueError("unsupported compression")


for save in ("CCGameManager.dat", "CCGameManager2.dat", "CCLocalLevels.dat", "CCLocalLevels2.dat"):
    path = save_dir / save
    if not path.exists():
        continue
    raw = path.read_bytes()
    xml = inflate(bytes(value ^ 11 for value in raw))
    document = ET.fromstring(xml)
    for dictionary in document.iter("d"):
        children = list(dictionary)
        fields = {children[i].text: children[i + 1].text for i in range(0, len(children) - 1, 2) if children[i].tag == "k"}
        if fields.get("k1") != target_id or not fields.get("k4"):
            continue
        encoded = fields["k4"]
        level = encoded if ";" in encoded else inflate(encoded).decode()
        objects = level.split(";")[1:]
        frequencies = collections.Counter()
        combinations = set()
        for entry in objects:
            if not entry:
                continue
            parts = entry.split(",")
            props = dict(zip(parts[::2], parts[1::2]))
            frequencies[props.get("1", "unknown")] += 1
            combinations.add(tuple(sorted(props.get("57", "").split("."))))
        (root / "references/orbit.level").write_text(encoded, encoding="utf-8")
        (root / "references/orbit-decoded.level").write_text(level, encoding="utf-8")
        report = {"id": int(target_id), "name": fields.get("k2"), "objects": sum(frequencies.values()),
                  "group_combinations": len(combinations), "text_objects": frequencies["914"],
                  "most_common_object_ids": frequencies.most_common(12), "encoded_bytes": len(encoded)}
        (root / "references/orbit-profile.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report))
        raise SystemExit(0)
print(json.dumps({"id": int(target_id), "cache_available": False}))
raise SystemExit(2)

