//
// Created by jarod on 2019-06-06.
//

#ifndef LIBRAFT_COUNT_STATEMACHINE_H
#define LIBRAFT_COUNT_STATEMACHINE_H

#include <mutex>
#include "util/tc_thread_rwlock.h"
#include "Count.h"
#include "StateMachine.h"

namespace rocksdb
{
class DB;
class Iterator;
class Comparator;
class ColumnFamilyHandle;
class WriteBatch;

}

using namespace Base;

class ApplyContext;

class CountStateMachine : public StateMachine
{
public:
	const static string COUNT_TYPE;
	const static string CIRCLE_TYPE;
	const static string RANDOM_STRING_TYPE;
	const static string SET_RANDOM_STRING_TYPE;

	/**
	 * 构造
	 * @param dataPath
	 */
	CountStateMachine(const string &dataPath);

	/**
	 * 析构
	 */
	virtual ~CountStateMachine();

	/**
     * 对状态机中数据进行snapshot，每个节点本地定时调用
     * @param snapshotDir snapshot数据输出目录
     */
	virtual void onSaveSnapshot(const string &snapshotDir);

	/**
	 * 读取snapshot到状态机，节点启动时 或者 节点安装快照后 调用
	 * @param snapshotDir snapshot数据目录
	 */
	virtual bool onLoadSnapshot(const string &snapshotDir);

	/**
	 * 启动时加载数据
	 * @return
	 */
	virtual int64_t onLoadData();

	/**
     * 将数据应用到状态机
     * @param dataBytes 数据二进制
     * @param appliedIndex, appliedIndex
     * @param callback, 如果是Leader, 且网路请求过来的, 则callback有值, 否则为NULL
     */
	virtual void onApply(const char *buff, size_t length, int64_t appliedIndex, const shared_ptr<ApplyContext> &context);

	/**
	 * 变成Leader
	 * @param term
	 */
	virtual void onBecomeLeader(int64_t term);

	/**
	 * 变成Follower
	 */
	virtual void onBecomeFollower();

	/**
	 * 开始选举的回调
	 * @param term 选举轮数
	 */
	virtual void onStartElection(int64_t term);

	/**
	 * 节点加入集群(Leader or Follower) & LeaderId 已经设置好!
	 * 此时能够正常对外提供服务了, 对于Follower收到请求也可以转发给Leader了
	 */
	virtual void onJoinCluster();
	/**
	 * 节点离开集群(重新发起投票, LeaderId不存在了)
	 * 此时无法正常对外提供服务了, 请求不能发送到当前节点
	 */
	virtual void onLeaveCluster();

	/**
	* 开始从Leader同步快照文件
	*/
	virtual void onBeginSyncShapshot();

	/**
	 * 结束同步快照
	 */
	virtual void onEndSyncShapshot();

	/**
	 * get
	 * @param req
	 * @param rsp
	 * @return
	 */
	int getCount(const QueryReq &req, CountRsp &rsp);

	/**
	 *
	 * @param req
	 * @param exist
	 * @return
	 */
	int hasRandom(const HasRandomReq &req, bool &exist);

	/**
	 * close
	 */
	void close();

protected:

	using onapply_type = std::function<void(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback)>;

	string getTableName(const string &table) { return "c-" + table; }

	rocksdb::ColumnFamilyHandle* createTable(const string &table);

	void onCount(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback);
	void onCircle(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback);
	void onRandomString(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback);
	void onSetRandomString(TarsInputStream<> &is, int64_t appliedIndex, const shared_ptr<ApplyContext> &callback);

	void open(const string &dbDir);

	string getDbDir() { return _raftDataDir + FILE_SEP + "rocksdb_data"; }

	string createRandomString(int length, INCLUDE_FLAG includes);

	int getCountNoLock(const string &key, tars::Int64 &value);

	int getNoLock(rocksdb::ColumnFamilyHandle*, const string &key,  bool &exist);

	int hasNoLock(rocksdb::ColumnFamilyHandle*, const string &key, bool &has);

protected:
	string          _raftDataDir;
	rocksdb::DB     *_db = NULL;
	std::mutex		_mutex;

	unordered_map<string, rocksdb::ColumnFamilyHandle*> _column_familys;

	unordered_map<string, onapply_type>	_onApply;
};


#endif //LIBRAFT_EXAMPLESTATEMACHINE_H
