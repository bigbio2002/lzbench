/*
 * hc_matchfinder.h - Lempel-Ziv matchfinding with a hash table of linked lists
 *
 * ---------------------------------------------------------------------------
 *
 *				   Algorithm
 *
 * This is a Hash Chains (hc) based matchfinder.
 *
 * The main data structure is a hash table where each hash bucket contains a
 * linked list (or "chain") of sequences whose first 4 bytes share the same hash
 * code.  Each sequence is identified by its starting position in the input
 * buffer.
 *
 * The algorithm processes the input buffer sequentially.  At each byte
 * position, the hash code of the first 4 bytes of the sequence beginning at
 * that position (the sequence being matched against) is computed.  This
 * identifies the hash bucket to use for that position.  Then, this hash
 * bucket's linked list is searched for matches.  Then, a new linked list node
 * is created to represent the current sequence and is prepended to the list.
 *
 * This algorithm has several useful properties:
 *
 * - It only finds true Lempel-Ziv matches; i.e., those where the matching
 *   sequence occurs prior to the sequence being matched against.
 *
 * - The sequences in each linked list are always sorted by decreasing starting
 *   position.  Therefore, the closest (smallest offset) matches are found
 *   first, which in many compression formats tend to be the cheapest to encode.
 *
 * - Although fast running time is not guaranteed due to the possibility of the
 *   lists getting very long, the worst degenerate behavior can be easily
 *   prevented by capping the number of nodes searched at each position.
 *
 * - If the compressor decides not to search for matches at a certain position,
 *   then that position can be quickly inserted without searching the list.
 *
 * - The algorithm is adaptable to sliding windows: just store the positions
 *   relative to a "base" value that is updated from time to time, and stop
 *   searching each list when the sequences get too far away.
 *
 * ---------------------------------------------------------------------------
 *
 *				Notes on usage
 *
 * The number of bytes that must be allocated for a given 'struct
 * hc_matchfinder' must be gotten by calling hc_matchfinder_size().
 *
 * ----------------------------------------------------------------------------
 *
 *				 Optimizations
 *
 * The main hash table and chains handle length 4+ matches.  Length 3 matches
 * are handled by a separate hash table with no chains.  This works well for
 * typical "greedy" or "lazy"-style compressors, where length 3 matches are
 * often only helpful if they have small offsets.  Instead of searching a full
 * chain for length 3+ matches, the algorithm just checks for one close length 3
 * match, then focuses on finding length 4+ matches.
 *
 * The longest_match() and skip_positions() functions are inlined into the
 * compressors that use them.  This isn't just about saving the overhead of a
 * function call.  These functions are intended to be called from the inner
 * loops of compressors, where giving the compiler more control over register
 * allocation is very helpful.  There is also significant benefit to be gained
 * from allowing the CPU to predict branches independently at each call site.
 * For example, "lazy"-style compressors can be written with two calls to
 * longest_match(), each of which starts with a different 'best_len' and
 * therefore has significantly different performance characteristics.
 *
 * Although any hash function can be used, a multiplicative hash is fast and
 * works well.
 *
 * On some processors, it is significantly faster to extend matches by whole
 * words (32 or 64 bits) instead of by individual bytes.  For this to be the
 * case, the processor must implement unaligned memory accesses efficiently and
 * must have either a fast "find first set bit" instruction or a fast "find last
 * set bit" instruction, depending on the processor's endianness.
 *
 * The code uses one loop for finding the first match and one loop for finding a
 * longer match.  Each of these loops is tuned for its respective task and in
 * combination are faster than a single generalized loop that handles both
 * tasks.
 *
 * The code also uses a tight inner loop that only compares the last and first
 * bytes of a potential match.  It is only when these bytes match that a full
 * match extension is attempted.
 *
 * ----------------------------------------------------------------------------
 */

#ifndef _XPACK_HC_MATCHFINDER_H_
#define _XPACK_HC_MATCHFINDER_H_

#include <string.h>

#include "lz_extend.h"
#include "lz_hash.h"
#include "unaligned.h"

#define HC_MATCHFINDER_HASH3_ORDER	15
#define HC_MATCHFINDER_HASH4_ORDER	16

struct hc_matchfinder {

	/* The hash table for finding length 3 matches */
	u32 hash3_tab[1UL << HC_MATCHFINDER_HASH3_ORDER];

	/* The hash table which contains the first nodes of the linked lists for
	 * finding length 4+ matches */
	u32 hash4_tab[1UL << HC_MATCHFINDER_HASH4_ORDER];

