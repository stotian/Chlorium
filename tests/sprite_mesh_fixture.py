"""Generate C++ test data directly from the shipped sprite-outline JSON."""
import json
import pathlib
import sys

source, output = map(pathlib.Path, sys.argv[1:])
meshes = json.loads(source.read_text(encoding="utf-8"))
lines = ["static void checkBundledMeshes() {"]
for name, polygons in meshes.items():
    lines.append("    { ConvexList list;")
    for polygon in polygons:
        points = ", ".join(
            "{" + repr(float(x)) + "f, " + repr(float(y)) + "f}"
            for x, y in polygon
        )
        lines.append("        { const std::vector<glm::vec2> points = {" + points + "};")
        lines.append("          const auto parsed = makeConvexSpritePolygon(points);")
        lines.append("          require(parsed.has_value(), " + json.dumps("bundled outline: " + name) + ");")
        lines.append("          list.addPolygon(*parsed); }")
    lines.append("      checkExactTriangles(list); }")
lines.append("}")
output.write_text("\n".join(lines) + "\n", encoding="utf-8")
