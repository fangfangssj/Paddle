//  Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>

#include "paddle/fluid/framework/selected_rows_utils.h"
#include "paddle/fluid/framework/var_type_traits.h"
namespace paddle {
namespace framework {

class Variable {
 public:
  template <typename T>
  const T& Get() const {
    static_assert(
        IsRegisteredVarType<T>(),
        "Not registered type. Please register T inside var_type_traits.h");
    PADDLE_ENFORCE_NOT_NULL(
        holder_, common::errors::NotFound("Variable is not initialized."));
    PADDLE_ENFORCE_EQ(
        holder_->Type(),
        VarTypeTrait<T>::kId,
        common::errors::InvalidArgument(
            "The Variable type must be %s, but the type it holds is %s.",
            ToTypeName(VarTypeTrait<T>::kId),
            ToTypeName(holder_->Type())));
    return *static_cast<const T*>(holder_->Ptr());
  }

  bool IsInitialized() const { return holder_ != nullptr; }

  template <typename T>
  T* GetMutable() {
    if (!holder_) {
      holder_.reset(new PlaceholderImpl<T>());
    } else {
      // If holder_ is RawTensor, call holder_->Ptr() GetMutable again. Used for
      // load_combine.
      if (holder_->Type() == VarTypeTrait<RawTensor>::kId &&
          holder_->Type() != VarTypeTrait<T>::kId) {
        return static_cast<RawTensor*>(holder_->Ptr())->GetMutable<T>();
      }
      PADDLE_ENFORCE_EQ(
          holder_->Type(),
          VarTypeTrait<T>::kId,
          common::errors::InvalidArgument(
              "The Variable type must be %s, but the type it holds is %s.",
              ToTypeName(VarTypeTrait<T>::kId),
              ToTypeName(holder_->Type())));
    }
    return static_cast<T*>(holder_->Ptr());
  }

  template <typename T>
  bool IsType() const {
    return holder_ && holder_->Type() == VarTypeTrait<T>::kId;
  }

  void Clear() { holder_.reset(); }

  int Type() const {
    PADDLE_ENFORCE_NOT_NULL(
        holder_, common::errors::NotFound("Variable is not initialized."));
    return holder_->Type();
  }

 private:
  // This method hides type T, so it doesn't appear as a template parameter of
  // Variable.
  phi::DenseTensor::InplaceVersion* InplaceVersionCounter();

 public:
  void SetInplaceVersionToZero();
  uint32_t CurrentInplaceVersion();
  void BumpInplaceVersion();

 private:
  struct Placeholder {
    virtual ~Placeholder() PADDLE_MAY_THROW {}

    inline int Type() const { return type_; }
    inline const void* Ptr() const { return ptr_; }
    inline void* Ptr() { return ptr_; }

   protected:
    inline void Init(void* p, int type) {
      ptr_ = p;
      type_ = type;
    }

    void* ptr_;
    int type_;
  };

  // Placeholder hides type T, so it doesn't appear as a template
  // parameter of Variable.
  template <typename T>
  struct PlaceholderImpl : public Placeholder {
    static_assert(
        IsRegisteredVarType<T>(),
        "Not registered type. Please register T inside var_type_traits.h");
    PlaceholderImpl() { this->Init(&obj_, VarTypeTrait<T>::kId); }

   private:
    T obj_;
  };

  // pointers to a PlaceholderImpl object indeed.
  std::shared_ptr<Placeholder> holder_;
};

inline phi::DenseTensor::InplaceVersion* Variable::InplaceVersionCounter() {
  phi::DenseTensor::InplaceVersion* version_counter_ptr(nullptr);
  if (IsType<phi::DenseTensor>()) {
    version_counter_ptr =
        &GetMutable<phi::DenseTensor>()->InplaceVersionCounter();
  } else if (IsType<phi::DenseTensor>()) {
    version_counter_ptr =
        &GetMutable<phi::DenseTensor>()->InplaceVersionCounter();

  } else if (IsType<phi::SelectedRows>()) {
    version_counter_ptr = &GetMutable<phi::SelectedRows>()
                               ->mutable_value()
                               ->InplaceVersionCounter();
  } else {
    VLOG(4) << "Only supports phi::DenseTensor, phi::DenseTensor, SelectedRows "
               "to have "
               "TensorInplaceVersion, but received type "
            << common::demangle(framework::ToTypeName(Type()));
  }
  return version_counter_ptr;
}

inline void Variable::SetInplaceVersionToZero() {
  auto inplace_version_counter = this->InplaceVersionCounter();
  if (inplace_version_counter)
    inplace_version_counter->SetInplaceVersionToZero();
}

inline uint32_t Variable::CurrentInplaceVersion() {
  auto version_counter_ptr = InplaceVersionCounter();
  if (version_counter_ptr) {
    return version_counter_ptr->CurrentVersion();
  } else {
    return 0;
  }
}

inline void Variable::BumpInplaceVersion() {
  auto version_counter_ptr = InplaceVersionCounter();
  if (version_counter_ptr) {
    return version_counter_ptr->Bump();
  } else {
    VLOG(4) << "Only supports phi::DenseTensor, phi::DenseTensor, SelectedRows "
               "to have "
               "TensorInplaceVersion, but received type "
            << common::demangle(framework::ToTypeName(Type()));
  }
}
}  // namespace framework
}  // namespace paddle
