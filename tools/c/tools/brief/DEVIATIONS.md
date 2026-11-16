# brief: accepted differences from tools/brief.py

Proven equal to the retired Python tools/brief.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-brief-1. The classification runs the C wall, not wall.py
- Behaviour: for a compiled, non-exact function without a current build/walls.json record (and without
  `--no-compile`), the C tool runs `build/tools/wall.exe <unit> <symbol> --json --root <root>` (next to brief.exe)
  where the Python ran wall.py as a subprocess.
- Reason: wall's port is proven
  by its own harness, so the verdict line is the same; a TimeoutExpired text names the C command line.

## D-brief-2. Sections are gathered one after the other
- Behaviour: the Python runs wall.py and unit_plan.plan_for in background threads while it gathers the other
  sections; the C tool runs them in turn, inside the section that reads them.
- Reason: the output is the same (each result is read in the same place); only the timings differ, and they are
  wall-clock numbers that the harness normalises (`"seconds"`, `"timings"`, `# timings:`).

## D-brief-3. unit_plan's disk cache is the C one
- Behaviour: the start section of an unstarted function reads unit_plan's cache build/index/unit_plan.cbin (the C
  port's format) where the Python reads build/index/unit_plan.pickle. With `--no-compile` a side whose cache is
  missing or stale shows no proposed unit; without it each side builds its own cache once.
- Reason: unit_plan's port keeps its own cache format (tools/c/tools/unit_plan); both caches hold the same model, so
  the proposed unit is the same once both are current.
