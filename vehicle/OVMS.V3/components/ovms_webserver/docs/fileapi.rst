========
File API
========

The file API enables web clients to read and write files at arbitrary VFS locations.

- API URL: ``/api/file``

- Methods:

  - ``GET`` -- read file
  - ``POST`` -- write file

- Parameters:

  - ``path`` -- the full path to the file
  - ``content`` -- the new file content on ``POST``

- Output:

  - HTTP status: 200 (OK) or 400 (Error)
  - HTTP body: file content on ``GET`` or error message

On writing, missing directories along the path will be created automatically.


-------------
Usage Example
-------------

The API can be accessed easily in the web frontend with the jQuery AJAX methods:

.. code-block:: javascript

  // Write object JSON encoded into file:
  var json = JSON.stringify(object);
  $.post("/api/file", { "path": "/sd/mystore/file1", "content": json })
    .done(function() {
      confirmdialog('Saved', '<p>File has been saved.</p>', ["OK"], 2);
    })
    .fail(function(jqXHR) {
      confirmdialog('Save failed', jqXHR.responseText, ["OK"]);
    });

  // Read JSON encoded object from file:
  $.get("/api/file", { "path": "/sd/mystore/file1" })
    .done(function(responseText) {
      var object = JSON.parse(responseText);
      // … process object …
    })
    .fail(function(jqXHR) {
      confirmdialog('Load failed', jqXHR.responseText, ["OK"]);
    });


---------------
External Access
---------------

From external clients, the API can be used by either registering a session cookie or by 
supplying the ``apikey`` parameter (as explained in the general authorization overview).

**Read example**::

  curl "http://192.168.4.1/api/file?apikey=password&path=/sd/mystore/file1"

**Write example**::

  curl "http://192.168.4.1/api/file" -d 'apikey=password' \
    -d 'path=/sd/mystore/file1' --data-urlencode 'content@localfile'


