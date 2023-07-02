#! /usr/bin/perl

use WWW::Google::Translate;
use File::Slurp;
use Data::Dumper;
use JSON::PP;

use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");
binmode(STDERR, ":utf8");

# ------ TRANSLATION ---------------------------------------------------------------------------

my $google_translate_api_key = "YOUR-KEY-GOES-HERE";

my $de_to_en = WWW::Google::Translate->new({
	key => $google_translate_api_key,
	default_source => 'de',
	default_target => 'en'
});

my %previous_translations;

sub de_to_en {
	($text_de) = @_;
	my $previously = $previous_translations{$text_de};
	return $previously if ($previously);

	my $en = $de_to_en->translate({ q=>$text_de });
	#print Dumper($en);
	if ($en->{data} && $en->{data}->{translations}) {
		my $ent =  $en->{data}->{translations}[0]{translatedText};
		$previous_translations{$text_de} = $ent;
		return $ent;
	}
}

sub test_translation {
    my $de = "17-stellige Fahrgestellnummer. Zum Zurücksetzen im Steuergerät wird das Argument '00000000000000000' verwendet. Hinweis: Der Argumentwert '00000000000000000' wird SGBD-intern in 0xFF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF gewandelt, bevor er an das CAS gesendet wird.";

    my $en = de_to_en($de);
    print "$de\n$en\n";

    print de_to_en("Differenz zwischen dem mittleren und minimalen Ladezsustand aller Zellen bezogen auf Nennkapazität") . "\n";

    my $en = de_to_en($de);
    print "Again:\n$de\n$en\n";
}

# test_translation;
# exit 99;

# ------ TRANSLATION ---------------------------------------------------------------------------

# ------ READ ECU DESCRIPTION  -----------------------------------------------------------------

my $tables = {};
my $tablename;
my $table;
my @headings;

while (<>) {
    chomp($_);
    if ( $_ eq "TEND" ) {
	$tables->{$tablename} = $table;
	$table = undef;
    }
    elsif ( $_ =~ /^TBEG *"(.*?)"/ ) {
	$tablename = $1;
	$table = [];
	print STDERR "$tablename\n";
    }
    elsif ( $_ =~ /^HEAD *"(.*?)"$/ ) {
	# $1 =~ s/^"//;  $1 =~ s/"$//;
	@headings = split(/", ?"/, $1);
    }
    elsif ( $_ =~ /^LINE *"(.*?)"$/ ) {
	my %line;
	@line{@headings} = split(/", ?"/, $1);
	foreach my $name ( qw(INFO TEXT ORTTEXT ARTTEXT) ) {
	    if ($line{$name} && $line{$name} ne '' ) {                   # && $tablename eq "SG_FUNKTIONEN") {
		$line{"${name}_EN"} = de_to_en($line{$name});
	    }
	}
	push $table, \%line;
    }
}

# ------ SHOW RESULTS  -------------------------------------------------------------------------

print STDERR Dumper($tables->{"SG_FUNKTIONEN"});

my $json = JSON::PP->new->pretty; # ->utf8(true);

print $json->encode( { tables => $tables } );
