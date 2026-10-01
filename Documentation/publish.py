import subprocess
import os
import sys

base_dir = os.path.dirname(os.path.abspath(__file__))

if os.name == "nt":
    make_bat = os.path.join(base_dir, "make.bat")
    subprocess.run([make_bat, "clean"], shell=True, cwd=base_dir)
    subprocess.run([make_bat, "html"], shell=True, cwd=base_dir)
else:
    subprocess.run(["make", "clean", "html"], cwd=base_dir)

os.chdir(os.path.join(base_dir, "build", "html"))

subprocess.run(["git", "init", "--initial-branch=main"])
subprocess.run(["touch", ".nojekyll"])
subprocess.run(["git", "add", "."])
subprocess.run(["git", "config", "user.email", "landscapecombinator@proton.me"])
subprocess.run(["git", "config", "user.name", "Landscape Combinator"])
subprocess.run(["git", "commit", "-m", "Documentation"])
subprocess.run(["git", "push", "-f", f"git@{sys.argv[1]}:LandscapeCombinator/LandscapeCombinator.git", "main:doc"])