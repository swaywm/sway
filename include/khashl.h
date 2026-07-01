/* The MIT License

   Copyright (c) 2019- by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef __AC_KHASHL_H
#define __AC_KHASHL_H

#define AC_VERSION_KHASHL_H "r40"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/************************************
 * Compiler specific configurations *
 ************************************/

#if UINT_MAX == 0xffffffffu
typedef unsigned int khint32_t;
#elif ULONG_MAX == 0xffffffffu
typedef unsigned long khint32_t;
#endif

#if ULONG_MAX == ULLONG_MAX
typedef unsigned long khint64_t;
#else
typedef unsigned long long khint64_t;
#endif

#ifndef kh_inline
#ifdef _MSC_VER
#define kh_inline __inline
#else
#define kh_inline inline
#endif
#endif /* kh_inline */

#ifndef klib_unused
#if (defined __clang__ && __clang_major__ >= 3) || (defined __GNUC__ && __GNUC__ >= 3)
#define klib_unused __attribute__ ((__unused__))
#else
#define klib_unused
#endif
#endif /* klib_unused */

#define KH_LOCAL static kh_inline klib_unused

typedef khint32_t khint_t;
typedef const char *kh_cstr_t;

/***********************
 * Configurable macros *
 ***********************/

#ifndef kh_max_count /* set the max load factor */
#define kh_max_count(cap) (((cap)>>1) + ((cap)>>2)) /* default load factor: 75% */
#endif

#ifndef kh_packed /* pack the key-value struct */
#define kh_packed __attribute__ ((__packed__))
#endif

#if !defined(Kmalloc) || !defined(Kcalloc) || !defined(Krealloc) || !defined(Kfree)
#define Kmalloc(km, type, cnt)       ((type*)malloc((cnt) * sizeof(type)))
#define Kcalloc(km, type, cnt)       ((type*)calloc((cnt), sizeof(type)))
#define Krealloc(km, type, ptr, cnt) ((type*)realloc((ptr), (cnt) * sizeof(type)))
#define Kfree(km, ptr)               free(ptr)
#endif

/****************************
 * Simple private functions *
 ****************************/

#define __kh_used(flag, i)       (flag[i>>5] >> (i&0x1fU) & 1U)
#define __kh_set_used(flag, i)   (flag[i>>5] |= 1U<<(i&0x1fU))
#define __kh_set_unused(flag, i) (flag[i>>5] &= ~(1U<<(i&0x1fU)))

#define __kh_fsize(m) ((m) < 32? 1 : (m)>>5)

static kh_inline khint_t __kh_splitmix32(khint_t *x) { khint_t z = (*x += 0x9e3779b9U); z = (z ^ (z >> 16)) * 0x21f0aaadU; z = (z ^ (z >> 15)) * 0x735a2d97U; return z ^ (z >> 15); }
static kh_inline khint_t __kh_h2b(khint_t hash, khint_t salt, khint_t bits) { return (hash ^ salt) * 2654435769U >> (32 - bits); } /* Fibonacci hashing */

/*******************
 * Hash table base *
 *******************/

#define __KHASHL_TYPE(HType, khkey_t) \
	typedef struct HType { \
		void *km; \
		unsigned short bits, salt; \
		khint_t count; \
		khint32_t *used; \
		khkey_t *keys; \
	} HType;

#define __KHASHL_PROTOTYPES(HType, prefix, khkey_t) \
	extern HType *prefix##_init(void); \
	extern HType *prefix##_init2(void *km); \
	extern void prefix##_destroy(HType *h); \
	extern void prefix##_clear(HType *h); \
	extern khint_t prefix##_getp(const HType *h, const khkey_t *key); \
	extern int prefix##_resize(HType *h, khint_t new_n_buckets); \
	extern khint_t prefix##_putp(HType *h, const khkey_t *key, int *absent); \
	extern void prefix##_del(HType *h, khint_t k);

