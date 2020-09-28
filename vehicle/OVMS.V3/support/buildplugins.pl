#!/usr/bin/perl

use JSON::XS;

#my $coder = JSON::XS->new->utf8->pretty->allow_nonref;
my $coder = JSON::XS->new->utf8->allow_nonref;

my $PLUGINS_TREE = './src'; # Path to GIT directory containing OVMS v3 firmware
my $PLUGINS_PATH = $PLUGINS_TREE . '/plugins';
my $REL_PATH     = './dst'; # Path to destination web server directory
my $REV_PATH     = $REL_PATH . '/plugins.rev';
my $REG_PATH     = $REL_PATH . '/plugins.json';

if (! -e $REV_PATH)
  {
  print "Creating new $REV_PATH\n";
  open my $rev, '>', $REV_PATH;
  print $rev 'unknown';
  close $rev;
  }

print "Updating plugin sources...\n";
chdir $PLUGINS_TREE;
system '/usr/bin/git fetch --tags';
system '/usr/bin/git pull';
chdir $PLUGINS_PATH;

my $EXIST_REV = slurp($REV_PATH);
my $PLUGINS_REV = `/usr/bin/git describe --always --tags --dirty`;
print "Plugins Revision: $PLUGINS_REV\n";

if ($EXIST_REV eq $PLUGINS_REV)
  {
  print "Plugins are unchanged. All done.\n";
  exit(0);
  }

# Rebuild the plugin metadata...

my @registry;
print "Building plugin repository...\n";
opendir my $dh, '.' || die "Can't open plugin directory: $!";
while (readdir $dh)
  {
  my $plugin = $_;
  next if ($plugin =~ /^\.\.?$/);
  next if (! -d $plugin);
  print "  $plugin\n";
  my $json = slurp($plugin . '/' . $plugin . '.json');
  eval
    {
    my $data = $coder->decode($json);
    push @registry,$data;
    };
  if ($@)
    {
    die "JSON metadata could not be decoded for '$plugin': $!";
    }
  }
closedir $dh;
print "Loaded metadata for ", (scalar @registry), " plugin(s)\n";

# Write out the metadata registry
print "Writing metadata registry to $REG_PATH\n";
open my $reg, '>', $REG_PATH;
print $reg $coder->encode(\@registry);
close $reg;

# Save the new revision
open my $rev,'>',$REV_PATH;
print $rev $PLUGINS_REV;
close $rev;

# Rsync release

print "Releasing plugins...\n";
system "/usr/bin/rsync -a --delete --delay-updates --exclude='plugins.rev' --exclude='plugins.json' $PLUGINS_PATH/ $REL_PATH";

exit(0);

sub slurp
  {
  my ($path) = @_;

  open my $fh, '<', $path or die "Can't open file $!";
  my $file_content = do { local $/; <$fh> };
  close $fh;

  return $file_content;
  }
