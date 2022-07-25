#include <cassert>
#include <iostream>
#include <vector>
#include "gtest/gtest.h"
#include "rafttest/RaftTest.h"
#include "CountServer.h"
#include "Count.h"

using namespace std;
using namespace Base;

class CountUnitTest : public testing::Test
{

public:
	CountUnitTest()
	{
	}

	~CountUnitTest()
	{
	}

	static void SetUpTestCase()
	{
	}
	static void TearDownTestCase()
	{

	}

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{
	}

	inline void testCount(const shared_ptr<CountServer> &server)
	{
		try
		{
			CountReq data;
			data.sBusinessName= "test";
			data.sKey = "abc";
			data.iNum = 1;
			data.iDefault = 15;

			CountPrx prx = server->node()->getBussLeaderPrx<CountPrx>();

			CountRsp rsp;

			int ret = prx->count(data, rsp);

			ASSERT_TRUE(ret == 0);

		}
		catch (exception &ex) {
			LOG_CONSOLE_DEBUG << ex.what() << endl;
		}
	}


	inline void testCircle(const shared_ptr<CountServer> &server)
	{
		try
		{
			CircleReq data;
			data.sBusinessName= "test";
			data.sKey = "abc";
			data.iNum = 1;
			data.iMinNum = 10;
			data.iMaxNum = 20;

			CountPrx prx = server->node()->getBussLeaderPrx<CountPrx>();

			CountRsp rsp;

			int ret = prx->circleCount(data, rsp);

			ASSERT_TRUE(ret == 0);

			ASSERT_TRUE(rsp.iCount >= data.iMinNum && rsp.iCount <= data.iMaxNum);
		}
		catch (exception &ex) {
			LOG_CONSOLE_DEBUG << ex.what() << endl;
		}
	}

};

TEST_F(CountUnitTest, TestRaft_Set)
{
	auto raftTest = std::make_shared<RaftTest<CountServer>>();
	raftTest->initialize("Base", "CountServer", "CountObj", "count-log", 22000, 32000);
	raftTest->setBussFunc(std::bind(&CountUnitTest::testCount, this, std::placeholders::_1));

	raftTest->testAll();
}


TEST_F(CountUnitTest, TestRaft_Circle)
{
	auto raftTest = std::make_shared<RaftTest<CountServer>>();
	raftTest->initialize("Base", "CountServer", "CountObj", "count-log", 22000, 32000);
	raftTest->setBussFunc(std::bind(&CountUnitTest::testCircle, this, std::placeholders::_1));

	raftTest->testAll();
}


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	return RUN_ALL_TESTS();
}