// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serialization warnings are only recorded in debug build.
#ifndef NDEBUG

#include <stddef.h>
#include <utility>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/serialization_test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::ArrayValidateParams;

// Creates an array of arrays of handles (2 X 3) for testing.
Array<Array<ScopedHandle>> CreateTestNestedHandleArray() {
  Array<Array<ScopedHandle>> array(2);
  for (size_t i = 0; i < array.size(); ++i) {
    Array<ScopedHandle> nested_array(3);
    for (size_t j = 0; j < nested_array.size(); ++j) {
      MessagePipe pipe;
      nested_array[j] = ScopedHandle::From(std::move(pipe.handle1));
    }
    array[i] = std::move(nested_array);
  }

  return array;
}

class SerializationWarningTest : public testing::Test {
 public:
  ~SerializationWarningTest() override {}

 protected:
  template <typename T>
  void TestWarning(StructPtr<T> obj,
                   mojo::internal::ValidationError expected_warning) {
    TestStructWarningImpl<T>(std::move(obj), expected_warning);
  }

  template <typename T>
  void TestWarning(InlinedStructPtr<T> obj,
                   mojo::internal::ValidationError expected_warning) {
    TestStructWarningImpl<T>(std::move(obj), expected_warning);
  }

  template <typename T, typename TPtr>
  void TestStructWarningImpl(TPtr obj,
                             mojo::internal::ValidationError expected_warning) {
    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::internal::SerializationContext context;
    mojo::internal::FixedBufferForTesting buf(
        GetSerializedSize_(obj, &context));
    typename T::Data_* data;
    Serialize_(std::move(obj), &buf, &data, &context);

    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  template <typename T>
  void TestArrayWarning(T obj,
                        mojo::internal::ValidationError expected_warning,
                        const ArrayValidateParams* validate_params) {
    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::internal::SerializationContext context;
    mojo::internal::FixedBufferForTesting buf(
        GetSerializedSize_(obj, &context));
    typename T::Data_* data;
    SerializeArray_(std::move(obj), &buf, &data, validate_params, &context);

    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  mojo::internal::SerializationWarningObserverForTesting warning_observer_;
};

TEST_F(SerializationWarningTest, HandleInStruct) {
  Struct2Ptr test_struct(Struct2::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);

  test_struct = Struct2::New();
  MessagePipe pipe;
  test_struct->hdl = ScopedHandle::From(std::move(pipe.handle1));

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, StructInStruct) {
  Struct3Ptr test_struct(Struct3::New());
  EXPECT_TRUE(!test_struct->struct_1);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct3::New();
  test_struct->struct_1 = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfStructsInStruct) {
  Struct4Ptr test_struct(Struct4::New());
  EXPECT_TRUE(!test_struct->data);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->data.resize(1);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->data.resize(0);

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);

  test_struct = Struct4::New();
  test_struct->data.resize(1);
  test_struct->data[0] = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, FixedArrayOfStructsInStruct) {
  Struct5Ptr test_struct(Struct5::New());
  EXPECT_TRUE(!test_struct->pair);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct5::New();
  test_struct->pair.resize(1);
  test_struct->pair[0] = Struct1::New();

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);

  test_struct = Struct5::New();
  test_struct->pair.resize(2);
  test_struct->pair[0] = Struct1::New();
  test_struct->pair[1] = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, StringInStruct) {
  Struct6Ptr test_struct(Struct6::New());
  EXPECT_TRUE(!test_struct->str);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct6::New();
  test_struct->str = "hello world";

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfArraysOfHandles) {
  Array<Array<ScopedHandle>> test_array = CreateTestNestedHandleArray();
  test_array[0] = nullptr;
  test_array[1][0] = ScopedHandle();

  ArrayValidateParams validate_params_0(
      0, true, new ArrayValidateParams(0, true, nullptr));
  TestArrayWarning(std::move(test_array), mojo::internal::VALIDATION_ERROR_NONE,
                   &validate_params_0);

  test_array = CreateTestNestedHandleArray();
  test_array[0] = nullptr;
  ArrayValidateParams validate_params_1(
      0, false, new ArrayValidateParams(0, true, nullptr));
  TestArrayWarning(std::move(test_array),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                   &validate_params_1);

  test_array = CreateTestNestedHandleArray();
  test_array[1][0] = ScopedHandle();
  ArrayValidateParams validate_params_2(
      0, true, new ArrayValidateParams(0, false, nullptr));
  TestArrayWarning(std::move(test_array),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                   &validate_params_2);
}

TEST_F(SerializationWarningTest, ArrayOfStrings) {
  Array<String> test_array(3);
  for (size_t i = 0; i < test_array.size(); ++i)
    test_array[i] = "hello";

  ArrayValidateParams validate_params_0(
      0, true, new ArrayValidateParams(0, false, nullptr));
  TestArrayWarning(std::move(test_array), mojo::internal::VALIDATION_ERROR_NONE,
                   &validate_params_0);

  test_array = Array<String>(3);
  for (size_t i = 0; i < test_array.size(); ++i)
    test_array[i] = nullptr;
  ArrayValidateParams validate_params_1(
      0, false, new ArrayValidateParams(0, false, nullptr));
  TestArrayWarning(std::move(test_array),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                   &validate_params_1);

  test_array = Array<String>(2);
  ArrayValidateParams validate_params_2(
      3, true, new ArrayValidateParams(0, false, nullptr));
  TestArrayWarning(std::move(test_array),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
                   &validate_params_2);
}

}  // namespace
}  // namespace test
}  // namespace mojo

#endif
