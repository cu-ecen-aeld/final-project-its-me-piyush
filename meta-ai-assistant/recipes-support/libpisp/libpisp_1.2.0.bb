SUMMARY = "Raspberry Pi ISP library for PiSP hardware"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3417a46e992fdf62e5759fba9baef7a7"

SRC_URI = "git://github.com/raspberrypi/libpisp.git;protocol=https;branch=main"
SRCREV = "v1.2.0"
PV = "1.2.0"

S = "${WORKDIR}/git"

inherit meson pkgconfig

DEPENDS = "nlohmann-json boost"

FILES:${PN} = "${libdir}/libpisp*.so.* ${datadir}/libpisp"
FILES:${PN}-dev = "${libdir}/libpisp*.so ${includedir} ${libdir}/pkgconfig"

INSANE_SKIP:${PN} += "buildpaths"
INSANE_SKIP:${PN}-dev += "buildpaths"
