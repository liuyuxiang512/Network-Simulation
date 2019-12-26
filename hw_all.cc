/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Yuxiang Liu    516021910395
*
* Work according to the project requirements of CS339 except for routing protocol
*
* This program reads an upper triangular adjacency matrix (e.g. adjacency_matrix.txt).
* The program also set-ups a wired network topology with P2P links according to the
* adjacency matrix with traffic flows.
*/

// ---------- Header Includes -------------------------------------------------
#include <iostream> 
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <time.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace std;
using namespace ns3;

// ---------- Prototypes ------------------------------------------------------
int max_q = 0;
int current_max_q = 0;

vector<vector<bool> > readNxNMatrix(string adj_mat_file_name);
void printMatrix(const char* description, vector<vector<bool> > array);

void TcPacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
	if (max_q <= int(newValue)) max_q = int(newValue);
	if (current_max_q <= int(newValue)) current_max_q = int(newValue);
}

void getInfo()
{
	//output the max queue for current time slot
	cout << "  Max Queue at ";
	cout << Simulator::Now().GetSeconds() << "\t" << current_max_q << endl;
	//reset for the next time slot
	current_max_q = 0;
}

NS_LOG_COMPONENT_DEFINE("GenericTopologyCreation");

int main(int argc, char *argv[])
{
	// ---------- Simulation Variables ------------------------------------------

	double SimTime = 120.00;
	double StartTime = 0.00001;
	double StopTime = 0.10000;
	int PacketSize = 210;

	string AppPacketRate("448Kbps");  // 210 bytes per packet and 3.75ms interval of packets
	string LinkRate("10Mbps");
	string LinkDelay("2ms");

	srand((unsigned)time(NULL));   // generate different seed each time for couples of hosts

	string tr_name("hw.tr");
	string flow_name("hw.xml");

	string adj_mat_file_name("scratch/adjacency_matrix.txt");

	CommandLine cmd;
	cmd.Parse(argc, argv);

	// ---------- End of Simulation Variables ----------------------------------

	// ---------- Read Adjacency Matrix ----------------------------------------

	vector<vector<bool> > Adj_Matrix;
	Adj_Matrix = readNxNMatrix(adj_mat_file_name);
	int n_nodes = Adj_Matrix.size();
	//printMatrix (adj_mat_file_name.c_str (),Adj_Matrix);

	// ---------- End of Read Adjacency Matrix ---------------------------------

	// ---------- Network Setup ------------------------------------------------
	NS_LOG_INFO("Create Nodes.");

	// Declare nodes objects
	NodeContainer nodes;
	nodes.Create(n_nodes);

	NS_LOG_INFO("Create P2P Link Attributes.");
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(LinkRate));
	p2p.SetChannelAttribute("Delay", StringValue(LinkDelay));
	p2p.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue(1));
	Ptr<QueueDisc> q[Adj_Matrix.size()][Adj_Matrix[0].size()];

	NS_LOG_INFO("Install Internet Stack to Nodes.");

	InternetStackHelper internet;
	internet.Install(NodeContainer::GetGlobal());

	NS_LOG_INFO("Set Queue.");
	TrafficControlHelper tch;
	uint16_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc");
	// Add the internal queue used by Red
	// 2M buffer for 210 bytes per packet, approximately 10000
	tch.AddInternalQueues(handle, 1, "ns3::DropTailQueue", "MaxPackets", UintegerValue(10000));

	NS_LOG_INFO("Assign Addresses to Nodes.");
	Ipv4AddressHelper ipv4_n;
	ipv4_n.SetBase("10.0.0.0", "255.255.255.0");

	NS_LOG_INFO("Create Links Between Nodes.");
	uint32_t linkCount = 0;

	for (size_t i = 0; i < Adj_Matrix.size(); i++)
	{
		for (size_t j = 0; j < Adj_Matrix[i].size(); j++)
		{
			if (Adj_Matrix[i][j] == 1)
			{
				NodeContainer n_links = NodeContainer(nodes.Get(i), nodes.Get(j));
				NetDeviceContainer n_devs = p2p.Install(n_links);

				QueueDiscContainer qdiscs = tch.Install(n_devs);
				q[i][j] = qdiscs.Get(1);
				q[i][j]->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueueTrace));

				ipv4_n.Assign(n_devs);
				ipv4_n.NewNetwork();
				linkCount++;
				NS_LOG_INFO("matrix element [" << i << "][" << j << "] is 1");
			}
			else
			{
				NS_LOG_INFO("matrix element [" << i << "][" << j << "] is 0");
			}
		}
	}
	NS_LOG_INFO("Number of links in the adjacency matrix is: " << linkCount);
	NS_LOG_INFO("Number of all nodes is: " << nodes.GetN());
	NS_LOG_INFO("Initialize Global Routing.");
	// Create router nodes, initialize routing database and set up the routing tables in the nodes.
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	// ---------- End of Network Set-up ----------------------------------------

	// ---------- Create Flows -------------------------------------
	NS_LOG_INFO("Setup Packet Sinks.");

	uint16_t port = 9;

	// Create a packet sink to receive these packets
	PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
	ApplicationContainer apps_sink[25][1200];
	ApplicationContainer apps[25][1200];
	int n_couple[1200];

	for (int t = 0; t < 1200; t++) {
		cout << endl << " *** *** *** StartTime:  " << StartTime + t * 0.1 << "  StopTime:  " << StopTime + t * 0.1 << " *** *** *** " << endl;

		// Create random number of random couples of hosts
		cout << endl << "*** Hosts Couples Info ***" << endl;
		n_couple[t] = (rand() % 25) + 1;  //n_couple[t] in [1,25]
		cout << "  Number of hosts couples  " << n_couple[t] << endl;

		for (int k = 0; k < n_couple[t]; k++) {
			//i, j in [0,50)
			int i = (rand() % 50), j;
			do {
				j = (rand() % 50);
			} while (j == i);
			cout << "  Client:  " << j << "  Server:  " << i << endl;

			// traffic flows from node[i] to node[j]
			apps_sink[k][t] = sink.Install(nodes.Get(i));
			apps_sink[k][t].Start(Seconds(StartTime + 0.1 * t));
			apps_sink[k][t].Stop(Seconds(StopTime + 0.1 * t));

			NS_LOG_INFO("Setup Traffic Sources.");

			// Create the OnOff application to send UDP datagrams of size 210 bytes at a rate of 448 Kb/s
			Ptr<Node> n = nodes.Get(j);
			Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
			Ipv4InterfaceAddress ipv4_int_addr = ipv4->GetAddress(1, 0);
			Ipv4Address ip_addr = ipv4_int_addr.GetLocal();
			OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ip_addr, port));
			onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.00375]"));
			onoff.SetAttribute("PacketSize", UintegerValue(PacketSize));  // int PacketSize = 210
			onoff.SetConstantRate(DataRate(AppPacketRate));  // string AppPacketRate("448Kbps")
			apps[k][t] = onoff.Install(nodes.Get(i));
			apps[k][t].Start(Seconds(StartTime + 0.1 * t));
			apps[k][t].Stop(Seconds(StopTime + 0.1 * t));
		}
	}

	// ---------- End of Create Flows ------------------------------

	// ---------- Simulation Monitoring ----------------------------------------
	NS_LOG_INFO("Configure Tracing.");

	AsciiTraceHelper ascii;
	p2p.EnableAsciiAll(ascii.CreateFileStream(tr_name.c_str()));

	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();

	NS_LOG_INFO("Run Simulation.");

	for (double s = 0.1000; s <= 120.00; s += 0.1000) {
		Simulator::Schedule(Seconds(s), &getInfo);
	}

	Simulator::Stop(Seconds(SimTime));
	Simulator::Run();
	flowmon->SerializeToXmlFile(flow_name.c_str(), true, true);

	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

	cout << endl << "*** Flow monitor statistics ***" << endl;
	int st = 1;
	for (int t = 0; t < 1200; t++) {
		cout << "  Time Slot:   " << t+1 << endl;
		cout << "*** *** *** *** ***" << endl;
		double throughput = 0.0, current_throughput;
		double delay = 0.0, current_delay;
		double loss = 0.0, current_loss;
		double jitter = 0.0, current_jitter;
		for (int num = st; num < st + n_couple[t]; num++) {
			cout << "  Flow ID:   " << num << endl;
			cout << "  Tx Packets:   " << stats[num].txPackets << endl;
			cout << "  Rx Packets:   " << stats[num].rxPackets << endl;
			current_loss = 1.0 - double(stats[num].rxPackets) / stats[num].txPackets;
			loss += current_loss;
			cout << "  Loss Rate:  " << current_loss << endl;

			current_throughput = stats[num].rxBytes * 8.0 / (stats[num].timeLastRxPacket.GetSeconds() - stats[num].timeFirstRxPacket.GetSeconds()) / 1000000;
			throughput += current_throughput;
			cout << "  Throughput: " << current_throughput << " Mbps" << endl;

			current_delay = stats[num].delaySum.GetSeconds() / stats[num].rxPackets;
			delay += current_delay;
			cout << "  Mean delay:   " << current_delay << endl;

			current_jitter = stats[num].jitterSum.GetSeconds() / (stats[num].rxPackets - 1);
			jitter += current_jitter;
			cout << "  Mean jitter:   " << current_jitter << endl;

			cout << "*** *** *** *** ***" << endl;
		}
		cout << "  Mean Delay:  ";
		//cout << StartTime + 0.1*t << '\t';
		cout << delay / n_couple[t] << endl;

		cout << "  Mean Throughput:  ";
		//cout << StartTime + 0.1*t << '\t';
		cout << throughput / n_couple[t] << endl;

		cout << "  Mean Loss Rate:  ";
		//cout << StartTime + 0.1*t << '\t';
		cout << loss / n_couple[t] << endl;

		cout << "  Mean Jitter:  ";
		//cout << StartTime + 0.1*t << '\t';
		cout << jitter / n_couple[t] << endl;

		cout << "*** *** *** *** *** *** *** *** *** ***" << endl;
		st += n_couple[t];
	}

	cout << "  Max Queue:   ";
	cout << max_q << endl;

	Simulator::Destroy();

	// ---------- End of Simulation Monitoring ---------------------------------

	return 0;
}

