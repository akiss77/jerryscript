/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecma-alloc.h"
#include "ecma-array-object.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"
#include "ecma-try-catch-macro.h"
#include "lit-char-helpers.h"

#ifndef CONFIG_DISABLE_REGEXP_BUILTIN
#include "ecma-regexp-object.h"
#include "re-compiler.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-regexp-prototype.inc.h"
#define BUILTIN_UNDERSCORED_ID regexp_prototype
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup regexpprototype ECMA RegExp.prototype object built-in
 * @{
 */

#ifndef CONFIG_DISABLE_ANNEXB_BUILTIN

/**
 * The RegExp.prototype object's 'compile' routine
 *
 * See also:
 *          ECMA-262 v5, B.2.5.1
 *
 * @return undefined        - if compiled successfully
 *         error ecma value - otherwise
 *
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_regexp_prototype_compile (ecma_value_t this_arg, /**< this argument */
                                       ecma_value_t pattern_arg, /**< pattern or RegExp object */
                                       ecma_value_t flags_arg) /**< flags */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (this_arg)
      || !ecma_object_class_is (ecma_get_object_from_value (this_arg), LIT_MAGIC_STRING_REGEXP_UL))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Incomplete RegExp type"));
  }
  else
  {
    ecma_string_t *pattern_string_p = NULL;
    uint16_t flags = 0;

    if (ecma_is_value_object (pattern_arg)
        && ecma_object_class_is (ecma_get_object_from_value (pattern_arg), LIT_MAGIC_STRING_REGEXP_UL))
    {
      if (!ecma_is_value_undefined (flags_arg))
      {
        ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Invalid argument of RegExp compile."));
      }
      else
      {
        /* Compile from existing RegExp object. */
        ecma_object_t *pattern_obj_p = ecma_get_object_from_value (pattern_arg);

        /* Get existing bytecode. */
        ecma_value_t *pattern_bc_prop_p = &(((ecma_extended_object_t *) pattern_obj_p)->u.class_prop.u.value);
        re_compiled_code_t *pattern_bc_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (re_compiled_code_t,
                                                                                *pattern_bc_prop_p);

        /* Get source and flags. */
        if (pattern_bc_p != NULL)
        {
          pattern_string_p = ecma_get_string_from_value (pattern_bc_p->pattern);
          ecma_ref_ecma_string (pattern_string_p);

          flags = pattern_bc_p->header.status_flags;
        }
        else
        {
          pattern_string_p = ecma_get_magic_string (LIT_MAGIC_STRING_EMPTY_NON_CAPTURE_GROUP);
        }
      }
    }
    else
    {
      /* Get source string. */
      ret_value = ecma_regexp_read_pattern_str_helper (pattern_arg, &pattern_string_p);

      /* Parse flags. */
      if (ecma_is_value_empty (ret_value) && !ecma_is_value_undefined (flags_arg))
      {
        ECMA_TRY_CATCH (flags_str_value,
                        ecma_op_to_string (flags_arg),
                        ret_value);
        ECMA_TRY_CATCH (flags_dummy,
                        re_parse_regexp_flags (ecma_get_string_from_value (flags_str_value), &flags),
                        ret_value);
        ECMA_FINALIZE (flags_dummy);
        ECMA_FINALIZE (flags_str_value);
      }
    }

    if (ecma_is_value_empty (ret_value))
    {
      ecma_value_t obj_this = ecma_op_to_object (this_arg);
      JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (obj_this));

      ecma_object_t *this_obj_p = ecma_get_object_from_value (obj_this);

      /* Try to compile bytecode from source (it will succeed if source is copied from existing RegExp). */
      const re_compiled_code_t *new_bc_p = NULL;
      ECMA_TRY_CATCH (bc_dummy,
                      re_compile_bytecode (&new_bc_p, pattern_string_p, flags),
                      ret_value);

      ecma_value_t *bc_prop_p = &(((ecma_extended_object_t *) this_obj_p)->u.class_prop.u.value);

      /* Free the old bytecode */
      re_compiled_code_t *old_bc_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (re_compiled_code_t, *bc_prop_p);
      if (old_bc_p != NULL)
      {
        ecma_bytecode_deref ((ecma_compiled_code_t *) old_bc_p);
      }

      ECMA_SET_INTERNAL_VALUE_POINTER (*bc_prop_p, new_bc_p);
      re_initialize_props (this_obj_p, new_bc_p);
      ret_value = ECMA_VALUE_UNDEFINED;

      ECMA_FINALIZE (bc_dummy);
      ecma_free_value (obj_this);
    }

    if (pattern_string_p != NULL)
    {
      ecma_deref_ecma_string (pattern_string_p);
    }
  }

  return ret_value;
} /* ecma_builtin_regexp_prototype_compile */

#endif /* !CONFIG_DISABLE_ANNEXB_BUILTIN */

