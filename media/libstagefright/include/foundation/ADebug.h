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

#ifndef A_DEBUG_H_

#define A_DEBUG_H_

#include <string.h>

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AString.h>
#include <utils/Log.h>
#include <utils/Trace.h>

namespace android {

inline static const char *asString(status_t i, const char *def = "??") {
    switch (i) {
        case NO_ERROR:              return "NO_ERROR";
        case UNKNOWN_ERROR:         return "UNKNOWN_ERROR";
        case NO_MEMORY:             return "NO_MEMORY";
        case INVALID_OPERATION:     return "INVALID_OPERATION";
        case BAD_VALUE:             return "BAD_VALUE";
        case BAD_TYPE:              return "BAD_TYPE";
        case NAME_NOT_FOUND:        return "NAME_NOT_FOUND";
        case PERMISSION_DENIED:     return "PERMISSION_DENIED";
        case NO_INIT:               return "NO_INIT";
        case ALREADY_EXISTS:        return "ALREADY_EXISTS";
        case DEAD_OBJECT:           return "DEAD_OBJECT";
        case FAILED_TRANSACTION:    return "FAILED_TRANSACTION";
        case BAD_INDEX:             return "BAD_INDEX";
        case NOT_ENOUGH_DATA:       return "NOT_ENOUGH_DATA";
        case WOULD_BLOCK:           return "WOULD_BLOCK";
        case TIMED_OUT:             return "TIMED_OUT";
        case UNKNOWN_TRANSACTION:   return "UNKNOWN_TRANSACTION";
        case FDS_NOT_ALLOWED:       return "FDS_NOT_ALLOWED";
        default:                    return def;
    }
}

#define LITERAL_TO_STRING_INTERNAL(x)    #x
#define LITERAL_TO_STRING(x) LITERAL_TO_STRING_INTERNAL(x)

#define CHECK(condition)                                \
    LOG_ALWAYS_FATAL_IF(                                \
            !(condition),                               \
            "%s",                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)    \
            " CHECK(" #condition ") failed.")

#define MAKE_COMPARATOR(suffix,op)                          \
    template<class A, class B>                              \
    AString Compare_##suffix(const A &a, const B &b) {      \
        AString res;                                        \
        if (!(a op b)) {                                    \
            res.append(a);                                  \
            res.append(" vs. ");                            \
            res.append(b);                                  \
        }                                                   \
        return res;                                         \
    }

MAKE_COMPARATOR(EQ,==)
MAKE_COMPARATOR(NE,!=)
MAKE_COMPARATOR(LE,<=)
MAKE_COMPARATOR(GE,>=)
MAKE_COMPARATOR(LT,<)
MAKE_COMPARATOR(GT,>)

#define CHECK_OP(x,y,suffix,op)                                         \
    do {                                                                \
        AString ___res = Compare_##suffix(x, y);                        \
        if (!___res.empty()) {                                          \
            AString ___full =                                           \
                __FILE__ ":" LITERAL_TO_STRING(__LINE__)                \
                    " CHECK_" #suffix "( " #x "," #y ") failed: ";      \
            ___full.append(___res);                                     \
                                                                        \
            LOG_ALWAYS_FATAL("%s", ___full.c_str());                    \
        }                                                               \
    } while (false)

#define CHECK_EQ(x,y)   CHECK_OP(x,y,EQ,==)
#define CHECK_NE(x,y)   CHECK_OP(x,y,NE,!=)
#define CHECK_LE(x,y)   CHECK_OP(x,y,LE,<=)
#define CHECK_LT(x,y)   CHECK_OP(x,y,LT,<)
#define CHECK_GE(x,y)   CHECK_OP(x,y,GE,>=)
#define CHECK_GT(x,y)   CHECK_OP(x,y,GT,>)

#define TRESPASS(...) \
        LOG_ALWAYS_FATAL(                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)            \
                " Should not be here. " __VA_ARGS__);

