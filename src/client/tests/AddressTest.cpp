/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include <common/Init.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <gtest/gtest_prod.h>
#include <nebula/client/ConnectionPool.h>
#include <nebula/client/Session.h>

#include "./ClientTest.h"

// Require a nebula server could access

#define kServerHost "graphd"

class AddressTest : public ClientTest {};

TEST_F(AddressTest, One) {
  nebula::ConnectionPool pool;
  pool.init({kServerHost ":9669"}, nebula::Config{});
  EXPECT_EQ(pool.size(), 10);
}

TEST_F(AddressTest, Multiple) {
  nebula::ConnectionPool pool;
  pool.init({"graphd:9669", "graphd1:9669", "graphd2:9669"}, nebula::Config{});
  EXPECT_EQ(pool.size(), 10);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  nebula::init(&argc, &argv);
  google::SetStderrLogging(google::GLOG_ERROR);

  return RUN_ALL_TESTS();
}
