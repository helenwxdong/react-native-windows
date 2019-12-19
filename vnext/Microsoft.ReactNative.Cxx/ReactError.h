// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "JSValue.h"

namespace winrt::Microsoft::ReactNative {

struct ReactError {
  std::string Code;
  std::string Message;
  JSValueObject UserInfo;
};

} // namespace winrt::Microsoft::ReactNative