#ifdef NDEBUG
#define CHECK_DBG CHECK
#define CHECK_EQ_DBG CHECK_EQ
#define CHECK_NE_DBG CHECK_NE
#define CHECK_LE_DBG CHECK_LE
#define CHECK_LT_DBG CHECK_LT
#define CHECK_GE_DBG CHECK_GE
#define CHECK_GT_DBG CHECK_GT
#define TRESPASS_DBG TRESPASS
#else
#define CHECK_DBG(condition)
#define CHECK_EQ_DBG(x,y)
#define CHECK_NE_DBG(x,y)
#define CHECK_LE_DBG(x,y)
#define CHECK_LT_DBG(x,y)
#define CHECK_GE_DBG(x,y)
#define CHECK_GT_DBG(x,y)
#define TRESPASS_DBG(...)
#endif

struct ADebug {
    enum Level {
        kDebugNone,             // no debug
        kDebugLifeCycle,        // lifecycle events: creation/deletion
        kDebugState,            // commands and events
        kDebugConfig,           // configuration
        kDebugInternalState,    // internal state changes
        kDebugAll,              // all
        kDebugMax = kDebugAll,

    };

    // parse the property or string to get a long-type level for a component name
    // string format is:
    // <level>[:<glob>][,<level>[:<glob>]...]
    // - <level> is 0-5 corresponding to ADebug::Level
    // - <glob> is used to match component name case insensitively, if omitted, it
    //   matches all components
    // - string is read left-to-right, and the last matching level is returned, or
    //   the def if no terms matched
    static long GetLevelFromSettingsString(
            const char *name, const char *value, long def);
    static long GetLevelFromProperty(
            const char *name, const char *value, long def);

    // same for ADebug::Level - performs clamping to valid debug ranges
    static Level GetDebugLevelFromProperty(
            const char *name, const char *propertyName, Level def = kDebugNone);

    // remove redundant segments of a codec name, and return a newly allocated
    // string suitable for debugging
    static char *GetDebugName(const char *name);

    inline static bool isExperimentEnabled(
            const char *name __unused /* nonnull */, bool allow __unused = true) {
#ifdef ENABLE_STAGEFRIGHT_EXPERIMENTS
        if (!strcmp(name, "legacy-adaptive")) {
            return getExperimentFlag(allow, name, 2, 1); // every other day
        } else if (!strcmp(name, "legacy-setsurface")) {
            return getExperimentFlag(allow, name, 3, 1); // every third day
        } else {
            ALOGE("unknown experiment '%s' (disabled)", name);
        }
#endif
        return false;
    }

    static const int kTraceLength = 1024;
    enum TraceSubmodule {
        NuPlayer,
        Codec,
        Extract,
        Mux,
        Render,
        Drm,
    };

    inline static bool isTraceEnabled(TraceSubmodule submodule) {
        return sTraceOptions & (((uint64_t)1) << (int)submodule);
    }

    inline static void TraceBegin(TraceSubmodule submodule,
            uint64_t tag, const char *name) {
        if (isTraceEnabled(submodule)) {
            atrace_begin(tag, name);
        }
    }

