// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlobj.h>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// These tests exercise the Delete-After-Reboot code which requires
// modifications to HKLM. This will fail on Vista and above if the user
// is not an admin or if UAC is on.
// I tried using RegOverridePredefKey to test, but MoveFileEx ignore this
// even on 32 bit machines :-( As such, running this test may pollute
// your PendingFileRenameOperations value.
class DeleteAfterRebootHelperTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary directory for testing and fill it with some files.
    std::wstring no_prefix;
    file_util::CreateNewTempDirectory(no_prefix, &temp_dir_);
    file_util::CreateTemporaryFileInDir(temp_dir_, &temp_file_);

    temp_subdir_ = temp_dir_.Append(L"subdir");
    file_util::CreateDirectory(temp_subdir_);
    file_util::CreateTemporaryFileInDir(temp_subdir_, &temp_subdir_file_);

    // Copy the current pending moves and then clear it if we can:
    if (IsUserAnAdmin()) {
      GetPendingMovesValue(&original_pending_moves_);
    }
  }
  virtual void TearDown() {
    // Delete the temporary directory if it's still there.
    file_util::Delete(temp_dir_, true);

    // Try and restore the pending moves value, if we have one.
    if (IsUserAnAdmin() && original_pending_moves_.size() > 1) {
      base::win::RegKey session_manager_key(
          HKEY_LOCAL_MACHINE, kSessionManagerKey,
          KEY_CREATE_SUB_KEY | KEY_SET_VALUE);
      if (!session_manager_key.Handle()) {
        // Couldn't open / create the key.
        DLOG(ERROR) << "Failed to open session manager key for writing.";
      }

      std::vector<char> buffer;
      StringArrayToMultiSZBytes(original_pending_moves_, &buffer);
      session_manager_key.WriteValue(kPendingFileRenameOps, &buffer[0],
                                     buffer.size(), REG_MULTI_SZ);
    }
  }

  // Compares two buffers of size len. Returns true if they are equal,
  // false otherwise. Standard warnings about making sure the buffers
  // are at least len chars long apply.
  template<class Type>
  bool CompareBuffers(Type* buf1, Type* buf2, int len) {
    Type* comp1 = buf1;
    Type* comp2 = buf2;
    for (int i = 0; i < len; i++) {
      if (*comp1 != *comp2)
        return false;
      comp1++;
      comp2++;
    }
    return true;
  }

  // Returns the size of the given list of wstrings in bytes, including
  // null chars, plus an additional terminating null char.
  // e.g. the length of all the strings * sizeof(wchar_t).
  virtual size_t WStringPairListSize(
      const std::vector<PendingMove>& string_list) {
    size_t length = 0;
    std::vector<PendingMove>::const_iterator iter(string_list.begin());
    for (; iter != string_list.end(); ++iter) {
      length += iter->first.size() + 1;  // +1 for the null char.
      length += iter->second.size() + 1;  // +1 for the null char.
    }
    length++;  // for the additional null char.
    return length * sizeof(wchar_t);
  }

  std::vector<PendingMove> original_pending_moves_;

  FilePath temp_dir_;
  FilePath temp_file_;
  FilePath temp_subdir_;
  FilePath temp_subdir_file_;
};
}

TEST_F(DeleteAfterRebootHelperTest, TestStringListToMultiSZConversions) {
  struct StringTest {
    wchar_t* test_name;
    wchar_t* str;
    DWORD length;
    size_t count;
  } tests[] = {
    { L"basic", L"foo\0bar\0fee\0bee\0boo\0bong\0\0", 26 * sizeof(wchar_t), 3 },
    { L"empty", L"\0\0", 2 * sizeof(wchar_t), 1 },
    { L"deletes", L"foo\0\0bar\0\0bizz\0\0", 16 * sizeof(wchar_t), 3 },
  };

  for (int i = 0; i < arraysize(tests); i++) {
    std::vector<PendingMove> string_list;
    EXPECT_TRUE(SUCCEEDED(
        MultiSZBytesToStringArray(reinterpret_cast<char*>(tests[i].str),
                                  tests[i].length, &string_list)))
        << tests[i].test_name;
    EXPECT_EQ(tests[i].count, string_list.size()) << tests[i].test_name;
    std::vector<char> buffer;
    buffer.resize(WStringPairListSize(string_list));
    StringArrayToMultiSZBytes(string_list, &buffer);
    EXPECT_TRUE(CompareBuffers(&buffer[0],
                reinterpret_cast<char*>(tests[i].str),
                tests[i].length)) << tests[i].test_name;
  }

  StringTest failures[] =
    { L"malformed", reinterpret_cast<wchar_t*>("oddnumb\0\0"), 9, 1 };

  for (int i = 0; i < arraysize(failures); i++) {
    std::vector<PendingMove> string_list;
    EXPECT_FALSE(SUCCEEDED(
        MultiSZBytesToStringArray(reinterpret_cast<char*>(failures[i].str),
                                  failures[i].length, &string_list)))
        << failures[i].test_name;
  }
}


