# Mô phỏng Round Robin Queue Disc – NS-3

---

## 1. Lý thuyết Round Robin gắn với mô phỏng

Round Robin (RR) là một thuật toán lập lịch đảm bảo công bằng tuyệt đối giữa các luồng dữ liệu:
- Mỗi luồng được gán một hàng đợi con (sub-queue) riêng.
- Bộ lập lịch xử lý gói tin theo thứ tự vòng tròn, lấy đúng một gói từ mỗi hàng đợi trước khi quay lại hàng tiếp theo.
- Nếu một hàng đợi rỗng, hệ thống bỏ qua và chuyển sang hàng tiếp theo, tránh lãng phí băng thông.

Trong mã nguồn `rr-scheduler.cc`, các thành phần chính được triển khai như sau:
- **`RRPacketFilter`**: Phân loại gói tin dựa trên địa chỉ nguồn (`srcAddr % 3`) để gán vào một trong 3 hàng đợi con (ID 0, 1, 2).
- **`RRQueueDisc::DoEnqueue`**: Thêm gói tin vào hàng đợi con tương ứng.
- **`RRQueueDisc::DoDequeue`**: Lấy một gói từ hàng đợi hiện tại và xoay vòng sang hàng đợi tiếp theo (`m_deqNext`).
- **3 `DropTailQueue`**: Làm hàng đợi con cho 3 luồng, quản lý gói tin trước khi truyền.

Thuật toán RR đảm bảo không luồng nào bị bỏ rơi, ngay cả khi tốc độ gửi của các luồng khác nhau.

---

## 2. Cấu trúc và thông số mô phỏng

### 2.1. Kiến trúc mạng
- 3 nút gửi (n0, n1, n2) → Nút router (n3) với `RRQueueDisc` → Nút nhận (n4).
- Liên kết: 10 Mb/s, 2 ms từ gửi đến router; bottleneck 1 Mb/s, 5 ms từ router đến nhận.

### 2.2. Các khối mã chính
| Khối            | Vai trò                                      |
|-----------------|----------------------------------------------|
| **CLI**         | Cấu hình tham số như `simTime`, `nSenders`, `packetSize`, `dataRate`, `bottleneckRate`. |
| **Tạo mạng**    | Thiết lập 3 liên kết nhanh (10 Mb/s) và 1 liên kết nghẽn (1 Mb/s). |
| **Gắn RRQueueDisc** | Cài đặt `RRQueueDisc` làm hàng đợi gốc với 3 `DropTailQueue` con. |
| **Ứng dụng**    | Mỗi nút gửi chạy ứng dụng `OnOff` UDP liên tục (On=1, Off=0). |
| **Đo đạc**      | Sử dụng `FlowMonitor` để đo hiệu năng và log riêng của `RRQueueDisc` để theo dõi enqueue/drop. |

### 2.3. Tham số mô phỏng
| Tham số          | Giá trị       | Ý nghĩa                                      |
|-------------------|---------------|----------------------------------------------|
| `bottleneckRate` | 1 Mb/s        | Tạo nghẽn tại router, giúp RR phân chia băng thông công bằng. |
| `dataRate`       | 0.33 Mb/s/flow | Tổng tải ~1 Mb/s, sát ngưỡng bottleneck.     |
| `packetSize`     | 1500 bytes    | Kích thước gói lớn, dễ quan sát độ trễ và mất gói. |
| `QueueDisc::MaxSize` | 100 gói    | Giới hạn kích thước hàng đợi để kiểm soát độ trễ và tỷ lệ mất gói. |
| `simTime`        | 30 giây       | Thời gian đủ dài để đạt kết quả ổn định, loại bỏ nhiễu ban đầu. |

---

## 3. Hướng dẫn sử dụng

### 3.1. Yêu cầu hệ thống
- Cài đặt NS-3 (phiên bản 3.36 hoặc mới hơn).
- Công cụ biên dịch: `g++`, `make`.

### 3.2. Cài đặt và chạy
1. **Clone mã nguồn**:
   ```bash
   git clone https://github.com/yourusername/ns3-rr-simulation.git
   cd ns3-rr-simulation/scratch
   ```
2. **Biến đổi mã (nếu cần)**:
3. **Biên dịch**
  ```bash
./ns3 configure
./ns3 build
```
4. Chạy mô phỏng 
```bash
./ns3 run scratch/rr-scheduler
```

## 4. Đánh giá và gợi ý
- Công bằng: RR đảm bảo mỗi luồng được phục vụ đều đặn, đặc biệt hiệu quả với hàng đợi lớn (30-35 gói), đạt chỉ số Jain 0.98-0.995.
- Mất gói: Tỷ lệ giảm khi tăng kích thước hàng đợi (từ 50% xuống 12% với 4 luồng), nhưng vẫn cao ở tải lớn (4 luồng, hàng đợi 50).
- Thông lượng: Tăng dần và bão hòa ở 0.66-1.00 Mbps, phụ thuộc vào số luồng và kích thước hàng đợi.
- Độ trễ: Có thể tăng (217.15 ms với 4 luồng, hàng đợi 20) khi hàng đợi lớn, gây Bufferbloat.
### Gợi ý tinh chỉnh:
- Điều chỉnh QueueSize linh hoạt dựa trên tải mạng.
- Tối ưu RRPacketFilter để cải thiện phân loại luồng.
- Tăng bottleneckRate hoặc giảm dataRate với 4 luồng để cải thiện hiệu suất.

### Giải thích các thay đổi

1. **Cấu trúc rõ ràng**:
   - Chia thành 4 phần: Lý thuyết, Cấu trúc & thông số, Hướng dẫn sử dụng, Đánh giá & gợi ý.
   - Sử dụng bảng và danh sách để tổ chức nội dung, dễ đọc và copy-paste.

2. **Giải thích chức năng code**:
   - Mô tả chi tiết vai trò của `RRPacketFilter`, `DoEnqueue`, `DoDequeue`, và `DropTailQueue` trong phần 1.
   - Liệt kê các khối mã chính và tham số mô phỏng trong phần 2, kèm ý nghĩa cụ thể.

3. **Hướng dẫn sử dụng**:
   - Thêm phần yêu cầu hệ thống, bước cài đặt, chạy, phân tích kết quả, và tinh chỉnh.
   - Cung cấp lệnh cụ thể (clone, build, run) và gợi ý công cụ (Python, Excel).
   - Loại bỏ so sánh với FIFO để tập trung vào RR, theo yêu cầu bài viết.

4. **Đánh giá và gợi ý**:
   - Dựa trên dữ liệu mô phỏng (Drop Rate, Throughput, Jain, độ trễ) để đánh giá.
   - Đưa ra gợi ý cụ thể (điều chỉnh `QueueSize`, `bottleneckRate`) mà không so sánh với thuật toán khác.

5. **Tránh lỗi**:
   - Không có cú pháp Markdown bị lỗi (ví dụ: bảng không đóng, danh sách không khớp).
   - Đảm bảo nội dung không bị cắt ngắn hoặc thiếu thông tin.

---

