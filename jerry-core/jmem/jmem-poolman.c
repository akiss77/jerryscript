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

/**
 * Memory pool manager implementation
 */

#include "jcontext.h"
#include "jmem.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup poolman Memory pool manager
 * @{
 */

/**
 * Finalize pool manager
 */
void
jmem_pools_finalize (void)
{
  jmem_pools_collect_empty ();

  JERRY_ASSERT (JERRY_CONTEXT (jmem_free_8_byte_chunk_p) == NULL);
#ifdef JERRY_CPOINTER_32_BIT
  JERRY_ASSERT (JERRY_CONTEXT (jmem_free_16_byte_chunk_p) == NULL);
#endif /* JERRY_CPOINTER_32_BIT */
} /* jmem_pools_finalize */

/**
 * Allocate a chunk of 8 bytes
 *
 * @return pointer to allocated chunk, if allocation was successful,
 *         or NULL - if not enough memory.
 */
inline void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_alloc_8 (void)
{
#ifdef JMEM_GC_BEFORE_EACH_ALLOC
  jmem_run_free_unused_memory_callbacks (JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);
#endif /* JMEM_GC_BEFORE_EACH_ALLOC */

  if (JERRY_CONTEXT (jmem_free_8_byte_chunk_p) != NULL)
  {
    const jmem_pools_chunk_t *const chunk_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);

    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = chunk_p->next_p;

    JMEM_VALGRIND_UNDEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    return (void *) chunk_p;
  }
  else
  {
    return (void *) jmem_heap_alloc_block (8);
  }
} /* jmem_pools_alloc_8 */

/**
 * Free an 8-byte chunk
 */
inline void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_free_8 (void *chunk_p) /**< pointer to the chunk */
{
  JERRY_ASSERT (chunk_p != NULL);

  jmem_pools_chunk_t *const chunk_to_free_p = (jmem_pools_chunk_t *) chunk_p;

  JMEM_VALGRIND_DEFINED_SPACE (chunk_to_free_p, 8);

  chunk_to_free_p->next_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = chunk_to_free_p;

  JMEM_VALGRIND_NOACCESS_SPACE (chunk_to_free_p, 8);
} /* jmem_pools_free_8 */

#ifdef JERRY_CPOINTER_32_BIT

/**
 * Allocate a chunk of 16 bytes
 *
 * @return pointer to allocated chunk, if allocation was successful,
 *         or NULL - if not enough memory.
 */
inline void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_alloc_16 (void)
{
#ifdef JMEM_GC_BEFORE_EACH_ALLOC
  jmem_run_free_unused_memory_callbacks (JMEM_FREE_UNUSED_MEMORY_SEVERITY_HIGH);
#endif /* JMEM_GC_BEFORE_EACH_ALLOC */

  if (JERRY_CONTEXT (jmem_free_16_byte_chunk_p) != NULL)
  {
    const jmem_pools_chunk_t *const chunk_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);

    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = chunk_p->next_p;

    JMEM_VALGRIND_UNDEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    return (void *) chunk_p;
  }
  else
  {
    return (void *) jmem_heap_alloc_block (16);
  }
} /* jmem_pools_alloc_16 */

/**
 * Free a 16-byte chunk
 */
inline void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_pools_free_16 (void *chunk_p) /**< pointer to the chunk */
{
  JERRY_ASSERT (chunk_p != NULL);

  jmem_pools_chunk_t *const chunk_to_free_p = (jmem_pools_chunk_t *) chunk_p;

  JMEM_VALGRIND_DEFINED_SPACE (chunk_to_free_p, 16);

  chunk_to_free_p->next_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = chunk_to_free_p;

  JMEM_VALGRIND_NOACCESS_SPACE (chunk_to_free_p, 16);
} /* jmem_pools_free_16 */

#endif /* JERRY_CPOINTER_32_BIT */

/**
 *  Collect empty pool chunks
 */
void
jmem_pools_collect_empty (void)
{
  jmem_pools_chunk_t *chunk_p = JERRY_CONTEXT (jmem_free_8_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_8_byte_chunk_p) = NULL;

  while (chunk_p)
  {
    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
    jmem_pools_chunk_t *const next_p = chunk_p->next_p;
    JMEM_VALGRIND_NOACCESS_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    jmem_heap_free_block (chunk_p, 8);
    chunk_p = next_p;
  }

#ifdef JERRY_CPOINTER_32_BIT
  chunk_p = JERRY_CONTEXT (jmem_free_16_byte_chunk_p);
  JERRY_CONTEXT (jmem_free_16_byte_chunk_p) = NULL;

  while (chunk_p)
  {
    JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
    jmem_pools_chunk_t *const next_p = chunk_p->next_p;
    JMEM_VALGRIND_NOACCESS_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

    jmem_heap_free_block (chunk_p, 16);
    chunk_p = next_p;
  }
#endif /* JERRY_CPOINTER_32_BIT */
} /* jmem_pools_collect_empty */

/**
 * @}
 * @}
 */
