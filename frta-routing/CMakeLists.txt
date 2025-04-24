build_lib(
  LIBNAME frta-routing
  SOURCE_FILES
    model/frta-routing-protocol.cc
    model/frta-routing-header.cc
    model/frta-state.cc
    model/frta-collision-detector.cc
    helper/frta-routing-helper.cc
  HEADER_FILES
    model/frta-routing-protocol.h
    model/frta-routing-header.h
    model/frta-state.h
    model/frta-collision-detector.h
    helper/frta-routing-helper.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libapplications}
    ${libmobility}
    ${libwifi}
    ${libflow-monitor}
    ${libnetanim}
)

