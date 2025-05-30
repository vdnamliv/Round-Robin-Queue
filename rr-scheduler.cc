#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-queue-disc-item.h"
#include <iomanip>

using namespace ns3;

// Định nghĩa thành phần log
NS_LOG_COMPONENT_DEFINE ("ns3rrQueueDisc");

// Custom Packet Filter for RRQueueDisc
class RRPacketFilter : public PacketFilter
{
public:
  static TypeId GetTypeId (void);
  RRPacketFilter () {}
  virtual ~RRPacketFilter () {}

private:
  virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const override;
  virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const override;
};

NS_OBJECT_ENSURE_REGISTERED (RRPacketFilter);

TypeId
RRPacketFilter::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RRPacketFilter")
    .SetParent<PacketFilter> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<RRPacketFilter> ();
  return tid;
}

bool
RRPacketFilter::CheckProtocol (Ptr<QueueDiscItem> item) const
{
  return DynamicCast<Ipv4QueueDiscItem> (item) != nullptr;
}

int32_t
RRPacketFilter::DoClassify (Ptr<QueueDiscItem> item) const
{
  Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
  if (!ipv4Item)
    return -1;

  Ipv4Header ipHeader = ipv4Item->GetHeader ();
  uint32_t srcAddr = ipHeader.GetSource ().Get ();
  return srcAddr % 3; // 3 sub-queues
}

// Custom Round Robin QueueDisc
class RRQueueDisc : public QueueDisc
{
public:
  static TypeId GetTypeId (void);
  RRQueueDisc();
  virtual ~RRQueueDisc();

  uint64_t GetEnqueued () const { return m_enqueued; }
  uint64_t GetDropped () const { return m_dropped; }

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item) override;
  virtual Ptr<QueueDiscItem> DoDequeue (void) override;
  virtual bool CheckConfig (void) override;
  virtual void InitializeParams (void) override;

  uint32_t m_sub;          // Number of sub-queues
  uint32_t m_deqNext;      // Next sub-queue to dequeue
  uint64_t m_enqueued;     // Total enqueued packets
  uint64_t m_dropped;      // Total dropped packets
};

// Đăng ký lớp với NS-3
NS_OBJECT_ENSURE_REGISTERED (RRQueueDisc);

TypeId
RRQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RRQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<RRQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The max queue size",
                   QueueSizeValue (QueueSize ("100p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("SubQueues",
                   "Số lượng hàng đợi con",
                   UintegerValue (3),
                   MakeUintegerAccessor (&RRQueueDisc::m_sub),
                   MakeUintegerChecker<uint32_t> (1));
  return tid;
}

RRQueueDisc::RRQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES),
    m_sub (3),
    m_deqNext (0),
    m_enqueued (0),
    m_dropped (0)
{
  NS_LOG_FUNCTION (this);
}

RRQueueDisc::~RRQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
RRQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  // Phân loại gói tin bằng PacketFilter
  int32_t flowId = Classify (item);
  if (flowId < 0 || flowId >= static_cast<int32_t> (m_sub))
    {
      NS_LOG_LOGIC ("Invalid flow ID -- dropping pkt");
      m_dropped++;
      DropBeforeEnqueue (item, "Invalid flow ID");
      return false;
    }

  // Kiểm tra kích thước hàng đợi chung
  if (GetCurrentSize () + item > GetMaxSize ())
    {
      NS_LOG_LOGIC ("Queue full -- dropping pkt");
      m_dropped++;
      DropBeforeEnqueue (item, "Queue disc limit exceeded");
      return false;
    }

  // Xếp gói tin vào hàng đợi con
  bool retval = GetInternalQueue (flowId)->Enqueue (item);
  if (retval)
    {
      m_enqueued++;
      NS_LOG_LOGIC ("Packet enqueued to sub-queue " << flowId);
    }

  NS_LOG_LOGIC ("Number packets in sub-queue " << flowId << ": " << GetInternalQueue (flowId)->GetNPackets ());
  NS_LOG_INFO ("Stats: Enqueued=" << m_enqueued << ", Dropped=" << m_dropped);
  return retval;
}

