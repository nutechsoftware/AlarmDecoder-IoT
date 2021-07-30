Creating a file ex "index.html" with the same name but add .tpl extesion ex "index.html.tpl" will cause
the web server code to process the file as a template.

The file can be zero length.

Enable template processing for index.html then upload the sketch data.
  \> touch index.html.tpl

Files with .gz extension will be returned when the partial request file is used.
ex. /favicon.ico will be sent from favicon.ico.gz if it exists and the server
will inform the client on the stream being compressed data so it can decompress it.

Template macros:
 ${0}  - Device Version string
 ${1}  - Uptime string DDDDd:HHh:MMm:SSs
 ${2}  - This device ipv4 address HOST
 ${3}  - Web client ipv4 address
 ${4}  - Protocol [HTTP/HTTPS]
 ${5}  - Device UUID
 ${6}  -
 ${7}  -
 ${8}  -
 ${9}  -
 ${10} -
