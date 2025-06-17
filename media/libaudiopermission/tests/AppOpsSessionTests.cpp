/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <android/content/AttributionSourceState.h>
#include <media/AppOpsSession.h>
#include <media/ValidatedAttributionSourceState.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>

using ::android::content::AttributionSourceState;
using ::android::media::permission::AppOpsSession;
using ::android::media::permission::Ops;
using ::com::android::media::permission::ValidatedAttributionSourceState;

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Ne;

class AppOpsSessionTests;

class AppOpsTestFacade {
    friend AppOpsSessionTests;

  public:
    bool startAccess(const ValidatedAttributionSourceState&, Ops) {
        if (allowed_) ++running_;
        return allowed_;
    }

    void stopAccess(const ValidatedAttributionSourceState&, Ops) { --running_; }

    bool checkAccess(const ValidatedAttributionSourceState&, Ops) { return allowed_; }

    uintptr_t addChangeCallback(const ValidatedAttributionSourceState&, Ops,
                                std::function<void(bool)> cb) {
        cb_ = cb;
        return 42;
    }

    void removeChangeCallback(uintptr_t) {}

  private:
    // Static abuse since this is copied into the test, and represents "global" state
    static inline std::function<void(bool)> cb_;
    static inline bool allowed_;
    static inline int running_;
};

class AppOpsSessionTests : public ::testing::Test {
  protected:
    static constexpr Ops mOps = {100, 101};

    // We must manually clear the facade state, since it is static, unlike the members of this
    // class, since the fixture is constructed per-test.
    void SetUp() override {
        AppOpsTestFacade::cb_ = nullptr;
        AppOpsTestFacade::running_ = 0;
        AppOpsTestFacade::allowed_ = false;
    }

    void facadeSetAllowed(bool isAllowed) { AppOpsTestFacade::allowed_ = isAllowed; }

    int facadeGetRunning() { return AppOpsTestFacade::running_; }

    void facadeTriggerChange(bool isPermitted) {
        EXPECT_THAT(isPermitted, Ne(AppOpsTestFacade::allowed_));
        facadeSetAllowed(isPermitted);
        AppOpsTestFacade::cb_(isPermitted);
    }

    // Trigger a change callback, but without modifying the underlying state.
    // Allows for simulating a callback which is reversed quickly and callbacks which may not
    // apply to our package.
    void facadeTriggerSpuriousChange(bool isPermitted) { facadeSetAllowed(isPermitted); }

    void dataDeliveryCb(bool shouldDeliver) { mDeliveredCbs.push_back(shouldDeliver); }

    const AttributionSourceState mAttr = []() {
        AttributionSourceState attr;
        attr.uid = 1;
        attr.pid = 2;
        attr.deviceId = 3;
        return attr;
    }();

    void initSession() {
        mAppOpsSession.emplace(
                ValidatedAttributionSourceState::createFromTrustedSource(mAttr), mOps,
                [this](bool x) { dataDeliveryCb(x); }, AppOpsTestFacade{});
    }

    // For verification of delivered callbacks
    // vector<bool> since it's a test
    std::vector<bool> mDeliveredCbs;
    std::optional<AppOpsSession<AppOpsTestFacade>> mAppOpsSession;
};

TEST_F(AppOpsSessionTests, beginDeliveryRequest_Allowed) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, beginDeliveryRequest_Denied) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, endDeliveryRequest_Ongoing) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);
    mAppOpsSession->endDeliveryRequest();
    EXPECT_EQ(facadeGetRunning(), 0);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, endDeliveryRequest_Paused) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);
    mAppOpsSession->endDeliveryRequest();
    EXPECT_EQ(facadeGetRunning(), 0);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, endDeliveryRequest_PausedByCb) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);
    facadeTriggerChange(false);
    EXPECT_EQ(facadeGetRunning(), 0);

    mAppOpsSession->endDeliveryRequest();
    EXPECT_EQ(facadeGetRunning(), 0);
}

TEST_F(AppOpsSessionTests, onPermittedFalse_Ongoing_Change) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);
    facadeTriggerChange(false);
    EXPECT_EQ(facadeGetRunning(), 0);
    EXPECT_THAT(mDeliveredCbs, ElementsAreArray({false}));
}

TEST_F(AppOpsSessionTests, onPermittedTrue_Ongoing_Change) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);
    facadeTriggerChange(true);
    EXPECT_EQ(facadeGetRunning(), 1);
    EXPECT_THAT(mDeliveredCbs, ElementsAreArray({true}));
}

TEST_F(AppOpsSessionTests, onPermittedTrue_Ongoing_Change_Spurious) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);
    facadeTriggerSpuriousChange(true);
    EXPECT_EQ(facadeGetRunning(), 0);
    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, onPermittedFalse_Ongoing_Same) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);
    facadeTriggerSpuriousChange(false);
    EXPECT_EQ(facadeGetRunning(), 0);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, onPermittedTrue_Ongoing_Same) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);
    facadeTriggerSpuriousChange(true);
    EXPECT_EQ(facadeGetRunning(), 1);

    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, onPermittedFalse_Paused_Change) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    mAppOpsSession->endDeliveryRequest();

    EXPECT_EQ(facadeGetRunning(), 0);
    facadeTriggerChange(false);
    EXPECT_EQ(facadeGetRunning(), 0);
    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, onPermittedTrue_Paused_Change) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    mAppOpsSession->endDeliveryRequest();

    facadeTriggerChange(true);
    EXPECT_EQ(facadeGetRunning(), 0);
    EXPECT_THAT(mDeliveredCbs, IsEmpty());
}

TEST_F(AppOpsSessionTests, dtor_Running) {
    facadeSetAllowed(true);
    initSession();
    EXPECT_TRUE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 1);

    // call dtor
    mAppOpsSession.reset();
    EXPECT_EQ(facadeGetRunning(), 0);
}

TEST_F(AppOpsSessionTests, dtor_NotRunning) {
    facadeSetAllowed(false);
    initSession();
    EXPECT_FALSE(mAppOpsSession->beginDeliveryRequest());
    EXPECT_EQ(facadeGetRunning(), 0);

    // call dtor
    mAppOpsSession.reset();
    EXPECT_EQ(facadeGetRunning(), 0);
}