#define __KHASHL_IMPL_BASIC(SCOPE, HType, prefix) \
	SCOPE HType *prefix##_init3(void *km, khint_t seed) { \
		HType *h = Kcalloc(km, HType, 1); \
		h->km = km; \
		if (seed != 0) h->salt = __kh_splitmix32(&seed); \
		return h; \
	} \
	SCOPE HType *prefix##_init2(void *km) { return prefix##_init3(km, 0); } \
	SCOPE HType *prefix##_init(void) { return prefix##_init2(0); } \
	SCOPE void prefix##_destroy(HType *h) { \
		if (!h) return; \
		Kfree(h->km, (void*)h->keys); Kfree(h->km, h->used); \
		Kfree(h->km, h); \
	} \
	SCOPE void prefix##_clear(HType *h) { \
		if (h && h->used) { \
			khint_t n_buckets = (khint_t)1U << h->bits; \
			memset(h->used, 0, __kh_fsize(n_buckets) * sizeof(khint32_t)); \
			h->count = 0; \
		} \
	}

#define __KHASHL_IMPL_GET(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE khint_t prefix##_getp_core(const HType *h, const khkey_t *key, khint_t hash) { \
		khint_t i, last, n_buckets, mask; \
		if (h->keys == 0) return 0; \
		n_buckets = (khint_t)1U << h->bits; \
		mask = n_buckets - 1U; \
		i = last = __kh_h2b(hash, h->salt, h->bits); \
		while (__kh_used(h->used, i) && !__hash_eq(h->keys[i], *key)) { \
			i = (i + 1U) & mask; \
			if (i == last) return n_buckets; \
		} \
		return !__kh_used(h->used, i)? n_buckets : i; \
	} \
	SCOPE khint_t prefix##_getp(const HType *h, const khkey_t *key) { return prefix##_getp_core(h, key, __hash_fn(*key)); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { return prefix##_getp_core(h, &key, __hash_fn(key)); }

#define __KHASHL_IMPL_RESIZE(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE int prefix##_resize(HType *h, khint_t new_n_buckets) { \
		khint32_t *new_used = 0; \
		khint_t j = 0, x = new_n_buckets, n_buckets, new_bits, new_mask; \
		while ((x >>= 1) != 0) ++j; \
		if (new_n_buckets & (new_n_buckets - 1)) ++j; \
		new_bits = j > 2? j : 2; \
		if (new_bits == h->bits) return 0; /* same size; no need to rehash */ \
		new_n_buckets = (khint_t)1U << new_bits; \
		if (h->count > kh_max_count(new_n_buckets)) return 0; /* requested size is too small */ \
		new_used = Kmalloc(h->km, khint32_t, __kh_fsize(new_n_buckets)); \
		if (!new_used) return -1; /* not enough memory */ \
		memset(new_used, 0, __kh_fsize(new_n_buckets) * sizeof(khint32_t)); \
		n_buckets = h->keys? (khint_t)1U<<h->bits : 0U; \
		if (n_buckets < new_n_buckets) { /* expand */ \
			khkey_t *new_keys = Krealloc(h->km, khkey_t, h->keys, new_n_buckets); \
			if (!new_keys) { Kfree(h->km, new_used); return -1; } \
			h->keys = new_keys; \
		} \
		new_mask = new_n_buckets - 1; \
		for (j = 0; j != n_buckets; ++j) { \
			khkey_t key; \
			if (!__kh_used(h->used, j)) continue; \
			key = h->keys[j]; \
			__kh_set_unused(h->used, j); \
			while (1) { /* kick-out process; sort of like in Cuckoo hashing */ \
				khint_t i; \
				i = __kh_h2b(__hash_fn(key), h->salt, new_bits); \
				while (__kh_used(new_used, i)) i = (i + 1U) & new_mask; \
				__kh_set_used(new_used, i); \
				if (i < n_buckets && __kh_used(h->used, i)) { /* kick out the existing element */ \
					{ khkey_t tmp = h->keys[i]; h->keys[i] = key; key = tmp; } \
					__kh_set_unused(h->used, i); /* mark it as deleted in the old hash table */ \
				} else { /* write the element and jump out of the loop */ \
					h->keys[i] = key; \
					break; \
				} \
			} \
		} \
		if (n_buckets > new_n_buckets) { /* shrink the hash table */ \
			khkey_t *new_keys = Krealloc(h->km, khkey_t, h->keys, new_n_buckets); \
			if (!new_keys) { Kfree(h->km, new_used); return -1; } \
			h->keys = new_keys; \
		} \
		Kfree(h->km, h->used); /* free the working space */ \
		h->used = new_used, h->bits = new_bits; \
		return 0; \
	}

