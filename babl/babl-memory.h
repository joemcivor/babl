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

#ifndef _BABL_MEMORY_H
#define _BABL_MEMORY_H

void   babl_set_malloc    (void     *(*malloc_function) (size_t  size));
void   babl_set_free      (void      (*free)            (void   *ptr));
int    babl_memory_sanity (void);

void * babl_malloc        (size_t      size);
void   babl_free          (void       *ptr,
                           ...);
void * babl_calloc        (size_t      nmemb,
                           size_t      size);
void * babl_realloc       (void       *ptr,
                           size_t      size);

size_t babl_sizeof        (void       *ptr);
void * babl_dup           (void       *ptr);

char * babl_strdup        (const char *s);
char * babl_strcat        (char       *dest,
                           const char *src);


#endif
