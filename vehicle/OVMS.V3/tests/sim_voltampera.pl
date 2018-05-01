#!/usr/bin/perl

use IPC::Open2;

my %pr =
  (
  "000d" => 20,
  "4369" => 0,
  "4368" => 0,
  "801f" => 120,
  "801e" => 130,
  "434f" => 140,
  "1c43" => 110,
  "8334" => 126,
  "2487" => 0
  );

my $vehicle = 'devbench.local';
my $port = 3000;

my($chld_out, $chld_in);
my $pid = open2($chld_out, $chld_in, "nc $vehicle $port");

select $chld_in; $| = 1;
select STDOUT; $| = 1;

print "Simulation running with pid #$pid\n";
while (<$chld_out>)
  {
  chop;
  print "$_\n";
  if (/^\d+\.\d+ .R11 (\S+)\s*(.*)/)
    {
    # Incoming can message
    my ($id,$rest) = ($1,$2);
    my (@bytes) = split(/ /,$rest);
    if (($bytes[0] eq '03')&&($bytes[1] eq '22'))
      {
      my $rid = sprintf("%03x",hex($id)+8);
      my $pid = $bytes[2].$bytes[3];
      my $response = sprintf "0.0 1R11 %s 03 62 %s %s %02x 00 00 00",$rid,$bytes[2],$bytes[3],$pr{$pid};
      print "Request $id #$pid, response $response\n";
      print $chld_in $response,"\n";
      }
    }
  }