// ---------- Function Definitions -------------------------------------------

vector<vector<bool> > readNxNMatrix(string adj_mat_file_name)
{
	ifstream adj_mat_file;
	adj_mat_file.open(adj_mat_file_name.c_str(), ios::in);
	if (adj_mat_file.fail())
	{
		NS_FATAL_ERROR("File " << adj_mat_file_name.c_str() << " not found");
	}
	vector<vector<bool> > array;
	int i, j, m, n;

	string line;
	getline(adj_mat_file, line);
	istringstream iss(line);
	vector<bool> row;

	iss >> m >> n;
	for (i = 0; i < m; i++) {
		row.push_back(0);
	}
	for (i = 0; i < m; i++) {
		array.push_back(row);
	}

	while (!adj_mat_file.eof())
	{
		string line;
		getline(adj_mat_file, line);
		istringstream iss(line);
		iss >> i >> j;
		if (line == "")
		{
			NS_LOG_WARN("WARNING: Ignoring blank row in the array: " << i);
			break;
		}
		array[i][j] = 1;
		array[j][i] = 1;
	}
	
	adj_mat_file.close();
	return array;
}

void printMatrix(const char* description, vector<vector<bool> > array)
{
	cout << "**** Start " << description << "********" << endl;
	for (size_t m = 0; m < array.size(); m++)
	{
		for (size_t n = 0; n < array[m].size(); n++)
		{
			cout << array[m][n] << ' ';
		}
		cout << endl;
	}
	cout << "**** End " << description << "********" << endl;
}

// ---------- End of Function Definitions ------------------------------------