// Copyright 2018 The Dawn Authors
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

#include "dawn_native/vulkan/QueueVk.h"

#include "dawn_native/vulkan/CommandBufferVk.h"
#include "dawn_native/vulkan/DeviceVk.h"

namespace dawn_native { namespace vulkan {

    Queue::Queue(Device* device) : QueueBase(device) {
    }

    Queue::~Queue() {
    }

    void Queue::Submit(uint32_t numCommands, CommandBuffer* const* commands) {
        Device* device = ToBackend(GetDevice());

        VkCommandBuffer commandBuffer = device->GetPendingCommandBuffer();
        for (uint32_t i = 0; i < numCommands; ++i) {
            commands[i]->RecordCommands(commandBuffer);
        }

        device->SubmitPendingCommands();
    }

}}  // namespace dawn_native::vulkan