TEST_F(DeleteAfterRebootHelperTest, TestFileDeleteScheduleAndUnschedule) {
  if (!IsUserAnAdmin()) {
    return;
  }

  EXPECT_TRUE(ScheduleDirectoryForDeletion(temp_dir_.value().c_str()));

  std::vector<PendingMove> pending_moves;
  EXPECT_TRUE(SUCCEEDED(GetPendingMovesValue(&pending_moves)));

  // We should see, somewhere in this key, deletion writs for
  // temp_file_, temp_subdir_file_, temp_subdir_ and temp_dir_ in that order.
  EXPECT_TRUE(pending_moves.size() > 3);

  // Get the short form of temp_file_ and use that to match.
  std::wstring short_temp_file(GetShortPathName(temp_file_.value().c_str()));

  // Scan for the first expected delete.
  std::vector<PendingMove>::const_iterator iter(pending_moves.begin());
  for (; iter != pending_moves.end(); iter++) {
    if (MatchPendingDeletePath(short_temp_file, iter->first))
      break;
  }

  // Check that each of the deletes we expect are there in order.
  FilePath expected_paths[] =
      { temp_file_, temp_subdir_file_, temp_subdir_, temp_dir_ };
  for (int i = 0; i < arraysize(expected_paths); ++i) {
    EXPECT_FALSE(iter == pending_moves.end());
    if (iter != pending_moves.end()) {
      std::wstring short_path_name(
          GetShortPathName(expected_paths[i].value().c_str()));
      EXPECT_TRUE(MatchPendingDeletePath(short_path_name, iter->first));
      ++iter;
    }
  }

  // Test that we can remove the pending deletes.
  EXPECT_TRUE(RemoveFromMovesPendingReboot(temp_dir_.value().c_str()));
  HRESULT hr = GetPendingMovesValue(&pending_moves);
  EXPECT_TRUE(hr == S_OK || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  std::vector<PendingMove>::const_iterator check_iter(pending_moves.begin());
  for (; check_iter != pending_moves.end(); ++check_iter) {
    EXPECT_FALSE(MatchPendingDeletePath(short_temp_file, check_iter->first));
  }
}

TEST_F(DeleteAfterRebootHelperTest, TestFileDeleteSchedulingWithActualDeletes) {
  if (!IsUserAnAdmin()) {
    return;
  }

  std::vector<PendingMove> initial_pending_moves;
  GetPendingMovesValue(&initial_pending_moves);
  size_t initial_pending_moves_size = initial_pending_moves.size();

  EXPECT_TRUE(ScheduleDirectoryForDeletion(temp_dir_.value().c_str()));

  std::vector<PendingMove> pending_moves;
  EXPECT_TRUE(SUCCEEDED(GetPendingMovesValue(&pending_moves)));

  // We should see, somewhere in this key, deletion writs for
  // temp_file_, temp_subdir_file_, temp_subdir_ and temp_dir_ in that order.
  EXPECT_TRUE(pending_moves.size() > 3);

  // Get the short form of temp_file_ and use that to match.
  std::wstring short_temp_file(GetShortPathName(temp_file_.value().c_str()));

  // Scan for the first expected delete.
  std::vector<PendingMove>::const_iterator iter(pending_moves.begin());
  for (; iter != pending_moves.end(); iter++) {
    if (MatchPendingDeletePath(short_temp_file, iter->first))
      break;
  }

  // Check that each of the deletes we expect are there in order.
  FilePath expected_paths[] =
      { temp_file_, temp_subdir_file_, temp_subdir_, temp_dir_ };
  for (int i = 0; i < arraysize(expected_paths); ++i) {
    EXPECT_FALSE(iter == pending_moves.end());
    if (iter != pending_moves.end()) {
      std::wstring short_path_name(
          GetShortPathName(expected_paths[i].value().c_str()));
      EXPECT_TRUE(MatchPendingDeletePath(short_path_name, iter->first));
      ++iter;
    }
  }

  // Delete the temporary directory.
  file_util::Delete(temp_dir_, true);

  // Test that we can remove the pending deletes.
  EXPECT_TRUE(RemoveFromMovesPendingReboot(temp_dir_.value().c_str()));
  HRESULT hr = GetPendingMovesValue(&pending_moves);
  EXPECT_TRUE(hr == S_OK || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  EXPECT_EQ(initial_pending_moves_size, pending_moves.size());

  std::vector<PendingMove>::const_iterator check_iter(pending_moves.begin());
  for (; check_iter != pending_moves.end(); ++check_iter) {
    EXPECT_FALSE(MatchPendingDeletePath(short_temp_file, check_iter->first));
  }
}

