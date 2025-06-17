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
#pragma once

#include <android/content/AttributionSourceState.h>

#include <iterator>

// AttributionSourceState are essentially an intrusive linked list, where the next field carries
// the pointer to the next element. These iterator helpers allow for convenient iteration over the
// entire attribution chain. Usage:
//   std::for_each(AttrSourceIter::begin(mAttributionSourceState), AttrSourceIter::end(), ...)
namespace android::media::permission::AttrSourceIter {

class ConstIter {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ::android::content::AttributionSourceState;
    using pointer = const value_type*;
    using reference = const value_type&;

    ConstIter(const ::android::content::AttributionSourceState* attr) : mAttr(attr) {}

    reference operator*() const { return *mAttr; }
    pointer operator->() const { return mAttr; }

    ConstIter& operator++() {
        mAttr = !mAttr->next.empty() ? mAttr->next.data() : nullptr;
        return *this;
    }
    ConstIter operator++(int) {
        ConstIter tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const ConstIter& a, const ConstIter& b) = default;

    static ConstIter end() { return ConstIter(nullptr); }

  private:
    const ::android::content::AttributionSourceState* mAttr;
};

/**
 * Non-const iterator. Note, AttributionSourceState is conceptually a linked list on the next field.
 * Be very careful if `next` is modified over iteration, as it can go wrong easily.
 */
class Iter {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ::android::content::AttributionSourceState;
    using pointer = value_type*;
    using reference = value_type&;

    Iter(::android::content::AttributionSourceState* attr) : mAttr(attr) {}

    reference operator*() const { return *mAttr; }
    pointer operator->() const { return mAttr; }

    Iter& operator++() {
        mAttr = !mAttr->next.empty() ? mAttr->next.data() : nullptr;
        return *this;
    }
    Iter operator++(int) {
        Iter tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const Iter& a, const Iter& b) = default;

    operator ConstIter() const { return ConstIter(mAttr); }

    static Iter end() { return Iter(nullptr); }

  private:
    ::android::content::AttributionSourceState* mAttr;
};

inline Iter begin(::android::content::AttributionSourceState& a) {
    return Iter(&a);
}
inline Iter end() {
    return Iter::end();
}
inline ConstIter cbegin(const ::android::content::AttributionSourceState& a) {
    return ConstIter(&a);
}
inline ConstIter cend() {
    return ConstIter::end();
}
}  // namespace android::media::permission::AttrSourceIter
