"""Run a tool through wibo, retrying the failures wibo produces under load.

Known flakes, none of which reproduce on a second run:
  * a run that hangs at 0% CPU and never returns (killed after `timeout`);
  * NgcAs/CPP/ngcld failing with "Could not read file" on an input that exists;
  * a run that dies on a signal or exits non-zero without printing anything.
Any other failure carries a diagnostic and is returned as it is, so real compile and link errors are
not delayed. Each retry is reported on stderr so a flaky host stays visible in the ninja output.
"""

import os
import subprocess
import sys
from typing import Dict, List, Optional

ATTEMPTS = 3
TRANSIENT = (b"Could not read file",)


def _transient(rc: int, out: bytes, empty_is_transient: bool) -> bool:
    if rc == 0:
        return False
    if rc < 0 or any(p in out for p in TRANSIENT):
        return True
    return empty_is_transient and not out.strip()


def run(
    cmd: List[str],
    env: Optional[Dict[str, str]] = None,
    timeout: Optional[float] = None,
    empty_is_transient: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess:
    """subprocess.run(cmd) with the retries above. Output is copied to this process's stdout and
    stderr once the final attempt is done, unless `capture` asks for it in the result (bytes)."""
    name = os.path.basename(cmd[1] if len(cmd) > 1 else cmd[0])
    res = subprocess.CompletedProcess(cmd, 1, b"", b"")
    for attempt in range(1, ATTEMPTS + 1):
        try:
            res = subprocess.run(cmd, env=env, timeout=timeout, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except subprocess.TimeoutExpired:
            res = subprocess.CompletedProcess(cmd, 1, b"", f"{name} hung for {timeout}s\n".encode())
            why = f"hung for {timeout}s"
        else:
            if not _transient(res.returncode, res.stdout + res.stderr, empty_is_transient):
                break
            why = f"failed (rc {res.returncode})"
        if attempt < ATTEMPTS:
            print(f"wibo: {name} {why}, retry {attempt}/{ATTEMPTS - 1}", file=sys.stderr)
    if not capture:
        sys.stdout.flush()
        sys.stdout.buffer.write(res.stdout)
        sys.stdout.buffer.flush()
        sys.stderr.flush()
        sys.stderr.buffer.write(res.stderr)
        sys.stderr.buffer.flush()
    return res