	/* The "next node" references for the linked lists.  The "next node" of
	 * the node for the sequence with position 'pos' is 'next_tab[pos]'. */
	u32 next_tab[];
};

/*
 * Return the number of bytes that must be allocated for a 'hc_matchfinder' that
 * can work with buffers up to the specified size.
 */
static forceinline size_t
hc_matchfinder_size(size_t max_bufsize)
{
	return sizeof(struct hc_matchfinder) + (max_bufsize * sizeof(u32));
}

/* Prepare the matchfinder for a new input buffer. */
static forceinline void
hc_matchfinder_init(struct hc_matchfinder *mf)
{
	memset(mf, 0, sizeof(*mf));
}

/*
 * Find the longest match longer than 'best_len' bytes.
 *
 * @mf
 *	The matchfinder structure.
 * @in_begin
 *	Pointer to the beginning of the input buffer.
 * @cur_pos
 *	The current position in the input buffer (the position of the sequence
 *	being matched against).
 * @best_len
 *	Require a match longer than this length.
 * @max_len
 *	The maximum permissible match length at this position.
 * @nice_len
 *	Stop searching if a match of at least this length is found.
 *	Must be <= @max_len.
 * @max_search_depth
 *	Limit on the number of potential matches to consider.  Must be >= 1.
 * @next_hashes
 *	The precomputed hash codes for the sequence beginning at @in_next.
 *	These will be used and then updated with the precomputed hashcodes for
 *	the sequence beginning at @in_next + 1.
 * @offset_ret
 *	If a match is found, its offset is returned in this location.
 *
 * Return the length of the match found, or 'best_len' if no match longer than
 * 'best_len' was found.
 */
static forceinline u32
hc_matchfinder_longest_match(struct hc_matchfinder * const restrict mf,
			     const u8 * const restrict in_begin,
			     const ptrdiff_t cur_pos,
			     u32 best_len,
			     const u32 max_len,
			     const u32 nice_len,
			     const u32 max_search_depth,
			     u32 next_hashes[restrict 2],
			     u32 * const restrict offset_ret)
{
	const u8 *in_next = in_begin + cur_pos;
	u32 depth_remaining = max_search_depth;
	const u8 *best_matchptr = in_next;
	u32 cur_node3, cur_node4;
	u32 hash3, hash4;
	u32 next_seq3, next_seq4;
	u32 seq4;
	const u8 *matchptr;
	u32 len;

	if (unlikely(max_len < 5)) /* can we read 4 bytes from 'in_next + 1'? */
		goto out;

	/* Get the precomputed hash codes */
	hash3 = next_hashes[0];
	hash4 = next_hashes[1];

	/* From the hash buckets, get the first node of each linked list. */
	cur_node3 = mf->hash3_tab[hash3];
	cur_node4 = mf->hash4_tab[hash4];

	/* Update for length 3 matches.  This replaces the singleton node in the
	 * 'hash3' bucket with the node for the current sequence. */
	mf->hash3_tab[hash3] = cur_pos;

	/* Update for length 4 matches.  This prepends the node for the current
	 * sequence to the linked list in the 'hash4' bucket. */
	mf->hash4_tab[hash4] = cur_pos;
	mf->next_tab[cur_pos] = cur_node4;

	/* Compute the next hash codes */
	next_seq4 = load_u32_unaligned(in_next + 1);
	next_seq3 = loaded_u32_to_u24(next_seq4);
	next_hashes[0] = lz_hash(next_seq3, HC_MATCHFINDER_HASH3_ORDER);
	next_hashes[1] = lz_hash(next_seq4, HC_MATCHFINDER_HASH4_ORDER);
	prefetchw(&mf->hash3_tab[next_hashes[0]]);
	prefetchw(&mf->hash4_tab[next_hashes[1]]);

	if (best_len < 4) {  /* No match of length >= 4 found yet? */

		/* Check for a length 3 match if needed */

		if (!cur_node3)
			goto out;

		seq4 = load_u32_unaligned(in_next);

		if (best_len < 3) {
			matchptr = &in_begin[cur_node3];
			if (load_u24_unaligned(matchptr) == loaded_u32_to_u24(seq4)) {
				best_len = 3;
				best_matchptr = matchptr;
			}
		}

		/* Check for a length 4 match */

		if (!cur_node4)
			goto out;

		for (;;) {
			/* No length 4 match found yet.  Check the first 4 bytes. */
			matchptr = &in_begin[cur_node4];

			if (load_u32_unaligned(matchptr) == seq4)
				break;

			/* The first 4 bytes did not match.  Keep trying. */
			cur_node4 = mf->next_tab[cur_node4];
			if (!cur_node4 || !--depth_remaining)
				goto out;
		}

		/* Found a match of length >= 4.  Extend it to its full length. */
		best_matchptr = matchptr;
		best_len = lz_extend(in_next, best_matchptr, 4, max_len);
		if (best_len >= nice_len)
			goto out;
		cur_node4 = mf->next_tab[cur_node4];
		if (!cur_node4 || !--depth_remaining)
			goto out;
	} else {
		if (!cur_node4 || best_len >= nice_len)
			goto out;
	}

	/* Check for matches of length >= 5 */

	for (;;) {
		for (;;) {
			matchptr = &in_begin[cur_node4];

			/* Already found a length 4 match.  Try for a longer
			 * match; start by checking either the last 4 bytes and
			 * the first 4 bytes, or the last byte.  (The last byte,
			 * the one which would extend the match length by 1, is
			 * the most important.) */
		#if UNALIGNED_ACCESS_IS_FAST
			if ((load_u32_unaligned(matchptr + best_len - 3) ==
			     load_u32_unaligned(in_next + best_len - 3)) &&
			    (load_u32_unaligned(matchptr) ==
			     load_u32_unaligned(in_next)))
		#else
			if (matchptr[best_len] == in_next[best_len])
		#endif
				break;

			/* Continue to the next node in the list */
			cur_node4 = mf->next_tab[cur_node4];
			if (!cur_node4 || !--depth_remaining)
				goto out;
		}

	#if UNALIGNED_ACCESS_IS_FAST
		len = 4;
	#else
		len = 0;
	#endif
		len = lz_extend(in_next, matchptr, len, max_len);
		if (len > best_len) {
			/* This is the new longest match */
			best_len = len;
			best_matchptr = matchptr;
			if (best_len >= nice_len)
				goto out;
		}

		/* Continue to the next node in the list */
		cur_node4 = mf->next_tab[cur_node4];
		if (!cur_node4 || !--depth_remaining)
			goto out;
	}
out:
	*offset_ret = in_next - best_matchptr;
	return best_len;
}

