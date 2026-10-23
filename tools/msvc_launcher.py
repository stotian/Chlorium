"""Normalize localized /showIncludes bytes so Ninja tracks every header."""
import subprocess
import sys


def normalize(line: bytes) -> bytes:
    if line.lstrip().startswith(b"Remarque") and b"inclusion du fichier" in line:
        marker = line.find(b":", line.find(b":") + 1)
        if marker != -1:
            return b"Note: including file: " + line[marker + 1:].lstrip(b" \t")
    return line


def main() -> int:
    child = subprocess.Popen(sys.argv[1:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in child.stdout:
        sys.stdout.buffer.write(normalize(line))
        sys.stdout.buffer.flush()
    return child.wait()


if __name__ == "__main__":
    raise SystemExit(main())
