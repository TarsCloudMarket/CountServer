//
// Created by jarod on 2019-07-22.
//

#include "CountImp.h"
#include "CountServer.h"
#include "CountStateMachine.h"

extern CountServer g_app;

void CountImp::initialize()
{
	_raftNode = ((CountServer*)this->getApplication())->node() ;
	_stateMachine = ((CountServer*)this->getApplication())->getStateMachine();
}

void CountImp::destroy()
{
}


int CountImp::count(const CountReq &req, CountRsp &rsp, tars::CurrentPtr current)
{
	LOG_CONSOLE_DEBUG << req.writeToJsonString() << endl;

	_raftNode->forwardOrReplicate(current, [&](){

		TarsOutputStream<BufferWriterString> os;

		os.write(CountStateMachine::COUNT_TYPE, 0);
		os.write(req, 1);

		return os.getByteBuffer();
	});

	return 0;
}

int CountImp::circleCount(const CircleReq &req, CountRsp &rsp, tars::CurrentPtr current)
{
	LOG_CONSOLE_DEBUG << req.writeToJsonString() << endl;

	_raftNode->forwardOrReplicate(current, [&](){

		TarsOutputStream<BufferWriterString> os;

		os.write(CountStateMachine::CIRCLE_TYPE, 0);
		os.write(req, 1);

		return os.getByteBuffer();
	});

	return 0;
}

int CountImp::query(const QueryReq &req, CountRsp &rsp, tars::TarsCurrentPtr current)
{
	if(req.leader && !_raftNode->isLeader())
	{
		_raftNode->forwardToLeader(current);
		return 0;
	}
	return _stateMachine->get(req, rsp);
}
