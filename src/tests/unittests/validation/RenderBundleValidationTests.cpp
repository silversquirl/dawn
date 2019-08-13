// Copyright 2019 The Dawn Authors
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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Constants.h"

#include "utils/ComboRenderBundleEncoderDescriptor.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

namespace {

    class RenderBundleValidationTest : public ValidationTest {
      protected:
        void SetUp() override {
            ValidationTest::SetUp();

            vsModule = utils::CreateShaderModule(device, utils::ShaderStage::Vertex, R"(
              #version 450
              layout(location = 0) in vec2 pos;
              layout (set = 0, binding = 0) uniform vertexUniformBuffer {
                  mat2 transform;
              };
              void main() {
              })");

            fsModule = utils::CreateShaderModule(device, utils::ShaderStage::Fragment, R"(
              #version 450
              layout (set = 1, binding = 0) uniform fragmentUniformBuffer {
                  vec4 color;
              };
              layout (set = 1, binding = 1) buffer storageBuffer {
                  float dummy[];
              };
              void main() {
              })");

            dawn::BindGroupLayout bgls[] = {
                utils::MakeBindGroupLayout(
                    device, {{0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer}}),
                utils::MakeBindGroupLayout(
                    device,
                    {
                        {0, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer},
                        {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::StorageBuffer},
                    })};

            dawn::PipelineLayoutDescriptor pipelineLayoutDesc;
            pipelineLayoutDesc.bindGroupLayoutCount = 2;
            pipelineLayoutDesc.bindGroupLayouts = bgls;

            pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

            utils::ComboRenderPipelineDescriptor descriptor = MakeRenderPipelineDescriptor();
            pipeline = device.CreateRenderPipeline(&descriptor);

            float data[4];
            dawn::Buffer buffer = utils::CreateBufferFromData(device, data, 4 * sizeof(float),
                                                              dawn::BufferUsageBit::Uniform);

            constexpr static float kVertices[] = {-1.f, 1.f, 1.f, -1.f, -1.f, 1.f};

            vertexBuffer = utils::CreateBufferFromData(device, kVertices, sizeof(kVertices),
                                                       dawn::BufferUsageBit::Vertex);

            // Dummy storage buffer.
            dawn::Buffer storageBuffer = utils::CreateBufferFromData(
                device, kVertices, sizeof(kVertices), dawn::BufferUsageBit::Storage);

            // Vertex buffer with storage usage for testing read+write error usage.
            vertexStorageBuffer = utils::CreateBufferFromData(
                device, kVertices, sizeof(kVertices),
                dawn::BufferUsageBit::Vertex | dawn::BufferUsageBit::Storage);

            bg0 = utils::MakeBindGroup(device, bgls[0], {{0, buffer, 0, 4 * sizeof(float)}});
            bg1 = utils::MakeBindGroup(
                device, bgls[1],
                {{0, buffer, 0, 4 * sizeof(float)}, {1, storageBuffer, 0, sizeof(kVertices)}});

            bg1Vertex = utils::MakeBindGroup(device, bgls[1],
                                             {{0, buffer, 0, 4 * sizeof(float)},
                                              {1, vertexStorageBuffer, 0, sizeof(kVertices)}});
        }

        utils::ComboRenderPipelineDescriptor MakeRenderPipelineDescriptor() {
            utils::ComboRenderPipelineDescriptor descriptor(device);
            descriptor.layout = pipelineLayout;
            descriptor.cVertexStage.module = vsModule;
            descriptor.cFragmentStage.module = fsModule;
            descriptor.cVertexInput.bufferCount = 1;
            descriptor.cVertexInput.cBuffers[0].stride = 2 * sizeof(float);
            descriptor.cVertexInput.cBuffers[0].attributeCount = 1;
            descriptor.cVertexInput.cAttributes[0].format = dawn::VertexFormat::Float2;

            return descriptor;
        }

        dawn::ShaderModule vsModule;
        dawn::ShaderModule fsModule;
        dawn::PipelineLayout pipelineLayout;
        dawn::RenderPipeline pipeline;
        dawn::Buffer vertexBuffer;
        dawn::Buffer vertexStorageBuffer;
        const uint64_t zeroOffset = 0;
        dawn::BindGroup bg0;
        dawn::BindGroup bg1;
        dawn::BindGroup bg1Vertex;
    };

}  // anonymous namespace

// Test creating and encoding an empty render bundle.
TEST_F(RenderBundleValidationTest, Empty) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
    pass.ExecuteBundles(1, &renderBundle);
    pass.EndPass();
    commandEncoder.Finish();
}

// Test executing zero render bundles.
TEST_F(RenderBundleValidationTest, ZeroBundles) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
    pass.ExecuteBundles(0, nullptr);
    pass.EndPass();
    commandEncoder.Finish();
}

