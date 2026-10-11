#!/usr/bin/perl
# SPDX-License-Identifier: MIT
# PVE-side native exec smoke test. No host file reads, uploads or shell interpolation.
use strict;
use warnings;
use JSON::PP ();
use MIME::Base64 qw(encode_base64 decode_base64);
use Time::HiRes qw(time sleep);
my ($vmid) = @ARGV;
die "usage: perl scripts/test-proxmox-exec.pl VMID\n"
    unless @ARGV == 1 && defined($vmid) && $vmid =~ /\A[1-9][0-9]{0,8}\z/;
require PVE::QemuServer::Monitor;
sub command {
    my ($name, $arguments) = @_;
    my $result = PVE::QemuServer::Monitor::mon_cmd($vmid, $name,
        (defined($arguments) ? %$arguments : ()), timeout => 10);
    die "$name failed: " . JSON::PP->new->canonical->encode($result->{error}) . "\n"
        if ref($result) eq 'HASH' && exists $result->{error};
    return $result;
}
sub check { die "$_[1]\n" unless $_[0]; }
sub launch {
    my ($arguments) = @_;
    my $result = command('guest-exec', $arguments);
    check(ref($result) eq 'HASH' && defined($result->{pid}) && !ref($result->{pid}), 'invalid exec PID');
    my $text = "$result->{pid}";
    check($text =~ /\A[1-9][0-9]{0,9}\z/ && $result->{pid} <= 2147483647, 'invalid native exec PID');
    return int($result->{pid}); # Normalize after string validation for JSON::XS.
}
sub finished {
    my ($pid) = @_;
    my $deadline = time()+30;
    while (time() < $deadline) {
        my $result = command('guest-exec-status', {pid => $pid});
        check(ref($result) eq 'HASH' && exists($result->{exited}), 'invalid exec status');
        return $result if $result->{exited};
        command('guest-ping');
        sleep(0.1);
    }
    die "exec test timed out; PID $pid may still need an exec-status query to reap its result\n";
}
sub captured {
    my ($result, $stream) = @_;
    my $value = $result->{"$stream-data"};
    check(defined($value) && !ref($value) && length($value) <= 5464, 'invalid capture length');
    my $decoded = decode_base64($value);
    check(encode_base64($decoded, '') eq $value, 'invalid canonical capture base64');
    check(!$result->{"$stream-truncated"}, 'unexpected output truncation');
    return $decoded;
}
command('guest-ping');
my $info = command('guest-info');
check(ref($info) eq 'HASH' && ref($info->{supported_commands}) eq 'ARRAY', 'invalid guest-info');
my %available = map { ref($_) eq 'HASH' && defined($_->{name}) && !ref($_->{name}) && $_->{enabled}
    ? ($_->{name} => 1) : () } @{$info->{supported_commands}};
check($available{'guest-exec'} && $available{'guest-exec-status'}, 'exec commands not advertised');
print JSON::PP->new->canonical->encode({agent => $info->{version}}), "\n";
my $literal = q{$(no); 'literal'};
my $pid = launch({path => '/bin/echo', arg => [$literal], 'capture-output' => JSON::PP::true});
my $result = finished($pid);
check(defined($result->{exitcode}) && $result->{exitcode} == 0
    && captured($result, 'out') eq "$literal\n" && captured($result, 'err') eq '', 'literal argv test failed');
# A real guest error proves reaping; a host transport exception/timeout does not.
my $reaped = PVE::QemuServer::Monitor::mon_cmd($vmid, 'guest-exec-status', pid => $pid, timeout => 10);
check(ref($reaped) eq 'HASH' && ref($reaped->{error}) eq 'HASH'
    && defined($reaped->{error}{class}) && !ref($reaped->{error}{class})
    && $reaped->{error}{class} eq 'GenericError', 'finished PID was not reaped');
my $bytes = "\x00\xffqga9\n";
$pid = launch({path => '/bin/cat', 'input-data' => encode_base64($bytes, ''), 'capture-output' => JSON::PP::true});
$result = finished($pid);
check(defined($result->{exitcode}) && $result->{exitcode} == 0 && captured($result, 'out') eq $bytes,
    'synthetic binary stdin/output failed');
$pid = launch({path => '/bin/echo', arg => ['merged'], 'capture-output' => 'merged'});
$result = finished($pid);
check(defined($result->{exitcode}) && $result->{exitcode} == 0 && captured($result, 'out') eq "merged\n"
    && !exists($result->{'err-data'}), 'merged capture failed');
$pid = launch({path => '/bin/sleep', arg => ['2']});
$result = command('guest-exec-status', {pid => $pid});
check(ref($result) eq 'HASH' && exists($result->{exited}) && !$result->{exited}, 'sleep did not report running');
command('guest-ping');
$result = finished($pid);
check(defined($result->{exitcode}) && $result->{exitcode} == 0 && !exists($result->{'out-data'}), 'sleep status failed');
print JSON::PP->new->canonical->encode({exec => 'passed', tests => ['literal-argv', 'binary-stdin', 'merged-output', 'responsive-ping', 'one-shot-reaping']}), "\n";
