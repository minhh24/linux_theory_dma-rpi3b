SUMMARY = "Simulated DMA Demonstration"
DESCRIPTION = "A program to demonstrate CPU-independent tasks using threads to simulate DMA."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://dma_pwm_demo.c"

S = "${WORKDIR}"

# Code này không dùng thư viện toán học nên không cần -lm
# Nhưng nó dùng thread, nên ta sẽ thêm -pthread vào do_compile

do_compile() {
    # Thêm cờ -pthread ở cuối để liên kết thư viện POSIX Threads
    ${CC} ${CFLAGS} ${S}/dma_pwm_demo.c -o dma-pwm-demo ${LDFLAGS} -pthread
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 dma-pwm-demo ${D}${bindir}/
}