Ptr<QueueDiscItem>
RRQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  for (uint32_t i = 0; i < m_sub; ++i)
    {
      uint32_t index = (m_deqNext + i) % m_sub;
      Ptr<QueueDiscItem> item = GetInternalQueue (index)->Dequeue ();
      if (item)
        {
          m_deqNext = (index + 1) % m_sub;
          NS_LOG_LOGIC ("Packet dequeued from sub-queue " << index);
          return item;
        }
    }

  NS_LOG_LOGIC ("All sub-queues empty");
  return nullptr;
}

bool
RRQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);

  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("RRQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () == 0)
    {
      Ptr<RRPacketFilter> filter = CreateObject<RRPacketFilter> ();
      AddPacketFilter (filter);
    }

  if (GetNInternalQueues () == 0)
    {
      // Thêm các hàng đợi DropTail cho mỗi sub-queue
      for (uint32_t i = 0; i < m_sub; ++i)
        {
          AddInternalQueue (CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>> (
              "MaxSize", QueueSizeValue (QueueSize ("100p"))));
        }
    }

  if (GetNInternalQueues () != m_sub)
    {
      NS_LOG_ERROR ("RRQueueDisc needs " << m_sub << " internal queues");
      return false;
    }

  return true;
}

void
RRQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  m_deqNext = 0;
  m_enqueued = 0;
  m_dropped = 0;
}

