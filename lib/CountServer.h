//
// Created by jarod on 2019-08-08.
//

#ifndef LIBRAFT_COUNT_SERVER_H
#define LIBRAFT_COUNT_SERVER_H

#include "servant/Application.h"
#include "RaftServer.h"

using namespace tars;

class CountImp;
class CountStateMachine;
class RaftNode;


class CountServer : public RaftServer<CountStateMachine, CountImp>
{
public:
	/**
	 * 析构函数
	 **/
	virtual ~CountServer() {};

	/**
	 * 服务初始化
	 **/
	virtual void initialize();

	/**
	 * 服务销毁
	 **/
	virtual void destroyApp();

protected:
	bool cmdGet(const string&command, const string&params, string& result);
	bool cmdSet(const string&command, const string&params, string& result);
	
public:
	int64_t _startCount = 1;
};


#endif //LIBRAFT_KVSERVER_H
