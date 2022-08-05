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

	LOG_CONSOLE_DEBUG << ServerConfig::DataPath << endl;
	//for debug
	if(ServerConfig::DataPath == "./debug-data/")
	{
		dataPath = ServerConfig::DataPath;

//		_index = TC_Endpoint(getConfig().get("/tars/application/server/Base.CountServer.RaftObjAdapter<endpoint>", "")).getPort();
//
//		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 20001"), TC_Endpoint("tcp -h 127.0.0.1 -p 30001 -t 60000")));
//		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 20002"), TC_Endpoint("tcp -h 127.0.0.1 -p 30002 -t 60000")));
//		_nodeInfo.nodes.push_back(std::make_pair(TC_Endpoint("tcp -h 127.0.0.1 -p 20003"), TC_Endpoint("tcp -h 127.0.0.1 -p 30003 -t 60000")));
	}
	else
	{
		addConfig("count.conf");
		conf.parseFile(ServerConfig::BasePath + "count.conf");

		dataPath = conf.get("/root<storage-path>");
	}

	LOG_CONSOLE_DEBUG << "data path:" << dataPath << ", index:" << _index << ", node size:" << _nodeInfo.nodes.size() << endl;

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

	onInitializeRaft(raftOptions, "CountObj", dataPath + "CountLog-" + TC_Common::tostr(_index));

	srand(time(NULL));

	TARS_ADD_ADMIN_CMD_NORMAL("count.get", CountServer::cmdGet);
	TARS_ADD_ADMIN_CMD_NORMAL("count.set", CountServer::cmdSet);
	TARS_ADD_ADMIN_CMD_NORMAL("count.circle", CountServer::cmdCircle);
}

void CountServer::destroyApp()
{
	_stateMachine->close();

	onDestroyRaft();
}


//storage.get table mkey ukey
bool CountServer::cmdGet(const string&command, const string&params, string& result)
{
	vector<string> v = TC_Common::sepstr<string>(params, " ");

	if(v.size() >= 2)
	{
		QueryReq skey;
		skey.leader = false;
		skey.sBusinessName = v[0];
		skey.sKey = v[1];

		CountRsp data;
		int ret = _stateMachine->getCount(skey, data);

		if(ret == RT_SUCC)
		{
			result += "data: " + data.writeToJsonString() + "\n";
		}
		else
		{
			result = "error, ret:" + etos((RetValue)ret);
		}
	}
	else
	{
		result = "Invalid parameters.Should be: count.get sBusinessName sKey";
	}
	return true;

}

bool CountServer::cmdSet(const string&command, const string&params, string& result)
{
	vector<string> v = TC_Common::sepstr<string>(params, " ");

	if(v.size() >= 4)
	{
		CountReq data;
		data.sBusinessName = v[0];
		data.sKey = v[1];
		data.iNum = TC_Common::strto<int>(v[2]);
		data.iDefault = TC_Common::strto<int>(v[3]);

		CountRsp rsp;

		CountPrx prx = _raftNode->getBussLeaderPrx<CountPrx>();
		int ret = prx->count(data, rsp);

		if(ret == RT_SUCC)
		{
			result = "count succ";
		}
		else
		{
			result = "set error, ret:" + etos((RetValue)ret);
		}
	}
	else
	{
		result = "Invalid parameters.Should be: count.set sBusinessName sKey value default";
	}
	return true;
}

bool CountServer::cmdCircle(const string&command, const string&params, string& result)
{
	vector<string> v = TC_Common::sepstr<string>(params, " ");

	if(v.size() >= 5)
	{
		CircleReq data;
		data.sBusinessName = v[0];
		data.sKey = v[1];
		data.iNum = TC_Common::strto<int>(v[2]);
		data.iMinNum = TC_Common::strto<int>(v[3]);
		data.iMaxNum = TC_Common::strto<int>(v[4]);

		CountRsp rsp;

		CountPrx prx = _raftNode->getBussLeaderPrx<CountPrx>();
		int ret = prx->circleCount(data, rsp);

		if(ret == RT_SUCC)
		{
			result = "count succ";
		}
		else
		{
			result = "set error, ret:" + etos((RetValue)ret);
		}
	}
	else
	{
		result = "Invalid parameters.Should be: count.circle sBusinessName sKey value min max";
	}
	return true;
}