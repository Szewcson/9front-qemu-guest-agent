# SPDX-License-Identifier: MIT
# Local script-test double only. This is NOT PVE or a host-integration test.
package PVE::QemuServer::Monitor;
use strict;
use warnings;
use B ();
use JSON::PP ();
use MIME::Base64 qw(encode_base64 decode_base64);

my ($handle, $next, $bytes, $position, $clock) = (undef, 1, '', 0, 0);
my $scenario = $ENV{QGA_PVE_MOCK_SCENARIO} // '';
my $execmock = $ENV{QGA_PVE_MOCK_EXEC} // '';
my (%jobs, $nextpid);
$nextpid = 100;
my $codec = JSON::PP->new;
if (($ENV{QGA_PVE_MOCK_CODEC} // '') eq 'xs') {
    require JSON::XS;
    $codec = JSON::XS->new;
}

sub mon_cmd {
    my ($vmid, $name, %args) = @_;
    die "mock VMID\n" unless $vmid == 103;
    die "mock timeout\n" unless delete($args{timeout}) == 10;
    die "nested arguments: mon_cmd requires flat parameters\n" if exists $args{arguments};
    # Cross a real JSON encode/decode boundary, instead of comparing Perl
    # values numerically and silently accepting string-typed wire arguments.
    %args = %{$codec->decode($codec->encode(\%args))};
    for my $key (qw(handle count offset time pid)) {
        next unless exists $args{$key};
        my $flags = B::svref_2object(\$args{$key})->FLAGS;
        return {error => {class => 'GenericError', desc => 'Noninteger wire argument'}}
            unless ($flags & B::SVf_IOK()) && !($flags & B::SVf_POK());
    }
    my %allowed = (
        'guest-file-open' => [qw(path mode)],
        'guest-file-close' => [qw(handle)],
        'guest-file-read' => [qw(handle count)],
        'guest-file-write' => [qw(handle buf-b64 count)],
        'guest-file-flush' => [qw(handle)],
        'guest-file-seek' => [qw(handle offset whence)],
        'guest-set-time' => [qw(time)],
        'guest-exec' => [qw(path arg env input-data capture-output)],
        'guest-exec-status' => [qw(pid)],
    );
    my %keys = map { $_ => 1 } @{$allowed{$name} // []};
    die "unexpected argument\n" if grep { !$keys{$_} } keys %args;
    return {} if $name eq 'guest-ping';
    if ($name eq 'guest-info') {
        if ($execmock) {
            my @names = $scenario eq 'missing-exec' ? () : qw(guest-exec guest-exec-status);
            return {version => 'mock-not-live', supported_commands =>
                [map { {name => $_, enabled => 1} } @names]};
        }
        my @names = qw(guest-file-open guest-file-close guest-file-read guest-file-write
            guest-file-seek guest-file-flush guest-set-time);
        pop @names if $scenario eq 'missing-clock';
        return {version => 'mock-not-live', supported_commands =>
            [map { {name => $_, enabled => 1} } @names]};
    }
    if ($execmock && $name eq 'guest-exec') {
        return {error => {class => 'GenericError', desc => "mock exec error\n\e"}}
            if $scenario eq 'exec-error';
        return {pid => '2147483648'} if $scenario eq 'pid-overflow';
        my $pid = $nextpid++;
        die "mock unexpected path\n" unless $args{path} =~ m{\A/bin/(echo|cat|sleep)\z};
        $jobs{$pid} = {args => {%args}, polls => 0};
        return {pid => $pid};
    }
    if ($execmock && $name eq 'guest-exec-status') {
        my $job = $jobs{$args{pid}};
        die "mock reap transport timeout\n" if !$job && $scenario eq 'reap-timeout';
        return {error => {class => 'GenericError', desc => 'Unknown or reaped exec PID'}} unless $job;
        return {exited => JSON::PP::false} if $job->{args}{path} eq '/bin/sleep' && !$job->{polls}++;
        my $a = $job->{args};
        my $out = $a->{path} eq '/bin/cat' ? decode_base64($a->{'input-data'}) :
            $a->{path} eq '/bin/echo' ? join(' ', @{$a->{arg}})."\n" : '';
        delete $jobs{$args{pid}} unless $scenario eq 'not-reaped';
        my $r = {exited => JSON::PP::true, exitcode => 0};
        if ($a->{'capture-output'}) {
            $r->{'out-data'} = $scenario eq 'bad-base64' ? 'not base64!' : encode_base64($out, '');
            $r->{'out-truncated'} = $scenario eq 'truncated' ? JSON::PP::true : JSON::PP::false;
            $r->{'err-data'} = '' unless $a->{'capture-output'} eq 'merged';
            $r->{'err-truncated'} = JSON::PP::false unless $a->{'capture-output'} eq 'merged';
        }
        return $r;
    }
    return [] if $name eq 'guest-get-vcpus' || $name eq 'guest-network-get-route';
    return {zone => 'mock', offset => 0} if $name eq 'guest-get-timezone';
    if ($name eq 'guest-file-open') {
        die "mock duplicate open\n" if defined $handle;
        return '9223372036854775808' if $scenario eq 'handle-overflow';
        die "unsafe fixture path\n" unless $args{path} =~
            m{\A/tmp/qga9-host-test-103-[0-9]+-[0-9]+-[0-9]+\.bin\z};
        die "mock mode\n" unless !defined($args{mode}) || $args{mode} eq 'w+';
        $handle = $scenario eq 'handle-max' ? 9223372036854775807 : $next++;
        $position = 0;
        return $handle;
    }
    if ($name eq 'guest-set-time') {
        die "mock clock failure\n" if $scenario eq 'clock-error';
        return {error => {class => 'GenericError', desc => 'mock structured clock error'}}
            if $scenario eq 'clock-qga-error';
        die "mock noninteger time\n" unless "$args{time}" =~ /\A[0-9]{19}\z/;
        $clock = $args{time};
        return {};
    }
    return $clock+1_000_000 if $name eq 'guest-get-time';
    die "mock invalid handle\n" unless defined($handle) && $args{handle} == $handle;
    if ($name eq 'guest-file-close') {
        undef $handle;
        print STDERR "mock: closed\n";
        return {};
    }
    return {} if $name eq 'guest-file-flush';
    if ($name eq 'guest-file-write') {
        return {error => {class => 'GenericError', desc => 'mock structured write error'}}
            if $scenario eq 'write-qga-error';
        return {count => 0, eof => 0} if $scenario eq 'short-write';
        $bytes = decode_base64($args{'buf-b64'});
        $position = length $bytes;
        return {count => length($bytes), eof => 0};
    }
    if ($name eq 'guest-file-seek') {
        die "mock seek\n" unless $args{offset} == 0 && $args{whence} eq 'set';
        $position = 0;
        return {position => 0, eof => 0};
    }
    if ($name eq 'guest-file-read') {
        die "mock read failure\n" if $scenario eq 'read-error';
        my $count = $args{count} // 4096;
        my $read = substr($bytes, $position, $count);
        $position += length $read;
        return {count => length($read), 'buf-b64' => encode_base64($read, ''),
            eof => length($read) < $count};
    }
    die "unknown mock command\n";
}
END {
    if (defined $handle) {
        print STDERR "mock: leaked handle\n";
        $? = 1;
    }
}
1;
