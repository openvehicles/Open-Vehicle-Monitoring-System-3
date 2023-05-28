# requirements can't depend on config
idf_component_register(SRCS "src/can.cpp" "src/canformat.cpp" "src/canformat_canswitch.cpp" "src/canformat_crtd.cpp" "src/canformat_gvret.cpp" "src/canformat_lawicel.cpp" "src/canformat_panda.cpp" "src/canformat_pcap.cpp" "src/canformat_raw.cpp" "src/canlog.cpp" "src/canlog_monitor.cpp" "src/canlog_tcpclient.cpp" "src/canlog_tcpserver.cpp" "src/canlog_udpclient.cpp" "src/canlog_udpserver.cpp" "src/canlog_vfs.cpp" "src/canplay.cpp" "src/canplay_vfs.cpp" "src/canutils.cpp"
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "main" "pcp" "ovms_buffer" "mongoose"
                       WHOLE_ARCHIVE)
