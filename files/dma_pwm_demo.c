/*
 * Chức năng
 * - Tạo ra dma_src.txt và dma_dst.txt.
 * - CPU đọc dma_src.txt vào bộ nhớ.
 * - DMA sao chép nội dung đó sang một vùng nhớ khác.
 * - Trong lúc DMA sao chép, CPU nhấp nháy LED trên GPIO18.
 * - CPU ghi nội dung từ bộ nhớ đích ra file dma_dst.txt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

// ----- Các định nghĩa phần cứng và Mailbox -----
#define BCM_PERI_BASE       0x3F000000
#define DMA_BASE            (BCM_PERI_BASE + 0x007000)
#define DMA_LEN             0x1000
#define PAGE_SIZE           4096

#define IOCTL_MBOX_PROPERTY _IOWR(100, 0, char *)
#define MEM_FLAG_L1_NONALLOCATING ((1 << 2) | (1 << 2))

typedef struct {
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t next_cb;
    uint32_t rsvd[2];
} __attribute__((aligned(32))) dma_cb_t;

typedef struct {
    uint32_t cs;
    uint32_t conblk_ad;
} dma_chan_regs_t;

#define DMA_TI_SRC_INC      (1 << 8)
#define DMA_TI_DEST_INC     (1 << 4)
#define DMA_CS_RESET        (1 << 31)
#define DMA_CS_ACTIVE       (1 << 0)
#define DMA_CHAN            5

#define SRC_FILENAME "dma_src.txt"
#define DST_FILENAME "dma_dst.txt"
#define SRC_CONTENT "nguyenquangminh_22139041\n"
#define GPIO_PATH "/sys/class/gpio"
#define GPIO_NUMBER "18"

static int mbox_fd = -1;
// ----- Các hàm Mailbox (giữ nguyên) -----
uint32_t mem_alloc(uint32_t size, uint32_t align, uint32_t flags) {
    uint32_t mbox_buf[32] __attribute__((aligned(16)));
    mbox_buf[0] = 9 * 4; mbox_buf[1] = 0;
    mbox_buf[2] = 0x3000c; mbox_buf[3] = 12; mbox_buf[4] = 0;
    mbox_buf[5] = size; mbox_buf[6] = align; mbox_buf[7] = flags;
    mbox_buf[8] = 0;
    if (ioctl(mbox_fd, IOCTL_MBOX_PROPERTY, mbox_buf) < 0) return 0;
    return mbox_buf[5];
}
uint32_t mem_lock(uint32_t handle) {
    uint32_t mbox_buf[32] __attribute__((aligned(16)));
    mbox_buf[0] = 7 * 4; mbox_buf[1] = 0;
    mbox_buf[2] = 0x3000d; mbox_buf[3] = 4; mbox_buf[4] = 0;
    mbox_buf[5] = handle; mbox_buf[6] = 0;
    if (ioctl(mbox_fd, IOCTL_MBOX_PROPERTY, mbox_buf) < 0) return 0;
    return mbox_buf[5];
}
void mem_free(uint32_t handle) {
    uint32_t mbox_buf[32] __attribute__((aligned(16)));
    mbox_buf[0] = 7 * 4; mbox_buf[1] = 0;
    mbox_buf[2] = 0x3000f; mbox_buf[3] = 4; mbox_buf[4] = 0;
    mbox_buf[5] = handle; mbox_buf[6] = 0;
    ioctl(mbox_fd, IOCTL_MBOX_PROPERTY, mbox_buf);
}

// ----- Các hàm tiện ích -----
int write_buffer_to_file(const char* path, const char* content, int size) {
    int fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) return -1;
    write(fd, content, size);
    close(fd);
    return 0;
}
// ----- Hàm CPU nhấp nháy LED (giữ nguyên) -----
void cpu_blink_task(int duration_sec) {
    printf("[CPU] Task started: Blinking GPIO%s for %d seconds.\n", GPIO_NUMBER, duration_sec);
    char direction_path[64], value_path[64];
    snprintf(direction_path, sizeof(direction_path), "%s/gpio%s/direction", GPIO_PATH, GPIO_NUMBER);
    snprintf(value_path, sizeof(value_path), "%s/gpio%s/value", GPIO_PATH, GPIO_NUMBER);
    int export_fd = open(GPIO_PATH "/export", O_WRONLY);
    if (export_fd >= 0) { write(export_fd, GPIO_NUMBER, strlen(GPIO_NUMBER)); close(export_fd); }
    usleep(100000);
    int dir_fd = open(direction_path, O_WRONLY);
    if (dir_fd >= 0) { write(dir_fd, "out", 3); close(dir_fd); }
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < duration_sec) {
        int val_fd = open(value_path, O_WRONLY); if(val_fd >= 0) { write(val_fd, "1", 1); close(val_fd); }
        usleep(300000);
        val_fd = open(value_path, O_WRONLY); if(val_fd >= 0) { write(val_fd, "0", 1); close(val_fd); }
        usleep(300000);
    }
    int unexport_fd = open(GPIO_PATH "/unexport", O_WRONLY);
    if (unexport_fd >= 0) { write(unexport_fd, GPIO_NUMBER, strlen(GPIO_NUMBER)); close(unexport_fd); }
    printf("[CPU] Task finished.\n");
}

int main() {
    if (getuid() != 0) { fprintf(stderr, "Error: Must be run as root.\n"); return 1; }

    // BƯỚC 1: CPU chuẩn bị file
    printf("[PREPARE] Creating source and destination files...\n");
    if (write_buffer_to_file(SRC_FILENAME, SRC_CONTENT, strlen(SRC_CONTENT)) != 0) {
        perror("Failed to create src file"); return 1;
    }
    if (write_buffer_to_file(DST_FILENAME, "", 0) != 0) {
        perror("Failed to create dst file"); return 1;
    }
    printf("[PREPARE] Files are ready.\n");

    // Mở các device cần thiết
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    mbox_fd = open("/dev/vcio", 0);
    if (mem_fd < 0 || mbox_fd < 0) { perror("Error opening devices"); return 1; }

    // Map thanh ghi DMA
    void *dma_virt_addr = mmap(NULL, DMA_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DMA_BASE);
    volatile dma_chan_regs_t *dma_regs = (dma_chan_regs_t *)(dma_virt_addr + 0x100 * DMA_CHAN);

    // BƯỚC 2: Cấp phát bộ nhớ DMA và chép nội dung file nguồn vào
    size_t content_len = strlen(SRC_CONTENT);
    uint32_t src_handle = mem_alloc(PAGE_SIZE, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING);
    uint32_t dst_handle = mem_alloc(PAGE_SIZE, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING);
    uint32_t cb_handle  = mem_alloc(PAGE_SIZE, PAGE_SIZE, MEM_FLAG_L1_NONALLOCATING);
    uint32_t src_bus = mem_lock(src_handle);
    uint32_t dst_bus = mem_lock(dst_handle);
    uint32_t cb_bus  = mem_lock(cb_handle);
    void* src_virt = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, src_bus & ~0xC0000000);
    void* dst_virt = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, dst_bus & ~0xC0000000);
    dma_cb_t* cb_virt = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, cb_bus & ~0xC0000000);
    
    // CPU chép nội dung vào bộ nhớ DMA nguồn
    memcpy(src_virt, SRC_CONTENT, content_len);
    memset(dst_virt, 0, content_len);

    // BƯỚC 3: Thiết lập Control Block và kích hoạt DMA
    cb_virt->ti = DMA_TI_SRC_INC | DMA_TI_DEST_INC;
    cb_virt->source_ad = src_bus;
    cb_virt->dest_ad = dst_bus;
    cb_virt->txfr_len = content_len;
    cb_virt->next_cb = 0;
    //chỗ này là gọi DMA
    dma_regs->cs = DMA_CS_RESET; usleep(100);
    dma_regs->conblk_ad = cb_bus;
    dma_regs->cs = DMA_CS_ACTIVE;
    printf("\n--- [MAIN] Real DMA copy (memory to memory) initiated. ---\n\n");

    // BƯỚC 4: CPU làm việc khác (nhấp nháy LED)
    cpu_blink_task(5);

    // BƯỚC 5: Chờ DMA hoàn thành
    printf("\n--- [MAIN] CPU finished task. Waiting for DMA transfer to complete... ---\n");
    while (dma_regs->cs & DMA_CS_ACTIVE) { usleep(1000); }
    printf("--- [MAIN] DMA transfer has ended. ---\n\n");

    // BƯỚC 6: CPU ghi kết quả từ bộ nhớ DMA đích ra file
    if (write_buffer_to_file(DST_FILENAME, dst_virt, content_len) != 0) {
        perror("Failed to write to destination file");
    } else {
        printf("[FINALIZE] Content successfully written to %s\n", DST_FILENAME);
    }

    // BƯỚC 7: Xác minh bằng cách so sánh file
    printf("\n[VERIFY] FINAL FILE CONTENTS:\n");
    // (In ra để trực quan)
    printf("  - dma_src.txt: %s", SRC_CONTENT);
    printf("  - dma_dst.txt: %.*s\n", (int)content_len, (char*)dst_virt);

    if (memcmp(src_virt, dst_virt, content_len) == 0) {
        printf("[RESULT] SUCCESS: Destination file content matches source.\n");
    } else {
        printf("[RESULT] FAILURE: Content does not match.\n");
    }

    // Dọn dẹp
    mem_free(src_handle); mem_free(dst_handle); mem_free(cb_handle);
    close(mbox_fd); close(mem_fd);
    printf("=== End demo ===\n");
    return 0;
}
