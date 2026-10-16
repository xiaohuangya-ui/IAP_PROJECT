"""Check formal Keil source, include and scatter paths without building."""
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1]
projects = [
    root / "Bootloader/MDK-ARM/Bootloader.uvprojx",
    root / "Application/MDK-ARM/Application_Phase9.uvprojx",
]
projects.extend(sorted((root / 'Tests/keil').rglob('*.uvprojx')))
result = []
for project in projects:
    tree = ET.parse(project)
    for target in tree.findall(".//Target"):
        name = target.findtext("TargetName")
        paths = []
        for node in target.findall(".//FilePath"):
            if node.text:
                paths.append(node.text)
        for node in target.findall(".//IncludePath"):
            if node.text:
                paths.extend(node.text.split(";"))
        scatter = target.findtext(".//ScatterFile")
        if scatter:
            paths.append(scatter)
        missing = []
        for raw in paths:
            expanded = re.sub(r"\$\(.*?\)", "", raw.strip())
            if not expanded:
                continue
            normalized = expanded.replace("\\", "/")
            resolved = (project.parent / normalized).resolve()
            if not resolved.exists():
                missing.append(raw)
        result.append({"project": str(project), "target": name,
                       "paths_checked": len(paths), "missing": missing})
print(json.dumps(result, indent=2))
assert all(not row["missing"] for row in result)
