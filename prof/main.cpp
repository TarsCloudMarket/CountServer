#include "Count.h"
#include <iostream>
#include "servant/Application.h"
#include "util/tc_option.h"

using namespace tars;
using namespace Base;

Communicator *_comm;
string matchObj = "Base.CountServer.CountObj@tcp -h 127.0.0.1 -p 30001";

struct Param
{
	int count;
	string call;
	int thread;

	CountPrx pPrx;
};

Param param;
std::atomic<int> callback(0);

struct CountCallback : public CountPrxCallback
{
	CountCallback(int64_t t, int i, int c) : start(t), cur(i), count(c)
	{

	}

	//call back
	virtual void callback_count(tars::Int32 ret,  const Base::CountRsp& rsp)
	{
		++callback;

		if(cur == count-1)
		{
			int64_t cost = TC_Common::now2us() - start;
			cout << "callback_count count:" << count << ", " << cost << " us, avg:" << 1.*cost/count << "us" << endl;
		}
	}
	virtual void callback_count_exception(tars::Int32 ret)
	{
		cout << "callback_count_exception:" << ret << endl;
	}

	virtual void callback_query(tars::Int32 ret, const Base::CountRsp &rsp)
	{
		++callback;

		if(cur == count-1)
		{
			int64_t cost = TC_Common::now2us() - start;
			cout << "callback_count count:" << count << ", " << cost << " us, avg:" << 1.*cost/count << "us" << endl;
		}
	}

	int64_t start;
	int     cur;
	int     count;
};

void syncCountCall(int c)
{
	int64_t t = TC_Common::now2us();

	CountReq req;
	req.sBusinessName = "test";
	req.sKey = "mkey";
	req.iNum = 1;

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		try
		{

			CountRsp rsp;
			param.pPrx->count(req, rsp);

//			TC_Common::sleep(1);
		}
		catch(exception& e)
		{
			cout << "exception:" << e.what() << endl;
		}
		++callback;
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "syncCall total:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}

void asyncCountCall(int c)
{
	int64_t t = TC_Common::now2us();
	CountReq req;
	req.sBusinessName = "test";
	req.sKey = "mkey";
	req.iNum = 1;

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		CountPrxCallbackPtr p = new CountCallback(t, i, c);

		try
		{
			param.pPrx->async_count(p, req);
		}
		catch(exception& e)
		{
			cout << "exception:" << e.what() << endl;
		}

		if(i - callback > 1000)
		{
			TC_Common::msleep(50);
		}
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "asyncCall send:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}

void ping(int c)
{
	for(int i = 0; i < c; i++)
	{
		param.pPrx->tars_ping();

		++callback;
	}
}

void syncQueryCall(int c)
{
	int64_t t = TC_Common::now2us();

	QueryReq req;
	req.sBusinessName = "test";
	req.sKey = "test";

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		try
		{

			CountRsp rsp;
			param.pPrx->query(req, rsp);
		}
		catch(exception& e)
		{
			cout << "exception:" << e.what() << endl;
		}
		++callback;
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "syncCall total:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}

void asyncQueryCall(int c)
{
	int64_t t = TC_Common::now2us();
	QueryReq req;
	req.sBusinessName = "test";
	req.sKey = "test";

	//发起远程调用
	for (int i = 0; i < c; ++i)
	{
		CountPrxCallbackPtr p = new CountCallback(t, i, c);

		try
		{
			param.pPrx->async_query(p, req);
		}
		catch(exception& e)
		{
			cout << "exception:" << e.what() << endl;
		}

		if(i - callback > 1000)
		{
			TC_Common::msleep(50);
		}
	}

	int64_t cost = TC_Common::now2us() - t;
	cout << "asyncCall send:" << cost << "us, avg:" << 1.*cost/c << "us" << endl;
}

int main(int argc, char *argv[])
{
	try
	{
		if (argc < 2)
		{
			cout << "Usage:" << argv[0] << " --count=1000 --call=[ping|count-sync|count-async|query-sync|query-async] --thread=1" << endl;

			return 0;
		}

		TC_Option option;
		option.decode(argc, argv);

		param.count = TC_Common::strto<int>(option.getValue("count"));
		if(param.count <= 0) param.count = 1000;
		param.call = option.getValue("call");
		if(param.call.empty()) param.call = "sync";
		param.thread = TC_Common::strto<int>(option.getValue("thread"));
		if(param.thread <= 0) param.thread = 1;

		_comm = new Communicator();

//        LocalRollLogger::getInstance()->logger()->setLogLevel(6);

		_comm->setProperty("sendqueuelimit", "1000000");
		_comm->setProperty("asyncqueuecap", "1000000");

		param.pPrx = _comm->stringToProxy<CountPrx>(matchObj);

		param.pPrx->tars_connect_timeout(50000);
		param.pPrx->tars_set_timeout(60 * 1000);
		param.pPrx->tars_async_timeout(60*1000);
//		param.pPrx->tars_ping();

		int64_t start = TC_Common::now2us();

		std::function<void(int)> func;

		if (param.call == "ping")
		{
			func = ping;
		}
		else if (param.call == "count-sync")
		{
			func = syncCountCall;
		}
		else if (param.call == "count-async")
		{
			func = asyncCountCall;
		}
		else if (param.call == "query-sync")
		{
			func = syncQueryCall;
		}
		else if (param.call == "query-async")
		{
			func = asyncQueryCall;
		}

		vector<std::thread*> vt;
		for(int i = 0 ; i< param.thread; i++)
		{
			vt.push_back(new std::thread(func, param.count));
		}

		std::thread print([&]{while(callback != param.count * param.thread) {
			cout << param.call << " : ----------finish count:" << callback << endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		};});

		for(size_t i = 0 ; i< vt.size(); i++)
		{
			vt[i]->join();
			delete vt[i];
		}

		cout << "(pid:" << std::this_thread::get_id() << ")"
		     << "(count:" << param.count << ")"
		     << "(use ms:" << (TC_Common::now2us() - start)/1000 << ")"
		     << endl;


		while(callback != param.count * param.thread) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		print.join();
		cout << "order:" << param.call << " ----------finish count:" << callback << endl;
	}
	catch(exception &ex)
	{
		cout << ex.what() << endl;
	}
	cout << "main return." << endl;

	return 0;
}
