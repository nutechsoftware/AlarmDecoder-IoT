mbedTLS certificates
***
For older espressif the public certificate needs to be downloaded from the site a module uses and placed into this folder and manually loaded during the the mbedTLS connection.

   Example using openssl command on Linux.
```console
openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null
```

Newer Espressif now has a certificate bundle api with common certificates that can be automatically linked into the firmware and loaded by the mbedTLS api. https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html
