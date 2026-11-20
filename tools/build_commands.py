#!/usr/bin/env python3
"""Print the PowerShell commands Mark runs, taken from each build script's own Usage line.

Claude pastes this output, never commands written from memory (2026-11-19: commands from memory had a
`git pull` that build_pc.ps1 already does, and called the scripts without -ExecutionPolicy Bypass).

Usage: python3 tools/build_commands.py [--pc] [--switch]   (both by default)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO_DIR = r"C:\S4ep2-XBLA-nx"


def usage(script, flags):
    text = (ROOT / "tools" / "win" / script).read_text(encoding="utf-8", errors="replace")
    m = re.search(r"#\s*Usage \(from [^)]*\):\s*(powershell .*?\.ps1)", text)
    if not m:
        sys.exit(f"STOP: no Usage line in tools/win/{script}; fix the script header before giving commands")
    return (m.group(1) + " " + flags).strip()


def main():
    args = set(sys.argv[1:]) or {"--pc", "--switch"}
    lines = [f"cd {REPO_DIR}"]
    if "--pc" in args:
        lines.append(usage("build_pc.ps1", "-Renut"))
    if "--switch" in args:
        lines.append(usage("build_switch.ps1", ""))
    print("```powershell")
    print("\n".join(lines))
    print("```")


if __name__ == "__main__":
    main()
