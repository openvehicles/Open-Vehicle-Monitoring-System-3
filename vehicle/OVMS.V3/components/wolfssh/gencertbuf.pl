#!/usr/bin/perl

# gencertbuf.pl
# version 1.0
# Updated 04/08/2019
#
# Copyright (C) 2014-2020 wolfSSL Inc.
#

use strict;
use warnings;

# ---- SCRIPT SETTINGS -------------------------------------------------------

# output C header file to write key buffers to
my $outputFile = "./wolfssh/certs_test.h";

# ecc keys to be converted

my @fileList_ecc = (
        [ "./keys/server-key-ecc.der",        "ecc_key_der_256" ],
        [ "./keys/server-key-ecc-384.der",    "ecc_key_der_384" ],
        [ "./keys/server-key-ecc-521.der",    "ecc_key_der_521" ]
        );


# 2048-bit keys to be converted

my @fileList_2048 = (
        [ "./keys/server-key-rsa.der", "rsa_key_der_2048" ],
        );

# ----------------------------------------------------------------------------

my $num_ecc = @fileList_ecc;
my $num_2048 = @fileList_2048;

# open our output file, "+>" creates and/or truncates
open OUT_FILE, "+>", $outputFile  or die $!;

print OUT_FILE "/* certs_test.h\n";
print OUT_FILE "*\n";
print OUT_FILE "* Copyright (C) 2014-2020 wolfSSL Inc.\n";
print OUT_FILE "*\n";
print OUT_FILE "* This file is part of wolfSSH.\n";
print OUT_FILE "*\n";
print OUT_FILE "* wolfSSH is free software; you can redistribute it and/or modify\n";
print OUT_FILE "* it under the terms of the GNU General Public License as published by\n";
print OUT_FILE "* the Free Software Foundation; either version 3 of the License, or\n";
print OUT_FILE "* (at your option) any later version.\n";
print OUT_FILE "*\n";
print OUT_FILE "* wolfSSH is distributed in the hope that it will be useful,\n";
print OUT_FILE "* but WITHOUT ANY WARRANTY; without even the implied warranty of\n";
print OUT_FILE "* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n";
print OUT_FILE "* GNU General Public License for more details.\n";
print OUT_FILE "*\n";
print OUT_FILE "* You should have received a copy of the GNU General Public License\n";
print OUT_FILE "* along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.\n";
print OUT_FILE "*/\n\n";
print OUT_FILE "#ifndef WOLFSSL_CERTS_TEST_H\n";
print OUT_FILE "#define WOLFSSL_CERTS_TEST_H\n\n";

# convert and print 2048-bit certs/keys
print OUT_FILE "#ifdef NO_FILESYSTEM\n\n";
for (my $i = 0; $i < $num_2048; $i++) {

    my $fname = $fileList_2048[$i][0];
    my $sname = $fileList_2048[$i][1];

    print OUT_FILE "/* $fname, 2048-bit */\n";
    print OUT_FILE "static const unsigned char $sname\[] =\n";
    print OUT_FILE "{\n";
    file_to_hex($fname);
    print OUT_FILE "};\n";
    print OUT_FILE "static const int sizeof_$sname = sizeof($sname);\n\n";
}

# convert and print ECC cert/keys
for (my $i = 0; $i < $num_ecc; $i++) {

    my $fname = $fileList_ecc[$i][0];
    my $sname = $fileList_ecc[$i][1];

    print OUT_FILE "/* $fname, ECC */\n";
    print OUT_FILE "static const unsigned char $sname\[] =\n";
    print OUT_FILE "{\n";
    file_to_hex($fname);
    print OUT_FILE "};\n";
    print OUT_FILE "static const int sizeof_$sname = sizeof($sname);\n\n";
}

print OUT_FILE "#endif /* NO_FILESYSTEM */\n\n";
print OUT_FILE "#endif /* WOLFSSL_CERTS_TEST_H */\n\n";

# close certs_test.h file
close OUT_FILE or die $!;

# print file as hex, comma-separated, as needed by C buffer
sub file_to_hex {
    my $fileName = $_[0];

    open my $fp, "<", $fileName or die $!;
    binmode($fp);

    my $fileLen = -s $fileName;
    my $byte;

    for (my $i = 0, my $j = 1; $i < $fileLen; $i++, $j++)
    {
        if ($j == 1) {
            print OUT_FILE "\t";
        }
        read($fp, $byte, 1) or die "Error reading $fileName";
        my $output = sprintf("0x%02X", ord($byte));
        print OUT_FILE $output;

        if ($i != ($fileLen - 1)) {
            print OUT_FILE ", ";
        }

        if ($j == 10) {
            $j = 0;
            print OUT_FILE "\n";
        }
    }

    print OUT_FILE "\n";

    close($fp);
}

