"""Bounded, best-effort cleanup for compiler/probe scratch directories.

Cleanup failures report residual paths without replacing compilation, probe,
negative-control or interruption results. No production code uses this helper.
"""
from collections.abc import Iterator
from contextlib import contextmanager
import json
from pathlib import Path
import sys
import tempfile
import time


# Six cleanup passes at most; only failed passes wait (3.1 seconds total).
# These delays belong to the test runner, never to CandidateUI or the DLL.
CLEANUP_RETRY_DELAYS = (0.1, 0.2, 0.4, 0.8, 1.6)


def cleanup_work_directory(directory: tempfile.TemporaryDirectory) -> None:
    for attempt in range(len(CLEANUP_RETRY_DELAYS) + 1):
        try:
            # Keep TemporaryDirectory's handling of read-only/partly removed files.
            # Explicit cleanup also detaches its finalizer, including on failure.
            directory.cleanup()
        except OSError as error:
            if attempt < len(CLEANUP_RETRY_DELAYS):
                time.sleep(CLEANUP_RETRY_DELAYS[attempt])
                continue
            # An executable can remain locked after the test process exits.
            # Do not replace a test exception, or fail a passing test, on cleanup.
            # Avoid warnings.warn(): -Werror would change the test result again.
            print(json.dumps({"phase": "cleanup", "status": "warning",
                              "directory": directory.name, "attempts": attempt + 1,
                              "error": f"{type(error).__name__}: {error}",
                              "test_result_unchanged": True}), file=sys.stderr, flush=True)
        return


@contextmanager
def temporary_work_directory(*, prefix: str) -> Iterator[Path]:
    directory = tempfile.TemporaryDirectory(prefix=prefix)
    try:
        yield Path(directory.name)
    finally:
        # No return/except around yield: compilation, probe and control failures
        # (including timeouts, SystemExit and Ctrl+C) must keep propagating.
        cleanup_work_directory(directory)
