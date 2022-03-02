//
// Created by jarod on 2019-08-08.
//

#include "CountServer.h"
#include "CountImp.h"
#include "CountStateMachine.h"
#include "RaftImp.h"
#include "RaftOptions.h"

void CountServer::initialize()
{
	string dataPath;
	TC_Config conf;

	if(!TC_Port::getEnv("KUBERNETES_PORT").empty())
	{
		dataPath = "/count-data/";
	}

	//for debug
	if(ServerConfig::DataPath == "./debug-data/")
	{
		_index = TC_Endpoint(getConfig().get("/tars/application/server/Base.CountServer.RaftObjAdapter<endpoint>", "")).getPort();

		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 10101"), TC_Endpoint("tcp -h 127.0.0.1 -p 10401 -t 60000")));
		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 10102"), TC_Endpoint("tcp -h 127.0.0.1 -p 10402 -t 60000")));
		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 10103"), TC_Endpoint("tcp -h 127.0.0.1 -p 10403 -t 60000")));
	}
	else
	{
		addConfig("count.conf");
		conf.parseFile(ServerConfig::BasePath + "count.conf");

		dataPath = conf.get("/root<storage-path>");
	}

	LOG_CONSOLE_DEBUG << "data path:" << ServerConfig::DataPath << ", index:" << _index << ", node size:" << _nodeInfo.nodes.size() << endl;

	RaftOptions raftOptions;
	raftOptions.electionTimeoutMilliseconds = TC_Common::strto<int>(conf.get("/root/raft<electionTimeoutMilliseconds>", "3000"));
	raftOptions.heartbeatPeriodMilliseconds = TC_Common::strto<int>(conf.get("/root/raft<heartbeatPeriodMilliseconds>", "300"));
	raftOptions.snapshotPeriodSeconds       = TC_Common::strto<int>(conf.get("/root/raft<snapshotPeriodSeconds>", "600"));
	raftOptions.maxLogEntriesPerRequest     = TC_Common::strto<int>(conf.get("/root/raft<maxLogEntriesPerRequest>", "100"));
	raftOptions.maxLogEntriesMemQueue       = TC_Common::strto<int>(conf.get("/root/raft<maxLogEntriesMemQueue>", "3000"));
	raftOptions.maxLogEntriesTransfering    = TC_Common::strto<int>(conf.get("/root/raft<maxLogEntriesTransfering>", "1000"));
	raftOptions.dataDir                     = TC_File::simplifyDirectory(dataPath + FILE_SEP + "raft-log-" + TC_Common::tostr(_index));

	TLOG_DEBUG("electionTimeoutMilliseconds:" << raftOptions.electionTimeoutMilliseconds << endl);
	TLOG_DEBUG("heartbeatPeriodMilliseconds:" << raftOptions.heartbeatPeriodMilliseconds << endl);
	TLOG_DEBUG("snapshotPeriodSeconds:" << raftOptions.snapshotPeriodSeconds << endl);
	TLOG_DEBUG("maxLogEntriesPerRequest:" << raftOptions.maxLogEntriesPerRequest << endl);
	TLOG_DEBUG("maxLogEntriesMemQueue:" << raftOptions.maxLogEntriesMemQueue << endl);
	TLOG_DEBUG("maxLogEntriesTransfering:" << raftOptions.maxLogEntriesTransfering << endl);
	TLOG_DEBUG("dataDir:" << raftOptions.dataDir << endl);

	_startCount = TC_Common::strto<int64_t>(conf.get("/root<start-count>", "1"));

	TLOG_DEBUG("start count:" << _startCount << endl);

	onInitializeRaft(raftOptions, "CountObj", dataPath + "CountLog-" + TC_Common::tostr(_index));

}

void CountServer::destroyApp()
{
	_stateMachine->close();

	onDestroyRaft();
}