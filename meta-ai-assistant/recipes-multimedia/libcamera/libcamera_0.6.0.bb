SUMMARY = "Linux libcamera framework - Raspberry Pi fork"
SECTION = "libs"
LICENSE = "GPL-2.0-or-later & LGPL-2.1-or-later"
LIC_FILES_CHKSUM = "\
    file://LICENSES/GPL-2.0-or-later.txt;md5=fed54355545ffd980b814dab4a3b312c \
    file://LICENSES/LGPL-2.1-or-later.txt;md5=2a4f4fd2128ea2f65047ee63fbca9f68 \
"

SRC_URI = "git://github.com/raspberrypi/libcamera.git;protocol=https;branch=next"
SRCREV = "f0e40f1c50bd0afe65727d6e407d0dcb42666ada"
PV = "0.6.0+rpt"
S = "${WORKDIR}/git"

DEPENDS = "libpisp openssl openssl-native meson-native ninja-native \
           python3-native python3-jinja2-native python3-ply-native \
           python3-pyyaml-native gnutls libevent libyaml"

inherit meson pkgconfig python3native

EXTRA_OEMESON = " \
    -Dpipelines=rpi/vc4,rpi/pisp \
    -Dipas=rpi/vc4,rpi/pisp \
    -Dgstreamer=disabled \
    -Dpycamera=disabled \
    -Ddocumentation=disabled \
    -Dtest=false \
    -Dcpp_args=-Wno-unaligned-access \
"

do_package:append() {
    bb.build.exec_func("do_resign_ipa", d)
}

do_resign_ipa() {
    privkey="${WORKDIR}/build/src/ipa-priv-key.pem"
    for dir in "${PKGD}/usr/lib/libcamera" "${PKGD}/usr/lib/libcamera/ipa"; do
        if [ -d "$dir" ] && [ -f "$privkey" ]; then
            for so in "$dir"/*.so; do
                if [ -f "$so" ]; then
                    openssl dgst -sha256 -sign "$privkey" "$so" > "${so}.sign"
                fi
            done
        fi
    done
}

FILES:${PN} = " \
    ${libdir}/libcamera*.so.* \
    ${libdir}/libcamera/*.so \
    ${libdir}/libcamera/*.so.sign \
    ${libdir}/libcamera/ipa/*.so \
    ${libdir}/libcamera/ipa/*.so.sign \
    ${libexecdir}/libcamera/* \
    ${datadir}/libcamera \
    ${bindir}/libcamerify \
"

FILES:${PN}-dev = " \
    ${includedir} \
    ${libdir}/libcamera*.so \
    ${libdir}/pkgconfig \
"


INSANE_SKIP:${PN} += "buildpaths"
INSANE_SKIP:${PN}-dev += "buildpaths"
