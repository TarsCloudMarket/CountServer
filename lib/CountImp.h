//
// Created by jarod on 2019-07-22.
//

#ifndef LIBRAFT_COUNTIDIMP_H
#define LIBRAFT_COUNTIDIMP_H

#include "Count.h"

using namespace Base;

class RaftNode;
class CountStateMachine;

class CountImp : public Count
{
public:

    virtual void initialize();

    virtual void destroy();

    virtual int count(const CountReq &req, CountRsp &rsp, tars::CurrentPtr current);

    virtual int query(const QueryReq &req, CountRsp &rsp, tars::CurrentPtr current);

protected:

	shared_ptr<RaftNode>    _raftNode;

	shared_ptr<CountStateMachine> _stateMachine;
};


#endif //LIBRAFT_RAFTCLIENTIMP_H
