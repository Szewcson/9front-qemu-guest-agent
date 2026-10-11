# SPDX-License-Identifier: MIT
"""Local exec script regressions, never evidence of real PVE integration."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

@unittest.skipUnless(shutil.which("perl"), "optional Perl tests")
class ProxmoxExecScriptTests(unittest.TestCase):
    def run_script(self, *args, scenario=""):
        env = os.environ.copy()
        env["PERL5LIB"] = os.pathsep.join(filter(None, (str(ROOT / "tests/pve-mock"), env.get("PERL5LIB"))))
        env["QGA_PVE_MOCK_EXEC"] = "1"
        env["QGA_PVE_MOCK_SCENARIO"] = scenario
        return subprocess.run(["perl", str(ROOT / "scripts/test-proxmox-exec.pl"), *args],
                              env=env, capture_output=True, text=True, timeout=10)

    def test_roundtrip_and_integer_pid_wire_type(self):
        r = self.run_script("103")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout.splitlines()[-1])["exec"], "passed")
        self.assertNotIn("retained_guest_fixture", r.stdout)

    def test_missing_capabilities(self):
        r = self.run_script("103", scenario="missing-exec")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("not advertised", r.stderr)

    def test_structured_error_is_escaped(self):
        r = self.run_script("103", scenario="exec-error")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("guest-exec failed:", r.stderr)
        self.assertNotIn("\x1b", r.stderr)

    def test_pid_overflow(self):
        r = self.run_script("103", scenario="pid-overflow")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("invalid native exec PID", r.stderr)

    def test_invalid_base64_and_truncation(self):
        for scenario in ("bad-base64", "truncated"):
            r = self.run_script("103", scenario=scenario)
            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn('"exec":"passed"', r.stdout)

    def test_result_must_be_reaped(self):
        r = self.run_script("103", scenario="not-reaped")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("not reaped", r.stderr)

    def test_transport_failure_is_not_reaping_success(self):
        r = self.run_script("103", scenario="reap-timeout")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("reap transport timeout", r.stderr)
        self.assertNotIn('"exec":"passed"', r.stdout)

    def test_invalid_arguments(self):
        for args in ((), ("0",), ("103;id",), ("103", "extra")):
            r = self.run_script(*args)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("usage:", r.stderr)