#define __KHASHL_IMPL_PUT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	SCOPE khint_t prefix##_putp_core(HType *h, const khkey_t *key, khint_t hash, int *absent) { \
		khint_t n_buckets, i, last, mask; \
		n_buckets = h->keys? (khint_t)1U<<h->bits : 0U; \
		*absent = -1; \
		if (h->count >= kh_max_count(n_buckets)) { /* rehashing */ \
			if (prefix##_resize(h, n_buckets + 1U) < 0) \
				return n_buckets; \
			n_buckets = (khint_t)1U<<h->bits; \
		} /* TODO: to implement automatically shrinking; resize() already support shrinking */ \
		mask = n_buckets - 1; \
		i = last = __kh_h2b(hash, h->salt, h->bits); \
		while (__kh_used(h->used, i) && !__hash_eq(h->keys[i], *key)) { \
			i = (i + 1U) & mask; \
			if (i == last) break; \
		} \
		if (!__kh_used(h->used, i)) { /* not present at all */ \
			h->keys[i] = *key; \
			__kh_set_used(h->used, i); \
			++h->count; \
			*absent = 1; \
		} else *absent = 0; /* Don't touch h->keys[i] if present */ \
		return i; \
	} \
	SCOPE khint_t prefix##_putp(HType *h, const khkey_t *key, int *absent) { return prefix##_putp_core(h, key, __hash_fn(*key), absent); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { return prefix##_putp_core(h, &key, __hash_fn(key), absent); }

#define __KHASHL_IMPL_DEL(SCOPE, HType, prefix, khkey_t, __hash_fn) \
	SCOPE int prefix##_del(HType *h, khint_t i) { \
		khint_t j = i, k, mask, n_buckets; \
		if (h->keys == 0) return 0; \
		n_buckets = (khint_t)1U<<h->bits; \
		mask = n_buckets - 1U; \
		while (1) { \
			j = (j + 1U) & mask; \
			if (j == i || !__kh_used(h->used, j)) break; /* j==i only when the table is completely full */ \
			k = __kh_h2b(__hash_fn(h->keys[j]), h->salt, h->bits); \
			if ((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j))) \
				h->keys[i] = h->keys[j], i = j; \
		} \
		__kh_set_unused(h->used, i); \
		--h->count; \
		return 1; \
	}

#define KHASHL_DECLARE(HType, prefix, khkey_t) \
	__KHASHL_TYPE(HType, khkey_t) \
	__KHASHL_PROTOTYPES(HType, prefix, khkey_t)

