// Copyright 2020 TensorFlow Authors
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

#include "tensorflow/compiler/tf2xla/xla_tensor/ops/convolution_overrideable.h"

#include "absl/strings/str_join.h"
#include "tensorflow/compiler/xla/xla_client/debug_macros.h"
#include "tensorflow/compiler/xla/xla_client/util.h"
#include "tensorflow/compiler/tf2xla/xla_tensor/convolution.h"
#include "tensorflow/compiler/tf2xla/xla_tensor/lowering_context.h"
#include "tensorflow/compiler/tf2xla/xla_tensor/ops/infer_output_shape.h"

namespace swift_xla {
namespace ir {
namespace ops {
namespace {

// The bias doesn't matter for shape inference.
xla::Shape NodeOutputShape(const Value& input, const Value& weight,
                           absl::Span<const xla::int64> stride,
                           absl::Span<const xla::int64> padding,
                           absl::Span<const xla::int64> dilation,
                           bool transposed,
                           absl::Span<const xla::int64> output_padding,
                           xla::int64 groups) {
  auto lower_for_shape_fn =
      [stride, padding, dilation, output_padding, transposed,
       groups](absl::Span<const xla::XlaOp> operands) -> xla::XlaOp {
    XLA_CHECK(operands.size() == 2 || operands.size() == 3);
    return BuildConvolutionOverrideable(operands[0], operands[1], stride,
                                        padding, dilation, transposed,
                                        output_padding, groups);
  };
  return InferOutputShape({input.shape(), weight.shape()}, lower_for_shape_fn);
}

}  // namespace

ConvolutionOverrideable::ConvolutionOverrideable(
    const Value& input, const Value& weight, const Value& bias,
    std::vector<xla::int64> stride, std::vector<xla::int64> padding,
    std::vector<xla::int64> dilation, bool transposed,
    std::vector<xla::int64> output_padding, xla::int64 groups)
    : Node(ir::OpKind(at::aten::convolution_overrideable),
           {input, weight, bias},
           [&]() {
             return NodeOutputShape(input, weight, stride, padding, dilation,
                                    transposed, output_padding, groups);
           },
           /*num_outputs=*/1,
           xla::util::MHash(stride, padding, dilation, transposed,
                            output_padding, groups)),
      stride_(std::move(stride)),
      padding_(std::move(padding)),
      dilation_(std::move(dilation)),
      output_padding_(std::move(output_padding)),
      transposed_(transposed),
      groups_(groups) {}

ConvolutionOverrideable::ConvolutionOverrideable(
    const Value& input, const Value& weight, std::vector<xla::int64> stride,
    std::vector<xla::int64> padding, std::vector<xla::int64> dilation,
    bool transposed, std::vector<xla::int64> output_padding, xla::int64 groups)
    : Node(ir::OpKind(at::aten::convolution_overrideable), {input, weight},
           [&]() {
             return NodeOutputShape(input, weight, stride, padding, dilation,
                                    transposed, output_padding, groups);
           },
           /*num_outputs=*/1,
           xla::util::MHash(stride, padding, dilation, transposed,
                            output_padding, groups)),
      stride_(std::move(stride)),
      padding_(std::move(padding)),
      dilation_(std::move(dilation)),
      output_padding_(std::move(output_padding)),
      transposed_(transposed),
      groups_(groups) {}

NodePtr ConvolutionOverrideable::Clone(OpList operands) const {
  return operands.size() == 3
             ? MakeNode<ConvolutionOverrideable>(
                   operands.at(0), operands.at(1), operands.at(2), stride_,
                   padding_, dilation_, transposed_, output_padding_, groups_)
             : MakeNode<ConvolutionOverrideable>(
                   operands.at(0), operands.at(1), stride_, padding_, dilation_,
                   transposed_, output_padding_, groups_);
}

XlaOpVector ConvolutionOverrideable::Lower(LoweringContext* loctx) const {
  xla::XlaOp input = loctx->GetOutputOp(operand(0));
  xla::XlaOp kernel = loctx->GetOutputOp(operand(1));
  xla::XlaOp output;
  if (operands().size() == 3) {
    xla::XlaOp bias = loctx->GetOutputOp(operand(2));
    output = BuildConvolutionOverrideableBias(input, kernel, bias, stride_,
                                              padding_, dilation_, transposed_,
                                              output_padding_, groups_);
  } else {
    XLA_CHECK_EQ(operands().size(), 2);
    output = BuildConvolutionOverrideable(input, kernel, stride_, padding_,
                                          dilation_, transposed_,
                                          output_padding_, groups_);
  }
  return ReturnOp(output, loctx);
}

std::string ConvolutionOverrideable::ToString() const {
  std::stringstream ss;
  ss << Node::ToString() << ", stride=[" << absl::StrJoin(stride_, ", ")
     << "], padding=[" << absl::StrJoin(padding_, ", ") << "], dilation=["
     << absl::StrJoin(dilation_, ", ") << "], transpose=" << transposed_
     << ", output_padding=[" << absl::StrJoin(output_padding_, ", ")
     << "], groups=" << groups_;
  return ss.str();
}

}  // namespace ops
}  // namespace ir
}  // namespace swift_xla
