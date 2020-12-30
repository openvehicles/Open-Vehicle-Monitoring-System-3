========================
Command Line Interpreter
========================

The command line interpreter or command parser presented by the OVMS
async serial console is constructed as a tree of command word tokens.
Enter a single question mark followed by RETURN to get the root command list,
like this::

 OVMS# ?
 .                    Run a script
 bms                  BMS framework
 boot                 BOOT framework
 can                  CAN framework
 ...

A root command may be followed by one of a list of additional tokens
called subcommands which in turn may be followed by further
subcommands down multiple levels, forming a tree.  The command and subcommand tokens
may be followed by parameters.  Use question mark at any level in the command
sequence to get the list of subcommands applicable at that point.  If
the next item to be entered is a parameter rather than a subcommand,
then a usage message will be displayed to indicate the required or
optional parameters.  The usage message will also be shown if the
command as entered is not valid.  The usage message is described in
further detail below.

Command tokens can be abbreviated so long as enough characters are
entered to uniquely identify the command.  Optionally pressing
TAB at that point will auto-complete the token.  If the abbreviated form is not
sufficient to be unique (in particular if no characters have been
entered yet) then TAB will show a concise list of the possible
subcommands and retype the portion of the command line already
entered so it can be completed.  Pressing TAB is legal at any point
in the command; if there is nothing more that can be completed
automatically then there will just be no response to the TAB.

OvmsCommand API
---------------

Each command word token in the tree is represented by an
``OvmsCommand`` object.  The ``OvmsCommand::RegisterCommand()``
function is used to create and add a command or subcommand token
object into the tree, returning an ``OvmsCommand*`` pointer to the new
object.  New commands are added to the root of the tree using the
global ``MyCommandApp.RegisterCommand()``.  Subcommands are added as
children of a command by calling ``RegisterCommand()`` using the
``OvmsCommand*`` pointer to the parent object, thus building the tree.
For example:

.. code-block:: C++
  
 OvmsCommand* cmd_wifi = MyCommandApp.RegisterCommand("wifi","WIFI framework", wifi_status);
 cmd_wifi->RegisterCommand("status","Show wifi status",wifi_status);
 cmd_wifi->RegisterCommand("reconnect","Reconnect wifi client",wifi_reconnect);

The ``RegisterCommand()`` function takes the following arguments:

* ``const char* name`` – the command token
* ``const char* title`` – one-line description for the command list
* ``void (*execute)(...)`` – does the work of the command
* ``const char *usage`` – parameter description for "Usage:" message
* ``int min`` – minimum number of parameters allowed
* ``int max`` – maximum number of parameters allowed
* ``bool secure`` – true for commands permitted only after *enable*
* ``int (*validate)(...)`` – validates parameters as explained later

The ``RegisterCommand()`` function tolerates duplicate registrations
of the same ``name`` at the same node of the tree by assuming that the
other arguments are also the same and returning the existing object.
This allows mulitple modules that can be configured independently to
share the same top-level command.  For example, the *obdii* command is
shared by the **vehicle** and **obd2ecu** modules.

Modules that can be dynamically loaded and unloaded must remove their
commands from the tree using ``UnregisterCommand(const char* name)``
before unloading.

It's important to note that many of the arguments to
``RegisterCommand()`` can and should be defaulted.  The default values
are as follows:

.. code-block:: C++

 execute = NULL
 usage = ""
 min = 0
 max = 0
 secure = true
 validate = NULL

For example, for secure, non-terminal commands (those with child
subcommands), such as the top-level framework commands like *bms* in
the list of root commands shown earlier, the model should simply be::

 RegisterCommand("name", "Title");

For secure, terminal subcommands (those with no children) that don't
require any additional parameters, the model should be::

 RegisterCommand("name", "Title", execute);

This model also applies if the command has children but the command
itself wants to execute a default operation if no subcommand is
specified.  It is incorrect to specify ``min = 0``, ``max = 1`` to
indicate an optional subcommand; that is indicated by the presence of
the ``execute`` function along with a non-empty array of child
subcommands.

Any command with required or optional parameters should provide a
"usage" string hinting about the parameters in addition to
specifying the minimum and maximum number of parameters allowed:

.. code-block:: C++

 RegisterCommand("name", "Title", execute, "usage", min, max);

