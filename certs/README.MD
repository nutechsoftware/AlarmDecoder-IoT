mbedTLS certificates
***
Newer Espressif now has a certificate bundle api with common certificates that can be automatically linked into the firmware and loaded by the mbedTLS api. https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html

  FIXME: 2021-07-19: Must have at least one certificate in this folder if MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE_PATH is
  enabled. For now keeping this dummy cert here.

Place user specific certificates here to be included in the certificate bundle by adding the PEM file here.

   Example using openssl command on Linux.
```console
openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null
```