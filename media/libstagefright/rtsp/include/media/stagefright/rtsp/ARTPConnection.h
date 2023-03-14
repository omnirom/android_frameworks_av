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

#ifndef A_RTP_CONNECTION_H_

#define A_RTP_CONNECTION_H_

#include <media/stagefright/foundation/AHandler.h>
#include <utils/List.h>
#include <sys/socket.h>

namespace android {

struct ABuffer;
struct ARTPSource;
struct ASessionDescription;

struct ARTPConnection : public AHandler {
    enum Flags {
        kRegularlyRequestFIR = 2,
        kViLTEConnection = 4,
    };

    explicit ARTPConnection(uint32_t flags = 0);

    void addStream(
            int rtpSocket, int rtcpSocket,
            const sp<ASessionDescription> &sessionDesc, size_t index,
            const sp<AMessage> &notify,
            bool injected);
    void seekStream();
    void removeStream(int rtpSocket, int rtcpSocket);

    void injectPacket(int index, const sp<ABuffer> &buffer);

    void setSelfID(const uint32_t selfID);
    void setStaticJitterTimeMs(const uint32_t jbTimeMs);
    void setTargetBitrate(int32_t targetBitrate);
    void setRtpSockOptEcn(int32_t sockOptEcn);
    void setIsIPv6(const char *localIp);

    // Creates a pair of UDP datagram sockets bound to adjacent ports
    // (the rtpSocket is bound to an even port, the rtcpSocket to the
    // next higher port).
    static void MakePortPair(
            int *rtpSocket, int *rtcpSocket, unsigned *rtpPort);
    // Creates a pair of UDP datagram sockets bound to assigned ip and
    // ports (the rtpSocket is bound to an even port, the rtcpSocket
    // to the next higher port).
    static void MakeRTPSocketPair(
            int *rtpSocket, int *rtcpSocket,
            const char *localIp, const char *remoteIp,
            unsigned localPort, unsigned remotePort, int64_t socketNetwork = 0,
            int32_t sockOptEcn = 0);

protected:
    virtual ~ARTPConnection();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum {
        kWhatAddStream,
        kWhatSeekStream,
        kWhatRemoveStream,
        kWhatPollStreams,
        kWhatInjectPacket,
        kWhatAlarmStream,
    };

    static const int64_t kSelectTimeoutUs;
    static const int64_t kMinOneSecondNotifyDelayUs;

    uint32_t mFlags;

    struct StreamInfo;
    List<StreamInfo> mStreams;

    bool mPollEventPending;
    int64_t mLastReceiverReportTimeUs;
    int64_t mLastBitrateReportTimeUs;
    int64_t mLastEarlyNotifyTimeUs;
    int64_t mLastCongestionNotifyTimeUs;

    int32_t mSelfID;
    int32_t mTargetBitrate;
    int32_t mRtpSockOptEcn;
    bool mIsIPv6;

    uint32_t mStaticJitterTimeMs;

    int32_t mCumulativeBytes;

    void onAddStream(const sp<AMessage> &msg);
    void onSeekStream(const sp<AMessage> &msg);
    void onRemoveStream(const sp<AMessage> &msg);
    void onPollStreams();
    void onAlarmStream(const sp<AMessage> msg);
    void onInjectPacket(const sp<AMessage> &msg);
    void onSendReceiverReports();
    void checkRxBitrate(int64_t nowUs);
    void notifyCongestionToUpperLayerIfNeeded(StreamInfo *s);
    void handleIpHeadersIfReceived(StreamInfo *s, struct msghdr sMsg);

    status_t receive(StreamInfo *info, bool receiveRTP);
    ssize_t send(const StreamInfo *info, const sp<ABuffer> buffer);

    status_t parseRTP(StreamInfo *info, const sp<ABuffer> &buffer);
    status_t parseRTPExt(StreamInfo *s, const uint8_t *extData, size_t extLen, int32_t *cvoDegrees);
    status_t parseRTCP(StreamInfo *info, const sp<ABuffer> &buffer);
    status_t parseSenderReport(StreamInfo *info, const uint8_t *data, size_t size);
    status_t parseReceiverReport(StreamInfo *info, const uint8_t *data, size_t size);
    status_t parseReceptionReportBlock(StreamInfo *info,
            int64_t recvTimeUs, uint32_t senderId, const uint8_t *data, size_t size);
    status_t parseTSFB(StreamInfo *info, const uint8_t *data, size_t size);
    status_t parsePSFB(StreamInfo *info, const uint8_t *data, size_t size);
    status_t parseBYE(StreamInfo *info, const uint8_t *data, size_t size);

    sp<ARTPSource> findSource(StreamInfo *info, uint32_t id);

    void postPollEvent();

    DISALLOW_EVIL_CONSTRUCTORS(ARTPConnection);
};

}  // namespace android

#endif  // A_RTP_CONNECTION_H_
