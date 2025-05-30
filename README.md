# Mô phỏng Round Robin Queue Disc – NS-3

---

## 1️⃣ Lý thuyết Round Robin gắn với mô phỏng

Round Robin (RR) là một **thuật toán lập lịch công bằng tuyệt đối**:
1. Giả sử có *n* hàng đợi con (sub-queues), mỗi sub-queue lưu gói của **một luồng (flow)**.
2. Bộ lập lịch lần lượt duyệt từng sub-queue theo **vòng tròn**: dequeue đúng **một** gói từ hàng hiện tại rồi chuyển sang hàng kế tiếp.
3. Nếu hàng đợi đó rỗng, bỏ qua và duyệt tiếp – đảm bảo **mỗi luồng được phục vụ công bằng**.

Trong mã nguồn `RR.txt`, các thành phần chính phản ánh lý thuyết này:
| Thành phần | Vai trò |
|------------|---------|
| `RRPacketFilter` | Gán **ID luồng** (0-1-2) dựa trên `srcAddr % 3`, phân loại gói vào 3 sub-queue. |
| `RRQueueDisc::DoEnqueue` | Đưa gói vào sub-queue tương ứng. |
| `RRQueueDisc::DoDequeue` | Lấy một gói từ hàng hiện tại, rồi **xoay vòng** sang hàng tiếp theo (`m_deqNext`). |
| 3 sub-queues `DropTailQueue` | Làm **hàng đợi con** cho 3 luồng forward. |

Thuật toán này đảm bảo **không flow nào bị bỏ quên**, ngay cả khi tốc độ gửi của các flow khác nhau.

---

## 2️⃣ Cấu trúc & thông số mô phỏng

### 2.1 Kiến trúc mạng
- n0 n1 n2 → n3 (router với RRQueueDisc) → n4 (10 Mb/s, 2 ms) ↳ bottleneck 1 Mb/s, 5 ms

### 2.2 Các khối mã chính
| Khối | Vai trò |
|------|---------|
| **CLI** | `simTime`, `nSenders`, `packetSize`, `dataRate`, `bottleneckRate`… |
| **Tạo mạng** | 3 đường link nhanh (10 Mb/s) và 1 link nghẽn (1 Mb/s). |
| **Gắn RRQueueDisc** | `tch.SetRootQueueDisc("ns3::RRQueueDisc")` với 3 `DropTailQueue`. |
| **Ứng dụng** | Mỗi node gửi `OnOff` UDP liên tục (`On=1`, `Off=0`). |
| **Đo đạc** | Dùng `FlowMonitor` và log riêng của `RRQueueDisc` (enqueue/drop). |

### 2.3 Tham số mô phỏng gốc
| Tham số | Giá trị | Ý nghĩa |
|---------|---------|---------|
| `bottleneckRate` | **1 Mb/s** | Ép router tắc nghẽn, RR phát huy vai trò chia sẻ công bằng. |
| `dataRate` | **0.33 Mb/s** mỗi flow | Tổng ~1 Mb/s – sát ngưỡng đường nghẽn. |
| `packetSize` | **1500 B** | Gói lớn, dễ thấy delay và drop. |
| `QueueDisc::MaxSize` | **100 p** | Hạn chế delay “kéo dài” và drop-rate quá cao. |
| `simTime` | **30 s** | Đủ dài để kết quả ổn định, loại bỏ burst ban đầu. |

---

## 3️⃣ Đánh giá & gợi ý tinh chỉnh

| Mục tiêu | Thông số nên thay đổi | Ghi chú |
|----------|----------------------|---------|
| **Công bằng (Jain)** | `dataRate` từng flow (0.1–1 Mb/s), số sub-queue (`m_sub`) | Kiểm tra RR chia đều “lượt phục vụ” cho nhiều flow. |
| **Drop rate** | `QueueSize` (50–500 pkt), `bottleneckRate` | Queue nhỏ → drop cao, queue lớn → delay cao. |
| **Delay & Jitter** | `bottleneckDelay`, `packetSize` | Xem RR ảnh hưởng độ trễ trung bình & jitter khi nghẽn. |
| **Tổng throughput** | `simTime` (10–60 s) | Thời gian ngắn dễ nhiễu, cần ~30 s để “bình ổn”. |
| **So sánh với FIFO** | Đổi `RRQueueDisc` thành `PfifoFastQueueDisc` | Giữ nguyên kịch bản, chỉ thay queue để so sánh công bằng / drop. |

> ✅ **Mẹo nhanh:**  
> - Giữ `dataRate` tổng ≈ 70–90 % băng thông bottleneck.  
> - `QueueSize 100p` để giữ delay < 150 ms.  
> - Chạy `RR` vs `FIFO` → so sánh throughput, Jain, PLR trong báo cáo.
