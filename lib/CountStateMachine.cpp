//
// Created by jarod on 2019-06-06.
//

#include "CountStateMachine.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/utilities/checkpoint.h"
#include "servant/Application.h"
#include "RaftNode.h"

const string CountStateMachine::COUNT_TYPE  = "1";
const string CountStateMachine::CIRCLE_TYPE  = "2";
const string CountStateMachine::RANDOM_STRING_TYPE  = "3";
const string CountStateMachine::SET_RANDOM_STRING_TYPE = "4";


//////////////////////////////////////////////////////////////////////////////////////
class TTLCompactionFilter : public rocksdb::CompactionFilter
{
public:
	virtual bool Filter(int /*level*/, const rocksdb::Slice& /*key*/,
			const rocksdb::Slice& existing_value,
			std::string* /*new_value*/,
			bool* /*value_changed*/) const {

		int expireTime = 0;
		TarsInputStream<> is;
		is.setBuffer(existing_value.data(), existing_value.size());
		is.read(expireTime, 0, false);
		if(expireTime !=0 && expireTime < TNOW)
		{
			//过期了!
			return true;
		}

		return false;
	}

	const char *Name() const
	{
		return "Count.TTLCompactionFilter";
	}

};

CountStateMachine::CountStateMachine(const string &dataPath)
{
	_raftDataDir = dataPath;

	_onApply[COUNT_TYPE] = std::bind(&CountStateMachine::onCount, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_onApply[CIRCLE_TYPE] = std::bind(&CountStateMachine::onCircle, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_onApply[RANDOM_STRING_TYPE] = std::bind(&CountStateMachine::onRandomString, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_onApply[SET_RANDOM_STRING_TYPE] = std::bind(&CountStateMachine::onSetRandomString, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

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
	options.level_compaction_dynamic_level_bytes = true;
	options.periodic_compaction_seconds = 3600;

	std::vector<rocksdb::ColumnFamilyDescriptor> column_families;

	vector<string> columnFamilies;

	rocksdb::Status status = rocksdb::DB::ListColumnFamilies(options, dbDir, &columnFamilies);

	if (columnFamilies.empty())
	{
		status = rocksdb::DB::Open(options, dbDir, &_db);
		if (!status.ok())
		{
			throw std::runtime_error(status.ToString());
		}
	}
	else
	{
		std::vector<rocksdb::ColumnFamilyDescriptor> columnFamiliesDesc;
		for (auto &f : columnFamilies)
		{
			rocksdb::ColumnFamilyDescriptor c;

			c.name = f;

			if (c.name != "default" )
			{
				c.options.compaction_filter = new TTLCompactionFilter();
			}

			columnFamiliesDesc.push_back(c);
		}

		std::vector<rocksdb::ColumnFamilyHandle *> handles;
		status = rocksdb::DB::Open(options, dbDir, columnFamiliesDesc, &handles, &_db);
		if (!status.ok())
		{
			TLOG_ERROR("Open " << dbDir << ", error:" << status.ToString() << endl);
			throw std::runtime_error(status.ToString());
		}

		for (auto &h : handles)
		{
			_column_familys[h->GetName()] = h;
		}
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

	//把正在使用的db关闭
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

	//把正在使用的db关闭
	close();

	//非启动时(安装节点)
	TC_File::removeFile(dataDir, true);
	TC_File::makeDirRecursive(dataDir);

	TLOG_DEBUG("copy: " << snapshotDir << " to " << dataDir << endl);

	//把快照文件copy到数据目录
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

	rsp.iRet = getCountNoLock(key, rsp.iCount);

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
		//如果客户端请求过来的, 直接回包
		//如果是其他服务器同步过来, 不用回包了
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

	rsp.iRet = getCountNoLock(key, rsp.iCount);

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
		//如果客户端请求过来的, 直接回包
		//如果是其他服务器同步过来, 不用回包了
		Base::Count::async_response_circleCount(callback->getCurrentPtr(), rsp.iRet, rsp);
	}
}

string CountStateMachine::createRandomString(int length, INCLUDE_FLAG includes)
{
	static const char  Upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static const char  Lower[] = "abcdefghijklmnopqrstuvwxyz";
	static const char  Digit[] = "0123456789";

	static size_t UpperLen = strlen(Upper);
	static size_t LowerLen = strlen(Lower);
	static size_t DigitLen = strlen(Digit);

	vector<const char*> v;

	size_t len = 0;

	if(includes & DIGIT)
	{
		v.push_back(Digit);
		len += DigitLen;
	}
	if(includes & LOWER)
	{
		v.push_back(Lower);
		len += LowerLen;
	}
	if(includes & UPPER)
	{
		v.push_back(Upper);
		len += UpperLen;
	}

	if(len == 0)
	{
		return "";
	}

	string s;
	s.resize(length);

	for(size_t i = 0; i < length; i++)
	{
		int at = rand() % len;

		if(includes & DIGIT)
		{
			if(at < DigitLen)
			{
				s[i] = Digit[at];
				continue;
			}
			else
			{
				at -= DigitLen;
			}
		}
		if(includes & LOWER)
		{
			if(at < LowerLen)
			{
				s[i] = Lower[at];
				continue;
			}
			else
			{
				at -= LowerLen;
			}
		}
		if(includes & UPPER)
		{
			if(at < UpperLen)
			{
				s[i] = Upper[at];
				continue;
			}
			else
			{
				at -= UpperLen;
			}
		}

	}

	return s;
}

rocksdb::ColumnFamilyHandle* CountStateMachine::createTable(const string &table)
{
	string tableName = getTableName(table);

	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _column_familys.find(tableName);
	if( it != _column_familys.end())
	{
		return it->second;
	}

	rocksdb::ColumnFamilyHandle* handle;
	rocksdb::ColumnFamilyOptions options;
	options.compaction_filter = new TTLCompactionFilter();

	auto status = _db->CreateColumnFamily(options, tableName, &handle);
	if (!status.ok())
	{
		TLOG_ERROR("CreateColumnFamily error:" << status.ToString() << endl);
		throw std::runtime_error(status.ToString());
	}

	_column_familys[tableName] = handle;

	return handle;
}

void CountStateMachine::onRandomString(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	TLOG_DEBUG("appliedIndex:" << appliedIndex << endl);

	RandomReq req;
	is.read(req, 1, false);

	rocksdb::ColumnFamilyHandle* handle = createTable(req.sBusinessName);

	string key = req.sKey;

	RandomRsp rsp;

	while(true)
	{
		rsp.sString = createRandomString(req.length, (INCLUDE_FLAG)req.includes);

		bool has;
		rsp.iRet = hasNoLock(handle, key + "-" + rsp.sString, has);

		if(rsp.iRet != RT_SUCC)
		{
			rsp.sMsg = "get data from rocksdb error!";
			break;
		}

		if(!has)
		{
			break;
		}
	}

	if(rsp.iRet == RT_SUCC)
	{
		int64_t expireTime = req.expireTime;
		if(expireTime <0)
		{
			expireTime = 0;
		}
		if(expireTime > 0)
		{
			expireTime = expireTime + TNOW;
		}

		key = key + "-" + rsp.sString;
		TarsOutputStream<BufferWriterString> os;
		os.write(expireTime, 0);
		os.write(rsp.sString, 1);

		rocksdb::WriteBatch batch;
		batch.Put(handle, key, os.getByteBuffer());
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
		Base::Count::async_response_random(callback->getCurrentPtr(), rsp.iRet, rsp);
	}
}


void CountStateMachine::onSetRandomString(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)
{
	TLOG_DEBUG("appliedIndex:" << appliedIndex << endl);

	SetRandomReq req;
	is.read(req, 1, false);

	rocksdb::ColumnFamilyHandle* handle = createTable(req.sBusinessName);

	string key = req.sKey + "-" + req.sString;

	int64_t expireTime = req.expireTime;
	if (expireTime < 0)
	{
		expireTime = 0;
	}

	if (expireTime > 0)
	{
		expireTime = expireTime + TNOW;
	}

	TarsOutputStream<BufferWriterString> os;
	os.write(expireTime, 0);
	os.write(req.sString, 1);

	rocksdb::WriteBatch batch;
	batch.Put(handle, key, os.getByteBuffer());
	batch.Put("lastAppliedIndex", rocksdb::Slice((const char*)&appliedIndex, sizeof(appliedIndex)));

	rocksdb::WriteOptions wOption;
	wOption.sync = false;

	auto s = _db->Write(wOption, &batch);

	if (!s.ok())
	{
		TLOG_ERROR("Put: key:" << key << ", error!" << endl);
		exit(-1);
	}

	if (callback)
	{
		Base::Count::async_response_setRandom(callback->getCurrentPtr(), RT_SUCC);
	}
}

int CountStateMachine::getCount(const QueryReq &req, CountRsp &rsp)
{
	std::string key = req.sBusinessName + "-" + req.sKey;

	rsp.iRet = getCountNoLock(key, rsp.iCount);

	return rsp.iRet;
}

int CountStateMachine::hasRandom(const HasRandomReq &req, bool &exist)
{
	rocksdb::ColumnFamilyHandle* handle = createTable(req.sBusinessName);

	std::string key = req.sKey + '-' + req.sString;

	return getNoLock(handle, key, exist);
}

int CountStateMachine::getCountNoLock(const string &key, tars::Int64 &count)
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

int CountStateMachine::getNoLock(rocksdb::ColumnFamilyHandle* handle, const string &key,  bool &exist)
{
	string data;
	rocksdb::Status s = _db->Get(rocksdb::ReadOptions(), handle, key, &data);
	if (s.ok())
	{
		TarsInputStream<> is;
		is.setBuffer(data.c_str(), data.length());

		tars::Int64 expireTime;
		is.read(expireTime, 0, false);

		if(expireTime < TNOW)
		{
			exist = false;
			return RT_SUCC;
		}

		exist = true;
	}
	else if (s.IsNotFound()) {
		exist = false;
	}
	else
	{
		TLOG_ERROR("Get: " << key << ", error:" << s.ToString() << endl);

		return RT_DATA_ERROR;
	}

	return RT_SUCC;
}

int CountStateMachine::hasNoLock(rocksdb::ColumnFamilyHandle* handle, const string &key, bool &has)
{
	string value;
	rocksdb::Status s = _db->Get(rocksdb::ReadOptions(), handle, key, &value);
	if (s.ok())
	{
		has = true;
	}
	else if (s.IsNotFound()) {
		has = false;
	}
	else
	{
		TLOG_ERROR("Get: " << key << ", error:" << s.ToString() << endl);

		return RT_DATA_ERROR;
	}

	return RT_SUCC;
}