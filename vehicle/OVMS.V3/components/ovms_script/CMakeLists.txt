set(srcs "src/ovms_script.cpp")
set(includes "src")
set(embed)

if (CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE)

  list(APPEND srcs
           "srcduk/ovms_duk_http.cpp"
           "srcduk/ovms_duk_util.cpp"
           "srcduk/ovms_duk_vfs.cpp"
           "srcduk/ovms_duktape.cpp"
     )
  list(APPEND includes
           "srcduk"
           "umm"
     )
  list(APPEND embed "jsmodembed/pubsub.js" "jsmodembed/json.js")

endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes}
                       REQUIRES "ovms_tls"
                       PRIV_REQUIRES "main" "duktape"
                       EMBED_FILES ${embed}
                       WHOLE_ARCHIVE)
