#!/usr/bin/perl

use Getopt::Long;
use Pod::Usage;
use Term::ReadKey;
use EV;
use AnyEvent;
use AnyEvent::ReadLine::Gnu;
use AnyEvent::MQTT;

########################################################################
# Command-Line parameter parsing

=head1 NAME

ovms_v3shell - OVMS v3 shell (over MQTT)

=head1 SYNOPSIS

ovms_v3shell [options]

Examples:

  ovms_v3shell -v

=head1 OPTIONS

=over 8

=item B<verbose>

Be verbose regarding work done.

=item B<server> [server name/address]

Server name (default api.openvehicles.com).

=item B<port> [server port]

Server port (default 1883)

=item B<username> [username]

Username to access server.

=item B<password> [password]

Password to access server.

=item B<id> [vehicleid]

Vehicle ID to access.

=back

=head1 DESCRIPTION

This program provides a textual console to an OVMS module, using
the server v3 (MQTT) protocol.

=cut

our $verbose = 0;       # "1" if verbose output is requested
our $server = 'api.openvehicles.com';
our $port = 1883;
our $username;
our $password;
our $vehicleid;
our $prefix;

GetOptions(
            'verbose'     => \$verbose,
            'server=s'    => \$server,
            'port=i'      => \$port,
            'username=s'  => \$username,
            'password=s'  => \$password,
            'prefix=s'    => \$prefix,
            'id=s'        => \$vehicleid,
            'help|?'      => \$help,
            'man'         => \$man
          ) or pod2usage(2);

pod2usage(1) if $help;
pod2usage( exitstatus => 0, verbose => 2 ) if ($man);

# STDOUT is non-buffered
select STDOUT;
$| = 1;

# Argument sanity checking

if (!defined $username)
  {
  print STDERR "User name (--username) must be specified\n";
  exit(1);
  }
elsif (!defined $vehicleid)
  {
  print STDERR "Vehicle ID (--id) must be specified\n";
  exit(1);
  }

if (!defined $password)
  {
  ReadMode('noecho');
  print "Password: ";
  $password = ReadLine(0);
  chomp $password;
  ReadMode 'normal';
  print "\n";
  }

$prefix = "ovms/$username/$vehicleid" if (!defined $prefix);

sub mqtt_response
  {
  my ($topic, $message) = @_;
  AnyEvent::ReadLine::Gnu->print($message);
  }

sub mqtt_error
  {
  my ($fatal,$message) = @_;
  AnyEvent::ReadLine::Gnu->print("MQTT error: $message\n");
  EV::break() if ($fatal);
  }

my $clientid = $ENV{'HOSTNAME'} . ':' . $$;
AnyEvent::ReadLine::Gnu->print("Connecting to $server:$port/$username...\n");
my $mqtt = AnyEvent::MQTT->new(
            host => $server,
            port => $port,
            keep_alive_timer => 60,
            user_name => $username,
            password => $password,
            will_topic => "$prefix/client/$clientid/active",
            will_message => '',
            client_id => $clientid,
            on_error => \&mqtt_error);

my $response_cv = $mqtt->subscribe(topic => "$prefix/client/$clientid/response/#",
                          callback => \&mqtt_response);

$mqtt->publish(topic => "$prefix/client/$clientid/active", message => '1');

my $rl = new AnyEvent::ReadLine::Gnu prompt => "OVMS# ", on_line => sub {
   # called for each line entered by the user
  if (!defined $_[0])
    { EV::break(); }
  else
    {
    $mqtt->publish(topic => "$prefix/client/$clientid/command/1",
                   message => $_[0]);
    }
  };

my $interrupt = AnyEvent->signal(signal => "INT", cb => sub {
  AnyEvent::ReadLine::Gnu->print ("Exit!\n");
  EV::break();
  });

EV::run();

print "\n";

