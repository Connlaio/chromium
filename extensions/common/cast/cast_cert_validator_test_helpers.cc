// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cast/cast_cert_validator_test_helpers.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "extensions/common/extension_paths.h"
#include "net/cert/pem_tokenizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_test_helpers {

namespace {

// Reads a file from the extensions test data directory
// (//src/extensions/test/data/)
std::string ReadTestFileToString(const base::StringPiece& file_name) {
  base::FilePath filepath;
  if (!PathService::Get(extensions::DIR_TEST_DATA, &filepath)) {
    ADD_FAILURE() << "Couldn't retrieve test data root";
    return std::string();
  }
  filepath = filepath.AppendASCII(file_name);

  // Read the full contents of the file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    ADD_FAILURE() << "Couldn't read file: " << filepath.value();
    return std::string();
  }

  return file_data;
}

}  // namespace

std::vector<std::string> ReadCertificateChainFromFile(
    const base::StringPiece& file_name) {
  std::string file_data = ReadTestFileToString(file_name);

  std::vector<std::string> pem_headers;
  pem_headers.push_back("CERTIFICATE");

  std::vector<std::string> certs;
  net::PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext())
    certs.push_back(pem_tokenizer.data());

  EXPECT_FALSE(certs.empty());
  return certs;
}

SignatureTestData ReadSignatureTestData(const base::StringPiece& file_name) {
  SignatureTestData result;

  std::string file_data = ReadTestFileToString(file_name);
  EXPECT_FALSE(file_data.empty());

  std::vector<std::string> pem_headers;
  pem_headers.push_back("MESSAGE");
  pem_headers.push_back("SIGNATURE SHA1");
  pem_headers.push_back("SIGNATURE SHA256");

  net::PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    const std::string& type = pem_tokenizer.block_type();
    const std::string& value = pem_tokenizer.data();

    if (type == "MESSAGE") {
      result.message = value;
    } else if (type == "SIGNATURE SHA1") {
      result.signature_sha1 = value;
    } else if (type == "SIGNATURE SHA256") {
      result.signature_sha256 = value;
    }
  }

  EXPECT_FALSE(result.message.empty());
  EXPECT_FALSE(result.signature_sha1.empty());
  EXPECT_FALSE(result.signature_sha256.empty());

  return result;
}

}  // namespace cast_test_helpers
