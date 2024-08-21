/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <executorch/runtime/core/exec_aten/exec_aten.h>
#include <executorch/runtime/core/exec_aten/util/dim_order_util.h>
#include <executorch/runtime/core/exec_aten/util/tensor_util.h>
#include <executorch/runtime/platform/assert.h>
#include <memory>
// NOTE: required by torchchat install_et.sh script.
// @nolint PATTERNLINT Ok to use stdlib for this optional library
#include <vector>

#ifdef USE_ATEN_LIB
#include <torch/torch.h>
#else
#include <executorch/runtime/core/portable_type/tensor.h>
#endif
#pragma once

namespace torch {
namespace executor {

/**
 * A tensor wrapper takes ownership of all the memory of the necessary metadata
 * for torch::executor::Tensor. Note that it doesn't own the data memory.
 */
class ManagedTensor {
 public:
  /// The type used for elements of `sizes()`.
  using SizesType = exec_aten::SizesType;
  /// The type used for elements of `dim_order()`.
  using DimOrderType = exec_aten::DimOrderType;
  /// The type used for elements of `strides()`.
  using StridesType = exec_aten::StridesType;

  ManagedTensor() = delete;

  explicit ManagedTensor(
      void* data,
      const std::vector<SizesType>& sizes,
      ScalarType dtype)
      : sizes_(sizes) {
#ifdef USE_ATEN_LIB
    tensor_ = torch::from_blob(data, sizes, dtype);
#else
    // Calculate strides.
    strides_ = std::vector<StridesType>(sizes_.size());
    if (sizes_.size() > 0) {
      strides_.back() = 1;
      for (size_t i = strides_.size() - 1; i > 0; --i) {
        strides_[i - 1] = strides_[i] * sizes_[i];
      }
    }

    // Allocate TensorImpl.
    tensor_impl_ = std::make_unique<TensorImpl>(
        dtype,
        sizes_.size(),
        sizes_.data(),
        data,
        /*dim_order=*/nullptr,
        strides_.data(),
        TensorShapeDynamism::DYNAMIC_BOUND);
#endif
  }

  void resize(const std::vector<SizesType>& new_sizes) {
    auto err = resize_tensor(
        this->get_aliasing_tensor(),
        exec_aten::ArrayRef<SizesType>(new_sizes.data(), new_sizes.size()));
    ET_CHECK(err == Error::Ok);
  }

  /**
   * Get the underlying Tensor object. This is assuming the copying is cheap.
   */
  Tensor get_aliasing_tensor() {
#ifdef USE_ATEN_LIB
    return tensor_;
#else
    return Tensor(tensor_impl_.get());
#endif
  }

 private:
  std::unique_ptr<TensorImpl> tensor_impl_;
  std::vector<SizesType> sizes_;
  std::vector<StridesType> strides_;
#ifdef USE_ATEN_LIB
  Tensor tensor_;
#endif
};

} // namespace executor
} // namespace torch