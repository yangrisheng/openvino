/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "lrn_kernel_within_channel_byxf_opt.h"
#include "kernel_selector_utils.h"
#include <vector>

namespace kernel_selector {
ParamsKey LRNKernelWithinChannelByxfOpt::GetSupportedKey() const {
    ParamsKey k;
    k.EnableInputDataType(Datatype::F16);
    k.EnableInputDataType(Datatype::F32);
    k.EnableOutputDataType(Datatype::F16);
    k.EnableOutputDataType(Datatype::F32);
    k.EnableOutputDataType(Datatype::INT8);
    k.EnableOutputDataType(Datatype::UINT8);
    k.EnableInputLayout(DataLayout::byxf);
    k.EnableOutputLayout(DataLayout::byxf);
    k.EnableLRNMode(LRNMode::WITHIN_CHANNEL);
    k.EnableLRNKernelDividerMode(KernelDividerMode::DYNAMIC);
    k.EnableLRNKernelDividerMode(KernelDividerMode::FIXED);
    k.EnableTensorOffset();
    k.EnableTensorPitches();
    k.EnableBatching();
    k.EnableDifferentTypes();
    return k;
}

JitConstants LRNKernelWithinChannelByxfOpt::GetJitConstants(
    const lrn_params& params,
    const LRNKernelBase::DispatchData& kd) const {
    const uint32_t round_norm_size = (params.localSize / 2) * 2 + 1;
    uint32_t numElement = round_norm_size * round_norm_size;
    const auto& input_dt = params.inputs[0].GetDType();

    if (params.normMode == LRNMode::ACROSS_CHANNEL) {
        numElement = round_norm_size;
    }

    const float num_element_div = 1.f / static_cast<float>(numElement);

    JitConstants jit = Parent::GetJitConstants(params, kd);
    jit.AddConstants({
        MakeJitConstant("NUM_ELEMENTS_DIV", num_element_div),
        MakeJitConstant("GWS_BATCH", 2),
        MakeJitConstant("GWS_FEATURE", 1),
        MakeJitConstant("GWS_YX", 0),
    });

    if (!params.fused_ops.empty()) {
        FusedOpsConfiguration conf = {"", {"b", "f + i", "y", "x"}, "lrn_result", input_dt, 1};
        jit.Merge(MakeFusedOpsJitConstants(params, {conf}));
    }

    return jit;
}

LRNKernelWithinChannelByxfOpt::Parent::DispatchData LRNKernelWithinChannelByxfOpt::SetDefault(
    const lrn_params& params) const {
    DispatchData kd = Parent::SetDefault(params);

    const auto& out = params.output;

    std::vector<size_t> global = {out.X().v * out.Y().v, CeilDiv(out.Feature().v, 8), out.Batch().v};
    auto local = GetOptimalLocalWorkGroupSizes(global, params.engineInfo);

    kd.gws0 = global[0];
    kd.gws1 = global[1];
    kd.gws2 = global[2];

    kd.lws0 = local[0];
    kd.lws1 = local[1];
    kd.lws2 = local[2];

    return kd;
}

bool LRNKernelWithinChannelByxfOpt::Validate(const Params& p, const optional_params& o) const {
    if (!LRNKernelBase::Validate(p, o)) {
        return false;
    }
    const lrn_params& params = static_cast<const lrn_params&>(p);
    if (params.inputs[0].Feature().v % 8 != 0) {
        return false;
    }
    return true;
}

KernelsData LRNKernelWithinChannelByxfOpt::GetKernelsData(const Params& params, const optional_params& options) const {
    return GetCommonKernelsData(params, options, FORCE_PRIORITY_7);
}
}  // namespace kernel_selector
