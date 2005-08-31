/* babl - dynamically extendable universal pixel conversion library.
 * Copyright (C) 2005, Øyvind Kolås.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "babl-internal.h"

static void *(* malloc_f) (size_t size) = malloc;
static void  (* free_f)   (void *ptr)   = free;

static void *first_malloc_used = NULL;
static void *first_free_used   = NULL;

void
babl_set_malloc (void *(* malloc_function) (size_t size))
{
  malloc_f = malloc_function;
}

void
babl_set_free   (void (* free_function) (void *ptr))
{
  free_f = free_function;
}

static char *signature = "So long and thanks for all the fish.";

typedef struct
{
  char   *signature;
  size_t  size;
} BablAllocInfo;

#define OFFSET   (sizeof(BablAllocInfo))

#define BAI(ptr)    ((BablAllocInfo*)(((void*)ptr)-OFFSET))
#define IS_BAI(ptr) (BAI(ptr)->signature == signature)

/* runtime statistics: */
static int mallocs  = 0;
static int frees    = 0;
static int strdups  = 0;
static int reallocs = 0;
static int callocs  = 0;
static int dups     = 0;

static const char *
mem_stats (void)
{
  static char buf[128];
  sprintf (buf, "mallocs:%i callocs:%i strdups:%i dups:%i allocs:%i frees:%i reallocs:%i\t|",
    mallocs, callocs, strdups, dups, mallocs+callocs+strdups+dups, frees, reallocs);
  return buf;
}

static void
functions_sanity (void)
{
  if (first_malloc_used != malloc_f ||
      first_free_used   != free_f)
    {
      if (first_malloc_used == NULL)
        {
          first_malloc_used = malloc_f;
          first_free_used   = free_f;
        }
      else
        {
          babl_fatal ("babl memory function(s) attempted switched on the fly");
        }
    }
}
  
/* Allocate /size/ bytes of memory 
 *
 * contents of memory undefined.
 */
void *
babl_malloc (size_t size)
{
  void *ret;

  babl_assert (size);

  functions_sanity ();
  ret = malloc_f (size + OFFSET);
  if (!ret)
    babl_fatal ("args=(%i): failed",  size);

  BAI(ret + OFFSET)->signature = signature;
  BAI(ret + OFFSET)->size      = size;
  mallocs++;
  return ret + OFFSET;
}

/* Create a duplicate allocation of the same size, note
 * that the exact location of the allocation needs to be
 * passed.
 */
void *
babl_dup (void *ptr)
{
  void *ret;
 
  babl_assert (IS_BAI (ptr));

  ret = babl_malloc (BAI(ptr)->size);
  memcpy (ret, ptr, BAI(ptr)->size);

  dups++;
  mallocs--;
  return NULL;
}

/* Free memory allocated by a babl function (note: babl_free
 * will complain if memory not allocated by babl is passed.)
 */
void
babl_free (void *ptr)
{
  if (!ptr)
    return;
  if(!IS_BAI(ptr))
    babl_fatal ("memory not allocated by babl allocator");
  functions_sanity ();
  free_f (BAI(ptr));
  frees++;
}

/* reallocate allocation to be in size instead, contents of
 * common allocated memory between old and new size is preserved.
 */
void *
babl_realloc (void   *ptr,
              size_t  size)
{
  void *ret;

  if (!ptr)
    {
      return babl_malloc (size);
    }

  babl_assert (IS_BAI (ptr));

  if (size==0)
    {
      babl_free (ptr);
      return NULL;
    }
  if (babl_sizeof (ptr) >= size)
    {
      return ptr;
    }
  else if (babl_sizeof (ptr) < size)
    {
      ret = babl_malloc (size);
      memcpy (ret, ptr, babl_sizeof (ptr));
      babl_free (ptr);
      reallocs++;
      return ret;
    }

  if (!ret)
    babl_fatal ("args=(%p, %i): failed",  ptr, size);
  
  return NULL;
}

/* allocate nmemb*size bytes and set it to all zeros. */
void *
babl_calloc (size_t nmemb,
             size_t size)
{
  void *ret = babl_malloc (nmemb*size);

  if (!ret)
    babl_fatal ("args=(%i, %i): failed",  nmemb, size);

  memset (ret, 0, nmemb*size);

  callocs++;
  mallocs--;
  return ret;
}

/* Returns the size of an allocation.
 */
size_t
babl_sizeof (void *ptr)
{
  babl_assert (IS_BAI (ptr));
  return BAI(ptr)->size;
}

/*  duplicate allocation needed for a string, and
 *  copy string contents, string is zero terminated.
 */
char *
babl_strdup (const char *s)
{
  char *ret;

  ret = babl_malloc (strlen (s)+1);
  if (!ret)
    babl_log ("args=(%s): failed",  s);
  strcpy (ret, s); 

  strdups++;
  mallocs--;
  return ret;
}

/* append string to babl allocated string dest, the returned
 * string is the new canonical position with src added to dest
 * if the dest allocation needed to be resized. Passing NULL
 * causes a new allocation (thus babl-memory sees NULL as the empty
 * string).
 */
char *
babl_strcat (char       *dest,
             const char *src)
{
  char *ret;
  int src_len;
  int dst_len;

  src_len = strlen (src);
  if (!dest)
    {
      ret = babl_malloc (src_len+1);
      strcpy (ret, src);
      return ret;
    }
  babl_assert (IS_BAI (dest));
  dst_len = strlen (dest);
  
  ret = dest;

  if (babl_sizeof (dest) < src_len + dst_len + 1)
    {
      size_t new_size = babl_sizeof (dest);
      while (new_size < src_len + dst_len + 1)
        new_size*=2;
      ret = babl_realloc (dest, new_size);
    }

  strcpy (&ret[dst_len], src);
  return ret;
}

/* performs a sanity check on memory, (checks if number of
 * allocations and frees on babl memory evens out to zero).
 */
int
babl_memory_sanity (void)
{
  if (frees != mallocs + strdups + callocs)
    {
      babl_log ("memory usage does not add up!\n"
"%s\n"
"\tbalance: %i-%i=%i\n",
  mem_stats(), (strdups+mallocs+callocs),frees, (strdups+mallocs+callocs)-frees);
      return -1;
    }
  return 0;
}
