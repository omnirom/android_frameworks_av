/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ANDROID_SERVERS_CAMERA_QTICAMERA2CLIENT_H
#define ANDROID_SERVERS_CAMERA_QTICAMERA2CLIENT_H

namespace android {

using namespace camera2;

class Camera2Client;

class QTICamera2Client: public virtual RefBase{
private:
    wp<Camera2Client> mParentClient;
    status_t stopPreviewExtn();

public:
    QTICamera2Client(sp<Camera2Client> client);
    ~QTICamera2Client();
    status_t setParametersExtn(Parameters &params);
    status_t startHFRRecording(Parameters &params);
    void stopHFRRecording(Parameters &params);
    status_t sendCommand(Parameters &params,int32_t cmd, int32_t arg1, int32_t arg2);
    status_t configureRaw(Parameters &params);

private:
    void stopPreviewForRestart(Parameters &params);
    status_t restartVideoHdr(Parameters &params);

};
}; // namespace android

#endif
