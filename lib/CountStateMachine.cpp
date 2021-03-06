//
// Created by jarod on 2019-06-06.
//

#include "CountStateMachine.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/utilities/checkpoint.h"
#include "servant/Application.h"
#include "RaftNode.h"

const string CountStateMachine::COUNT_TYPE  = "1";
const string CountStateMachine::CIRCLE_TYPE  = "2";

CountStateMachine::CountStateMachine(const string &dataPath)
{
	_raftDataDir = dataPath;

	_onApply[COUNT_TYPE] = std::bind(&CountStateMachine::onCount, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_onApply[CIRCLE_TYPE] = std::bind(&CountStateMachine::onCircle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

CountStateMachine::~CountStateMachine()
{
	close();
}

void CountStateMachine::open(const string &dbDir)
{
	TLOG_DEBUG("db: " << dbDir << endl);

	tars::TC_File::makeDirRecursive(dbDir);

	// open rocksdb data dir
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status = rocksdb::DB::Open(options, dbDir, &_db);
	if (!status.ok()) {
		throw std::runtime_error(status.ToString());
	}
}

void CountStateMachine::close()
{
	if (_db) {
		_db->Close();
		delete _db;
		_db = NULL;
	}
}

void CountStateMachine::onBecomeLeader(int64_t term)
{
	TARS_NOTIFY_NORMAL("onBecomeLeader term:" + TC_Common::tostr(term));
//	LOG_CONSOLE_DEBUG << "onBecomeLeader term:" <<  term << endl;
	TLOG_DEBUG("term:" << term << endl);
}

void CountStateMachine::onBecomeFollower()
{
	TARS_NOTIFY_NORMAL("onBecomeFollower");
	TLOG_DEBUG("onBecomeFollower" << endl);
}

void CountStateMachine::onBeginSyncShapshot()
{
	TARS_NOTIFY_NORMAL("onBeginSyncShapshot");
	TLOG_DEBUG("onBeginSyncShapshot" << endl);
}

void CountStateMachine::onEndSyncShapshot()
{
	TARS_NOTIFY_NORMAL("onEndSyncShapshot");
	TLOG_DEBUG("onEndSyncShapshot" << endl);
}

void CountStateMachine::onStartElection(int64_t term)
{
	TARS_NOTIFY_NORMAL("start election");
}

void CountStateMachine::onJoinCluster()
{
	TARS_NOTIFY_NORMAL("join cluster");
}

void CountStateMachine::onLeaveCluster()
{
	TARS_NOTIFY_NORMAL("leave cluster");
}

int64_t CountStateMachine::onLoadData()
{
	TLOG_DEBUG("onLoadData" << endl);

	string dataDir = getDbDir();

	TC_File::makeDirRecursive(dataDir);

	//??????????????????db??????
	close();

	open(dataDir);

	int64_t lastAppliedIndex = 0;

	string value;
	auto s = _db->Get(rocksdb::ReadOptions(), "lastAppliedIndex", &value);
	if(s.ok())
	{
		lastAppliedIndex = *(int64_t*)value.c_str();

	}
	else if(s.IsNotFound())
	{
		lastAppliedIndex = 0;
	}
	else if(!s.ok())
	{
		TLOG_ERROR("Get lastAppliedIndex error!" << s.ToString() << endl);
		exit(-1);
	}

	TLOG_DEBUG("lastAppliedIndex:" << lastAppliedIndex << endl);

	return lastAppliedIndex;
}

void CountStateMachine::onSaveSnapshot(const string &snapshotDir)
{
	TLOG_DEBUG("onSaveSnapshot:" << snapshotDir << endl);

	rocksdb::Checkpoint *checkpoint = NULL;

	rocksdb::Status s = rocksdb::Checkpoint::Create(_db, &checkpoint);

	assert(s.ok());

	checkpoint->CreateCheckpoint(snapshotDir);

	delete checkpoint;
}

bool CountStateMachine::onLoadSnapshot(const string &snapshotDir)
{
	string dataDir = getDbDir();

	//??????????????????db??????
	close();

	//????????????(????????????)
	TC_File::removeFile(dataDir, true);
	TC_File::makeDirRecursive(dataDir);

	TLOG_DEBUG("copy: " << snapshotDir << " to " << dataDir << endl);

	//???????????????copy???????????????
	TC_File::copyFile(snapshotDir, dataDir);

	onLoadData();
	
    return true;
}

void CountStateMachine::onApply(const char *buff, size_t length, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	TarsInputStream<> is;
	is.setBuffer(buff, length);

	string type;
	is.read(type, 0, true);

	TLOG_DEBUG(type << ", appliedIndex:" << appliedIndex << ", size:" << _onApply.size() << endl);
	LOG_CONSOLE_DEBUG << "length:" << length << ", type:" << type << ", appliedIndex:" << appliedIndex << ", size:" << _onApply.size() << endl;

	auto it = _onApply.find(type);
	assert(it != _onApply.end());

	it->second(is, appliedIndex, callback);
}

void CountStateMachine::onCount(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	TLOG_DEBUG("appliedIndex:" << appliedIndex << endl);

	CountReq req;
	is.read(req, 1, false);

	string key = req.sBusinessName + "-" + req.sKey;

	CountRsp rsp;
	rsp.iCount = req.iDefault;

	rsp.iRet = getNoLock(key, rsp.iCount);

	if(rsp.iRet != RT_SUCC)
	{
		rsp.sMsg = "get data from rocksdb error!";
	}
	else
	{
		rsp.iCount = rsp.iCount + req.iNum;

		string sCount = TC_Common::tostr(rsp.iCount);

		rocksdb::WriteBatch batch;
		batch.Put(key, sCount);
		batch.Put("lastAppliedIndex", rocksdb::Slice((const char *)&appliedIndex, sizeof(appliedIndex)));

		rocksdb::WriteOptions wOption;
		wOption.sync = false;

		auto s = _db->Write(wOption, &batch);

		if(!s.ok())
		{
			rsp.iRet = RT_APPLY_ERROR;
			rsp.sMsg = "save data to rocksdb error!";
			TLOG_ERROR("Put: key:" << key << ", error!" << endl);
			exit(-1);
		}
	}

	if(callback)
	{
		//??????????????????????????????, ????????????
		//????????????????????????????????????, ???????????????
		Base::Count::async_response_count(callback->getCurrentPtr(), rsp.iRet, rsp);
	}
}

void CountStateMachine::onCircle(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	TLOG_DEBUG("appliedIndex:" << appliedIndex << endl);

	CircleReq req;
	is.read(req, 1, false);

	string key = req.sBusinessName + "-" + req.sKey;

	CountRsp rsp;
	rsp.iCount = req.iMinNum;

	rsp.iRet = getNoLock(key, rsp.iCount);

	if(rsp.iRet != RT_SUCC)
	{
		rsp.sMsg = "get data from rocksdb error!";
	}
	else
	{
		rsp.iCount = rsp.iCount + req.iNum;
		if(rsp.iCount > req.iMaxNum)
		{
			rsp.iCount = req.iMinNum;
		}

		string sCount = TC_Common::tostr(rsp.iCount);

		rocksdb::WriteBatch batch;
		batch.Put(key, sCount);
		batch.Put("lastAppliedIndex", rocksdb::Slice((const char *)&appliedIndex, sizeof(appliedIndex)));

		rocksdb::WriteOptions wOption;
		wOption.sync = false;

		auto s = _db->Write(wOption, &batch);

		if(!s.ok())
		{
			rsp.iRet = RT_APPLY_ERROR;
			rsp.sMsg = "save data to rocksdb error!";
			TLOG_ERROR("Put: key:" << key << ", error!" << endl);
			exit(-1);
		}
	}

	if(callback)
	{
		//??????????????????????????????, ????????????
		//????????????????????????????????????, ???????????????
		Base::Count::async_response_circleCount(callback->getCurrentPtr(), rsp.iRet, rsp);
	}
}

int CountStateMachine::get(const QueryReq &req, CountRsp &rsp)
{
	std::string key = req.sBusinessName + "-" + req.sKey;

	rsp.iRet = getNoLock(key, rsp.iCount);

	return rsp.iRet;
}

int CountStateMachine::getNoLock(const string &key, tars::Int64 &count)
{
	std::string value;
	rocksdb::Status s = _db->Get(rocksdb::ReadOptions(), key, &value);
	if (s.ok())
	{
		count = TC_Common::strto<tars::Int64>(value);
	}
	else if (s.IsNotFound()) {
	}
	else
	{
		TLOG_ERROR("Get: " << key << ", error:" << s.ToString() << endl);

		return RT_DATA_ERROR;
	}

	return RT_SUCC;
}