The ``usage`` argument only needs to describe the parameters that
follow this (sub)command because the full usage message is dynamically
generated.  The message begins with the text "Usage: " followed by the
names of the ancestors of this subcommand back to the root of
the tree plus the name of this subcommand itself.  That is, the
message starts with all the tokens entered to this point.  The message
continues with a description of subcommands and/or parameters that may be
entered next, as specified by the ``usage`` string.

.. note:: The usage message is *not* resricted to a single line; the
  ``usage`` string can include additional lines of explanatory text,
  separated by ``\n`` (newline) characters, to help convey the meaning
  of the paramters and the purpose of the command.

The ``usage`` string syntax conventions for specifying alternative and
optional parameters are similar to those of usage messages in
Unix-like systems.  The string can also include special codes to
direct the dynamic generation of the message:

* ``$C`` expands to the list of children commands as ``child1|child2|child3``.
* ``[$C]`` expands to list optional children as ``[child1|child2|child3]``.
* ``$G$`` expands to the usage string of the first child; this would
  typically used after ``$C`` so the usage message shows the list of
  children and then the parameters or next-level subcommands that can
  follow the children.  This is useful when the usage string is the
  same for all or most of the children as in this example::

   Usage: power adc|can1|can2|can3|egpio|esp32|sdcard|simcom|spi|wifi deepsleep|devel|off|on|sleep|status

* ``$Gfoo$`` expands to the usage of the child named "foo"; this variant
  would be used when not all the children have the same usage but it
  would still be helpful to show the usage of one that's not first.
* ``$L`` lists a full usage message for each of the children on separate lines.
  This provides more help than just showing the list of children but
  at the expense of longer output.

For subcommands that take parameters, the ``usage`` string contains
explicit text to list the parameters:

* Parameter names or descriptions are enclosed in angle brackets to
  distinguish the them from command tokens, for example ``<metric> <value>``.
  Since the angle brackets demarcate each parameter, spaces may be
  included in the description.
* Parameters that are optional are further enclosed in square
  brackets, like ``<id> <name> [<value>]``.
* When there are alternative forms or meanings for a parameter, the
  alternatives are separated by vertical bar as in ``<task names or
  ids>|*|=`` which indicates that the parameter can be either of the
  characters ``*`` or ``=`` instead of a list of task names or ids.  A
  variant form encloses the alternatives in curly braces as in
  ``<param> {<instance> | *}``.
* One or more additional lines of explanatory text can be included
  like this::

  "<id>\nUse ID from connection list / 0 to close all"

For non-terminal commands (those with children subcommands) the
``usage`` argument can be omitted because the default value of ``""`` is
interpreted as ``$C``.  For commands that have children subcommands that
are optional (because an ``execute`` function is included) the default
``usage`` argument is interpreted as ``[$C]``.

Execute Function
^^^^^^^^^^^^^^^^

The ``execute`` function performs whatever work is required for the
command.  Its signature is as follows:

.. code-block:: C++

  void (*execute)(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)

* ``int verbosity`` – tells how much output is appropriate (e.g., shell
  vs. SMS)
* ``OvmsWriter* writer`` – object to which output is delivered, e.g. console
* ``OvmsCommand* cmd`` – the command that held the ``execute``
  function pointer
* ``int argc`` – how many parameters are being supplied to the function
* ``const char* const* argv`` – the parameter list

Any output appropriate for the command is accomplished through
``puts()`` or ``printf()`` calls on the ``writer`` object.  The ``cmd``
pointer may allow sharing one ``execute`` function among multiple
related command objects and provides access to members of the command
object such as ``GetName()``.

The ``argc`` count will be constrained to the ``min`` and ``max``
values specified for the ``cmd`` object, so if the minimum and maximum
are the same then the ``execute`` function does not need to check.
However, if parameters are expected then their values must be validated.

Validate Function
^^^^^^^^^^^^^^^^^

Most commands do not need to specify a ``validate`` function.  It
supports extensions of the original command parser design for two use
cases:

1. For commands that store the possible values of a parameter in a
   ``NameMap<T>`` or ``CNameMap<T>``, the ``validate`` function
   enables TAB auto-completion when entering that parameter.

2. The original design only allowed parameters to be collected by the
   terminal subcommand.  That forced an unnatural word order for some
   commands.  The ``validate`` function enables non-terminal
   subcommands to take one or more parameters followed by multiple
   levels of children subcommands.  The parameters may be strings
   looked up in a ``NameMap<T>`` or ``CNameMap<T>`` or they could be
   something else like a number that can be validated by value.  The
   ``validate`` function must indicate success for parsing to continue
   to the children subcommands.  The return value is the number of
   parameters validated if successful or -1 if not.

