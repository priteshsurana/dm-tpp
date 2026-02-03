#include "result.h"
#include "error.h"
#include <gtest/gtest.h>
#include <string>

using namespace dmtpp::core;

TEST(ResultTest,OkValue) {
    Result<int, Error> result = Result<int, Error>::Ok(42);
    ASSERT_TRUE(result.is_ok());
    ASSERT_FALSE(result.is_err());
    EXPECT_EQ(result.unwrap(), 42);
}