#define KHASHL_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_TYPE(HType, khkey_t) \
	__KHASHL_IMPL_BASIC(SCOPE, HType, prefix) \
	__KHASHL_IMPL_GET(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_RESIZE(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_PUT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	__KHASHL_IMPL_DEL(SCOPE, HType, prefix, khkey_t, __hash_fn)

/***************************
 * Ensemble of hash tables *
 ***************************/

typedef struct {
	khint_t sub, pos;
} kh_ensitr_t;

#define KHASHE_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	KHASHL_INIT(KH_LOCAL, HType##_sub, prefix##_sub, khkey_t, __hash_fn, __hash_eq) \
	typedef struct HType { \
		void *km; \
		khint64_t count:54, bits:8; \
		HType##_sub *sub; \
	} HType; \
	SCOPE HType *prefix##_init3(void *km, int bits, khint_t seed) { \
		HType *g; \
		g = Kcalloc(km, HType, 1); \
		if (!g) return 0; \
		g->bits = bits, g->km = km; \
		g->sub = Kcalloc(km, HType##_sub, 1U<<bits); \
		if (seed != 0) { \
			khint_t i, rng = seed; \
			for (i = 0; i < 1U<<bits; ++i) \
				g->sub[i].salt = __kh_splitmix32(&rng); \
		} \
		return g; \
	} \
	SCOPE HType *prefix##_init2(void *km, int bits) { return prefix##_init3(km, bits, 0); } \
	SCOPE HType *prefix##_init(int bits) { return prefix##_init2(0, bits); } \
	SCOPE void prefix##_destroy(HType *g) { \
		int t; \
		if (!g) return; \
		for (t = 0; t < 1<<g->bits; ++t) { Kfree(g->km, (void*)g->sub[t].keys); Kfree(g->km, g->sub[t].used); } \
		Kfree(g->km, g->sub); Kfree(g->km, g); \
	} \
	SCOPE kh_ensitr_t prefix##_getp(const HType *g, const khkey_t *key) { \
		khint_t hash, low, ret; \
		kh_ensitr_t r; \
		HType##_sub *h; \
		hash = __hash_fn(*key); \
		low = hash & ((1U<<g->bits) - 1); \
		h = &g->sub[low]; \
		ret = prefix##_sub_getp_core(h, key, hash); \
		if (ret == kh_end(h)) r.sub = low, r.pos = (khint_t)-1; \
		else r.sub = low, r.pos = ret; \
		return r; \
	} \
	SCOPE kh_ensitr_t prefix##_get(const HType *g, const khkey_t key) { return prefix##_getp(g, &key); } \
	SCOPE kh_ensitr_t prefix##_putp(HType *g, const khkey_t *key, int *absent) { \
		khint_t hash, low, ret; \
		kh_ensitr_t r; \
		HType##_sub *h; \
		hash = __hash_fn(*key); \
		low = hash & ((1U<<g->bits) - 1); \
		h = &g->sub[low]; \
		ret = prefix##_sub_putp_core(h, key, hash, absent); \
		if (*absent) ++g->count; \
		r.sub = low, r.pos = ret; \
		return r; \
	} \
	SCOPE kh_ensitr_t prefix##_put(HType *g, const khkey_t key, int *absent) { return prefix##_putp(g, &key, absent); } \
	SCOPE int prefix##_del(HType *g, kh_ensitr_t itr) { \
		HType##_sub *h = &g->sub[itr.sub]; \
		int ret; \
		ret = prefix##_sub_del(h, itr.pos); \
		if (ret) --g->count; \
		return ret; \
	} \
	SCOPE void prefix##_clear(HType *g) { \
		int i; \
		for (i = 0; i < 1U<<g->bits; ++i) prefix##_sub_clear(&g->sub[i]); \
		g->count = 0; \
	} \
	SCOPE void prefix##_resize(HType *g, khint64_t new_n_buckets) { \
		khint_t j; \
		for (j = 0; j < 1U<<g->bits; ++j) \
			prefix##_sub_resize(&g->sub[j], new_n_buckets >> g->bits); \
	}

/*****************************
 * More convenient interface *
 *****************************/

/* common */

#define KHASHL_SET_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; } kh_packed HType##_s_bucket_t; \
	static kh_inline khint_t prefix##_s_hash(HType##_s_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_s_eq(HType##_s_bucket_t x, HType##_s_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_s, HType##_s_bucket_t, prefix##_s_hash, prefix##_s_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_s_init(); } \
	SCOPE HType *prefix##_init2(void *km) { return prefix##_s_init2(km); } \
	SCOPE HType *prefix##_init3(void *km, khint_t seed) { return prefix##_s_init3(km, seed); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_s_destroy(h); } \
	SCOPE void prefix##_resize(HType *h, khint_t new_n_buckets) { prefix##_s_resize(h, new_n_buckets); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_s_bucket_t t; t.key = key; return prefix##_s_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_s_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_s_bucket_t t; t.key = key; return prefix##_s_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_s_clear(h); }

