# requirements can't depend on config
idf_component_register(SRCS "./crypt_base64.cpp" "./crypt_crc.cpp" "./crypt_hmac.cpp" "./crypt_md5.cpp" "./crypt_rc4.cpp"
                       INCLUDE_DIRS .
                       REQUIRES
                       PRIV_REQUIRES
                       WHOLE_ARCHIVE)
