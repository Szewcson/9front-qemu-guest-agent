# SPDX-License-Identifier: MIT
"""Run the same C capability validator used by the native smoke fixture."""
import copy
import json
import os
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("QGA_CORE_TEST_BINARY", ROOT / "build" / "qga9-core-test")).resolve()
STDIO = [
    "guest-sync", "guest-sync-delimited", "guest-ping", "guest-info",
    "guest-get-osinfo", "guest-get-host-name", "guest-get-time",
    "guest-network-get-interfaces", "guest-get-vcpus", "guest-get-timezone",
    "guest-network-get-route", "guest-get-disks",
    "guest-file-open", "guest-file-close", "guest-file-read", "guest-file-write",
    "guest-file-seek", "guest-file-flush",
]


def metadata(load=False):
    names = STDIO + (["guest-get-load"] if load else [])
    return {"return": {"version": "qga9-test", "supported_commands": [
        {"name": name, "enabled": True, "success-response": True} for name in names
    ]}}


class CapabilityTests(unittest.TestCase):
    def validate(self, data):
        if not isinstance(data, bytes):
            data = json.dumps(data).encode()
        return subprocess.run([str(BINARY), "--stdio-info"], input=data,
                              capture_output=True, timeout=10)

    def test_valid_stdio_capabilities_with_optional_load(self):
        for load in (False, True):
            value = metadata(load)
            # Enumeration order must not define command semantics.
            value["return"]["supported_commands"].reverse()
            result = self.validate(value)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, b"")

    def assert_invalid(self, value):
        result = self.validate(value)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(b"FAIL: invalid stdio capabilities", result.stderr)
        for diagnostic in (b"AddressSanitizer", b"LeakSanitizer", b"runtime error:"):
            self.assertNotIn(diagnostic, result.stderr)

    def test_empty_malformed_error_or_incomplete_capabilities_fail(self):
        cases = [b"", b"invalid-json", b"{}", b"[]", b" " * 65537,
                 {"error": {"class": "GenericError", "desc": "synthetic"}},
                 {"return": {}}, {"return": {"version": "test", "supported_commands": []}}]
        base = metadata()
        for key, wrong in (("version", 3), ("supported_commands", {})):
            value = copy.deepcopy(base)
            value["return"][key] = wrong
            cases.append(value)
        for key, wrong in (("enabled", False), ("enabled", 1),
                           ("success-response", False), ("success-response", "true")):
            value = copy.deepcopy(base)
            value["return"]["supported_commands"][0][key] = wrong
            cases.append(value)
        value = copy.deepcopy(base)
        value["return"]["supported_commands"].append(value["return"]["supported_commands"][0])
        cases.append(value)
        value = copy.deepcopy(base)
        value["error"] = {"class": "GenericError", "desc": "synthetic"}
        cases.append(value)
        for case in cases:
            with self.subTest(case=str(case)[:80]):
                self.assert_invalid(case)

    def test_management_only_or_unknown_commands_are_rejected(self):
        for name in ("guest-shutdown", "guest-set-time", "guest-exec",
                     "guest-exec-status", "guest-unexpected"):
            value = metadata()
            value["return"]["supported_commands"].append({
                "name": name, "enabled": True, "success-response": name != "guest-shutdown",
            })
            with self.subTest(name=name):
                self.assert_invalid(value)
