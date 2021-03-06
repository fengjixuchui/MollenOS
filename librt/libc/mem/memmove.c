/*
FUNCTION
	<<memmove>>---move possibly overlapping memory

INDEX
	memmove

ANSI_SYNOPSIS
	#include <string.h>
	void *memmove(void *<[dst]>, const void *<[src]>, size_t <[length]>);

TRAD_SYNOPSIS
	#include <string.h>
	void *memmove(<[dst]>, <[src]>, <[length]>)
	void *<[dst]>;
	void *<[src]>;
	size_t <[length]>;

DESCRIPTION
	This function moves <[length]> characters from the block of
	memory starting at <<*<[src]>>> to the memory starting at
	<<*<[dst]>>>. <<memmove>> reproduces the characters correctly
	at <<*<[dst]>>> even if the two areas overlap.


RETURNS
	The function returns <[dst]> as passed.

PORTABILITY
<<memmove>> is ANSI C.

<<memmove>> requires no supporting OS subroutines.

QUICKREF
	memmove ansi pure
*/

#include <string.h>
#include <internal/_string.h>
#include <stdint.h>

void* memmove(void *destination, const void* source, size_t count)
{
	char *dst = (char *)destination;
	const char *src = (char *)source;
	long *aligned_dst;
	const long *aligned_src;

	if (src < dst && dst < src + count)
	{
		/* Destructive overlap...have to copy backwards */
		src += count;
		dst += count;
		while (count--)
	{
		*--dst = *--src;
	}
	}
	else
	{
		/* Use optimizing algorithm for a non-destructive copy to closely 
			match memcpy. If the size is small or either SRC or DST is unaligned,
			then punt into the byte copy loop.  This should be rare.  */
		if (!TOO_SMALL(count) && !UNALIGNED (src, dst))
		{
			aligned_dst = (long*)dst;
			aligned_src = (long*)src;

			/* Copy 4X long words at a time if possible.  */
			while (count >= BIGBLOCKSIZE)
			{
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				count -= BIGBLOCKSIZE;
			}

			/* Copy one long word at a time if possible.  */
			while (count >= LITTLEBLOCKSIZE)
			{
				*aligned_dst++ = *aligned_src++;
				count -= LITTLEBLOCKSIZE;
			}

			/* Pick up any residual with a byte copier.  */
			dst = (char*)aligned_dst;
			src = (char*)aligned_src;
		}

		while (count--)
		{
			*dst++ = *src++;
		}
	}

	return destination;
}