// Test successfully creating and encoding a render bundle into a command buffer.
TEST_F(RenderBundleValidationTest, SimpleSuccess) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
    renderBundleEncoder.SetPipeline(pipeline);
    renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
    renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
    renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
    renderBundleEncoder.Draw(3, 0, 0, 0);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
    pass.ExecuteBundles(1, &renderBundle);
    pass.EndPass();
    commandEncoder.Finish();
}

// Test render bundles do not inherit command buffer state
TEST_F(RenderBundleValidationTest, StateInheritance) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    // Render bundle does not inherit pipeline so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);

        pass.SetPipeline(pipeline);

        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        ASSERT_DEVICE_ERROR(dawn::RenderBundle renderBundle = renderBundleEncoder.Finish());

        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not inherit bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);

        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);

        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        ASSERT_DEVICE_ERROR(dawn::RenderBundle renderBundle = renderBundleEncoder.Finish());

        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not inherit pipeline and bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);

        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);

        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        ASSERT_DEVICE_ERROR(dawn::RenderBundle renderBundle = renderBundleEncoder.Finish());

        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not inherit buffers so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);

        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);

        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        ASSERT_DEVICE_ERROR(dawn::RenderBundle renderBundle = renderBundleEncoder.Finish());

        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test render bundles do not persist command buffer state
TEST_F(RenderBundleValidationTest, StatePersistence) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    // Render bundle does not persist pipeline so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        pass.ExecuteBundles(1, &renderBundle);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not persist bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        pass.ExecuteBundles(1, &renderBundle);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not persist pipeline and bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        pass.ExecuteBundles(1, &renderBundle);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle does not persist buffers so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        pass.ExecuteBundles(1, &renderBundle);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test executing render bundles clears command buffer state
TEST_F(RenderBundleValidationTest, ClearsState) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    // Render bundle clears pipeline so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetPipeline(pipeline);
        pass.ExecuteBundles(1, &renderBundle);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle clears bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.ExecuteBundles(1, &renderBundle);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle clears pipeline and bind groups so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.ExecuteBundles(1, &renderBundle);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Render bundle clears buffers so the draw is invalid.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.ExecuteBundles(1, &renderBundle);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.Draw(3, 0, 0, 0);
        pass.EndPass();

        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Test executing 0 bundles does not clear command buffer state.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.ExecuteBundles(0, nullptr);
        pass.Draw(3, 0, 0, 0);

        pass.EndPass();
        commandEncoder.Finish();
    }
}

// Test creating and encoding multiple render bundles.
TEST_F(RenderBundleValidationTest, MultipleBundles) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundle renderBundles[2] = {};

    dawn::RenderBundleEncoder renderBundleEncoder0 = device.CreateRenderBundleEncoder(&desc);
    renderBundleEncoder0.SetPipeline(pipeline);
    renderBundleEncoder0.SetBindGroup(0, bg0, 0, nullptr);
    renderBundleEncoder0.SetBindGroup(1, bg1, 0, nullptr);
    renderBundleEncoder0.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
    renderBundleEncoder0.Draw(3, 1, 0, 0);
    renderBundles[0] = renderBundleEncoder0.Finish();

    dawn::RenderBundleEncoder renderBundleEncoder1 = device.CreateRenderBundleEncoder(&desc);
    renderBundleEncoder1.SetPipeline(pipeline);
    renderBundleEncoder1.SetBindGroup(0, bg0, 0, nullptr);
    renderBundleEncoder1.SetBindGroup(1, bg1, 0, nullptr);
    renderBundleEncoder1.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
    renderBundleEncoder1.Draw(3, 1, 0, 0);
    renderBundles[1] = renderBundleEncoder1.Finish();

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
    pass.ExecuteBundles(2, renderBundles);
    pass.EndPass();
    commandEncoder.Finish();
}

// Test that is is valid to execute a render bundle more than once.
TEST_F(RenderBundleValidationTest, ExecuteMultipleTimes) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
    renderBundleEncoder.SetPipeline(pipeline);
    renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
    renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
    renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
    renderBundleEncoder.Draw(3, 1, 0, 0);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
    pass.ExecuteBundles(1, &renderBundle);
    pass.ExecuteBundles(1, &renderBundle);
    pass.ExecuteBundles(1, &renderBundle);
    pass.EndPass();
    commandEncoder.Finish();
}

// Test that it is an error to call Finish() on a render bundle encoder twice.
TEST_F(RenderBundleValidationTest, FinishTwice) {
    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = dawn::TextureFormat::RGBA8Uint;

    dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
    renderBundleEncoder.Finish();
    ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
}

