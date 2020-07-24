/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#include "idl/processor.h"
#include "idl/backend.h"

#include "CUnit/Theory.h"

#define IDL_INPUT(struct_name,member_type,member_name) ""\
"struct " struct_name " {\n"\
"    " member_type " " member_name ";"\
"};"

#define IDL_OUTPUT(struct_name,member_type,default_value,member_name) ""\
"class " struct_name " {\n"\
"private:\n"\
"  " member_type " " member_name "_;\n"\
"\n"\
"public:\n"\
"  " struct_name "() :\n"\
"      " member_name "_(" default_value ") {}\n"\
"\n"\
"  explicit " struct_name "(\n"\
"      " member_type " " member_name ") :\n"\
"          " member_name "_(" member_name ") {}\n"\
"\n"\
"  " member_type " " member_name "() const { return this->" member_name "_; }\n"\
"  " member_type "& " member_name "() { return this->" member_name "_; }\n"\
"  void " member_name "(" member_type " _val_) { this->" member_name "_ = _val_; }\n"\
"};\n"

static char mem_buf[4096];
static bool initial_run = true;

static void
test_base_type(const char *input, uint32_t flags, int32_t retcode, const char *output)
{
  int32_t ret;
  idl_tree_t *tree;
  idl_node_t *node;
  idl_backend_ctx ctx;
  bool expected_output;

  ret = idl_parse_string(input, flags, &tree);
  CU_ASSERT(ret == retcode);
  if (ret != 0)
    return;
  node = tree->root;
  CU_ASSERT_PTR_NOT_NULL(node);
  if (!node)
    return;
  ctx = idl_backend_context_new(2, NULL);
  CU_ASSERT_PTR_NOT_NULL(ctx);
  ret = idl_file_out_new_membuf(ctx, mem_buf, sizeof(mem_buf));
  CU_ASSERT(ret == IDL_RETCODE_OK);
  ret = idl_backendGenerate(ctx, tree);
  CU_ASSERT(ret == IDL_RETCODE_OK);
  expected_output = (strncmp(mem_buf,output,strlen(output)) == 0);
  if (!expected_output) {
    printf("%s\n", mem_buf);
    printf("%s\n", output);
    for (size_t i = 0; i < strlen(output); ++i) {
      if (mem_buf[i] != output[i]) {
        printf("Diff starts here.....\n%s\n", &output[i]);
        break;
      }
    }
  }
  CU_ASSERT(expected_output);
  idl_file_out_close(ctx);
}

CU_TheoryDataPoints(idl_backend, cpp11) =
{
  /* Series of IDL input */
  CU_DataPoints(const char *, IDL_INPUT("Position","short","x1"),
                              IDL_INPUT("Position","long","x2"),
                              IDL_INPUT("Position","long long","x3"),
                              IDL_INPUT("Position","unsigned short","x4"),
                              IDL_INPUT("Position","unsigned long","x5"),
                              IDL_INPUT("Position","unsigned long long","x6"),
                              IDL_INPUT("Position","octet","x7"),
                              IDL_INPUT("Position","char","x8"),
                              IDL_INPUT("Position","wchar","x9"),
                              IDL_INPUT("try","float","x10"),
                              IDL_INPUT("Position","double","catch")),
  /* Series of corresponding C++ output */
  CU_DataPoints(const char *, IDL_OUTPUT("Position","int16_t","0","x1"),
                              IDL_OUTPUT("Position","int32_t","0","x2"),
                              IDL_OUTPUT("Position","int64_t","0","x3"),
                              IDL_OUTPUT("Position","uint16_t","0","x4"),
                              IDL_OUTPUT("Position","uint32_t","0","x5"),
                              IDL_OUTPUT("Position","uint64_t","0","x6"),
                              IDL_OUTPUT("Position","uint8_t","0","x7"),
                              IDL_OUTPUT("Position","char","0","x8"),
                              IDL_OUTPUT("Position","wchar","0","x9"),
                              IDL_OUTPUT("_cxx_try","float","0.0f","x10"),
                              IDL_OUTPUT("Position","double","0.0","_cxx_catch"))
};

CU_Theory((const char *input, const char *output), idl_backend, cpp11, .timeout = 30)
{
#if 0
  if (initial_run) {
    printf("Sleeping for 8 seconds. Please attach debugger...\n");
    Sleep(8000);
    initial_run = false;
  }
#endif
  test_base_type(input, 0u, IDL_RETCODE_OK, output);
}
