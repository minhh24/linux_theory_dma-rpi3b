1. tạo metadata và thư mục 
Từ folder ~yocto/poky/build gõ 2 câu lệnh:
bitbake-layers create-layer ../meta-int
bitbake-layers add-layer ../meta-int
Câu lệnh đầu tiên sẽ thực hiện tạo ra meta-data mới với tên tuỳ ý và câu lệnh tiếp theo sẽ tự động thêm meta-data vào file ~yocto/poky/build/conf/bblayers.conf
Tạo thư mục như hình

2.nội dung file https://github.com/minhh24/linux-theory/tree/master

3. chuẩn bị môi trường build
pc84@pc84-Legion:~/yocto-final/poky$ source oe-init-build-env

5. bitbake
trong yocto custom cần phải bitbake để tạo ra file .ipk
pc84@pc84-Legion:~/yocto-final/poky/build$ bitbake dma-pwm-demo
file .ipk sẽ nằm ở tmp/deploy/ipk/cortexa7t2hf-neon-vfpv4/dma-pwm-demo_1.0-r0_cortexa7t2hf-neon-vfpv4.ipk

5. kết nối led và module uart
trên module uart nối GND với GND trên pi. TX nối RX, RX nối TX
led nối (+) vào GPIO18. (-) nối GND

6. coppy file .ipk qua raspi
Sử dụng lệnh: pc84@pc84-Legion:~$ sudo picocom -b 115200 /dev/ttyUSB0
truy cập dưới tên raspberrypi3 login: root
coppy file qua pi3b bằng cách
pc84@pc84-Legion:~/yocto-final/poky/build$ scp tmp/deploy/ipk/cortexa7t2hf-neon-vfpv4/dma-pwm-demo_1.0-r0_cortexa7t2hf-neon-vfpv4.ipk root@192.168.2.1:/tmp/

8. truy cập ssh vào pi qua uart và chạy 
kiểm tra trong /var/volatile/tmp có tmp chưa :
root@raspberrypi3:/var/volatile/tmp# ls
có rồi thì cài file trên pc vừa chuyển qua:root@raspberrypi3:/var/volatile/tmp# opkg install dma-pwm-demo_1.0-r0_cortexa7t2hf-neon-vfpv4.ipk 
lúc đầu tạo file dma_src.txt với nội dung “nguyenquangminh_22139041”. Và tạo thêm file dma_dst.txt nội dung để trống. tất cả nằm trong root@raspberrypi3:/var/volatile/tmp# 

chạy file và ta sẽ thu được kết quả: dma-pwm-demo 

có thể thấy như file này. CPU đã ra lệnh cho DMA coppy chuỗi “nguyenquangminh_22139041” từ dma_src.txt sang dma_dst.txt của folder này và sau khi ra lệnh xong thì CPU đi bink led. Chứng tỏ CPU vừa có thể làm việc khác sau khi ra lệnh cho DMA coppy, CPU không phải bận đi coppy dữ liệu.

kết quả là trong dma_src.txt và dma_dst.txt đều có nội dung “nguyenquangminh_22139041”
Trong đoạn code này, Mailbox đang được dùng theo kiểu Polling.
Giải thích: Hàm mailbox (mem_alloc, mem_lock, mem_free) thực chất chỉ là wrapper để gửi message property request xuống firmware VideoCore (thông qua /dev/vcio).
	- Khi CPU cần cấp phát/lock/free vùng nhớ DMA, nó tự tạo buffer mbox_buf[], gọi ioctl(IOCTL_MBOX_PROPERTY, mbox_buf), rồi sau đó CPU chính nó đọc kết quả trả về ngay trong mảng mbox_buf.
- Không có cơ chế interrupt báo ngược lại khi mailbox có dữ liệu mới. CPU chỉ “đặt câu hỏi” và “chờ lấy câu trả lời” trực tiếp.
	- Như vậy, cơ chế hoạt động đúng với kiểu Polling: CPU chủ động kiểm tra và lấy dữ liệu từ mailbox, không có sự kiện ngắt hoặc callback tự động.
Nói ngắn gọn: Code demo DMA file copy này sử dụng Mailbox Polling mode, vì CPU gửi request rồi tự mình đọc kết quả, chứ không chờ interrupt từ mailbox.


video demo https://www.youtube.com/watch?v=w4BLRHbd6_o

Nguyen Quang Minh - 0916254336


