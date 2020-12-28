========================
Command Line Interpreter
========================

The command line interpreter or command parser presented by the OVMS
async serial console is constructed as a tree of command word tokens.
Enter a single "?" followed by <RETURN> to get the root command list,
like this::

 OVMS# ?
 .                    Run a script
 bms                  BMS framework
 boot                 BOOT framework
 can                  CAN framework
 ...

A root command may be followed by one of a list of additional tokens
called subcommands which in turn may be followed by further
subcommands down multiple levels.  The command and subcommand tokens
may be followed by parameters.  Use "?" at any level in the command
sequence to get the list of subcommands applicable at that point.  If
the next item to be entered is a parameter rather than a subcommand,
then a usage message will be displayed to indicate the required or
optional parameters.  The usage message will also be shown if the
command as entered is not valid.  The usage message is described in
further detail below.

Command tokens can be abbreviated so long as enough characters are
entered to uniquely identify the command, then optionally pressing
<TAB> will auto-complete the token.  If the abbreviated form is not
sufficient to be unique (in particular if no characters have been
entered yet) then <TAB> will show a concise list of the possible
subcommands and retype the portion of the command line already
entered so it can be completed.  Pressing <TAB> is legal at any point
in the command; if there is nothing more that can be completed
automatically then there will just be no response to the <TAB>.

The ``OvmsCommand::RegisterCommand()`` function is used to add command and
subcommand tokens are added to the tree.  New commands are added to
the root of the tree using the global ``MyCommandApp.RegiserCommand()``.
Subcommands are added as children of a command using the OvmsCommand
pointer returned when ``RegisterCommand()`` is called for the parent, thus
building the tree.  For example::

 OvmsCommand* cmd_wifi = MyCommandApp.RegisterCommand("wifi","WIFI framework", wifi_status);
 cmd_wifi->RegisterCommand("status","Show wifi status",wifi_status);
 cmd_wifi->RegisterCommand("reconnect","Reconnect wifi client",wifi_reconnect);

The ``RegisterCommand()`` function takes the following arguments:

* ``const char* name`` – the command token
* ``const char* title`` – one-line description for command list
* ``void (*execute)(...)`` – does the work of the command
* ``const char *usage`` – parameter description for "Usage:" line
* ``int min`` – minimum number of parameters allowed
* ``int max`` – maximum number of parameters allowed
* ``bool secure`` – true for commands permitted only after *enable*
* ``int (*validate)(...)`` – validates parameters as explained later

It's important to note that many of these arguments can and should be
defaulted.  The default values are as follows:

* ``execute`` = NULL
* ``usage`` = ""
* ``min`` = 0
* ``max`` = 0
* ``secure`` = true
* ``validate`` = NULL

For example, for secure, non-terminal commands (those with child
subcommands), such as the top-level framework commands like *bms* in
the list of root commands shown earlier, the model should simply be::

 RegisterCommand("name", "Title");

For secure, terminal subcommands (those with no children) that don't
require any additional parameters, the model should be::

 RegisterCommand("name", "Title", execute);

This model also applies if the command has children but the command
itself wants to execute a default operation if no subcommand is
specified.  It is incorrect to specify ``min`` = 0, ``max`` = 1 to
indicate an optional subcommand; that is indicated by the presence of
the ``execute`` function at the same time as a non-empty children
array.

Any command with required or optional parameters should provide a
"usage" string hinting about the parameters in addition to
specifying the minimum and maximum number of parameters allowed::

 RegisterCommand("name", "Title", execute, "usage", min, max);

The ``usage`` argument only needs to describe the parameters that
follow this (sub)command because the full usage message is dynamically
generated.  The message begins with the text "Usage: " followed by
names of the ancestors of the this subcommand back to the root of
the tree plus the name of this subcommand itself.  That is, the
message starts with all the tokens entered to this point followed by
additional description of subcommands or parameters that may be
entered next, as determined by the usage string.

The usage string syntax conventions for specifying alternative and
optional parameters are similar to those of usage messages in
Unix-like systems.  The string can also include special codes to
direct the dynamic generation of the message:

* $C expands to the list of children commands as child1|child2|child3.
* [$C] expands to optional children as [child1|child2|child3].
* $G$ expands to the usage string of the first child; this would
  typically used after $C so the usage message shows the list of
  children and then the parameters or next-level subcommands that can
  follow the children.  This is useful when the usage string is the
  same for all or most of the children as in this example::

   Usage: power adc|can1|can2|can3|egpio|esp32|sdcard|simcom|spi|wifi deepsleep|devel|off|on|sleep|status

* $Gfoo$ expands to the usage of the child named "foo"; this variant
  would be used when not all the children have the same usage but it
  would still be helpful to show the usage of one that's not first.
* $L lists a separate full usage message for each of the children.
  This provides more help than just showing the list of children but
  at the expense of longer output.
* For terminal subcommands (those with no children) that take
  additional parameters, the usage string contains explicit text
  to list the parameters:

  * Parameter names or descriptions are enclosed in angle brackets to
    distinguish the them from command tokens, for example "<metric> <value>".
  * Parameters that are optional are further enclosed in square
    brackets, like "<id> <name> [<value>]".
  * When there are alternative forms or meanings for a parameter, the
    alternatives are separated by vertical bar as in "<task names or
    ids>|\*|=" which indicates that the parameter can be either of the
    characters '\*' or '=' instead of a list of task names or ids.  An
    variant form encloses the alternatives in curly braces as in
    "<param> {<instance> | \*}".
  * One or more additional lines of explanatory text can be included
    like this: "<id>\\nUse ID from connection list / 0 to close all".

For non-terminal commands (those with children subcommands) the
``usage`` argument can be omitted because the default value of \"\" is
interpreted as "$C".  For commands that have children subcommands that
are optional (because an ``execute`` function is included) the default
``usage`` argument is interpreted as “[$C]”.

Most commands do not need to specify a ``validate`` function.  It
supports two extensions of the original command parser design:

* For commands that store the possible values of a parameter in a
  NameMap<T> or CNameMap<T>, the ``validate`` function enables <TAB>
  auto-completion when entering that parameter.  For example, see the
  "config" command in main/ovms_config.cpp.
* The original design only allowed parameters to be collected by the
  terminal subcommand.  That forced an unnatural word order for some
  commands.  The ``validate`` function enables non-terminal
  subcommands to take one or more parameters followed by multiple
  children subcommands.  The parameters may allow <TAB> completion if
  the possible values are stored in NameMap<T> or CNameMap<T> or they
  could be something else like a number that can be validated by
  value.  The ``validate`` function must indicate success for parsing
  to continue to the children subcommands.  The return value is the
  number of parameters validated if successful or -1 if not.

The "location" command is an example that includes an intermediate
parameter and also utilizes the "$L" form of the usage string::

 OVMS# location action enter ?
 Usage: location action enter <location> acc <profile>
 Usage: location action enter <location> homelink 1|2|3
 Usage: location action enter <location> notify <text>

See components/ovms_location/src/ovms_location.cpp for the
implementation of the ``location_validate()`` function and the
``RegisterCommand()`` calls to build the command subtree.