    inline static void TraceBegin(TraceSubmodule submodule,
            uint64_t tag, const char *className,
            const char *functionName) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s::%s", className, functionName);
            atrace_begin(tag, msg);
        }
    }

    inline static void TraceBegin(TraceSubmodule submodule,
            uint64_t tag, const char *className,
            const char *functionName, const void *thisPtr) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s::%s@%p", className, functionName, thisPtr);
            atrace_begin(tag, msg);
        }
    }

    inline static void TraceEnd(TraceSubmodule submodule, uint64_t tag) {
        if (isTraceEnabled(submodule)) {
            atrace_end(tag);
        }
    }

    inline static void TraceAsyncBegin(TraceSubmodule submodule,
            uint64_t tag, int cookie, const char *name, const void *thisPtr) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s@%p", name, thisPtr);
            atrace_async_begin(tag, msg, cookie);
        }
    }

    inline static void TraceAsyncEnd(TraceSubmodule submodule,
            uint64_t tag, int cookie, const char *name, const void *thisPtr) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s@%p", name, thisPtr);
            atrace_async_end(tag, msg, cookie);
        }
    }

    inline static void TraceInt(TraceSubmodule submodule,
            uint64_t tag, int32_t value, const char *name) {
        if (isTraceEnabled(submodule)) {
            atrace_int(tag, name, value);
        }
    }

    inline static void TraceInt(TraceSubmodule submodule,
            uint64_t tag, int32_t value, const char *name, const void *thisPtr) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s@%p", name, thisPtr);
            atrace_int(tag, msg, value);
        }
    }

    inline static void TraceInt64(TraceSubmodule submodule,
            uint64_t tag, int64_t value, const char *name, const void *thisPtr) {
        if (isTraceEnabled(submodule)) {
            char msg[kTraceLength];
            snprintf(msg, sizeof(msg), "%s@%p", name, thisPtr);
            atrace_int64(tag, msg, value);
        }
    }

    class ScopedTrace {
    public:
        inline ScopedTrace(TraceSubmodule submodule, uint64_t tag, const char *name)
        : mSubmodule(submodule), mTag(tag) {
            TraceBegin(mSubmodule, mTag, name);
        }

        inline ScopedTrace(TraceSubmodule submodule, uint64_t tag, const char *className,
               const char *functionName)
        : mSubmodule(submodule), mTag(tag) {
            TraceBegin(mSubmodule, mTag, className, functionName);
        }

        inline ScopedTrace(TraceSubmodule submodule, uint64_t tag, const char *className,
               const char *functionName, const void *thisPtr)
        : mSubmodule(submodule), mTag(tag) {
            TraceBegin(mSubmodule, mTag, className, functionName, thisPtr);
        }

        inline ~ScopedTrace() {
            TraceEnd(mSubmodule, mTag);
        }

    private:
        const TraceSubmodule mSubmodule;
        const uint64_t mTag;
    };

private:
    // pass in allow, so we can print in the log if the experiment is disabled
    static bool getExperimentFlag(
            bool allow, const char *name, uint64_t modulo, uint64_t limit,
            uint64_t plus = 0, uint64_t timeDivisor = 24 * 60 * 60 /* 1 day */);

    static uint64_t getTraceOptionsFromProperty();
    static uint64_t sTraceOptions; // from property persist.sys.media.traces
};

}  // namespace android

#define VTRACE_SUBMODULE_NUPLAYER    android::ADebug::TraceSubmodule::NuPlayer
#define VTRACE_SUBMODULE_CODEC       android::ADebug::TraceSubmodule::Codec
#define VTRACE_SUBMODULE_EXTRACT     android::ADebug::TraceSubmodule::Extract
#define VTRACE_SUBMODULE_MUX         android::ADebug::TraceSubmodule::Mux
#define VTRACE_SUBMODULE_RENDER      android::ADebug::TraceSubmodule::Render
#define VTRACE_SUBMODULE_DRM         android::ADebug::TraceSubmodule::Drm

#define VTRACE_IS_ENABLED() android::ADebug::isTraceEnabled(TRACE_SUBMODULE)

#define VTRACE_CONNECT(ptr1, ptr2) \
    do { \
        char msg[android::ADebug::kTraceLength]; \
        snprintf(msg, sizeof(msg), "Trace connect %p %p", ptr1, ptr2); \
        ALOGE("Trace connect %p %p", ptr1, ptr2); \
        VTRACE_SCOPE(msg); \
    } while (0)

#define VTRACE_SCOPE(name) android::ADebug::ScopedTrace ___scopedTrace(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, name)

#define VTRACE_CALL() android::ADebug::ScopedTrace ___scopedTrace(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, __CLASS__, __FUNCTION__)

#define VTRACE_METHOD() android::ADebug::ScopedTrace ___scopedTrace(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, __CLASS__, __FUNCTION__, this)

#define VTRACE_ASYNC_BEGIN(name, cookie) android::ADebug::TraceAsyncBegin(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, cookie, name, this)

#define VTRACE_ASYNC_END(name, cookie) android::ADebug::TraceAsyncEnd(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, cookie, name, this)

#define VTRACE_INT(name, value) android::ADebug::TraceInt(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, value, name, this)

#define VTRACE_INT64(name, value) android::ADebug::TraceInt64(TRACE_SUBMODULE, \
        ATRACE_TAG_VIDEO, value, name, this)

#define VTRACE_STRING(str) \
    do { \
        VTRACE_SCOPE(str); \
    } while (0)

#endif  // A_DEBUG_H_
