#!/usr/bin/perl

use Config::IniFiles;

my $src = shift @ARGV;
die 'Syntax: vece_to_bin.pl <src> [<dst>]' if (!defined $src);
my $dst = shift @ARGV;
$dst = $src.'.bin.gz' if (!defined $dst);

my $cfg = Config::IniFiles->new( -file => $src );
die join(' ','Cannot parse tr.vece:',@Config::IniFiles::errors) if (!defined $cfg);

die 'TR-DEBUG section required' if (! $cfg->SectionExists('TR-DEBUG'));
die 'TR-ENDUSER section required' if (! $cfg->SectionExists('TR-ENDUSER'));

my %ids;
foreach my $type ('TR-DEBUG','TR-ENDUSER')
  {
  foreach my $id ($cfg->Parameters($type))
    {
    $ids{$id}{$type} = $cfg->val($type,$id);
    }
  }

open my $df, '|-', "gzip --best --stdout --name >$dst";
foreach my $id (sort {$a <=> $b} keys %ids)
  {
  my $enduser = $ids{$id}{'TR-ENDUSER'}; $enduser='' if (!defined $enduser);
  my $debug = $ids{$id}{'TR-DEBUG'}; $debug='' if (!defined $debug);
  print $df pack('nCC',$id,length($enduser),length($debug)),$enduser,$debug;
  }
close $df;

print "Processed ",(scalar keys %ids)," from $src to $dst\n";
exit(0);

