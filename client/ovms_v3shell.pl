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
our $tls = 0;
our $username;
our $password;
our $vehicleid;
our $prefix;

GetOptions(
            'verbose'     => \$verbose,
            'server=s'    => \$server,
            'port=i'      => \$port,
            'tls'         => \$tls,
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

our $rl;
my $commandsoutstanding = 0;

sub newprompt()
  {
  $rl->set_prompt(
    ($commandsoutstanding == 0)?"OVMS# ":"OVMS($commandsoutstanding)# ");
  $rl->rl_redisplay();
  }

our $clientid;
our $mqtt;

sub mqtt_logout()
  {
  $mqtt->publish(topic => "$prefix/client/$clientid/active", message => '0') if ($mqtt);
  }

sub mqtt_response
  {
  my ($topic, $message) = @_;

  $commandsoutstanding--;
  &newprompt();
  AnyEvent::ReadLine::Gnu->print($message);
  &newprompt();
  }

sub mqtt_event
  {
  my ($topic, $message) = @_;

  AnyEvent::ReadLine::Gnu->print("EVENT  $message\n");
  &newprompt();
  }

sub mqtt_metric
  {
  my ($topic, $message) = @_;

  if ($message ne '')
    {
    $topic = substr($topic,length($prefix)+8);
    $topic =~ s/\//\./g;
    AnyEvent::ReadLine::Gnu->print("METRIC $topic=$message\n");
    &newprompt();
    }
  }

sub mqtt_notify
  {
  my ($topic, $message) = @_;

  if ($message ne '')
    {
    $topic = substr($topic,length($prefix)+8);
    $topic =~ s/\//\./g;
    AnyEvent::ReadLine::Gnu->print("NOTIFY $topic: $message\n");
    &newprompt();
    }
  }

sub mqtt_error
  {
  my ($fatal,$message) = @_;

  AnyEvent::ReadLine::Gnu->print("MQTT ERROR: $message\n");
  &newprompt();
  if ($fatal)
    {
    &mqtt_logout();
    EV::break()
    }
  }

$clientid = $ENV{'HOSTNAME'} . ':' . $$;
AnyEvent::ReadLine::Gnu->print("Connecting to $server:$port/$username...\n");
$mqtt = AnyEvent::MQTT->new(
            host => $server,
            port => $port,
            tls => $tls,
            keep_alive_timer => 60,
            user_name => $username,
            password => $password,
            will_topic => "$prefix/client/$clientid/active",
            will_message => '0',
            client_id => $clientid,
            on_error => \&mqtt_error);

my $response_cv = $mqtt->subscribe(topic => "$prefix/client/$clientid/response/#",
                          callback => \&mqtt_response);
my $event_cv = $mqtt->subscribe(topic => "$prefix/event",
                          callback => \&mqtt_event);
my $metric_cv = $mqtt->subscribe(topic => "$prefix/metric/#",
                          callback => \&mqtt_metric);
my $notify_cv = $mqtt->subscribe(topic => "$prefix/notify/#",
                          callback => \&mqtt_notify);

my $peer_timer = AnyEvent->timer(after => 0, interval => 60, cb => sub {
  $mqtt->publish(topic => "$prefix/client/$clientid/active", message => '1');
  });

$rl = new AnyEvent::ReadLine::Gnu prompt => "OVMS# ", on_line => sub {
   # called for each line entered by the user
  if (!defined $_[0])
    {
    &mqtt_logout();
    EV::break();
    }
  elsif (length($_[0]) != 0)
    {
    $commandsoutstanding++;
    &newprompt();
    $mqtt->publish(topic => "$prefix/client/$clientid/command/1",
                   message => $_[0]);
    }
  };
$rl->ornaments(1);

my $interrupt = AnyEvent->signal(signal => "INT", cb => sub {
  AnyEvent::ReadLine::Gnu->print ("Exit!\n");
  &mqtt_logout();
  EV::break();
  });

EV::run();

print "\n";

