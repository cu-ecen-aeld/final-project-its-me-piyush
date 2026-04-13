SUMMARY = "llama.cpp - LLM inference"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "git://github.com/ggerganov/llama.cpp.git;protocol=https;branch=master"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=ON \
    -DLLAMA_CURL=OFF \
    -DGGML_OPENSSL=OFF \
    -DLLAMA_SERVER_SSL=OFF \
    -DHTTPLIB_REQUIRE_OPENSSL=OFF \
    -DBUILD_SHARED_LIBS=OFF \
"

do_configure:prepend() {
    # Remove SSL/crypto from httplib vendored library
    find ${S}/vendor -name "CMakeLists.txt" -exec \
        sed -i 's/find_package(OpenSSL/#find_package(OpenSSL/g' {} \;
    find ${S}/vendor -name "CMakeLists.txt" -exec \
        sed -i 's/target_link_libraries.*ssl.*crypto.*//g' {} \;
    # Disable httplib SSL globally
    sed -i 's/#define CPPHTTPLIB_OPENSSL_SUPPORT/\/\/#define CPPHTTPLIB_OPENSSL_SUPPORT/' \
        ${S}/vendor/cpp-httplib/httplib.h 2>/dev/null || true
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/bin/llama-simple ${D}${bindir}/llama-simple
    install -d ${D}/opt/llama.cpp/models
}

FILES:${PN} += " \
    ${bindir}/llama-simple \
    /opt/llama.cpp/models \
"
