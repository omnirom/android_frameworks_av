/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef A_RTP_WRITER_H_

#define A_RTP_WRITER_H_

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/base64.h>
#include <media/stagefright/MediaWriter.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <android/multinetwork.h>
#include "TrafficRecorder.h"

#define LOG_TO_FILES    0

namespace android {

struct ABuffer;
class MediaBuffer;

struct ARTPWriter : public MediaWriter {
    explicit ARTPWriter(int fd);
    explicit ARTPWriter(int fd, String8& localIp, int localPort,
                                String8& remoteIp, int remotePort,
                                uint32_t seqNo);

    virtual status_t addSource(const sp<MediaSource> &source);
    virtual bool reachedEOS();
    virtual status_t start(MetaData *params);
    virtual status_t stop();
    virtual status_t pause();
    void updateCVODegrees(int32_t cvoDegrees);
    void updatePayloadType(int32_t payloadType);
    void updateSocketOpt();
    void updateSocketNetwork(int64_t socketNetwork);
    uint32_t getSequenceNum();
    virtual uint64_t getAccumulativeBytes() override;

    virtual void onMessageReceived(const sp<AMessage> &msg);
    virtual void setTMMBNInfo(uint32_t opponentID, uint32_t bitrate);

protected:
    virtual ~ARTPWriter();

private:
    enum {
        kWhatStart  = 'strt',
        kWhatStop   = 'stop',
        kWhatRead   = 'read',
        kWhatSendSR = 'sr  ',
    };

    enum {
        kFlagStarted  = 1,
        kFlagEOS      = 2,
    };

    Mutex mLock;
    Condition mCondition;
    uint32_t mFlags;

    int mFd;

#if LOG_TO_FILES
    int mRTPFd;
    int mRTCPFd;
#endif

    sp<MediaSource> mSource;
    sp<ALooper> mLooper;
    sp<AHandlerReflector<ARTPWriter> > mReflector;

    bool mIsIPv6;
    int mRTPSocket, mRTCPSocket;
    struct sockaddr_in mLocalAddr;
    struct sockaddr_in mRTPAddr;
    struct sockaddr_in mRTCPAddr;
    struct sockaddr_in6 mLocalAddr6;
    struct sockaddr_in6 mRTPAddr6;
    struct sockaddr_in6 mRTCPAddr6;
    int32_t mRtpLayer3Dscp;
    int32_t mRtpSockOptEcn;
    net_handle_t mRTPSockNetwork;

    AString mProfileLevel;
    AString mSeqParamSet;
    AString mPicParamSet;

    MediaBufferBase *mVPSBuf;
    MediaBufferBase *mSPSBuf;
    MediaBufferBase *mPPSBuf;

    uint32_t mClockRate;
    uint32_t mSourceID;
    uint32_t mPayloadType;
    uint32_t mSeqNo;
    uint32_t mRTPTimeBase;
    uint32_t mNumRTPSent;
    uint32_t mNumRTPOctetsSent;

    uint32_t mOpponentID;
    uint32_t mBitrate;
    typedef uint64_t Bytes;
    sp<TrafficRecorder<uint32_t /* Time */, Bytes> > mTrafficRec;

    int32_t mNumSRsSent;
    int32_t mRTPCVOExtMap;
    int32_t mRTPCVODegrees;

    enum {
        INVALID,
        H265,
        H264,
        H263,
        AMR_NB,
        AMR_WB,
    } mMode;

    static uint64_t GetNowNTP();
    uint32_t getRtpTime(int64_t timeUs);

    void initState();
    void onRead(const sp<AMessage> &msg);
    void onSendSR(const sp<AMessage> &msg);

    void addSR(const sp<ABuffer> &buffer);
    void addSDES(const sp<ABuffer> &buffer);
    void addTMMBN(const sp<ABuffer> &buffer);

    void makeH264SPropParamSets(MediaBufferBase *buffer);
    void dumpSessionDesc();

    void sendBye();
    void sendVPSSPSPPSIfIFrame(MediaBufferBase *mediaBuf, int64_t timeUs);
    void sendSPSPPSIfIFrame(MediaBufferBase *mediaBuf, int64_t timeUs);
    void sendHEVCData(MediaBufferBase *mediaBuf);
    void sendAVCData(MediaBufferBase *mediaBuf);
    void sendH263Data(MediaBufferBase *mediaBuf);
    void sendAMRData(MediaBufferBase *mediaBuf);

    void send(const sp<ABuffer> &buffer, bool isRTCP);
    void makeSocketPairAndBind(String8& localIp, int localPort, String8& remoteIp, int remotePort);

    void ModerateInstantTraffic(uint32_t samplePeriod, uint32_t limitBytes);
    DISALLOW_EVIL_CONSTRUCTORS(ARTPWriter);
};

}  // namespace android

#endif  // A_RTP_WRITER_H_
