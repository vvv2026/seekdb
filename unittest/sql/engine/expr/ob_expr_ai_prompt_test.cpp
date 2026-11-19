/*
 * Copyright (c) 2025 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Unit tests of the AI_PROMPT object helpers.
 *
 * The mysql test cases of the competition live in
 * tools/deploy/mysql_test/test_suite/ai_function/t/ai_prompt.test, these tests
 * cover the pure helpers which build and expand the prompt object so that the
 * behaviour can be verified without a running observer.
 */

#include <gtest/gtest.h>
#include "lib/allocator/page_arena.h"
#include "lib/string/ob_string.h"
#include "sql/engine/expr/ob_expr_ai/ob_ai_func_utils.h"

using namespace oceanbase::common;
using namespace oceanbase::sql;

class ObAIPromptTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
  }

  void TearDown() override
  {
  }

  static ObString to_str(const char *value)
  {
    return ObString(value, static_cast<int32_t>(strlen(value)));
  }
};

// a prompt object holds exactly the template and the args
TEST_F(ObAIPromptTest, test_ob_is_vaild_prompt_object)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObString a("a");
  ObJsonString a_str(a);
  ObString raw_str("{\"template\": \"{0}+{1}={2} 吗？请回答true或false\", \"args\": [\"1\", \"2\", \"3\"]}");
  ObJsonObject *prompt_object = NULL;
  ObString prompt_str;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::get_json_object_form_str(allocator, raw_str, prompt_object));
  ASSERT_TRUE(ObAIFuncPromptObjectUtils::is_valid_prompt_object(prompt_object));
  prompt_object->add("a", &a_str);
  ASSERT_FALSE(ObAIFuncPromptObjectUtils::is_valid_prompt_object(prompt_object));
}

// every placeholder of the template is replaced by the matching argument
TEST_F(ObAIPromptTest, test_replace_all_str_args_in_template)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObString raw_str("{\"template\": \"{0}+{1}={2} 吗？请回答true或false\", \"args\": [\"1\", \"2\", \"3\"]}");
  ObJsonObject *prompt_object = NULL;
  ObString prompt_str;
  ObString result_str;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::get_json_object_form_str(allocator, raw_str, prompt_object));
  ASSERT_TRUE(ObAIFuncPromptObjectUtils::is_valid_prompt_object(prompt_object));
  ASSERT_EQ(OB_SUCCESS, ObAIFuncPromptObjectUtils::replace_all_str_args_in_template(allocator, prompt_object, prompt_str));
  ob_write_string(allocator, prompt_str, result_str, true);
  ASSERT_EQ(result_str, "1+2=3 吗？请回答true或false");
}

// the same placeholder may be used more than once and the arguments may be
// json values, they are rendered with their json representation
TEST_F(ObAIPromptTest, test_repeated_and_json_arguments)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObString raw_str(
      "{\"template\":\"{0}/{0}/{1}/{2}\","
      "\"args\":[\"oceanbase\",{\"version\":4},[1,true,null]]}");
  ObJsonObject *prompt_object = nullptr;
  ObString prompt_str;
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncJsonUtils::get_json_object_form_str(allocator, raw_str, prompt_object));
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncPromptObjectUtils::replace_all_str_args_in_template(
                allocator, prompt_object, prompt_str));
  ASSERT_EQ(ObString("oceanbase/oceanbase/{\"version\": 4}/[1, true, null]"), prompt_str);
}

// a prompt object may be nested in the args of another prompt object
TEST_F(ObAIPromptTest, test_nested_prompt_and_invalid_placeholder)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObString nested_raw(
      "{\"template\":\"nested={0}; again={0}\",\"args\":[{"
      "\"template\":\"{0}-{1}\",\"args\":[\"a\",{\"k\":2}]}]}");
  ObJsonObject *nested_object = nullptr;
  ObString prompt_str;
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncJsonUtils::get_json_object_form_str(allocator, nested_raw, nested_object));
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncPromptObjectUtils::replace_all_str_args_in_template(
                allocator, nested_object, prompt_str));
  ASSERT_EQ(ObString("nested=a-{\"k\": 2}; again=a-{\"k\": 2}"), prompt_str);

  // a placeholder which does not match any argument is rejected
  ObString invalid_raw("{\"template\":\"{1}\",\"args\":[\"only-zero\"]}");
  ObJsonObject *invalid_object = nullptr;
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncJsonUtils::get_json_object_form_str(allocator, invalid_raw, invalid_object));
  ASSERT_EQ(OB_INVALID_ARGUMENT,
            ObAIFuncPromptObjectUtils::replace_all_str_args_in_template(
                allocator, invalid_object, prompt_str));
}

// the prompt object is built from the template and the args
TEST_F(ObAIPromptTest, test_construct_prompt_object)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObJsonArray *args_array = nullptr;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::get_json_array(allocator, args_array));
  ObJsonString *arg_node = nullptr;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::get_json_string(allocator, to_str("world"), arg_node));
  ASSERT_EQ(OB_SUCCESS, args_array->append(arg_node));

  ObJsonObject *prompt_object = nullptr;
  ObString template_str = to_str("hello {0}");
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncPromptObjectUtils::construct_prompt_object(
                allocator, template_str, args_array, prompt_object));
  ASSERT_TRUE(ObAIFuncPromptObjectUtils::is_valid_prompt_object(prompt_object));

  ObString printed;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::print_json_to_str(allocator, prompt_object, printed));
  ASSERT_STREQ("{\"args\": [\"world\"], \"template\": \"hello {0}\"}", printed.ptr());
}

// an empty template is not a valid prompt
TEST_F(ObAIPromptTest, test_construct_prompt_object_with_empty_template)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObJsonArray *args_array = nullptr;
  ASSERT_EQ(OB_SUCCESS, ObAIFuncJsonUtils::get_json_array(allocator, args_array));
  ObJsonObject *prompt_object = nullptr;
  ObString template_str = to_str("");
  ASSERT_EQ(OB_INVALID_ARGUMENT,
            ObAIFuncPromptObjectUtils::construct_prompt_object(
                allocator, template_str, args_array, prompt_object));
  ASSERT_TRUE(OB_ISNULL(prompt_object));
}

// the unicode content survives the template expansion
TEST_F(ObAIPromptTest, test_replace_unicode_content)
{
  ObArenaAllocator allocator(ObModIds::TEST);
  ObString raw_str("{\"template\":\"say {0}\",\"args\":[\"你好\"]}");
  ObJsonObject *prompt_object = nullptr;
  ObString prompt_str;
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncJsonUtils::get_json_object_form_str(allocator, raw_str, prompt_object));
  ASSERT_EQ(OB_SUCCESS,
            ObAIFuncPromptObjectUtils::replace_all_str_args_in_template(
                allocator, prompt_object, prompt_str));
  ASSERT_EQ(ObString("say 你好"), prompt_str);
}
