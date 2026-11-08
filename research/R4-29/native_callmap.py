"""Static navigation of the measured GD image. Does not execute game routines."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "build-research/python"))
import capstone
import pefile
from pe_ranges import function_ranges

GAME = Path("C:/Users/stoti/Chlorium/test-game/GeometryDash.exe")
BINDINGS = Path("C:/Users/stoti/Chlorium/build/bindings/bindings/Geode/source")


def analyze(provenance):
    raw = GAME.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != provenance["game_sha256"].lower():
        raise ValueError("Static image differs from the measured game")
    pe = pefile.PE(data=raw)
    try:
        if pe.FILE_HEADER.Machine != 0x8664 or pe.FILE_HEADER.TimeDateStamp != 0x69702DA0:
            raise ValueError("Unsupported GD image")
        image = pe.get_memory_mapped_image()
        symbols = {}
        sources = {}
        for filename in ("PlayLayer.cpp", "GJBaseGameLayer.cpp", "GameObject.cpp", "EnhancedGameObject.cpp", "EffectGameObject.cpp", "AnimatedGameObject.cpp", "GJEffectManager.cpp", "GameToolbox.cpp"):
            path = BINDINGS / filename
            if not path.is_file():
                continue
            text = path.read_text(encoding="utf-8")
            for match in re.finditer(r"^auto ([\w:]+)\([^\n]*\) ->[^\n]* \{\n(.*?)^\}", text, re.M | re.S):
                address = re.search(r"base::get\(\) \+ (0x[0-9a-fA-F]+)", match[2])
                if address:
                    symbols.setdefault(int(address[1], 16), set()).add(match[1])
            sources[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        imports = {entry.address - pe.OPTIONAL_HEADER.ImageBase:
                   dll.dll.decode("ascii", errors="replace") + "!" + (entry.name.decode("ascii", errors="replace") if entry.name else "ordinal_" + str(entry.ordinal))
                   for dll in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []) for entry in dll.imports}
        decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        decoder.detail = True
        ranges = function_ranges(pe)
        functions = []
        for start, name in ((0x3AF3A0, "PlayLayer::updateVisibility"), (0x22DD50, "GJBaseGameLayer::moveObjects")):
            fragments = [(begin, end) for begin, end, owner in ranges if owner == start]
            if not fragments or not any(begin == start for begin, _ in fragments):
                raise ValueError("Missing native procedure ownership")
            instructions, undecoded = [], []
            for begin, end in fragments:
                decoded = list(decoder.disasm(image[begin:end], begin))
                consumed = sum(instruction.size for instruction in decoded)
                if consumed != end - begin:
                    undecoded.append([hex(begin + consumed), hex(end)])
                instructions.extend(decoded)
            def inside(address):
                return any(begin <= address < end for begin, end in fragments)
            backwards = [(i.operands[0].imm, i.address) for i in instructions
                         if i.mnemonic.startswith("j") and i.operands and i.operands[0].type == capstone.x86.X86_OP_IMM
                         and inside(i.operands[0].imm) and i.operands[0].imm < i.address]
            external_transfers = [{"site_rva": hex(i.address), "target_rva": hex(i.operands[0].imm)} for i in instructions
                                  if i.mnemonic.startswith("j") and i.operands and i.operands[0].type == capstone.x86.X86_OP_IMM
                                  and not inside(i.operands[0].imm)]
            calls = []
            for instruction in instructions:
                if instruction.mnemonic != "call":
                    continue
                operand = instruction.operands[0]
                call = {"site_rva": hex(instruction.address), "in_backward_branch_span": any(a <= instruction.address <= b for a, b in backwards)}
                if operand.type == capstone.x86.X86_OP_IMM:
                    call.update(kind="direct", target_rva=hex(operand.imm), names=sorted(symbols.get(operand.imm, [])))
                elif operand.type == capstone.x86.X86_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                    slot = instruction.address + instruction.size + operand.mem.disp
                    call.update(kind="indirect_rip", import_slot_rva=hex(slot), name=imports.get(slot, "unresolved"))
                else:
                    call.update(kind="virtual_or_indirect", operand=instruction.op_str)
                calls.append(call)
            functions.append({"name": name, "rva": hex(start),
                              "fragments": [{"begin_rva": hex(begin), "end_rva": hex(end), "sha256": hashlib.sha256(image[begin:end]).hexdigest()} for begin, end in fragments],
                              "decoding_complete": not undecoded, "undecoded_ranges": undecoded,
                              "transfers_outside_procedure": external_transfers,
                              "backward_branch_spans": len(backwards), "call_sites": calls})
        return {"static_only": True, "game_sha256": digest, "generated_binding_source_hashes": sources,
                "limits": "Call sites and backward branch spans are not dynamic frequencies, purity proofs or permission to suppress callbacks.",
                "functions": functions}
    finally:
        pe.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("provenance", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = analyze(json.loads(args.provenance.read_text(encoding="utf-8-sig")))
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    for function in result["functions"]:
        calls = function["call_sites"]
        print(function["name"], len(function["fragments"]), "owned fragments;", len(calls), "static call sites:", dict(Counter(call["kind"] for call in calls)), "decoded:", function["decoding_complete"])
