PACKAGECONFIG:remove:rpi = "raspberrypi"
EXTRA_OEMESON:append = " -Dpipelines=rpi/vc4,rpi/pisp -Dipas=rpi/vc4,rpi/pisp"
