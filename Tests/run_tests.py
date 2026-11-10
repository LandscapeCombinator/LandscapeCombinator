"""Generates the actors tagged LCTest_Quick (and LCTest_Full with --full) in each map and fails on errors.
Usage: python Tests/run_tests.py <engine folder> [--full] [--nobuild]"""
import os, platform, re, subprocess, sys
from pathlib import Path

MAPS = ["/LandscapeCombinator/L_LandscapeCombinatorExamples", "/LandscapeCombinator/L_PositionBased_France"]

os.system("")  # enables ANSI colors in the Windows console
RELEVANT = re.compile(r"Starting Generation with|Generation for")

def show(line):
    if "libcurl info message" in line: return
    line = re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line.rstrip())
    color = "31" if ": Error: " in line else "33" if ": Warning: " in line else "36" if RELEVANT.search(line) else None
    if color: print(f"\033[{color}m{line}\033[0m", flush=True)

system = platform.system()
engine = ([a for a in sys.argv[1:] if not a.startswith("--")] or sys.exit("usage: python Tests/run_tests.py <engine folder> [--full] [--nobuild]"))[0]
editor = Path(engine) / "Engine/Binaries" / {"Windows": "Win64", "Darwin": "Mac"}.get(system, "Linux") / ("UnrealEditor-Cmd" + (".exe" if system == "Windows" else ""))
here = Path(__file__).resolve().parent
uproject = next(p for d in here.parents for p in d.glob("*.uproject"))
tag = "LCTest_Full" if "--full" in sys.argv else "LCTest_Quick"

if "--nobuild" not in sys.argv:
    batch = Path(engine) / "Engine/Build/BatchFiles" / {"Windows": "Build.bat", "Darwin": "Mac/Build.sh"}.get(system, "Linux/Build.sh")
    target = next(uproject.parent.glob("Source/*Editor.Target.cs"), None)
    target = target.name.split(".")[0] if target else uproject.stem + "Editor"
    plat = {"Windows": "Win64", "Darwin": "Mac"}.get(system, "Linux")
    if subprocess.run([str(batch), target, plat, "Development", f"-Project={uproject}", "-WaitMutex"]).returncode: sys.exit("build failed")
failed = 0
for map_ in MAPS:
    cmd = [str(editor), str(uproject), f"-ExecCmds=py {(here / 'lc_inner.py').as_posix()} {map_} {tag}", "-unattended", "-DisablePlugins=MetaHuman,MetaHumanCalibrationDiagnostics,MetaHumanCharacterUAF,MetaHumanRuntime,MetaHumanGenerator,MetaHumanBodyTracker,MetaHumanAnimationTools,MetaHumanCalibrationProcessing,MetaHumanCharacter,MetaHumanCoreML,MetaHumanCoreTech,MetaHumanCrowd,MetaHumanLiveLink,MetaHumanSDK", "-nosplash", "-nullrhi", "-stdout", "-FullStdOutLogOutput"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    errors, pending, done = [], set(), None
    for line in proc.stdout:
        show(line)
        if ": Error: " in line: errors.append(line.strip())
        if m := re.search(r"LCTEST_TRIGGER (.+)", line): pending.add(m[1].strip())
        if m := re.search(r"Generation for (.+) (finished|failed)", line): pending.discard(m[1].strip())
        if m := re.search(r"LCTEST_DONE (\d+)", line): done = int(m[1])
        if done is not None and not pending: break
    if system == "Windows": subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)], capture_output=True)
    proc.kill()
    if done == 0: errors.append(f"no actor tagged {tag}")
    if done is None or pending: errors.append(f"editor exited early, still pending: {sorted(pending)}")
    print("FAIL" if errors else "PASS", map_)
    for e in errors[:20]: print("   ", e)
    failed += bool(errors)
sys.exit(failed)