// Test that it is invalid to create a render bundle with no texture formats
TEST_F(RenderBundleValidationTest, RequiresAtLeastOneTextureFormat) {
    // Test failure case.
    {
        utils::ComboRenderBundleEncoderDescriptor desc = {};
        ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(&desc));
    }

    // Test success with one color format.
    {
        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        desc.cColorFormats[0] = dawn::TextureFormat::RGBA8Uint;
        device.CreateRenderBundleEncoder(&desc);
    }

    // Test success with a depth stencil format.
    {
        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.cDepthStencilFormat = dawn::TextureFormat::Depth24PlusStencil8;
        desc.depthStencilFormat = &desc.cDepthStencilFormat;
        device.CreateRenderBundleEncoder(&desc);
    }
}

// Test that resource usages are validated inside render bundles.
TEST_F(RenderBundleValidationTest, UsageTracking) {
    DummyRenderPass renderPass(device);

    utils::ComboRenderBundleEncoderDescriptor desc = {};
    desc.colorFormatsCount = 1;
    desc.cColorFormats[0] = renderPass.attachmentFormat;

    dawn::RenderBundle renderBundle0;
    dawn::RenderBundle renderBundle1;

    // First base case is successful. |bg1Vertex| does not reference |vertexBuffer|.
    {
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1Vertex, 0, nullptr);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        renderBundle0 = renderBundleEncoder.Finish();
    }

    // Second base case is successful. |bg1| does not reference |vertexStorageBuffer|
    {
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexStorageBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        renderBundle1 = renderBundleEncoder.Finish();
    }

    // Test that a render bundle which sets a buffer as both vertex and storage is invalid.
    // |bg1Vertex| references |vertexStorageBuffer|
    {
        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1Vertex, 0, nullptr);
        renderBundleEncoder.SetVertexBuffers(0, 1, &vertexStorageBuffer, &zeroOffset);
        renderBundleEncoder.Draw(3, 0, 0, 0);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }

    // When both render bundles are in the same pass, |vertexStorageBuffer| is used
    // as both read and write usage. This is invalid.
    // renderBundle0 uses |vertexStorageBuffer| as a storage buffer.
    // renderBundle1 uses |vertexStorageBuffer| as a vertex buffer.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle0);
        pass.ExecuteBundles(1, &renderBundle1);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // |vertexStorageBuffer| is used as both read and write usage. This is invalid.
    // The render pass uses |vertexStorageBuffer| as a storage buffer.
    // renderBundle1 uses |vertexStorageBuffer| as a vertex buffer.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1Vertex, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);

        pass.ExecuteBundles(1, &renderBundle1);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // |vertexStorageBuffer| is used as both read and write usage. This is invalid.
    // renderBundle0 uses |vertexStorageBuffer| as a storage buffer.
    // The render pass uses |vertexStorageBuffer| as a vertex buffer.
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);

        pass.ExecuteBundles(1, &renderBundle0);

        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg0, 0, nullptr);
        pass.SetBindGroup(1, bg1, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexStorageBuffer, &zeroOffset);
        pass.Draw(3, 0, 0, 0);

        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test that encoding SetPipline with an incompatible color format produces an error.
TEST_F(RenderBundleValidationTest, PipelineColorFormatMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 3;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
    renderBundleDesc.cColorFormats[1] = dawn::TextureFormat::RG16Float;
    renderBundleDesc.cColorFormats[2] = dawn::TextureFormat::R16Sint;

    utils::ComboRenderPipelineDescriptor renderPipelineDesc = MakeRenderPipelineDescriptor();
    renderPipelineDesc.colorStateCount = 3;
    renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
    renderPipelineDesc.cColorStates[1]->format = dawn::TextureFormat::RG16Float;
    renderPipelineDesc.cColorStates[2]->format = dawn::TextureFormat::R16Sint;

    // Test the success case.
    {
        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.Finish();
    }

    // Test the failure case for mismatched format types.
    {
        utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
        desc.cColorStates[1]->format = dawn::TextureFormat::RGBA8Unorm;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }

    // Test the failure case for missing format
    {
        utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
        desc.colorStateCount = 2;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }
}

// Test that encoding SetPipline with an incompatible depth stencil format produces an error.
TEST_F(RenderBundleValidationTest, PipelineDepthStencilFormatMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
    renderBundleDesc.cDepthStencilFormat = dawn::TextureFormat::Depth24PlusStencil8;
    renderBundleDesc.depthStencilFormat = &renderBundleDesc.cDepthStencilFormat;

    utils::ComboRenderPipelineDescriptor renderPipelineDesc = MakeRenderPipelineDescriptor();
    renderPipelineDesc.colorStateCount = 1;
    renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
    renderPipelineDesc.depthStencilState = &renderPipelineDesc.cDepthStencilState;
    renderPipelineDesc.cDepthStencilState.format = dawn::TextureFormat::Depth24PlusStencil8;

    // Test the success case.
    {
        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.Finish();
    }

    // Test the failure case for mismatched format.
    {
        utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
        desc.cDepthStencilState.format = dawn::TextureFormat::Depth24Plus;
        desc.depthStencilState = &desc.cDepthStencilState;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }

    // Test the failure case for missing format.
    {
        utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
        desc.depthStencilState = nullptr;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }
}

