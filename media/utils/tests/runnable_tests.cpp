/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "runnable_tests"

#include <mediautils/Runnable.h>

#include <gtest/gtest.h>

using namespace android::mediautils;

struct Func {
    inline static int sMoveCtor = 0;
    inline static int sDtor = 0;
    // accumulator for call operator of this object
    inline static int sSum = 0;
    static constexpr int VAL1 = 7;
    static constexpr int VAL2 = 4;

    Func(int v) : value(v) {}
    Func(const Func&) = delete;
    Func(Func&& other) : value(other.value) { sMoveCtor++; }
    Func& operator=(const Func&) = delete;
    ~Func() { sDtor++; }

    void operator()() { sSum += value; }

  private:
    const int value;
};

class RunnableTests : public ::testing::Test {
  protected:
    void SetUp() override {
        Func::sMoveCtor = 0;
        Func::sDtor = 0;
        Func::sSum = 0;
    }
};

TEST_F(RunnableTests, testEmpty) {
    Runnable r1{};
    Runnable r2{nullptr};
    // empty func should do nothing, instead of crash
    r1();
    r2();
    EXPECT_FALSE(r1);
    EXPECT_FALSE(r2);
}

static int foo() {
    return 5;
}

struct Copy {
    Copy() {}
    Copy(const Copy&) {}
    Copy(Copy&&) {}
    void operator()(){}
};

TEST_F(RunnableTests, testCompile) {
    const Copy b{};
    Runnable r1{std::move(b)};
    Runnable r2{b};
    Runnable r4{foo};
    std::unique_ptr<int> ptr;
    auto move_only = [ptr = std::move(ptr)](){};
    Runnable r5{std::move(move_only)};
    auto copyable = [](){};
    Runnable r6{copyable};
}

TEST_F(RunnableTests, testBool) {
    Runnable r1{[]() {}};
    EXPECT_TRUE(r1);
}

TEST_F(RunnableTests, testCall) {
    Runnable r1{Func{Func::VAL1}};
    EXPECT_TRUE(r1);
    r1();
    EXPECT_EQ(Func::sSum, Func::VAL1);
}

TEST_F(RunnableTests, testDtor) {
    {
        Runnable r1{Func{Func::VAL1}};
    }
    EXPECT_EQ(Func::sDtor, 2);
}

TEST_F(RunnableTests, testMoveCtor) {
    {
        Runnable moved_from{Func{Func::VAL1}};
        EXPECT_EQ(Func::sMoveCtor, 1);
        EXPECT_EQ(Func::sDtor, 1);
        Runnable r1{std::move(moved_from)};
        EXPECT_EQ(Func::sDtor, 2);  // impl detail that we destroy internal obj after move
        EXPECT_EQ(Func::sMoveCtor, 2);
        EXPECT_TRUE(r1);
        EXPECT_FALSE(moved_from);
        r1();
        EXPECT_EQ(Func::sSum, Func::VAL1);
    }
    EXPECT_EQ(Func::sDtor, 3);
}

TEST_F(RunnableTests, testMoveAssign) {
    {
        Runnable r1{Func{Func::VAL2}};
        Runnable moved_from{Func{Func::VAL1}};
        EXPECT_EQ(Func::sMoveCtor, 2);
        EXPECT_EQ(Func::sDtor, 2);
        r1();
        EXPECT_EQ(Func::sSum, 4);
        r1 = std::move(moved_from);
        EXPECT_EQ(Func::sDtor, 4);  // impl detail that we destroy internal obj after move
        EXPECT_EQ(Func::sMoveCtor, 3);
        EXPECT_TRUE(r1);
        EXPECT_FALSE(moved_from);
        r1();  // value should now hold Func::VAL1
        EXPECT_EQ(Func::sSum, Func::VAL2 + Func::VAL1);
    }
    EXPECT_EQ(Func::sDtor, 5);
}
