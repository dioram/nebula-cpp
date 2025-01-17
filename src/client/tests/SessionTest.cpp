/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include <common/Init.h>
#include <common/datatypes/Geography.h>
#include <folly/json.h>
#include <folly/synchronization/Baton.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>
#include <nebula/client/Config.h>
#include <nebula/client/ConnectionPool.h>
#include <nebula/client/Session.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "./ClientTest.h"

// Require a nebula server could access

#define kServerHost "graphd"

class SessionTest : public ClientTest {
 protected:
  static void runOnce(nebula::ConnectionPool& pool) {
    auto session = pool.getSession("root", "nebula");
    ASSERT_TRUE(session.valid());

    // ping
    EXPECT_TRUE(session.ping());

    // execute
    auto result = session.execute("YIELD 1");
    ASSERT_EQ(result.errorCode, nebula::ErrorCode::SUCCEEDED);
    nebula::DataSet expected({"1"});
    expected.emplace_back(nebula::List({1}));
    EXPECT_TRUE(verifyResultWithoutOrder(*result.data, expected));

    // explain
    result = session.execute("EXPLAIN SHOW HOSTS");
    ASSERT_EQ(result.errorCode, nebula::ErrorCode::SUCCEEDED);
    EXPECT_NE(result.planDesc, nullptr);
    // TODO(shylock) check the plan

    // async execute
    folly::Baton<> b;
    session.asyncExecute("YIELD 1", [&b](auto&& cbResult) {
      ASSERT_EQ(cbResult.errorCode, nebula::ErrorCode::SUCCEEDED);
      nebula::DataSet cbExpected({"1"});
      cbExpected.emplace_back(nebula::List({1}));
      EXPECT_TRUE(verifyResultWithoutOrder(*cbResult.data, cbExpected));
      b.post();
    });
    b.wait();

    // retry connection
    ASSERT_EQ(session.retryConnect(), nebula::ErrorCode::SUCCEEDED);

    // execute
    result = session.execute("YIELD 1");
    ASSERT_EQ(result.errorCode, nebula::ErrorCode::SUCCEEDED);
    EXPECT_TRUE(verifyResultWithoutOrder(*result.data, expected));

    // release
    session.release();

    ASSERT_FALSE(session.valid());

    // ping
    EXPECT_FALSE(session.ping());

    // check release
    result = session.execute("YIELD 1");
    ASSERT_EQ(result.errorCode, nebula::ErrorCode::E_DISCONNECTED);

    // async execute
    folly::Baton<> b2;
    session.asyncExecute("YIELD 1", [&b2](auto&& cbResult) {
      ASSERT_EQ(cbResult.errorCode, nebula::ErrorCode::E_DISCONNECTED);
      b2.post();
    });
    b2.wait();

    // ping
    EXPECT_FALSE(session.ping());
  }
};

TEST_F(SessionTest, Basic) {
  nebula::ConnectionPool pool;
  pool.init({kServerHost ":9669"}, nebula::Config{});
  LOG(INFO) << "Testing once.";
  runOnce(pool);

  LOG(INFO) << "Testing reopen.";
  runOnce(pool);
}

TEST_F(SessionTest, OverUse) {
  nebula::ConnectionPool pool;
  nebula::Config c;
  pool.init({kServerHost ":9669"}, c);
  std::vector<nebula::Session> sessions;
  for (std::size_t i = 0; i < c.maxConnectionPoolSize_; ++i) {
    sessions.emplace_back(pool.getSession("root", "nebula"));
  }
  auto session = pool.getSession("root", "nebula");
  EXPECT_FALSE(session.valid());
}

TEST_F(SessionTest, MTSafe) {
  nebula::ConnectionPool pool;
  nebula::Config c;
  pool.init({kServerHost ":9669"}, c);
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < c.maxConnectionPoolSize_; ++i) {
    threads.emplace_back([&pool]() {
      using namespace std::chrono_literals;  // NOLINT
      std::this_thread::sleep_for(1s);

      auto session = pool.getSession("root", "nebula");
      EXPECT_TRUE(session.valid());

      session.release();
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  EXPECT_EQ(pool.size(), c.maxConnectionPoolSize_);
}

TEST_F(SessionTest, InvalidAddress) {
  nebula::ConnectionPool pool;
  nebula::Config c;
  pool.init({"xxxx"}, c);
  EXPECT_EQ(pool.size(), 0);
}

TEST_F(SessionTest, Data) {
  nebula::ConnectionPool pool;
  nebula::Config c{10, 0, 300, 0, "", false};
  pool.init({kServerHost ":9669"}, c);
  auto session = pool.getSession("root", "nebula");
  ASSERT_TRUE(session.valid());

  auto resp = session.execute(
      "CREATE SPACE IF NOT EXISTS data_test(vid_type = FIXED_STRING(16));"
      "USE data_test;"
      "CREATE TAG IF NOT EXISTS  geo(any_shape Geography, only_point Geography(point), "
      "only_lineString Geography(linestring), only_polygon Geography(polygon));");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED) << *resp.errorMsg;

  ::sleep(30);

  resp = session.execute(
      "INSERT VERTEX geo VALUES 'v101':(ST_Point(3, 8), ST_Point(4, 6), "
      "ST_GeogFromText('LINESTRING(0 1, 2 3)'), ST_GeogFromText('POLYGON((0 1, 1 2, 2 3, 0 1))'))");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED) << *resp.errorMsg;

  // execute
  resp = session.execute(
      "FETCH PROP ON geo 'v101' YIELD geo.any_shape, geo.only_point, geo.only_lineString, "
      "geo.only_polygon");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);
  nebula::DataSet expected(
      {"geo.any_shape", "geo.only_point", "geo.only_lineString", "geo.only_polygon"});
  nebula::Geography geogPoint1(nebula::Point(nebula::Coordinate(3, 8)));
  nebula::Geography geogPoint2(nebula::Point(nebula::Coordinate(4, 6)));
  nebula::Geography geogLineString(nebula::LineString(
      std::vector<nebula::Coordinate>{nebula::Coordinate(0, 1), nebula::Coordinate(2, 3)}));
  nebula::Geography geogPolygon(nebula::Polygon(std::vector<std::vector<nebula::Coordinate>>{
      std::vector<nebula::Coordinate>{nebula::Coordinate(0, 1),
                                      nebula::Coordinate(1, 2),
                                      nebula::Coordinate(2, 3),
                                      nebula::Coordinate(0, 1)}}));
  expected.emplace_back(nebula::List({geogPoint1, geogPoint2, geogLineString, geogPolygon}));
  EXPECT_TRUE(verifyResultWithoutOrder(*resp.data, expected));
  EXPECT_EQ(geogPoint1.toString(), "POINT(3 8)");
  EXPECT_EQ(geogPoint2.toString(), "POINT(4 6)");
  EXPECT_EQ(geogLineString.toString(), "LINESTRING(0 1, 2 3)");
  EXPECT_EQ(geogPolygon.toString(), "POLYGON((0 1, 1 2, 2 3, 0 1))");

  resp = session.execute("DROP SPACE IF EXISTS data_test");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);
}

