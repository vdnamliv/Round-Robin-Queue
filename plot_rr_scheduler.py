import pandas as pd
import matplotlib.pyplot as plt

# Đọc file CSV
csv_file = 'rr-scheduler-results.csv'
df = pd.read_csv(csv_file)

# Lọc các dòng chi tiết và tổng quan
df_detailed = df.dropna(subset=['Flow'])
df_summary = df[df['Flow'].isna()]

# Tính nFlows từ số dòng chi tiết
n_flows = df_detailed.groupby(['Bandwidth', 'QueueSize'])['Flow'].count().rename('nFlows').reset_index()
df_detailed = df_detailed.merge(n_flows, on=['Bandwidth', 'QueueSize'])

# Tính trung bình các chỉ số
df_grouped = df_detailed.groupby(['Bandwidth', 'QueueSize', 'nFlows']).agg({
    'Throughput': 'mean',
    'LossRate': 'mean',
    'AvgDelay': 'mean'
}).reset_index()

# Gộp JainFairness từ dòng tổng quan
df_summary = df_summary.rename(columns={'JainFairness': 'JainFairness_summary'})
df_grouped = df_grouped.merge(df_summary[['Bandwidth', 'QueueSize', 'JainFairness_summary']], 
                             on=['Bandwidth', 'QueueSize'], how='left')
df_grouped['JainFairness'] = df_grouped['JainFairness_summary'].fillna(0)  # Giá trị mặc định 0 nếu thiếu

# Thêm DataRate (ước lượng)
df_grouped['DataRate'] = df_grouped['Throughput'] * df_grouped['nFlows']

# 1. Throughput vs. DataRate
plt.figure(figsize=(10, 6))
for n_flow in df_grouped['nFlows'].unique():
    data = df_grouped[df_grouped['nFlows'] == n_flow]
    plt.plot(data['DataRate'], data['Throughput'], marker='o', label=f'nFlows = {n_flow}')
plt.xlabel('DataRate (Mbps)')
plt.ylabel('Average Throughput per Flow (Mbps)')
plt.title('Throughput vs. DataRate')
plt.legend()
plt.grid(True)
plt.savefig('throughput_vs_datarate.png')
plt.close()

# 2. PLR vs. QueueSize
plt.figure(figsize=(10, 6))
for datarate in df_grouped['DataRate'].unique():
    data = df_grouped[df_grouped['DataRate'] == datarate]
    plt.plot(data['QueueSize'], data['LossRate'], marker='o', label=f'DataRate = {datarate:.2f} Mbps')
plt.xlabel('QueueSize (packets)')
plt.ylabel('Average Loss Rate (%)')
plt.title('PLR vs. QueueSize')
plt.legend()
plt.grid(True)
plt.savefig('plr_vs_queuesize.png')
plt.close()

# 3. Mean Delay vs. BottleneckRate
plt.figure(figsize=(10, 6))
for queuesize in df_grouped['QueueSize'].unique():
    data = df_grouped[df_grouped['QueueSize'] == queuesize]
    plt.plot(data['Bandwidth'], data['AvgDelay'], marker='o', label=f'QueueSize = {queuesize}')
plt.xlabel('Bottleneck Rate (Mbps)')
plt.ylabel('Average Delay (ms)')
plt.title('Mean Delay vs. BottleneckRate')
plt.legend()
plt.grid(True)
plt.savefig('delay_vs_bottleneck.png')
plt.close()

# 4. JainFairness vs. nFlows
plt.figure(figsize=(10, 6))
for datarate in df_grouped['DataRate'].unique():
    data = df_grouped[df_grouped['DataRate'] == datarate]
    plt.plot(data['nFlows'], data['JainFairness'], marker='o', label=f'DataRate = {datarate:.2f} Mbps')
plt.xlabel('Number of Flows')
plt.ylabel('Jain Fairness Index')
plt.title('JainFairness vs. nFlows')
plt.legend()
plt.xticks([2, 3, 4])
plt.grid(True)
plt.savefig('jainfairness_vs_nflows.png')
plt.close()

print("Biểu đồ đã được lưu dưới dạng file PNG trong thư mục hiện tại.")
