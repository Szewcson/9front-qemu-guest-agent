"""Exercise actual C handlers over a bidirectional stream, not mocked replies."""
import json
import os
from pathlib import Path
import random
import select
import subprocess
import unittest


BINARY = Path(os.environ.get("QGA_TEST_BINARY", "../build/qga9-stream-test")).resolve()
PING = b'{"execute":"guest-ping"}'


def request(command, arguments=None, **fields):
    fields["execute"] = command
    if arguments is not None:
        fields["arguments"] = arguments
    return json.dumps(fields, ensure_ascii=False, separators=(",", ":")).encode()


class ProtocolTests(unittest.TestCase):
    def run_stream(self, payload, read_size=7, write_size=3):
        result = subprocess.run(
            [str(BINARY), str(read_size), str(write_size)],
            input=payload, capture_output=True, timeout=10, check=True,
        )
        self.assertEqual(result.stderr, b"")
        responses = []
        for line in result.stdout.splitlines(keepends=True):
            self.assertTrue(line.endswith(b"\r\n"), line)
            responses.append(json.loads(line.removeprefix(b"\xff")))
        return result.stdout, responses

    def assert_error(self, payload, expected="GenericError"):
        _, responses = self.run_stream(payload)
        self.assertEqual(len(responses), 1)
        self.assertEqual(responses[0]["error"]["class"], expected)
        self.assertIsInstance(responses[0]["error"]["desc"], str)
        self.assertNotIn("return", responses[0])
        return responses[0]

    def test_ping(self):
        self.assertEqual(self.run_stream(PING)[1], [{"return": {}}])

    def test_info(self):
        info = self.run_stream(request("guest-info"))[1][0]["return"]
        self.assertEqual(info["version"], "qga9-0.6.1-dev")
        supported = info["supported_commands"]
        self.assertEqual({c["name"] for c in supported}, {
            "guest-sync", "guest-sync-delimited", "guest-ping", "guest-info",
        })
        self.assertEqual(len(supported), 4)
        for command in supported:
            self.assertIs(command["enabled"], True)
            self.assertIs(command["success-response"], True)

    def test_sync_signed_64_bit(self):
        for number in [0, -1, 2**53 + 1, 2**63 - 1, -(2**63)]:
            with self.subTest(number=number):
                self.assertEqual(self.run_stream(request("guest-sync", {"id": number}))[1],
                                 [{"return": number}])

    def test_sync_delimited(self):
        output, responses = self.run_stream(
            b"\xff" + request("guest-sync-delimited", {"id": 2**63 - 1}, id="request")
        )
        self.assertTrue(output.startswith(b"\xff{"))
        self.assertEqual(output.count(b"\xff"), 1)
        self.assertEqual(responses, [{"return": 2**63 - 1, "id": "request"}])

    def test_invalid_delimited_sync_has_no_success_sentinel(self):
        output, _ = self.run_stream(request("guest-sync-delimited", {"id": "42"}))
        self.assertNotIn(b"\xff", output)
        self.assert_error(request("guest-sync-delimited", {"id": "42"}))

    def test_sync_range_and_types(self):
        for number in [2**63, -(2**63)-1, 2**64-1, True, False, None, "5", [], {}, 1.5]:
            with self.subTest(number=number):
                self.assert_error(request("guest-sync", {"id": number}))
        self.assert_error(b'{"execute":"guest-sync","arguments":{"id":1e0}}')

    def test_sync_missing_and_extra_arguments(self):
        for arguments in [None, {}, {"id": 1, "extra": 3}, {"other": 1}]:
            self.assert_error(request("guest-sync", arguments))

    def test_invalid_request_structure(self):
        for payload in [b"[]\n", b"null\n", b"42\n", b"{}", b'{"execute":1}',
                        b'{"execute":null}', b'{"execute":[]}',
                        b'{"execute":"guest-ping","arguments":null}',
                        b'{"execute":"guest-ping","arguments":[]}',
                        b'{"execute":"guest-ping","extra":1}']:
            with self.subTest(payload=payload):
                self.assert_error(payload)

    def test_no_argument_commands(self):
        self.assertEqual(self.run_stream(request("guest-ping", {}))[1], [{"return": {}}])
        for command in ["guest-ping", "guest-info"]:
            self.assert_error(request(command, {"extra": 1}))

    def test_unknown_command(self):
        error = self.assert_error(request("no-such-command", id=42), "CommandNotFound")
        self.assertEqual(error["id"], 42)

    def test_native_queries_require_registered_backend(self):
        for command in ["guest-get-vcpus", "guest-get-timezone", "guest-network-get-route", "guest-get-disks"]:
            with self.subTest(command=command):
                self.assert_error(request(command), "Unsupported")

    def test_all_pending_standard_commands_are_known(self):
        for command in ["guest-suspend-ram", "guest-suspend-disk", "guest-suspend-hybrid",
                        "guest-set-vcpus", "guest-get-memory-blocks", "guest-set-memory-blocks",
                        "guest-get-memory-block-info", "guest-get-cpustats", "guest-get-load",
                        "guest-get-fsinfo", "guest-get-diskstats",
                        "guest-get-users", "guest-get-devices"]:
            with self.subTest(command=command):
                self.assert_error(request(command), "Unsupported")

    def test_embedded_nul_does_not_alias_command(self):
        self.assert_error(request("guest-ping\x00extra"), "CommandNotFound")

    def test_policy_excluded_commands(self):
        for command in ["guest-exec", "guest-exec-status", "guest-file-open",
                        "guest-file-read", "guest-file-write", "guest-set-user-password",
                        "guest-fsfreeze-freeze", "guest-fsfreeze-thaw", "guest-fstrim"]:
            with self.subTest(command=command):
                self.assert_error(request(command), "Unsupported")

    def test_unregistered_read_only_callbacks_are_not_fabricated(self):
        for command in ["guest-get-osinfo", "guest-get-host-name", "guest-get-time",
                        "guest-network-get-interfaces"]:
            self.assert_error(request(command), "Unsupported")

    def test_shutdown_disabled_for_all_modes(self):
        # No actual shutdown callback or external execution exists in this slice.
        for mode in [None, "halt", "powerdown", "reboot"]:
            arguments = {} if mode is None else {"mode": mode}
            self.assert_error(request("guest-shutdown", arguments), "Unsupported")

    def test_shutdown_error_leaves_stream_usable(self):
        _, responses = self.run_stream(request("guest-shutdown", {"mode": "reboot"}) + PING)
        self.assertEqual(responses[0]["error"]["class"], "Unsupported")
        self.assertEqual(responses[1], {"return": {}})

    def test_fragmented_reads_and_partial_writes(self):
        payload = request("guest-sync", {"id": 2**63-1}, id="fragments")
        for read_size in [1, 2, 7, 255, 256]:
            for write_size in [1, 2, 3, 255, 65536]:
                with self.subTest(read_size=read_size, write_size=write_size):
                    self.assertEqual(self.run_stream(payload, read_size, write_size)[1],
                                     [{"return": 2**63-1, "id": "fragments"}])

    def test_consecutive_commands_without_newlines(self):
        payload = PING + request("guest-sync-delimited", {"id": 123}) + PING
        output, responses = self.run_stream(payload)
        self.assertEqual(responses, [{"return": {}}, {"return": 123}, {"return": {}}])
        self.assertEqual(output.count(b"\xff"), 1)

    def test_multiline_json(self):
        payload = b'{\n "execute" : "guest-ping",\n "arguments" : {}\n}\n'
        self.assertEqual(self.run_stream(payload)[1], [{"return": {}}])

    def test_resync_in_partial_string(self):
        payload = b'{"execute":"unfinished\\' + b"\xff" + request("guest-sync-delimited", {"id": 82})
        self.assertEqual(self.run_stream(payload)[1], [{"return": 82}])

    def test_resync_in_partial_object(self):
        payload = b'{"execute":' + b"\xff" + PING
        self.assertEqual(self.run_stream(payload)[1], [{"return": {}}])

    def test_transaction_id_arbitrary_json(self):
        for value in [None, False, 123, -3.5, "string", [], {},
                      {"quoted": '"\\\n\t\x00', "unicode": "ż☃😀", "nested": [1, True, None]}]:
            with self.subTest(value=value):
                output, responses = self.run_stream(request("guest-ping", id=value))
                output.decode("ascii")
                self.assertEqual(responses, [{"return": {}, "id": value}])

    def test_escaped_keys_and_command(self):
        payload = b'{"\\u0065xecute":"guest-\\u0070ing","id":"\\ud83d\\ude00"}'
        self.assertEqual(self.run_stream(payload)[1], [{"return": {}, "id": "😀"}])

    def test_duplicate_decoded_keys(self):
        for payload in [b'{"execute":"guest-ping","execute":"guest-info"}',
                        b'{"execute":"guest-ping","\\u0065xecute":"guest-info"}',
                        b'{"execute":"guest-ping","id":{"a":0,"\\u0061":1}}']:
            self.assert_error(payload)

    def test_invalid_unicode(self):
        for value in [b'"\xc0\xaf"', b'"\xed\xa0\x80"', b'"\xf4\x90\x80\x80"',
                      b'"\x80"', b'"\\ud800"', b'"\\udc00"', b'"\\ud800\\u0041"',
                      b'"\\u12xz"', b'"\xe2\x82"']:
            self.assert_error(b'{"execute":"guest-ping","id":' + value + b'}')

    def test_malformed_json(self):
        for payload in [b'{"execute":"guest-ping",}', b'{"execute":"guest-ping" "id":1}',
                        b'{"execute":"guest-ping","id":01}',
                        b'{"execute":"guest-ping","id":1.}',
                        b'{"execute":"guest-ping","id":1e}',
                        b'{"execute":"guest-ping","id":tru}',
                        b'{"execute":"guest-ping","id":"\\q"}',
                        b'{"execute":"guest-ping","id":"\x00"}',
                        b'{"execute":"guest-ping","id":[1,]}']:
            with self.subTest(payload=payload):
                self.assert_error(payload)

    def test_truncated_input(self):
        for payload in [b'{', b'{"execute":', b'{"execute":"guest-',
                        b'{"execute":"guest-ping"']:
            self.assert_error(payload)

    def test_oversized_input_and_line_recovery(self):
        payload = b'{"execute":"guest-ping","id":"' + b"x" * 9000 + b'"}\n' + PING
        _, responses = self.run_stream(payload)
        self.assertEqual(len(responses), 2)
        self.assertEqual(responses[0]["error"]["class"], "GenericError")
        self.assertEqual(responses[-1], {"return": {}})

    def test_oversized_input_and_sentinel_recovery(self):
        payload = b'{"id":"' + b"x" * 9000 + b"\xff" + PING
        _, responses = self.run_stream(payload)
        self.assertEqual(len(responses), 2)
        self.assertEqual(responses[-1], {"return": {}})

    def test_exact_message_limit(self):
        base = request("guest-ping", id="")
        payload = request("guest-ping", id="x" * (8192 - len(base)))
        self.assertEqual(len(payload), 8192)
        self.assertEqual(self.run_stream(payload)[1], [{"return": {}, "id": "x" * (8192-len(base))}])

    def test_depth_and_token_limits(self):
        self.assert_error(b'{"execute":"guest-ping","id":' + b'[' * 18 + b'0' + b']' * 18 + b'}')
        self.assert_error(request("guest-ping", id=[0] * 600))

    def test_large_output_escaping(self):
        value = "\x01" * 1300
        output, responses = self.run_stream(request("guest-ping", id=value))
        self.assertGreater(len(output), len(value))
        self.assertEqual(responses, [{"return": {}, "id": value}])

    def test_deterministic_fuzz_and_resync(self):
        rng = random.Random(0x9F0A)
        payload = bytearray()
        for index in range(2000):
            payload += rng.randbytes(rng.randrange(0, 256))
            payload += b"\xff" + request("guest-sync-delimited", {"id": index})
        _, responses = self.run_stream(bytes(payload), 31, 65536)
        recovered = [r["return"] for r in responses if isinstance(r.get("return"), int)]
        self.assertEqual(recovered, list(range(2000)))

    def test_responds_without_waiting_for_eof_or_newline(self):
        process = subprocess.Popen([str(BINARY), "1", "1"], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            process.stdin.write(PING)
            process.stdin.flush()
            response = bytearray()
            while not response.endswith(b"\r\n"):
                readable, _, _ = select.select([process.stdout], [], [], 3)
                self.assertTrue(readable, "Agent waited for EOF/newline")
                response += os.read(process.stdout.fileno(), 256)
            self.assertEqual(json.loads(response), {"return": {}})
            process.stdin.close()
            self.assertEqual(process.wait(timeout=3), 0)
            self.assertEqual(process.stderr.read(), b"")
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=3)
            process.stdout.close()
            process.stderr.close()
            if not process.stdin.closed:
                process.stdin.close()


if __name__ == "__main__":
    unittest.main()
