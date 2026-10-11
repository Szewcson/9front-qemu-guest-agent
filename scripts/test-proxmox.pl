#!/usr/bin/perl
# SPDX-License-Identifier: MIT
# Run on the PVE host. Uses its synchronized QGA client, not a raw socket reader.
use strict;
use warnings;
use JSON::PP ();
use Config;
use MIME::Base64 qw(encode_base64);
use Time::HiRes qw(gettimeofday);

my ($vmid, $option) = @ARGV;
die "usage: perl scripts/test-proxmox.pl VMID [--set-time]\n"
    unless @ARGV >= 1 && @ARGV <= 2 && defined($vmid)
    && $vmid =~ /\A[1-9][0-9]{0,8}\z/
    && (!defined($option) || $option eq '--set-time');
die "64-bit Perl integers are required for QGA handles and nanosecond time\n"
    unless $Config{ivsize} >= 8;
# Loaded at runtime so perl -c also works on a non-PVE development host.
require PVE::QemuServer::Monitor;

sub command {
    my ($name, $arguments) = @_;
    # mon_cmd takes a flat argument list and constructs the wire arguments itself.
    my %parameters = (defined($arguments) ? %$arguments : (), timeout => 10);
    my $result = PVE::QemuServer::Monitor::mon_cmd($vmid, $name, %parameters);
    # The QGA branch can return {error => ...} without throwing an exception.
    die "$name failed: " . JSON::PP->new->canonical->encode($result->{error}) . "\n"
        if ref($result) eq 'HASH' && exists $result->{error};
    return $result;
}

sub check {
    my ($ok, $message) = @_;
    die "$message\n" unless $ok;
}

sub file_handle {
    my ($value) = @_;
    check(defined($value) && !ref($value), 'invalid file handle');
    my $text = "$value";
    check($text =~ /\A[1-9][0-9]{0,18}\z/
        && (length($text) < 19 || $text le '9223372036854775807'),
        'invalid file handle');
    # String validation sets Perl's string flag. JSON::XS otherwise emits "1"
    # rather than 1; normalize AFTER validation and before passing to PVE.
    return int($value);
}

command('guest-ping');
my $info = command('guest-info');
check(ref($info) eq 'HASH' && ref($info->{supported_commands}) eq 'ARRAY',
    'invalid guest-info response');
my %available;
for my $entry (@{$info->{supported_commands}}) {
    next unless ref($entry) eq 'HASH' && !ref($entry->{name})
        && defined($entry->{name}) && $entry->{enabled};
    $available{$entry->{name}} = 1;
}
for my $name (qw(guest-file-open guest-file-close guest-file-read guest-file-write
        guest-file-seek guest-file-flush)) {
    check($available{$name}, "required command not advertised: $name");
}
check(!defined($option) || $available{'guest-set-time'},
    'guest-set-time is not advertised');
print JSON::PP->new->canonical->encode({agent => $info->{version}}), "\n";
for my $name (qw(guest-get-vcpus guest-get-timezone guest-network-get-route)) {
    print JSON::PP->new->canonical->encode({command => $name, result => command($name)}), "\n";
}

my ($seconds, $microseconds) = gettimeofday();
my $path = "/tmp/qga9-host-test-$vmid-$seconds-$microseconds-$$.bin";
my $bytes = "\x00\xffqga9\n";
my $encoded = encode_base64($bytes, '');
my $handle;
my $ok = eval {
    # Deliberately creates a synthetic file, never takes an operator-chosen path.
    $handle = file_handle(command('guest-file-open', {path => $path, mode => 'w+'}));
    my $write = command('guest-file-write', {handle => $handle, 'buf-b64' => $encoded});
    check(ref($write) eq 'HASH' && defined($write->{count})
        && $write->{count} == length($bytes), 'short or invalid write: '
        . JSON::PP->new->canonical->encode($write));
    command('guest-file-flush', {handle => $handle});
    my $seek = command('guest-file-seek', {handle => $handle, offset => 0, whence => 'set'});
    check(ref($seek) eq 'HASH' && defined($seek->{position}) && $seek->{position} == 0,
        'seek did not return to the beginning');
    my $read = command('guest-file-read', {handle => $handle, count => length($bytes)+1});
    check(ref($read) eq 'HASH' && defined($read->{count})
        && $read->{count} == length($bytes) && defined($read->{'buf-b64'})
        && $read->{'buf-b64'} eq $encoded && $read->{eof}, 'binary/EOF round-trip failed');
    my $closing = $handle;
    undef $handle; # Close consumes the guest descriptor, even on an error.
    command('guest-file-close', {handle => $closing});
    $handle = file_handle(command('guest-file-open', {path => $path}));
    $read = command('guest-file-read', {handle => $handle});
    check(ref($read) eq 'HASH' && defined($read->{count})
        && $read->{count} == length($bytes) && defined($read->{'buf-b64'})
        && $read->{'buf-b64'} eq $encoded, 'reopened file content differs');
    $closing = $handle;
    undef $handle;
    command('guest-file-close', {handle => $closing});
    1;
};
my $failure = $@;
if (defined $handle) {
    eval { command('guest-file-close', {handle => $handle}); };
    $failure .= "cleanup failed: $@" if $@;
}
die $failure unless $ok;
print JSON::PP->new->canonical->encode({files => 'passed', retained_guest_fixture => $path}), "\n";

if (defined $option) {
    # Explicit opt-in: changes system time AND RTC to the host's current UTC time.
    ($seconds, $microseconds) = gettimeofday();
    check($seconds >= 0 && $seconds <= 3155759999, 'host time outside native RTC range');
    my $time = $seconds*1_000_000_000 + $microseconds*1_000;
    command('guest-set-time', {time => $time});
    my $after = command('guest-get-time');
    check(defined($after) && !ref($after) && "$after" =~ /\A[0-9]{1,19}\z/
        && $after >= $time && $after-$time <= 10_000_000_000,
        'guest clock readback is not near the supplied host time');
    print JSON::PP->new->canonical->encode({clock => 'passed', guest_time_ns => $after}), "\n";
}
