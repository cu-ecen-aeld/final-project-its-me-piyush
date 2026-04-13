SRCREV = "593f63bf981de1a572bbb46e79e7d8b169e96fae"
PV = "1.7.0+git"
SRC_URI:remove = "file://0002-Revert-Support-compressed-pixel-formats-when-saving-.patch"

PACKAGECONFIG[libav] = "-Denable_libav=enabled, -Denable_libav=disabled, libav"
PACKAGECONFIG[drm] = "-Denable_drm=enabled, -Denable_drm=disabled, libdrm"
PACKAGECONFIG[egl] = "-Denable_egl=enabled, -Denable_egl=disabled, virtual/egl"
PACKAGECONFIG[opencv] = "-Denable_opencv=enabled, -Denable_opencv=disabled, opencv"
PACKAGECONFIG[qt] = "-Denable_qt=enabled, -Denable_qt=disabled, qtbase"
PACKAGECONFIG[tflite] = "-Denable_tflite=enabled, -Denable_tflite=disabled"

FILES:${PN} += " \
    ${datadir}/rpi-camera-assets \
    ${datadir}/rpi-camera-assets/* \
    ${libdir}/rpicam-apps-postproc/*.so \
    ${libdir}/rpicam-apps-preview/*.so \
"

INSANE_SKIP:${PN}-dev += "buildpaths"