/*
 * Advance the matchfinder, but don't search for matches.
 *
 * @mf
 *	The matchfinder structure.
 * @in_begin
 *	Pointer to the beginning of the input buffer.
 * @cur_pos
 *	The current position in the input buffer (the position of the sequence
 *	being matched against).
 * @end_pos
 *	The length of the input buffer.
 * @next_hashes
 *	The precomputed hash codes for the sequence beginning at @in_next.
 *	These will be used and then updated with the precomputed hashcodes for
 *	the sequence beginning at @in_next + @count.
 * @count
 *	The number of bytes to advance.  Must be > 0.
 *
 * Returns @in_next + @count.
 */
static forceinline const u8 *
hc_matchfinder_skip_positions(struct hc_matchfinder * const restrict mf,
			      const u8 * const restrict in_begin,
			      const ptrdiff_t cur_pos,
			      const ptrdiff_t end_pos,
			      const u32 count,
			      u32 next_hashes[restrict 2])
{
	const u8 *in_next = in_begin + cur_pos;
	const u8 * const stop_ptr = in_next + count;

	if (likely(count + 5 <= end_pos - cur_pos)) {
		u32 hash3, hash4;
		u32 next_seq3, next_seq4;

		hash3 = next_hashes[0];
		hash4 = next_hashes[1];
		do {
			mf->hash3_tab[hash3] = in_next - in_begin;
			mf->next_tab[in_next - in_begin] = mf->hash4_tab[hash4];
			mf->hash4_tab[hash4] = in_next - in_begin;

			next_seq4 = load_u32_unaligned(++in_next);
			next_seq3 = loaded_u32_to_u24(next_seq4);
			hash3 = lz_hash(next_seq3, HC_MATCHFINDER_HASH3_ORDER);
			hash4 = lz_hash(next_seq4, HC_MATCHFINDER_HASH4_ORDER);

		} while (in_next != stop_ptr);

		prefetchw(&mf->hash3_tab[hash3]);
		prefetchw(&mf->hash4_tab[hash4]);
		next_hashes[0] = hash3;
		next_hashes[1] = hash4;
	}

	return stop_ptr;
}

#endif
