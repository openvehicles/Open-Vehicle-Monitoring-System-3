#!/usr/bin/perl

my %skus;
if (open my $sku,'<','fasttech.skus.txt')
  {
  while (<$sku>)
    {
    chop;
    my ($sku,$description) = split /,/,$_,2;
    $skus{$sku} = $description;
    }
  close $sku;
  }

while (<>)
  {
  chop;
  if (/^(\d+)\s+(\d+)$/)
    {
    my ($sku,$qty) = ($1,$2);
    my $d = $skus{$sku}; $d='' if (!defined $d);
    printf("%5d %s %s\n",$qty,$sku,$d);
    }
  }
