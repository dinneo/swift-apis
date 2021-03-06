/*
 * Copyright 2020 TensorFlow Authors
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

#ifndef X10_XLA_TENSOR_WRAPPER_H_
#define X10_XLA_TENSOR_WRAPPER_H_

#include <stdbool.h>
#include <stdlib.h>

#include "swift_bindings/device_wrapper.h"

#ifdef __cplusplus
#include "tensorflow/compiler/tf2xla/xla_tensor/tensor.h"
#include "tensorflow/core/profiler/lib/traceme.h"
using OpaqueMaterializedTensor = at::Tensor;
using OpaqueXLATensor = swift_xla::XLATensor;
using OpaqueXLAShape = xla::util::MaybeRef<xla::Shape>;
using XLAAnnotationScope = tensorflow::profiler::TraceMe;
using OpaqueString = std::string;
extern "C" {
#else
typedef struct OpaqueXLATensor {
} OpaqueXLATensor;
typedef struct OpaqueXLAShape {
} OpaqueXLAShape;
typedef struct OpaqueMaterializedTensor {
} OpaqueMaterializedTensor;
typedef struct XLAAnnotationScope {
} XLAAnnotationScope;
typedef struct OpaqueString {
} OpaqueString;
#endif

XLAAnnotationScope* MakeAnnotationScope(const char* scope);
void DestroyAnnotationScope(XLAAnnotationScope* scope);

// Scalar utilities:
#define LIST_SCALAR_TYPES(_)     \
  _(Bool, Bool, bool)            \
  _(Float, Float, float)         \
  _(BFloat16, BFloat16, int16_t) \
  _(Half, Half, int16_t)         \
  _(Double, Double, double)      \
  _(UInt8, Byte, uint8_t)        \
  _(Int8, Char, int8_t)          \
  _(Int16, Short, int16_t)       \
  _(Int32, Int, int32_t)         \
  _(Int64, Long, int64_t)

enum XLATensorScalarType {
#define DEFINE_ENUM_CASE(name, aten_name, type) XLATensorScalarType_##name,
  LIST_SCALAR_TYPES(DEFINE_ENUM_CASE)
#undef DEFINE_ENUM_CASE
};

enum XLAScalarTypeTag { XLAScalarTypeTag_i, XLAScalarTypeTag_d };
typedef struct XLAScalar {
  enum XLAScalarTypeTag tag;
  union Value {
    int64_t i;
    double d;
  } value;
} XLAScalar;

#ifdef __cplusplus
at::ScalarType ToScalarType(XLATensorScalarType type);
XLATensorScalarType FromScalarType(at::ScalarType type);
#endif

// Tensor utilities:
OpaqueXLATensor* XLATensor_makeScalar(XLAScalar value,
                                      enum XLATensorScalarType type,
                                      const struct CDevice cdevice);

// TODO(parkers): Make aliasing constructor.
OpaqueXLATensor* copyTensor(enum XLATensorScalarType type, const void* value,
                            size_t num_entries, const size_t* shape,
                            size_t rank, const struct CDevice device);
// Copies tensor directly using xla's linearizer into temporary memory and then
// schedule an async copy to device. This avoids copies at the cost of being
// eager about doing a device copy. Except for this explicit copy, it is
// identical to copyTensor.
OpaqueXLATensor* copyTensorAndMakeResident(enum XLATensorScalarType type,
                                           const void* value,
                                           size_t num_entries,
                                           const size_t* shape, size_t rank,
                                           const struct CDevice device,
                                           bool to_reduced_precision);
void destroyTensor(OpaqueXLATensor* t);
OpaqueMaterializedTensor* XLATensor_materialize(OpaqueXLATensor* t);
void destroyMaterializedTensor(OpaqueMaterializedTensor* t);
const void* MaterializedTensor_getData(OpaqueMaterializedTensor* t);
enum XLATensorScalarType MaterializedTensor_getType(
    OpaqueMaterializedTensor* t);
enum XLATensorScalarType XLATensor_dtype(OpaqueXLATensor* a);
enum XLATensorScalarType XLATensor_physical_scalar_type(OpaqueXLATensor* a);

// Shape utilities:

OpaqueXLAShape* fetchTensorShape(OpaqueXLATensor* tensor);
void destroyXLAShape(OpaqueXLAShape* shape);
size_t XLAShape_getRank(OpaqueXLAShape* shape);
const int64_t* XLAShape_getDimensions(OpaqueXLAShape* shape);

enum TFPadding {
  TFPadding_VALID = 1,     // No padding.
  TFPadding_SAME = 2,      // Input and output layers have the same size.
  TFPadding_EXPLICIT = 3,  // Padding is explicitly specified
};

// Tensor format for input/output activations used in convolution operations.
// The mnemonics specify the meaning of each tensor dimension sorted from
// largest to smallest memory stride.
// N = Batch, H = Image Height, W = Image Width, C = Number of Channels.
enum TFDataFormat {
  TFDataFormat_NHWC = 0,
  TFDataFormat_NCHW = 1,
  TFDataFormat_NCHW_VECT_C = 2,
};

#ifdef __cplusplus
namespace x10 {
tensorflow::TensorFormat ToTFFormat(TFDataFormat data_format);
}  // namespace x10
#endif

enum TFMirrorPadMode {
  TFMirrorPadMode_REFLECT = 1,
  TFMirrorPadMode_SYMMETRIC = 2,
};

// XLA utilities:

typedef struct Int64ArrayRef {
  const int64_t* data;
  size_t size;
#ifdef __cplusplus
  tensorflow::gtl::ArraySlice<const xla::int64> slice() {
    static_assert(sizeof(int64_t) == sizeof(xla::int64), "Sanity");
    return tensorflow::gtl::ArraySlice<const xla::int64>(
        reinterpret_cast<const xla::int64*>(data), size);
  }
#endif
} Int64ArrayRef;

typedef struct OpaqueXLATensorArrayRef {
  OpaqueXLATensor* const* data;
  size_t size;
#ifdef __cplusplus
  std::vector<swift_xla::XLATensor> array() {
    std::vector<swift_xla::XLATensor> result;
    result.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      result.push_back(*data[i]);
    }
    return result;
  }
#endif
} OpaqueXLATensorArrayRef;

typedef struct Optional_XLAScalarType {
  bool has_value;
  enum XLATensorScalarType type;
#ifdef __cplusplus
  c10::optional<at::ScalarType> value() {
    if (!has_value) return absl::nullopt;
    return ToScalarType(type);
  }
#endif
} Optional_XLAScalarType;

typedef struct OpaqueXLATensor_pair {
  OpaqueXLATensor* x;
  OpaqueXLATensor* y;
} OpaqueXLATensor_pair;

typedef struct StridedSliceSpec {
  Int64ArrayRef begin;
  Int64ArrayRef end;
  Int64ArrayRef strides;
  Int64ArrayRef processing_sizes;
  Int64ArrayRef final_sizes;
} StridedSliceSpec;

typedef struct PaddingConfigDimension {
  int64_t edge_padding_low;
  int64_t edge_padding_high;
  int64_t interior_padding;
} PaddingConfigDimension;

typedef struct PaddingConfig {
  const PaddingConfigDimension* dimensions;
  size_t count;
} PaddingConfig;

void destroyOpaqueXLATensorArrayRef(OpaqueXLATensorArrayRef tensor_list);

void destroyStridedSliceSpec(StridedSliceSpec* strided_slice_spec);

// Ops:
OpaqueXLATensor* XLATensor_abs(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_acos(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_acosh(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_add(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_all(OpaqueXLATensor* input, Int64ArrayRef dimensions,
                               bool keep_reduced_dimensions);
OpaqueXLATensor* XLATensor_any(OpaqueXLATensor* input, Int64ArrayRef dimensions,
                               bool keep_reduced_dimensions);
OpaqueXLATensor* XLATensor_arange(XLAScalar start, XLAScalar end,
                                  XLAScalar step, const struct CDevice device,
                                  enum XLATensorScalarType scalar_type);
OpaqueXLATensor* XLATensor_argmax(OpaqueXLATensor* a, int64_t dim,
                                  bool keepdim);
OpaqueXLATensor* XLATensor_argmin(OpaqueXLATensor* a, int64_t dim,
                                  bool keepdim);
OpaqueXLATensor* XLATensor_asin(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_asinh(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_atan(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_atanh(OpaqueXLATensor* a);
OpaqueXLATensor_pair XLATensor_broadcast_tensors(OpaqueXLATensor* a,
                                                 OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_cat(OpaqueXLATensorArrayRef tensors, int64_t dim);
OpaqueXLATensor* XLATensor_ceil(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_clamp(OpaqueXLATensor* input, OpaqueXLATensor* min,
                                 OpaqueXLATensor* max);
OpaqueXLATensor* XLATensor_constant_pad_nd(OpaqueXLATensor* input,
                                           Int64ArrayRef pad, XLAScalar value);
OpaqueXLATensor* XLATensor_cos(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_cosh(OpaqueXLATensor* a);
OpaqueXLATensorArrayRef XLATensor_cross_replica_sum(
    OpaqueXLATensorArrayRef inputs, double scale);
OpaqueXLATensor* XLATensor_cumprod(OpaqueXLATensor* a, int64_t dim,
                                   Optional_XLAScalarType dtype, bool exclusive,
                                   bool reverse);
OpaqueXLATensor* XLATensor_cumsum(OpaqueXLATensor* a, int64_t dim,
                                  Optional_XLAScalarType dtype, bool exclusive,
                                  bool reverse);
OpaqueXLATensor* XLATensor_diagonal_value(OpaqueXLATensor* a, int64_t offset,
                                          int64_t dim1, int64_t dim2);
OpaqueXLATensor* XLATensor_div(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_eq(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_exp(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_expand(OpaqueXLATensor* a, Int64ArrayRef dims);
OpaqueXLATensor* XLATensor_expm1(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_flip(OpaqueXLATensor* input, Int64ArrayRef dims);
OpaqueXLATensor* XLATensor_floor(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_full(Int64ArrayRef size, XLAScalar value,
                                const struct CDevice device,
                                enum XLATensorScalarType type);
OpaqueXLATensor* XLATensor_ge(OpaqueXLATensor* x, OpaqueXLATensor* y);
OpaqueXLATensor* XLATensor_gt(OpaqueXLATensor* x, OpaqueXLATensor* y);
OpaqueXLATensor* XLATensor_index(OpaqueXLATensor* input,
                                 OpaqueXLATensorArrayRef indices,
                                 int64_t start_dim);
OpaqueXLATensor* XLATensor_is_finite(OpaqueXLATensor* input);
OpaqueXLATensor* XLATensor_is_inf(OpaqueXLATensor* input);
OpaqueXLATensor* XLATensor_is_nan(OpaqueXLATensor* input);
OpaqueXLATensor* XLATensor_le(OpaqueXLATensor* x, OpaqueXLATensor* y);
OpaqueXLATensor* XLATensor_lt(OpaqueXLATensor* x, OpaqueXLATensor* y);
OpaqueXLATensor* XLATensor_log(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_log1p(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_log_softmax(OpaqueXLATensor* a, int64_t dim);
OpaqueXLATensor* XLATensor_log_softmax_backward(OpaqueXLATensor* grad_output,
                                                OpaqueXLATensor* output,
                                                int64_t dim);
OpaqueXLATensor* XLATensor_logical_cast(OpaqueXLATensor* input,
                                        enum XLATensorScalarType dest_type);
OpaqueXLATensor* XLATensor_logicalAnd(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_logicalNot(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_logicalOr(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_matmul(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_max(OpaqueXLATensor* input, int64_t dim,
                               bool keepdim);
OpaqueXLATensor* XLATensor_maximum(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_mean(OpaqueXLATensor* a, Int64ArrayRef dims,
                                bool keep_reduced_dimensions,
                                Optional_XLAScalarType dtype);
OpaqueXLATensor* XLATensor_min(OpaqueXLATensor* input, int64_t dim,
                               bool keepdim);
OpaqueXLATensor* XLATensor_minimum(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_mul(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_mm(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_ne(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_neg(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_nll_loss(OpaqueXLATensor* input,
                                    OpaqueXLATensor* target, int ignore_index);
OpaqueXLATensor* XLATensor_permute_value(OpaqueXLATensor* a, Int64ArrayRef arr);
OpaqueXLATensor* XLATensor_physical_cast(OpaqueXLATensor* input,
                                         enum XLATensorScalarType dest_type);
OpaqueXLATensor* XLATensor_pow(OpaqueXLATensor* base,
                               OpaqueXLATensor* exponent);
OpaqueXLATensor* XLATensor_pow(OpaqueXLATensor* base,
                               OpaqueXLATensor* exponent);
OpaqueXLATensor* XLATensor_prod(OpaqueXLATensor* a, Int64ArrayRef dims,
                                bool keep_reduced_dimensions,
                                Optional_XLAScalarType dtype);
OpaqueXLATensor_pair XLATensor_qr(OpaqueXLATensor* input, bool some);
OpaqueXLATensor* XLATensor_relu(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_rem(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_repeat(OpaqueXLATensor* input,
                                  Int64ArrayRef repeats);
OpaqueXLATensor* XLATensor_resize_value(OpaqueXLATensor* a, Int64ArrayRef arr);
OpaqueXLATensor* XLATensor_round_to_even(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_rsqrt(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_select(OpaqueXLATensor* a, int64_t dim,
                                  int64_t index);
OpaqueXLATensor* XLATensor_sigmoid(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_sign(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_sin(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_sinh(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_slice(OpaqueXLATensor* a, int64_t dim, int64_t start,
                                 int64_t end, int64_t step);
OpaqueXLATensor* XLATensor_softmax(OpaqueXLATensor* a, int64_t dim);
OpaqueXLATensorArrayRef XLATensor_split_with_sizes(OpaqueXLATensor* input,
                                                   Int64ArrayRef split_size,
                                                   int64_t dim);
OpaqueXLATensor* XLATensor_sqrt(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_squeeze(OpaqueXLATensor* a, int64_t dim);
OpaqueXLATensor* XLATensor_stack(OpaqueXLATensorArrayRef tensors, int64_t dim);
OpaqueString* XLATensor_ir_text(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_sub(OpaqueXLATensor* a, OpaqueXLATensor* b);
OpaqueXLATensor* XLATensor_sum(OpaqueXLATensor* a, Int64ArrayRef dims,
                               bool keep_reduced_dimensions,
                               Optional_XLAScalarType dtype);
OpaqueXLATensor* XLATensor_tan(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_tanh(OpaqueXLATensor* a);
OpaqueXLATensor* XLATensor_tf_Conv(OpaqueXLATensor* input,
                                   OpaqueXLATensor* filter, bool depthwise,
                                   Int64ArrayRef strides,
                                   enum TFPadding padding,
                                   Int64ArrayRef explicit_paddings,
                                   enum TFDataFormat data_format,
                                   Int64ArrayRef dilations);
OpaqueXLATensor* XLATensor_tf_ConvBackpropFilter(
    OpaqueXLATensor* input, Int64ArrayRef filter_sizes,
    OpaqueXLATensor* out_backprop, bool depthwise, Int64ArrayRef strides,
    enum TFPadding padding, Int64ArrayRef explicit_paddings,
    enum TFDataFormat data_format, Int64ArrayRef dilations);
OpaqueXLATensor* XLATensor_tf_ConvBackpropInput(
    Int64ArrayRef input_sizes, OpaqueXLATensor* filter,
    OpaqueXLATensor* out_backprop, bool depthwise, Int64ArrayRef strides,
    enum TFPadding padding, Int64ArrayRef explicit_paddings,
    enum TFDataFormat data_format, Int64ArrayRef dilations);
OpaqueXLATensor* XLATensor_tf_MirrorPad(OpaqueXLATensor* input,
                                        Int64ArrayRef padding,
                                        enum TFMirrorPadMode mode);
OpaqueXLATensor* XLATensor_tf_MirrorPadGrad(OpaqueXLATensor* grad_output,
                                            Int64ArrayRef input_size,
                                            Int64ArrayRef padding,
                                            enum TFMirrorPadMode mode);
OpaqueXLATensor* XLATensor_tf_OneHot(OpaqueXLATensor* indices,
                                     OpaqueXLATensor* on_value,
                                     OpaqueXLATensor* off_value, int64_t depth,
                                     int64_t axis);
OpaqueXLATensor* XLATensor_tf_StatelessRandomNormal(
    Int64ArrayRef size, OpaqueXLATensor* seeds, const struct CDevice device,
    enum XLATensorScalarType type);
OpaqueXLATensor* XLATensor_tf_StatelessRandomUniform(
    Int64ArrayRef size, OpaqueXLATensor* seeds, OpaqueXLATensor* minvalue,
    OpaqueXLATensor* maxvalue, const struct CDevice device,
    enum XLATensorScalarType type);
OpaqueXLATensor* XLATensor_tf_UnsortedSegmentSum(OpaqueXLATensor* data,
                                                 OpaqueXLATensor* indices,
                                                 int64_t num_segments);
OpaqueXLATensor* XLATensor_threshold_backward(OpaqueXLATensor* grad_output,
                                              OpaqueXLATensor* input,
                                              float threshold);
OpaqueXLATensor* XLATensor_truncated_normal(OpaqueXLATensor* input);
OpaqueXLATensor* XLATensor_to(OpaqueXLATensor* a, const struct CDevice* device,
                              Optional_XLAScalarType dtype);
OpaqueXLATensor* XLATensor_update_slice(OpaqueXLATensor* input,
                                        OpaqueXLATensor* source,
                                        Int64ArrayRef base_indices);
OpaqueXLATensor* XLATensor_where(OpaqueXLATensor* condition,
                                 OpaqueXLATensor* input,
                                 OpaqueXLATensor* other);
OpaqueXLATensor* XLATensor_xla_pad(OpaqueXLATensor* input,
                                   XLAScalar padding_value,
                                   PaddingConfig padding_config);
OpaqueXLATensor* XLATensor_xla_slice(OpaqueXLATensor* input,
                                     Int64ArrayRef begin, Int64ArrayRef end,
                                     Int64ArrayRef strides);
// Retrieves the device for a given tensor.
struct CDevice XLATensor_device(OpaqueXLATensor* t);
// Creates a float tensor on the current device filled with random numbers in
// the [0, 1) interval.
OpaqueXLATensor* XLATensor_rand(Int64ArrayRef size, int64_t seed);
// Sets whether to use full matrix multiplication precision mode in the TPU
// backend. Only used for testing, it has a substantial performance cost.
void SetMatMulPrecision(bool use_full_precision);

StridedSliceSpec* ComputeIndexingBoundsAndStrides(
    Int64ArrayRef input_sizes, Int64ArrayRef begin, Int64ArrayRef end,
    Int64ArrayRef strides, int32_t begin_mask, int32_t end_mask,
    int32_t ellipsis_mask, int32_t new_axis_mask, int32_t shrink_axis_mask);

void PrintMetrics();

// Randomly shuffles the array defined by (data, size) by seed and then
// returns the result.
void SeededRandomShuffle(size_t* data, size_t size, int64_t seed);

// Safe string handling helpers.
void DeleteString(OpaqueString* str);
const char* GetStringCStr(OpaqueString* str);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // X10_XLA_TENSOR_WRAPPER_H_
