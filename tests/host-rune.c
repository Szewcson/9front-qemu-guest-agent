/* SPDX-License-Identifier: MIT */
/* Linux-only codec adapter; native JSON code calls 9front libc runetochar. */
#include <iconv.h>
#include <stddef.h>

int
qgahostrunetochar(char *out, unsigned int rune)
{
	iconv_t codec;
	char input[4], *from, *to;
	size_t inbytes, outbytes, result;
	int i;
	if(out == NULL || rune > 0x10ffff || (rune >= 0xd800 && rune <= 0xdfff)) return -1;
	for(i=0; i<4; i++) input[i] = (char)(rune >> (8*i));
	codec = iconv_open("UTF-8", "UTF-32LE");
	/* GCC models iconv_open's malloc attribute but misses its (iconv_t)-1
	 * failure sentinel. That value owns no descriptor and must not be closed. */
#ifdef QGA_ANALYZER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
	if(codec == (iconv_t)-1) return -1;
#ifdef QGA_ANALYZER
#pragma GCC diagnostic pop
#endif
	from = input; to = out; inbytes = outbytes = 4;
	result = iconv(codec, &from, &inbytes, &to, &outbytes);
	iconv_close(codec);
	return result == (size_t)-1 || inbytes != 0 ? -1 : (int)(4-outbytes);
}