The signature of the ``validate`` function is as follows:

.. code-block:: C++

 int (*validate)(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)

* ``OvmsWriter* writer`` – object to which output is delivered, e.g. console

* ``OvmsCommand* cmd`` – the command that held the ``validate``
  function pointer

* ``int argc`` – how many parameters are being supplied to the function

* ``const char* const* argv`` – the parameter list

* ``bool complete`` – true for TAB completion of the last parameter
  (case 1), false when validating intermediate parameters before
  calling ``execute`` on the terminal descendant command (case 2)

The ``writer`` and ``cmd`` arguments are the same as for the
``execute`` function.  The ``argc`` count is never more than ``max``
and, if ``complete`` is false, never less than ``min``.  However, when
``complete`` is true to request TAB auto-completion and ``max`` is
greater than 1, ``argc`` will be at least 1 but may be less than
``min`` because it indicates how many parameters have been entered so
far.  The TAB auto-completion is performed on the last parameter
entered after validating any preceding parameters.  If ``min`` and
``max`` are both 1 then it is not necessary to check ``argc``.

If the acceptable values of a parameter are stored in a ``NameMap<T>``
or ``CNameMap<T>``, those maps implement a ``Validate()`` function
that will perform the validation needed for the ``validate``
function covering both the true and false cases of ``complete``.
Those maps also implement a ``FindUniquePrefix()`` function that may
be used to validate preceding parameters for commands that take
multiple parameters.

The ``config_validate()`` function for the *config* command in
``main/ovms_config.cpp`` is an example implementation of use case 1
for a command taking three parameters with TAB auto-completion on the
first two:

.. code-block:: C++

 int config_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
   {
   if (!MyConfig.ismounted())
     return -1;
   // argv[0] is the <param>
   if (argc == 1)
     return MyConfig.m_map.Validate(writer, argc, argv[0], complete);
   // argv[1] is the <instance>
   if (argc == 2)
     {
     OvmsConfigParam* const* p = MyConfig.m_map.FindUniquePrefix(argv[0]);
     if (!p)	// <param> was not valid, so can't check <instance>
       return -1;
     return (*p)->m_map.Validate(writer, argc, argv[1], complete);
     }
   // argv[2] is the value, which we can't validate
   return -1;
   }

The *location* command in
``components/ovms_location/src/ovms_location.cpp`` is an example of
use case 2 as it includes an intermediate parameter and also utilizes
the ``$L`` form of the usage string::

 OVMS# location action enter ?
 Usage: location action enter <location> acc <profile>
 Usage: location action enter <location> homelink 1|2|3
 Usage: location action enter <location> notify <text>

The following excerpt shows the implementation of the
``location_validate()`` function and a subset of the
``RegisterCommand()`` calls to build the command subtree.  This
example shows how simple the validation code can be -- sometimes just
one line to call ``Validate()``.  In this case the code does need to
check ``argc`` because the function is shared by multiple subcommand
objects taking 1 or 2 parameters.

.. code-block:: C++

 int location_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
   {
   if (argc == 1)
     return MyLocations.m_locations.Validate(writer, argc, argv[0], complete);
   return -1;
   }

   OvmsCommand* cmd_location = MyCommandApp.RegisterCommand("location","LOCATION framework");
   OvmsCommand* cmd_action = cmd_location->RegisterCommand("action","Set an action for a location");
   OvmsCommand* cmd_enter = cmd_action->RegisterCommand("enter","Set an action upon entering a location", NULL, "<location> $L", 1, 1, true, location_validate);
   OvmsCommand* enter_homelink = cmd_enter->RegisterCommand("homelink","Transmit Homelink signal");
   enter_homelink->RegisterCommand("1","Homelink 1 signal",location_homelink,"", 0, 0, true);
   enter_homelink->RegisterCommand("2","Homelink 2 signal",location_homelink,"", 0, 0, true);
   enter_homelink->RegisterCommand("3","Homelink 3 signal",location_homelink,"", 0, 0, true);
   cmd_enter->RegisterCommand("acc","ACC profile",location_acc,"<profile>", 1, 1, true);
   cmd_enter->RegisterCommand("notify","Text notification",location_notify,"<text>", 1, INT_MAX, true);