int
main (int argc, char *argv[])
{
  double simTime = 30.0;
  uint32_t nSenders = 3;
  uint32_t packetSize = 1500;
  std::string dataRate = "0.33Mbps";
  std::string bottleneckRate = "1Mbps";
  std::string bottleneckDelay = "5ms";

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("nSenders", "Number of sender nodes", nSenders);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "Data rate per flow", dataRate);
  cmd.AddValue ("bottleneckRate", "Bottleneck link data rate", bottleneckRate);
  cmd.AddValue ("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
  cmd.Parse (argc, argv);

  // Bật log
  LogComponentEnable ("ns3rrQueueDisc", LOG_LEVEL_INFO);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer senders;
  senders.Create (nSenders);
  NodeContainer router;
  router.Create (1);
  NodeContainer receiver;
  receiver.Create (1);

  // Configure point-to-point links
  PointToPointHelper pFast;
  pFast.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pFast.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper pSlow;
  pSlow.SetDeviceAttribute ("DataRate", StringValue (bottleneckRate));
  pSlow.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
  pSlow.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  // Install devices
  std::vector<NetDeviceContainer> senderDevices;
  senderDevices.push_back (pFast.Install (senders.Get (0), router.Get (0)));
  senderDevices.push_back (pFast.Install (senders.Get (1), router.Get (0)));
  senderDevices.push_back (pFast.Install (senders.Get (2), router.Get (0)));
  NetDeviceContainer bottleneckDevices = pSlow.Install (router.Get (0), receiver.Get (0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> senderInterfaces;
  address.SetBase ("10.0.1.0", "255.255.255.0");
  senderInterfaces.push_back (address.Assign (senderDevices[0]));
  address.SetBase ("10.0.2.0", "255.255.255.0");
  senderInterfaces.push_back (address.Assign (senderDevices[1]));
  address.SetBase ("10.0.3.0", "255.255.255.0");
  senderInterfaces.push_back (address.Assign (senderDevices[2]));
  address.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterfaces = address.Assign (bottleneckDevices);

  // Bật định tuyến toàn cục
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install custom RRQueueDisc
  TrafficControlHelper tch;
  tch.Uninstall (router.Get (0)->GetDevice (nSenders));
  tch.SetRootQueueDisc ("ns3::RRQueueDisc");
  QueueDiscContainer qdiscs = tch.Install (router.Get (0)->GetDevice (nSenders));
  Ptr<QueueDisc> rootQdisc = qdiscs.Get (0);
  rootQdisc->Initialize ();

  // Install OnOff and PacketSink applications
  uint16_t port = 9000;
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (receiver.Get (0));
  sinkApps.Start (Seconds (0.5));
  sinkApps.Stop (Seconds (simTime));

  auto InstallFlow = [&] (Ptr<Node> src, double start, uint16_t clientPort, uint32_t flowId)
  {
    OnOffHelper onoff ("ns3::UdpSocketFactory",
                       Address (InetSocketAddress (bottleneckInterfaces.GetAddress (1), clientPort)));
    onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
    onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app = onoff.Install (src);
    app.Start (Seconds (start));
    app.Stop (Seconds (simTime));
    NS_LOG_INFO ("Flow " << flowId << " sending to " << bottleneckInterfaces.GetAddress (1));
  };
  InstallFlow (senders.Get (0), 1.0, port, 1);
  InstallFlow (senders.Get (1), 1.2, port, 2);
  InstallFlow (senders.Get (2), 1.4, port, 3);

  // Install FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Get queue statistics
  Ptr<RRQueueDisc> rrq = DynamicCast<RRQueueDisc> (rootQdisc);
  uint64_t enq = rrq->GetEnqueued ();
  uint64_t drop = rrq->GetDropped ();
  double rate = enq ? 100.0 * drop / enq : 0.0;

  std::cout << "\n=====  SIMPLE ROUND-ROBIN QUEUE  =====\n"
            << "Enqueued packets : " << enq << "\n"
            << "Dropped packets  : " << drop << "\n"
            << "Drop rate        : " << std::fixed
            << std::setprecision (2) << rate << " %\n"
            << "=======================================\n";

  // Process FlowMonitor results
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::vector<double> throughputs;
  std::vector<double> delays;

  double totalThroughput = 0.0;
  double totalDelay = 0.0;
  uint64_t totalPackets = 0;
  uint64_t totalLost = 0;

  for (const auto& stat : stats)
    {
      Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow (stat.first);
      double duration = stat.second.timeLastRxPacket.GetSeconds () - stat.second.timeFirstTxPacket.GetSeconds ();
      double throughput = duration > 0 ? stat.second.rxBytes * 8.0 / duration / 1e6 : 0.0; // Mbps
      double delay = stat.second.rxPackets > 0 ? (stat.second.delaySum.GetSeconds () / stat.second.rxPackets) * 1000 : 0.0; // ms
      double lossRatio = (stat.second.txPackets + stat.second.lostPackets) > 0 ? (100.0 * stat.second.lostPackets / (stat.second.txPackets + stat.second.lostPackets)) : 0.0;
      throughputs.push_back (throughput);
      delays.push_back (delay);
      totalThroughput += throughput;
      totalDelay += delay;
      totalPackets += stat.second.rxPackets;
      totalLost += stat.second.lostPackets;

      NS_LOG_INFO ("Flow " << stat.first << ": txPackets=" << stat.second.txPackets << ", rxPackets=" << stat.second.rxPackets << ", lostPackets=" << stat.second.lostPackets);

      std::cout << "Flow " << stat.first << " (" << tuple.sourceAddress << ":" << tuple.sourcePort
                << " -> " << tuple.destinationAddress << ":" << tuple.destinationPort << ")\n"
                << "  Throughput: " << throughput << " Mbps\n"
                << "  Mean Delay: " << delay << " ms\n"
                << "  Packet Loss Ratio: " << lossRatio << "%\n";
    }

  // Calculate Jain's Fairness Index
  double sum = 0.0, sumSq = 0.0;
  for (double t : throughputs)
    {
      sum += t;
      sumSq += t * t;
    }
  double fairnessIndex = throughputs.empty () ? 0.0 : (sum * sum) / (throughputs.size () * sumSq);

  // Calculate average delay and jitter
  double avgDelay = stats.empty () ? 0.0 : totalDelay / stats.size ();
  double jitter = 0.0;
  for (double d : delays)
    {
      jitter += std::pow (d - avgDelay, 2);
    }
  jitter = delays.empty () ? 0.0 : std::sqrt (jitter / delays.size ());

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "Overall Metrics:\n"
            << "  Total Throughput: " << totalThroughput << " Mbps\n"
            << "  Average Delay: " << avgDelay << " ms\n"
            << "  Jitter: " << jitter << " ms\n"
            << "  Total Packet Loss Ratio: " << (totalPackets + totalLost > 0 ? totalLost * 100.0 / (totalPackets + totalLost) : 0.0) << "%\n"
            << "  Jain's Fairness Index: " << fairnessIndex << std::endl;

  // Lưu FlowMonitor output
  monitor->SerializeToXmlFile ("rr-scheduler-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}
