# SPDX-License-Identifier: MIT
"""Local PVE-helper API/cleanup mocks, never a real hypervisor connection."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("perl"), "optional Perl script tests")
class ProxmoxScriptTests(unittest.TestCase):
    def run_script(self, *args, scenario=""):
        env = os.environ.copy()
        # Allow an explicitly supplied, temporary JSON::XS installation for the
        # PVE serialization regression; never require XS for native guest code.
        env["PERL5LIB"] = os.pathsep.join(filter(None, (
            str(ROOT / "tests" / "pve-mock"), env.get("PERL5LIB"),
        )))
        env["QGA_PVE_MOCK_SCENARIO"] = scenario
        return subprocess.run(
            ["perl", str(ROOT / "scripts" / "test-proxmox.pl"), *args],
            env=env, capture_output=True, text=True, timeout=10,
        )

    def test_file_roundtrip_and_flat_helper_arguments(self):
        result = self.run_script("103")
        self.assertEqual(result.returncode, 0, result.stderr)
        records = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(records[-1]["files"], "passed")
        self.assertTrue(records[-1]["retained_guest_fixture"].startswith("/tmp/qga9-host-test-103-"))
        self.assertEqual(result.stderr.count("mock: closed"), 2)
        self.assertNotIn('"clock"', result.stdout)

    def test_explicit_clock_opt_in(self):
        result = self.run_script("103", "--set-time")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout.splitlines()[-1])["clock"], "passed")

    def test_failed_read_closes_handle(self):
        result = self.run_script("103", scenario="read-error")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mock read failure", result.stderr)
        self.assertIn("mock: closed", result.stderr)
        self.assertNotIn("leaked handle", result.stderr)

    def test_missing_clock_fails_before_file_mutation(self):
        result = self.run_script("103", "--set-time", scenario="missing-clock")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest-set-time is not advertised", result.stderr)
        self.assertNotIn("mock: closed", result.stderr)

    def test_clock_error_is_not_reported_as_success(self):
        result = self.run_script("103", "--set-time", scenario="clock-error")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mock clock failure", result.stderr)
        self.assertNotIn('"clock":"passed"', result.stdout)

    def test_structured_qga_write_error_is_preserved_and_handle_closed(self):
        result = self.run_script("103", scenario="write-qga-error")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest-file-write failed:", result.stderr)
        self.assertIn("mock structured write error", result.stderr)
        self.assertIn('"class":"GenericError"', result.stderr)
        self.assertIn("mock: closed", result.stderr)
        self.assertNotIn("leaked handle", result.stderr)

    def test_structured_qga_clock_error_is_not_false_success(self):
        result = self.run_script("103", "--set-time", scenario="clock-qga-error")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest-set-time failed:", result.stderr)
        self.assertNotIn('"clock":"passed"', result.stdout)

    def test_short_write_is_diagnostic_and_not_retried(self):
        result = self.run_script("103", scenario="short-write")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('short or invalid write: {"count":0,"eof":0}', result.stderr)
        self.assertEqual(result.stderr.count("mock: closed"), 1)

    def test_maximum_signed_handle_preserves_integer_wire_type(self):
        result = self.run_script("103", scenario="handle-max")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr.count("mock: closed"), 2)

    def test_unsigned_handle_overflow_is_rejected_before_mutation(self):
        result = self.run_script("103", scenario="handle-overflow")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invalid file handle", result.stderr)
        self.assertNotIn("mock: closed", result.stderr)

    def test_invalid_arguments(self):
        for args in ((), ("0",), ("103;id",), ("103", "--wrong"), ("103", "--set-time", "extra")):
            with self.subTest(args=args):
                result = self.run_script(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("usage:", result.stderr)


if __name__ == "__main__":
    unittest.main()
