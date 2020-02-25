=======
Startup
=======

On startup, there is no banner/welcome message from the server. The caller initiates
the protocol by sending a welcome message to the server:

1. For cars

  MP-C <protection scheme> <token> <digest> <car id>

2. For interactive user apps

  MP-A <protection scheme> <token> <digest> <car id>

3. For noninteractive batch apps

  MP-B <protection scheme> <token> <digest> <car id>

4. For servers

  MP-S <protection scheme> <token> <digest> <car id>

The server responds with a welcome message to the caller:

  MP-S <protection scheme> <token> <digest>