/**
 * The RegExp.prototype object's 'exec' routine
 *
 * See also:
 *          ECMA-262 v5, 15.10.6.2
 *
 * @return array object containing the results - if the matched
 *         null                                - otherwise
 *
 *         May raise error, so returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_regexp_prototype_exec (ecma_value_t this_arg, /**< this argument */
                                    ecma_value_t arg) /**< routine's argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (this_arg)
      || !ecma_object_class_is (ecma_get_object_from_value (this_arg), LIT_MAGIC_STRING_REGEXP_UL))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Incomplete RegExp type"));
  }
  else
  {
    ecma_value_t obj_this = ecma_op_to_object (this_arg);
    JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (obj_this));

    ECMA_TRY_CATCH (input_str_value,
                    ecma_op_to_string (arg),
                    ret_value);

    ecma_object_t *obj_p = ecma_get_object_from_value (obj_this);
    ecma_value_t *bytecode_prop_p = &(((ecma_extended_object_t *) obj_p)->u.class_prop.u.value);

    void *bytecode_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (void, *bytecode_prop_p);

    if (bytecode_p == NULL)
    {
      /* Missing bytecode means empty RegExp: '/(?:)/', so always return empty string. */
      ecma_value_t arguments_list[1];
      arguments_list[0] = ecma_make_magic_string_value (LIT_MAGIC_STRING__EMPTY);

      ret_value = ecma_op_create_array_object (arguments_list, 1, false);

      re_set_result_array_properties (ecma_get_object_from_value (ret_value),
                                      ecma_get_string_from_value (input_str_value),
                                      1,
                                      0);
    }
    else
    {
      ret_value = ecma_regexp_exec_helper (obj_this, input_str_value, false);
    }

    ECMA_FINALIZE (input_str_value);
    ecma_free_value (obj_this);
  }

  return ret_value;
} /* ecma_builtin_regexp_prototype_exec */

/**
 * The RegExp.prototype object's 'test' routine
 *
 * See also:
 *          ECMA-262 v5, 15.10.6.3
 *
 * @return true  - if match is not null
 *         false - otherwise
 *
 *         May raise error, so returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_regexp_prototype_test (ecma_value_t this_arg, /**< this argument */
                                    ecma_value_t arg) /**< routine's argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  ECMA_TRY_CATCH (match_value,
                  ecma_builtin_regexp_prototype_exec (this_arg, arg),
                  ret_value);

  ret_value = ecma_make_boolean_value (!ecma_is_value_null (match_value));

  ECMA_FINALIZE (match_value);

  return ret_value;
} /* ecma_builtin_regexp_prototype_test */

/**
 * The RegExp.prototype object's 'toString' routine
 *
 * See also:
 *          ECMA-262 v5, 15.10.6.4
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value.
 */
static ecma_value_t
ecma_builtin_regexp_prototype_to_string (ecma_value_t this_arg) /**< this argument */
{
  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  if (!ecma_is_value_object (this_arg)
      || !ecma_object_class_is (ecma_get_object_from_value (this_arg), LIT_MAGIC_STRING_REGEXP_UL))
  {
    ret_value = ecma_raise_type_error (ECMA_ERR_MSG ("Incomplete RegExp type"));
  }
  else
  {
    ecma_value_t obj_this = ecma_op_to_object (this_arg);
    JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (obj_this));

    ecma_object_t *obj_p = ecma_get_object_from_value (obj_this);

    /* Get bytecode. */
    ecma_value_t *bc_prop_p = &(((ecma_extended_object_t *) obj_p)->u.class_prop.u.value);
    re_compiled_code_t *bc_p = ECMA_GET_INTERNAL_VALUE_ANY_POINTER (re_compiled_code_t,
                                                                    *bc_prop_p);

    /* Get source and flags. */
    ecma_string_t *source_str_p;
    uint16_t flags = 0;

    if (bc_p != NULL)
    {
      source_str_p = ecma_get_string_from_value (bc_p->pattern);
      flags = bc_p->header.status_flags;
    }
    else
    {
      source_str_p = ecma_get_magic_string (LIT_MAGIC_STRING_EMPTY_NON_CAPTURE_GROUP);
    }

    ecma_string_t *output_str_p = ecma_get_magic_string (LIT_MAGIC_STRING_SLASH_CHAR);
    output_str_p = ecma_concat_ecma_strings (output_str_p, source_str_p);

    lit_utf8_byte_t flags_str[4];
    lit_utf8_byte_t *flags_p = flags_str;

    *flags_p++ = LIT_CHAR_SLASH;

    /* Check the global flag */
    if (flags & RE_FLAG_GLOBAL)
    {
      *flags_p++ = LIT_CHAR_LOWERCASE_G;
    }

    /* Check the ignoreCase flag */
    if (flags & RE_FLAG_IGNORE_CASE)
    {
      *flags_p++ = LIT_CHAR_LOWERCASE_I;
    }

    /* Check the multiline flag */
    if (flags & RE_FLAG_MULTILINE)
    {
      *flags_p++ = LIT_CHAR_LOWERCASE_M;
    }

    lit_utf8_size_t size = (lit_utf8_size_t) (flags_p - flags_str);
    output_str_p = ecma_append_chars_to_string (output_str_p, flags_str, size, size);

    ret_value = ecma_make_string_value (output_str_p);

    ecma_free_value (obj_this);
  }

  return ret_value;
} /* ecma_builtin_regexp_prototype_to_string */

/**
 * @}
 * @}
 * @}
 */

#endif /* !CONFIG_DISABLE_REGEXP_BUILTIN */