#define KHASHL_MAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; } kh_packed HType##_m_bucket_t; \
	static kh_inline khint_t prefix##_m_hash(HType##_m_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_m_eq(HType##_m_bucket_t x, HType##_m_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_m, HType##_m_bucket_t, prefix##_m_hash, prefix##_m_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_m_init(); } \
	SCOPE HType *prefix##_init2(void *km) { return prefix##_m_init2(km); } \
	SCOPE HType *prefix##_init3(void *km, khint_t seed) { return prefix##_m_init3(km, seed); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_m_destroy(h); } \
	SCOPE void prefix##_resize(HType *h, khint_t new_n_buckets) { prefix##_m_resize(h, new_n_buckets); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_m_bucket_t t; t.key = key; return prefix##_m_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_m_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_m_bucket_t t; t.key = key; return prefix##_m_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_m_clear(h); }

/* cached hashes to trade memory for performance when hashing and comparison are expensive */

#define __kh_cached_hash(x) ((x).hash)

#define KHASHL_CSET_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; khint_t hash; } kh_packed HType##_cs_bucket_t; \
	static kh_inline int prefix##_cs_eq(HType##_cs_bucket_t x, HType##_cs_bucket_t y) { return x.hash == y.hash && __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_cs, HType##_cs_bucket_t, __kh_cached_hash, prefix##_cs_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_cs_init(); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_cs_destroy(h); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_cs_bucket_t t; t.key = key; t.hash = __hash_fn(key); return prefix##_cs_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_cs_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_cs_bucket_t t; t.key = key, t.hash = __hash_fn(key); return prefix##_cs_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_cs_clear(h); }

