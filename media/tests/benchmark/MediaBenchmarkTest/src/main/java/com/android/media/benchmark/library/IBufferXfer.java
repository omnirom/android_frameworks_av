/*
 * Copyright (C) 2022 The Android Open Source Project
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

package com.android.media.benchmark.library;
import android.media.MediaCodec;

import androidx.annotation.NonNull;

import java.util.ArrayDeque;
import java.nio.ByteBuffer;
/**
 * interfaces that can be used to implement
 * sending of buffers to external and receive using callbacks
 */
public class IBufferXfer {
  static class BufferXferInfo {
      public ByteBuffer buf;
      public int idx;
      public Object obj;
      public ArrayDeque<MediaCodec.BufferInfo> infos;
      public int flag;
      public int bytesRead;
      public boolean isComplete = true;
      public long presentationTimeUs;
  }

    public interface IProducerData {
          ByteBuffer getBuffer();
          ArrayDeque<MediaCodec.BufferInfo> getInfo();
    }
    public interface IProducer {
        // sets the consumer for sending buffers using 'consume'
        boolean setConsumer(@NonNull IConsumer consumer);
        // to enable consumers to send set of buffers back after consumption
        boolean returnBuffers(@NonNull ArrayDeque<IProducerData> buffers);
    }

    public interface IConsumer {
        // To let consumer know that if will receive buffers from here
        // also to return buffers using 'returnBuffers'
        boolean setProducer(@NonNull IProducer producer);

        // Called by producer to send buffers to consumer.
        boolean consume(final ArrayDeque<IProducerData> buffers);
    }
}
