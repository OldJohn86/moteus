// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "mjlib/base/system_error.h"

#include "mjlib/micro/static_function.h"

namespace mjlib {
namespace micro {

using VoidCallback = StaticFunction<void (void)>;
using ErrorCallback = StaticFunction<void (const base::error_code&)>;
using SizeCallback = StaticFunction<void (const base::error_code&, ssize_t)>;

}
}