// Test that encoding SetPipline with an incompatible sample count produces an error.
TEST_F(RenderBundleValidationTest, PipelineSampleCountMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
    renderBundleDesc.sampleCount = 4;

    utils::ComboRenderPipelineDescriptor renderPipelineDesc = MakeRenderPipelineDescriptor();
    renderPipelineDesc.colorStateCount = 1;
    renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
    renderPipelineDesc.sampleCount = 4;

    // Test the success case.
    {
        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.Finish();
    }

    // Test the failure case.
    {
        renderPipelineDesc.sampleCount = 1;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
        renderBundleEncoder.SetPipeline(pipeline);
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }
}

// Test that encoding ExecuteBundles with an incompatible color format produces an error.
TEST_F(RenderBundleValidationTest, RenderPassColorFormatMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 3;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
    renderBundleDesc.cColorFormats[1] = dawn::TextureFormat::RG16Float;
    renderBundleDesc.cColorFormats[2] = dawn::TextureFormat::R16Sint;

    dawn::RenderBundleEncoder renderBundleEncoder =
        device.CreateRenderBundleEncoder(&renderBundleDesc);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::TextureDescriptor textureDesc = {};
    textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
    textureDesc.size = dawn::Extent3D({400, 400, 1});

    textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
    dawn::Texture tex0 = device.CreateTexture(&textureDesc);

    textureDesc.format = dawn::TextureFormat::RG16Float;
    dawn::Texture tex1 = device.CreateTexture(&textureDesc);

    textureDesc.format = dawn::TextureFormat::R16Sint;
    dawn::Texture tex2 = device.CreateTexture(&textureDesc);

    // Test the success case
    {
        utils::ComboRenderPassDescriptor renderPass({
            tex0.CreateDefaultView(),
            tex1.CreateDefaultView(),
            tex2.CreateDefaultView(),
        });

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test the failure case for mismatched format
    {
        utils::ComboRenderPassDescriptor renderPass({
            tex0.CreateDefaultView(),
            tex1.CreateDefaultView(),
            tex0.CreateDefaultView(),
        });

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Test the failure case for missing format
    {
        utils::ComboRenderPassDescriptor renderPass({
            tex0.CreateDefaultView(),
            tex1.CreateDefaultView(),
        });

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test that encoding ExecuteBundles with an incompatible depth stencil format produces an
// error.
TEST_F(RenderBundleValidationTest, RenderPassDepthStencilFormatMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
    renderBundleDesc.cDepthStencilFormat = dawn::TextureFormat::Depth24Plus;
    renderBundleDesc.depthStencilFormat = &renderBundleDesc.cDepthStencilFormat;

    dawn::RenderBundleEncoder renderBundleEncoder =
        device.CreateRenderBundleEncoder(&renderBundleDesc);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::TextureDescriptor textureDesc = {};
    textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
    textureDesc.size = dawn::Extent3D({400, 400, 1});

    textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
    dawn::Texture tex0 = device.CreateTexture(&textureDesc);

    textureDesc.format = dawn::TextureFormat::Depth24Plus;
    dawn::Texture tex1 = device.CreateTexture(&textureDesc);

    textureDesc.format = dawn::TextureFormat::Depth32Float;
    dawn::Texture tex2 = device.CreateTexture(&textureDesc);

    // Test the success case
    {
        utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()},
                                                    tex1.CreateDefaultView());

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test the failure case for mismatched format
    {
        utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()},
                                                    tex2.CreateDefaultView());

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Test the failure case for missing format
    {
        utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()});

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test that encoding ExecuteBundles with an incompatible sample count produces an error.
TEST_F(RenderBundleValidationTest, RenderPassSampleCountMismatch) {
    utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
    renderBundleDesc.colorFormatsCount = 1;
    renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;

    dawn::RenderBundleEncoder renderBundleEncoder =
        device.CreateRenderBundleEncoder(&renderBundleDesc);
    dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

    dawn::TextureDescriptor textureDesc = {};
    textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
    textureDesc.size = dawn::Extent3D({400, 400, 1});

    textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
    dawn::Texture tex0 = device.CreateTexture(&textureDesc);

    textureDesc.sampleCount = 4;
    dawn::Texture tex1 = device.CreateTexture(&textureDesc);

    // Test the success case
    {
        utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()});

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test the failure case
    {
        utils::ComboRenderPassDescriptor renderPass({tex1.CreateDefaultView()});

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}
