#!/usr/bin/env perl

use strict;
use warnings;

use Getopt::Long;
use File::Temp 'tmpnam';
use Fcntl qw/SEEK_SET SEEK_CUR SEEK_END/;

my $CROSS_COMPILE=$ENV{CROSS_COMPILE} // '';

my ($offset) = (0);

GetOptions ("print-offset" => \$offset);

my $ELF = $ARGV[0];

my (@output, @sections, %symbols);

@output = `readelf -S $ELF`;

for (@output) {
        next unless /^\s*\[\s*(\d+)\s*\]\s+(\S+)\s+\S+\s+([[:xdigit:]]+)/;

        $sections[$1] = {name => $2, off => hex($3)};
        do { printf "0x08%x\n", hex($3); exit 0 } if $offset && $1 eq $ARGV[1];
}

@output = `readelf -Ws $ELF`;

for (@output) {
        next unless /([[:xdigit:]]+)\s+(\d+)\s+\S+\s+\S+\s+\S+\s+(\d+)\s+(\S+)$/;

        $symbols{$4} = {off => hex($1), len => $2, sec => $3};
        do { printf "0x%08x\n", hex($1); exit 0 } if $offset && $4 eq $ARGV[1];
}

exit 1 if $offset;

sub symbol_by_name {
        my ($name) = @_;
        my ($symlen, $symoff);
        my $ret;

        my $tmp = tmpnam();

        $ret = system("${CROSS_COMPILE}objcopy", '-Obinary', "-j$name", $ELF, $tmp);
        exit ($ret >> 8 || 1) if $ret;

        if (-s $tmp == 0) {
                die "symbol not found\n" unless exists $symbols{$name};
                my %sym = %{$symbols{$name}};
                my %sec = %{$sections[$sym{sec}]};

                $symlen = $sym{len};
                $symoff = $sym{off} - $sec{off};

                print("$ENV{CROSS_COMPILE}objcopy", '-Obinary', "-j$sec{name}", $ELF, $tmp);
                exit ($ret >> 8 || 1) if $ret;
        };

        die "failed to copy out section\n" if -s $tmp == 0;

        open my $fh, "<", $tmp or die "$!\n";

        my ($length, $bin) = ($symlen // 128);

        seek $fh, $symoff, SEEK_SET if defined $symoff;

        while (($ret = read($fh, $bin, $length)) > 0) {
                syswrite STDOUT, $bin, $ret;
                $length -= $ret if defined $symlen;
        }

        unlink $tmp;

        die "error reading: $!\n" unless defined $ret;
}

symbol_by_name($ARGV[1]);
exit 0;