#define KHASHL_CMAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; khint_t hash; } kh_packed HType##_cm_bucket_t; \
	static kh_inline int prefix##_cm_eq(HType##_cm_bucket_t x, HType##_cm_bucket_t y) { return x.hash == y.hash && __hash_eq(x.key, y.key); } \
	KHASHL_INIT(KH_LOCAL, HType, prefix##_cm, HType##_cm_bucket_t, __kh_cached_hash, prefix##_cm_eq) \
	SCOPE HType *prefix##_init(void) { return prefix##_cm_init(); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_cm_destroy(h); } \
	SCOPE khint_t prefix##_get(const HType *h, khkey_t key) { HType##_cm_bucket_t t; t.key = key; t.hash = __hash_fn(key); return prefix##_cm_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, khint_t k) { return prefix##_cm_del(h, k); } \
	SCOPE khint_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_cm_bucket_t t; t.key = key, t.hash = __hash_fn(key); return prefix##_cm_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_cm_clear(h); }

/* ensemble for huge hash tables */

#define KHASHE_SET_INIT(SCOPE, HType, prefix, khkey_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; } kh_packed HType##_es_bucket_t; \
	static kh_inline khint_t prefix##_es_hash(HType##_es_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_es_eq(HType##_es_bucket_t x, HType##_es_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHE_INIT(KH_LOCAL, HType, prefix##_es, HType##_es_bucket_t, prefix##_es_hash, prefix##_es_eq) \
	SCOPE HType *prefix##_init(int bits) { return prefix##_es_init(bits); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_es_destroy(h); } \
	SCOPE void prefix##_resize(HType *h, khint64_t new_n_buckets) { prefix##_es_resize(h, new_n_buckets); } \
	SCOPE kh_ensitr_t prefix##_get(const HType *h, khkey_t key) { HType##_es_bucket_t t; t.key = key; return prefix##_es_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, kh_ensitr_t k) { return prefix##_es_del(h, k); } \
	SCOPE kh_ensitr_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_es_bucket_t t; t.key = key; return prefix##_es_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_es_clear(h); }

#define KHASHE_MAP_INIT(SCOPE, HType, prefix, khkey_t, kh_val_t, __hash_fn, __hash_eq) \
	typedef struct { khkey_t key; kh_val_t val; } kh_packed HType##_em_bucket_t; \
	static kh_inline khint_t prefix##_em_hash(HType##_em_bucket_t x) { return __hash_fn(x.key); } \
	static kh_inline int prefix##_em_eq(HType##_em_bucket_t x, HType##_em_bucket_t y) { return __hash_eq(x.key, y.key); } \
	KHASHE_INIT(KH_LOCAL, HType, prefix##_em, HType##_em_bucket_t, prefix##_em_hash, prefix##_em_eq) \
	SCOPE HType *prefix##_init(int bits) { return prefix##_em_init(bits); } \
	SCOPE void prefix##_destroy(HType *h) { prefix##_em_destroy(h); } \
	SCOPE void prefix##_resize(HType *h, khint64_t new_n_buckets) { prefix##_em_resize(h, new_n_buckets); } \
	SCOPE kh_ensitr_t prefix##_get(const HType *h, khkey_t key) { HType##_em_bucket_t t; t.key = key; return prefix##_em_getp(h, &t); } \
	SCOPE int prefix##_del(HType *h, kh_ensitr_t k) { return prefix##_em_del(h, k); } \
	SCOPE kh_ensitr_t prefix##_put(HType *h, khkey_t key, int *absent) { HType##_em_bucket_t t; t.key = key; return prefix##_em_putp(h, &t, absent); } \
	SCOPE void prefix##_clear(HType *h) { prefix##_em_clear(h); }

/**************************
 * Public macro functions *
 **************************/

#define kh_bucket(h, x) ((h)->keys[x])
#define kh_size(h) ((h)->count)
#define kh_capacity(h) ((h)->keys? 1U<<(h)->bits : 0U)
#define kh_end(h) kh_capacity(h)

#define kh_key(h, x) ((h)->keys[x].key)
#define kh_val(h, x) ((h)->keys[x].val)
#define kh_exist(h, x) __kh_used((h)->used, (x))

#define kh_foreach(h, x) for ((x) = 0; (x) != kh_end(h); ++(x)) if (kh_exist((h), (x)))

#define kh_ens_key(g, x) kh_key(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_val(g, x) kh_val(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_exist(g, x) kh_exist(&(g)->sub[(x).sub], (x).pos)
#define kh_ens_is_end(x) ((x).pos == (khint_t)-1)
#define kh_ens_size(g) ((g)->count)

#define kh_ens_foreach(g, x) for ((x).sub = 0; (x).sub != 1<<(g)->bits; ++(x).sub) for ((x).pos = 0; (x).pos != kh_end(&(g)->sub[(x).sub]); ++(x).pos) if (kh_ens_exist((g), (x)))

/**************************************
 * Common hash and equality functions *
 **************************************/

#define kh_eq_generic(a, b) ((a) == (b))
#define kh_eq_str(a, b) (strcmp((a), (b)) == 0)
#define kh_hash_dummy(x) ((khint_t)(x))

static kh_inline khint_t kh_hash_uint32(khint_t x) { /* murmur finishing */
	x ^= x >> 16;
	x *= 0x85ebca6bU;
	x ^= x >> 13;
	x *= 0xc2b2ae35U;
	x ^= x >> 16;
	return x;
}

static kh_inline khint_t kh_hash_uint64(khint64_t x) { /* splitmix64; see https://nullprogram.com/blog/2018/07/31/ for inversion */
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31;
	return (khint_t)x;
}

static kh_inline khint_t kh_hash_str(kh_cstr_t s) { /* FNV1a */
	khint_t h = 2166136261U;
	const unsigned char *t = (const unsigned char*)s;
	for (; *t; ++t)
		h ^= *t, h *= 16777619;
	return h;
}

static kh_inline khint_t kh_hash_bytes(int len, const unsigned char *s) {
	khint_t h = 2166136261U;
	int i;
	for (i = 0; i < len; ++i)
		h ^= s[i], h *= 16777619;
	return h;
}

#endif /* __AC_KHASHL_H */
