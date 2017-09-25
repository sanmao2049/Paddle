/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include "paddle/operators/reduce_op.h"

namespace paddle {
namespace operators {

using framework::Tensor;

class ReduceOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(const framework::InferShapeContext &ctx) const override {
    PADDLE_ENFORCE_NOT_NULL(ctx.InputVar("X"),
                            "Input(X) of ReduceOp should not be null.");
    PADDLE_ENFORCE_NOT_NULL(ctx.OutputVar("Out"),
                            "Output(Out) of ReduceOp should not be null.");
    auto x_dims = ctx.Input<Tensor>("X")->dims();
    auto x_rank = x_dims.size();
    PADDLE_ENFORCE_LE(x_rank, 6, "Tensors with rank at most 6 are supported.");
    int dim = ctx.Attr<int>("dim");
    if (dim < 0) dim = x_rank + dim;
    PADDLE_ENFORCE_LT(
        dim, x_rank,
        "The dim should be in the range [-rank(input), rank(input)).");
    bool keep_dim = ctx.Attr<bool>("keep_dim");
    auto dims_vector = vectorize(x_dims);
    if (keep_dim || x_rank == 1) {
      dims_vector[dim] = 1;
    } else {
      dims_vector.erase(dims_vector.begin() + dim);
    }
    auto out_dims = framework::make_ddim(dims_vector);
    ctx.Output<framework::Tensor>("Out")->Resize(out_dims);
    if (dim != 0) {
      // Only pass LoD when not reducing on the first dim
      ctx.ShareLoD("X", /*->*/ "Out");
    }
  }
};

class ReduceGradOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(const framework::InferShapeContext &ctx) const override {
    PADDLE_ENFORCE_NOT_NULL(ctx.InputVar("X"), "Input(X) should not be null.");
    PADDLE_ENFORCE_NOT_NULL(ctx.InputVar(framework::GradVarName("Out")),
                            "Input(Out@GRAD) should not be null.");
    auto x_dims = ctx.Input<Tensor>("X")->dims();
    auto x_rank = x_dims.size();
    PADDLE_ENFORCE_LE(x_rank, 6, "Tensors with rank at most 6 are supported.");
    int dim = ctx.Attr<int>("dim");
    if (dim < 0) dim = x_rank + dim;
    PADDLE_ENFORCE_LT(
        dim, x_rank,
        "The dim should be in the range [-rank(input), rank(input)).");
    auto *x_grad =
        ctx.Output<framework::LoDTensor>(framework::GradVarName("X"));
    if (x_grad) x_grad->Resize(x_dims);
  }
};

class ReduceOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  ReduceOpMaker(framework::OpProto *proto, framework::OpAttrChecker *op_checker)
      : OpProtoAndCheckerMaker(proto, op_checker) {
    AddInput(
        "X",
        "(Tensor) The input tensor. Tensors with rank at most 6 are supported");
    AddOutput("Out", "(Tensor) The result tensor.");
    AddAttr<int>(
        "dim",
        "(int, default 1) The dimension to reduce. "
        "Must be in the range [-rank(input), rank(input)). "
        "If `dim < 0`, the dim to reduce is `rank + dim`. "
        "Noting that reducing on the first dim will make the LoD info lost.")
        .SetDefault(0);
    AddAttr<bool>("keep_dim",
                  "(bool, default false) "
                  "If true, retain the reduced dimension with length 1.")
        .SetDefault(false);
    comment_ = R"DOC(
{ReduceOP} operator computes the {reduce} of input tensor along the given dimension. 
The result tensor has 1 fewer dimension than the input unless `keep_dim` is true.
)DOC";
    AddComment(comment_);
  }

 protected:
  std::string comment_;

  void Replace(std::string &src, std::string from, std::string to) {
    std::size_t len_from = std::strlen(from.c_str());
    std::size_t len_to = std::strlen(to.c_str());
    for (std::size_t pos = src.find(from); pos != std::string::npos;
         pos = src.find(from, pos + len_to)) {
      src.replace(pos, len_from, to);
    }
  }

  void SetComment(std::string name, std::string op) {
    Replace(comment_, "{ReduceOP}", name);
    Replace(comment_, "{reduce}", op);
  }
};

class ReduceSumOpMaker : public ReduceOpMaker {
 public:
  ReduceSumOpMaker(framework::OpProto *proto,
                   framework::OpAttrChecker *op_checker)
      : ReduceOpMaker(proto, op_checker) {
    SetComment("ReduceSum", "sum");
    AddComment(comment_);
  }
};

class ReduceMeanOpMaker : public ReduceOpMaker {
 public:
  ReduceMeanOpMaker(framework::OpProto *proto,
                    framework::OpAttrChecker *op_checker)
      : ReduceOpMaker(proto, op_checker) {
    SetComment("ReduceMean", "mean");
    AddComment(comment_);
  }
};

class ReduceMaxOpMaker : public ReduceOpMaker {
 public:
  ReduceMaxOpMaker(framework::OpProto *proto,
                   framework::OpAttrChecker *op_checker)
      : ReduceOpMaker(proto, op_checker) {
    SetComment("ReduceMax", "max");
    AddComment(comment_);
  }
};

class ReduceMinOpMaker : public ReduceOpMaker {
 public:
  ReduceMinOpMaker(framework::OpProto *proto,
                   framework::OpAttrChecker *op_checker)
      : ReduceOpMaker(proto, op_checker) {
    SetComment("ReduceMin", "min");
    AddComment(comment_);
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;

REGISTER_OP(reduce_sum, ops::ReduceOp, ops::ReduceSumOpMaker, reduce_sum_grad,
            ops::ReduceGradOp);
REGISTER_OP_CPU_KERNEL(
    reduce_sum,
    ops::ReduceKernel<paddle::platform::CPUPlace, float, ops::SumFunctor>);
REGISTER_OP_CPU_KERNEL(reduce_sum_grad,
                       ops::ReduceGradKernel<paddle::platform::CPUPlace, float,
                                             ops::SumGradFunctor>);

REGISTER_OP(reduce_mean, ops::ReduceOp, ops::ReduceMeanOpMaker,
            reduce_mean_grad, ops::ReduceGradOp);
REGISTER_OP_CPU_KERNEL(
    reduce_mean,
    ops::ReduceKernel<paddle::platform::CPUPlace, float, ops::MeanFunctor>);
REGISTER_OP_CPU_KERNEL(reduce_mean_grad,
                       ops::ReduceGradKernel<paddle::platform::CPUPlace, float,
                                             ops::MeanGradFunctor>);

REGISTER_OP(reduce_max, ops::ReduceOp, ops::ReduceMaxOpMaker, reduce_max_grad,
            ops::ReduceGradOp);
REGISTER_OP_CPU_KERNEL(
    reduce_max,
    ops::ReduceKernel<paddle::platform::CPUPlace, float, ops::MaxFunctor>);
REGISTER_OP_CPU_KERNEL(reduce_max_grad,
                       ops::ReduceGradKernel<paddle::platform::CPUPlace, float,
                                             ops::MaxOrMinGradFunctor>);

REGISTER_OP(reduce_min, ops::ReduceOp, ops::ReduceMaxOpMaker, reduce_min_grad,
            ops::ReduceGradOp);
REGISTER_OP_CPU_KERNEL(
    reduce_min,
    ops::ReduceKernel<paddle::platform::CPUPlace, float, ops::MinFunctor>);
REGISTER_OP_CPU_KERNEL(reduce_min_grad,
                       ops::ReduceGradKernel<paddle::platform::CPUPlace, float,
                                             ops::MaxOrMinGradFunctor>);