TEST_F(SessionTest, Timeout) {
  nebula::ConnectionPool pool;
  nebula::Config c{10, 0, 100, 0, "", false};
  pool.init({kServerHost ":9669"}, c);
  auto session = pool.getSession("root", "nebula");
  ASSERT_TRUE(session.valid());

  auto resp = session.execute(
      "CREATE SPACE IF NOT EXISTS session_test(vid_type = FIXED_STRING(16));use "
      "session_test;CREATE EDGE IF NOT EXISTS like();");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED) << *resp.errorMsg;

  ::sleep(30);

  resp = session.execute(
      "INSERT EDGE like() VALUES 'Tim Duncan'->'Tony Parker':(), 'Tony "
      "Parker'->'Tim Duncan':();");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED) << *resp.errorMsg;

  // execute
  resp = session.execute(
      "use session_test;GO 100000 STEPS FROM 'Tim Duncan' OVER like YIELD like._dst;");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::E_RPC_FAILURE) << *resp.errorMsg;

  resp = session.execute(
      "SHOW QUERIES "
      "| YIELD $-.SessionID AS sid, $-.ExecutionPlanID AS eid, $-.DurationInUSec AS dur "
      "WHERE $-.DurationInUSec > 1000000 AND $-.`Query` CONTAINS 'GO' "
      "| ORDER BY $-.dur "
      "| KILL QUERY(session=$-.sid, plan=$-.eid)");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);

  resp = session.execute("DROP SPACE IF EXISTS session_test");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);
}

TEST_F(SessionTest, JsonResult) {
  nebula::ConnectionPool pool;
  nebula::Config c{10, 0, 10, 0, "", false};
  pool.init({kServerHost ":9669"}, c);
  auto session = pool.getSession("root", "nebula");
  ASSERT_TRUE(session.valid());

  auto resp = session.executeJson("YIELD 1");
  folly::parseJson(resp);

  folly::Baton<> b;
  session.asyncExecuteJson("YIELD 1", [&b](std::string&& asyncResp) {
    folly::parseJson(asyncResp);
    b.post();
  });
  b.wait();
}

TEST_F(SessionTest, DurationResult) {
  nebula::ConnectionPool pool;
  nebula::Config c{10, 0, 10, 0, "", false};
  pool.init({kServerHost ":9669"}, c);
  auto session = pool.getSession("root", "nebula");
  ASSERT_TRUE(session.valid());

  auto resp = session.execute("YIELD duration({years: 1, seconds: 2}) AS d");
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);

  nebula::DataSet expected({"d"});
  expected.emplace_back(nebula::Row({nebula::Duration().addYears(1).addSeconds(2)}));
  EXPECT_TRUE(verifyResultWithoutOrder(*resp.data, expected));
}

TEST_F(SessionTest, ExecuteParameter) {
  nebula::ConnectionPool pool;
  nebula::Config c{10, 0, 10, 0, "", false};
  pool.init({kServerHost ":9669"}, c);
  auto session = pool.getSession("root", "nebula");
  ASSERT_TRUE(session.valid());

  auto resp = session.executeWithParameter("YIELD $var AS var", {{"var", 1}});
  ASSERT_EQ(resp.errorCode, nebula::ErrorCode::SUCCEEDED);

  nebula::DataSet expected({"var"});
  expected.emplace_back(nebula::Row({1}));
  EXPECT_TRUE(verifyResultWithoutOrder(*resp.data, expected));

  folly::Baton<> b;
  session.asyncExecuteWithParameter(
      "YIELD $var AS var", {{"var", 1}}, [&b, expected = std::move(expected)](auto&& aResp) {
        ASSERT_EQ(aResp.errorCode, nebula::ErrorCode::SUCCEEDED);
        EXPECT_TRUE(verifyResultWithoutOrder(*aResp.data, expected));
        b.post();
      });
  b.wait();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  nebula::init(&argc, &argv);
  google::SetStderrLogging(google::GLOG_INFO);

  return RUN_ALL_TESTS();
}
