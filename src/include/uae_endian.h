/*
 * uae_endian.h - the single authoritative host byte order test.
 *
 * UAE4All keeps every piece of Amiga data that is wider than one byte in
 * *host words*: on a little-endian host each 16 bit Amiga word is stored
 * byte-swapped inside its host word (so a plain u16 load already returns the
 * M68k word, and byte accesses XOR the address with 1), while on a big-endian
 * host the bytes are already in Amiga order and no swapping happens at all.
 *
 * Every file that depends on that convention must take the same branch.  When
 * they disagree the emulator silently mixes the two conventions - e.g. the
 * FAME register union laid out for big-endian while the memory accessors still
 * XOR the address with 1, which corrupts every byte and odd-address access.
 *
 * The PS3 build defines __PS3__ and __BIG_ENDIAN__ (see CMakeLists.txt), a
 * plain big-endian GCC target usually defines __BIG_ENDIAN__ too and always
 * defines __BYTE_ORDER__, so all of them are honoured here.
 */
#ifndef UAE_ENDIAN_H
#define UAE_ENDIAN_H

#ifndef UAEBIGENDIAN
#  if defined(__BIG_ENDIAN__) || defined(WORDS_BIGENDIAN) || defined(__PS3__) \
      || (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) \
          && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define UAEBIGENDIAN 1
#  else
#    define UAEBIGENDIAN 0
#  endif
#endif

#endif /* UAE_ENDIAN_H */
