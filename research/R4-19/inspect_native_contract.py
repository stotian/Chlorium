"""Read-only contract inventory; never loads GD or writes proprietary bytes."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--game", type=Path, default=Path("C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/GeometryDash.exe"))
parser.add_argument("--dependencies", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if args.dependencies:
    sys.path.insert(0, str(args.dependencies.resolve()))
import capstone
import pefile

pe = pefile.PE(str(args.game))
if pe.FILE_HEADER.Machine != 0x8664 or pe.FILE_HEADER.TimeDateStamp != 0x69702DA0 or pe.OPTIONAL_HEADER.SizeOfImage != 0xA3A000:
    raise SystemExit("Unsupported GD identity")
data = pe.get_memory_mapped_image()
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
decoder.detail = True
functions = [
    ("resetAreaObjectValues", 0x227C30, 0x279, 0xDE24CD39A33793B3),
    ("updateAreaObjectLastValues", 0x227EB0, 0x94, 0x7E900F672011E2D2),
    ("moveAreaObject", 0x22AAB0, 0xE1, 0x01DE6F3EDBA821E9),
    ("transformAreaObjects", 0x229720, 0x662, None),
]
report = {"timestamp": hex(pe.FILE_HEADER.TimeDateStamp), "image_size": hex(pe.OPTIONAL_HEADER.SizeOfImage),
          "game_launched": False, "native_readers_globally_resolved": False,
          "native_suppression_authorized_by_this_report": False, "functions": []}
for name, rva, length, expected in functions:
    body = bytes(data[rva:rva+length])
    fingerprint = 14695981039346656037
    for value in body:
        fingerprint = ((fingerprint ^ value) * 1099511628211) & ((1 << 64) - 1)
    if expected is not None and fingerprint != expected:
        raise SystemExit(f"Body fingerprint mismatch: {name}")
    instructions = list(decoder.disasm(body, rva))
    starts = {i.address for i in instructions}
    if sum(i.size for i in instructions) != length or instructions[-1].mnemonic != "ret":
        raise SystemExit(f"Incomplete instruction coverage: {name}")
    branches, calls, rip_operands = [], [], []
    for instruction in instructions:
        operand = instruction.operands[0] if instruction.operands else None
        if instruction.group(capstone.CS_GRP_JUMP):
            if operand.type != capstone.x86.X86_OP_IMM or operand.imm not in starts:
                raise SystemExit(f"Unresolved branch: {name} {instruction.address:x}")
            branches.append(hex(instruction.address))
        if instruction.mnemonic == "call":
            classification = "unresolved_native_barrier"
            if name != "transformAreaObjects":
                if operand.type == capstone.x86.X86_OP_IMM:
                    classification = {0x227EB0: "relocated_last_values", 0x227C30: "relocated_reset",
                                      0x12D2A0: "forbidden_growth_trap"}.get(operand.imm)
                elif operand.type == capstone.x86.X86_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RAX:
                    classification = {0x490: "fixture_section_observer", 0x508: "unreachable_update_false_scale",
                                      0x520: "unreachable_update_false_rotation"}.get(operand.mem.disp)
                else:
                    classification = None
                if not classification:
                    raise SystemExit(f"Unclassified executed-body call: {name} {instruction.address:x}")
            calls.append({"site": hex(instruction.address), "operand": instruction.op_str, "classification": classification})
        for source in instruction.operands:
            if source.type == capstone.x86.X86_OP_MEM and source.mem.base == capstone.x86.X86_REG_RIP:
                target = instruction.address+instruction.size+source.mem.disp
                classification = "unresolved_native_barrier"
                if name != "transformAreaObjects":
                    if instruction.address != 0x227D73 or target != 0x622C24 or data[target:target+4] != b"\x00\x00\x80\x3f":
                        raise SystemExit("Unclassified executed-body RIP operand")
                    classification = "relocated_float_one_constant"
                rip_operands.append({"site": hex(instruction.address), "target": hex(target), "classification": classification})
    report["functions"].append({"name": name, "rva": hex(rva), "length": length,
        "fnv64": hex(fingerprint), "sha256": hashlib.sha256(body).hexdigest(),
        "linear_instruction_coverage": True, "branch_targets_at_instruction_boundaries": True,
        "instruction_count": len(instructions), "branch_count": len(branches), "calls": calls, "rip_operands": rip_operands})
args.output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
print("PASS 4 complete native-body inventories; closed jumps; calls remain explicit barriers")
