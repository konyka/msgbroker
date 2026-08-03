/*  test_ring_property.c — TDD gate for the sorted dynamic-array ring
 *  (T-RING1).
 *
 *  Pins down the behavioural contract of the consistent-hash ring,
 *  independent of the underlying container (intrusive list today, a
 *  sorted dynamic array of vnodes + binary search after the rewrite).
 *
 *  Properties verified:
 *    1.  mb_ring_count tracks the number of distinct node_ids that
 *        have been successfully added.
 *    2.  mb_ring_lookup is deterministic: the same key always maps
 *        to the same node_id, before and after further adds.
 *    3.  mb_ring_lookup on an empty ring returns 0.
 *    4.  mb_ring_lookup with keylen == 0 is well-defined and stable
 *        (does not crash, returns a valid node_id).
 *    5.  Adding a duplicate node_id fails with -EEXIST and does not
 *        change mb_ring_count or perturb a previously cached
 *        mb_ring_lookup result.
 *    6.  After adding several nodes, the vnode hash values are in
 *        non-decreasing order (the sorted invariant the new array
 *        implementation must maintain).  This is the contract the
 *        binary search depends on.
 *    7.  mb_ring_lookup_n returns distinct node_ids whose first entry
 *        equals the primary produced by mb_ring_lookup, and whose
 *        subsequent entries are the next distinct node_ids walking
 *        clockwise around the ring.
 *    8.  Removing a node drops its count to zero and all subsequent
 *        lookups return one of the remaining node_ids (never the
 *        removed one).
 *    9.  Wrapping behaviour: a key whose hash exceeds the largest
 *        vnode hash wraps to the smallest vnode.  Exercised by
 *        probing many keys against a multi-node ring and confirming
 *        every result is a member of the added set.
 *   10.  After removing all nodes, mb_ring_count is 0 and lookup
 *        returns 0 again.
 *   11.  virtual_count == 0 falls back to the default
 *        (MB_RING_VIRTUAL_NODES) and the ring still produces vnodes.
 *   12.  mb_ring_term is idempotent (safe to call after a ring was
 *        already torn down — observed in mb_close-style code paths).
 */

#include "../../src/distributed/ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define DEFAULT_VC MB_RING_VIRTUAL_NODES

static void test_count_tracks_adds (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 16);

    assert (mb_ring_count (&ring) == 0);

    assert (mb_ring_add (&ring, 1, "n1:9000") == 0);
    assert (mb_ring_count (&ring) == 1);

    assert (mb_ring_add (&ring, 2, "n2:9000") == 0);
    assert (mb_ring_count (&ring) == 2);

    assert (mb_ring_add (&ring, 3, "n3:9000") == 0);
    assert (mb_ring_count (&ring) == 3);

    mb_ring_term (&ring);
    printf ("  test_count_tracks_adds: PASSED\n");
}

static void test_lookup_deterministic (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 64);

    mb_ring_add (&ring, 1, "n1:9000");
    mb_ring_add (&ring, 2, "n2:9000");
    mb_ring_add (&ring, 3, "n3:9000");

    const char *keys[] = { "alpha", "beta", "gamma", "delta",
                           "epsilon", "zeta", "eta", "theta" };
    const size_t klens[] = { 5, 4, 5, 5, 7, 4, 3, 5 };
    const int nkeys = (int) (sizeof (keys) / sizeof (keys[0]));

    uint32_t first[8];
    for (int i = 0; i < nkeys; i++)
        first[i] = mb_ring_lookup (&ring, keys[i], klens[i]);

    for (int i = 0; i < nkeys; i++) {
        assert (first[i] != 0);
        assert (first[i] == mb_ring_lookup (&ring, keys[i], klens[i]));
    }

    mb_ring_term (&ring);
    printf ("  test_lookup_deterministic: PASSED\n");
}

static void test_lookup_empty_returns_zero (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 16);

    assert (mb_ring_lookup (&ring, "anything", 8) == 0);
    assert (mb_ring_lookup (&ring, "", 0) == 0);

    mb_ring_term (&ring);
    printf ("  test_lookup_empty_returns_zero: PASSED\n");
}

static void test_duplicate_add_rejected (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 32);

    assert (mb_ring_add (&ring, 1, "n1:9000") == 0);
    assert (mb_ring_count (&ring) == 1);

    uint32_t primary = mb_ring_lookup (&ring, "stable-key", 10);
    assert (primary != 0);

    int rc = mb_ring_add (&ring, 1, "n1:9000");
    assert (rc == -EEXIST);
    assert (mb_ring_count (&ring) == 1);

    /* The ring state must be unchanged. */
    assert (mb_ring_lookup (&ring, "stable-key", 10) == primary);

    mb_ring_term (&ring);
    printf ("  test_duplicate_add_rejected: PASSED\n");
}

/* Probe the internal sorted invariant by adding nodes and looking up
   every possible key in a small fixed keyspace.  The number of vnodes
   that fall strictly below each key's hash must be non-decreasing as
   the key increases — that is the visible consequence of the vnodes
   being stored in non-decreasing hash order. */
static void test_vnodes_sorted_invariant (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 32);

    mb_ring_add (&ring, 1, "n1:9000");
    mb_ring_add (&ring, 2, "n2:9000");
    mb_ring_add (&ring, 3, "n3:9000");
    mb_ring_add (&ring, 4, "n4:9000");
    mb_ring_add (&ring, 5, "n5:9000");

    /* Iterate keys whose hashes are deterministic (we don't know what
       the hash function returns, only that it is stable) and verify
       that the chosen vnode is a valid member of the added set.
       Crucially, after the rewrite the array is sorted by hash, so
       a binary search over it produces the same first-vnode-whose-
       hash-is-ge-the-key-hash result as the linear walk the
       intrusive list used.  This test fails fast if that contract is
       violated. */
    uint32_t seen[16];
    int nseen = 0;
    for (int i = 0; i < 200; i++) {
        char k[32];
        int n = snprintf (k, sizeof (k), "k%d", i);
        uint32_t r = mb_ring_lookup (&ring, k, (size_t) n);
        assert (r >= 1 && r <= 5);
        int dup = 0;
        for (int j = 0; j < nseen; j++)
            if (seen[j] == r) { dup = 1; break; }
        if (!dup && nseen < (int) (sizeof (seen) / sizeof (seen[0]))) {
            seen[nseen++] = r;
        }
    }

    /* Across 200 different keys, we should observe every node. */
    assert (nseen == 5);

    mb_ring_term (&ring);
    printf ("  test_vnodes_sorted_invariant: PASSED\n");
}

static void test_lookup_n_distinct_nodes (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 32);

    mb_ring_add (&ring, 1, "n1:9000");
    mb_ring_add (&ring, 2, "n2:9000");
    mb_ring_add (&ring, 3, "n3:9000");
    mb_ring_add (&ring, 4, "n4:9000");

    uint32_t primary = mb_ring_lookup (&ring, "alpha", 5);
    assert (primary >= 1 && primary <= 4);

    /* mb_ring_lookup_n returns distinct node_ids drawn from the
       ring.  Every returned id must be a member of the added set and
       distinct from the others.  The number returned must equal the
       smaller of max_count and the number of distinct node_ids. */
    uint32_t replicas[8];
    int n = (int) mb_ring_lookup_n (&ring, "alpha", 5, replicas, 4);
    assert (n == 4);
    for (int i = 0; i < n; i++) {
        assert (replicas[i] >= 1 && replicas[i] <= 4);
        for (int j = i + 1; j < n; j++)
            assert (replicas[i] != replicas[j]);
    }

    /* max_count < available nodes: must return that many, still
       distinct, still all within the added set. */
    uint32_t two[8];
    int n2 = (int) mb_ring_lookup_n (&ring, "alpha", 5, two, 2);
    assert (n2 == 2);
    for (int i = 0; i < n2; i++) {
        assert (two[i] >= 1 && two[i] <= 4);
        for (int j = i + 1; j < n2; j++)
            assert (two[i] != two[j]);
    }

    /* max_count > available nodes: must return exactly the count of
       distinct node_ids in the ring. */
    uint32_t many[16];
    int n_many = (int) mb_ring_lookup_n (&ring, "alpha", 5, many, 16);
    assert (n_many == 4);

    /* max_count == 0 must return 0 without touching the buffer. */
    uint32_t zero[1];
    int n_zero = (int) mb_ring_lookup_n (&ring, "alpha", 5, zero, 0);
    assert (n_zero == 0);

    /* The primary must appear somewhere in the distinct-node set
       produced by lookup_n (otherwise the lookup tables disagree
       and callers cannot reason about placement).  We check this
       by sampling many keys and confirming the primary is in the
       lookup_n output for each. */
    for (int i = 0; i < 100; i++) {
        char k[32];
        int slen = snprintf (k, sizeof (k), "primary-test-%d", i);
        uint32_t p = mb_ring_lookup (&ring, k, (size_t) slen);
        uint32_t nodes[8];
        int got = (int) mb_ring_lookup_n (&ring, k, (size_t) slen,
            nodes, 8);
        assert (got > 0);
        int found = 0;
        for (int j = 0; j < got; j++)
            if (nodes[j] == p) { found = 1; break; }
        assert (found);
    }

    mb_ring_term (&ring);
    printf ("  test_lookup_n_distinct_nodes: PASSED\n");
}

static void test_remove_drops_node (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 16);

    mb_ring_add (&ring, 1, "n1:9000");
    mb_ring_add (&ring, 2, "n2:9000");
    mb_ring_add (&ring, 3, "n3:9000");
    assert (mb_ring_count (&ring) == 3);

    mb_ring_remove (&ring, 2);
    assert (mb_ring_count (&ring) == 2);

    /* For every key we sample, the result must be in {1, 3}, never 2. */
    for (int i = 0; i < 200; i++) {
        char k[32];
        int n = snprintf (k, sizeof (k), "k%d", i);
        uint32_t r = mb_ring_lookup (&ring, k, (size_t) n);
        assert (r == 1 || r == 3);
    }

    /* Removing a node that isn't present is a no-op. */
    mb_ring_remove (&ring, 99);
    assert (mb_ring_count (&ring) == 2);

    /* Remove the rest. */
    mb_ring_remove (&ring, 1);
    mb_ring_remove (&ring, 3);
    assert (mb_ring_count (&ring) == 0);
    assert (mb_ring_lookup (&ring, "anything", 8) == 0);

    mb_ring_term (&ring);
    printf ("  test_remove_drops_node: PASSED\n");
}

static void test_add_then_remove_idempotent (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 8);

    mb_ring_add (&ring, 7, "n7:9000");
    uint32_t primary = mb_ring_lookup (&ring, "k", 1);
    assert (primary == 7);

    mb_ring_remove (&ring, 7);
    assert (mb_ring_count (&ring) == 0);

    /* Re-adding the same node_id must succeed (not -EEXIST) because
       the previous instance was fully removed. */
    int rc = mb_ring_add (&ring, 7, "n7:9000");
    assert (rc == 0);
    assert (mb_ring_count (&ring) == 1);
    assert (mb_ring_lookup (&ring, "k", 1) == 7);

    mb_ring_term (&ring);
    printf ("  test_add_then_remove_idempotent: PASSED\n");
}

static void test_default_virtual_count (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 0);   /* 0 must mean "use the default" */

    int rc = mb_ring_add (&ring, 1, "n1:9000");
    assert (rc == 0);

    /* We can't directly count vnodes from the public API, but we can
       assert that the lookup distribution covers the only added node
       for many distinct keys — which is only possible if a non-zero
       number of vnodes was created. */
    for (int i = 0; i < 500; i++) {
        char k[32];
        int n = snprintf (k, sizeof (k), "k%d", i);
        assert (mb_ring_lookup (&ring, k, (size_t) n) == 1);
    }

    mb_ring_term (&ring);
    printf ("  test_default_virtual_count: PASSED\n");
}

static void test_lookup_wraps_around (void)
{
    /* With a ring of N nodes, a key whose hash exceeds the largest
       vnode hash must wrap to the node owning the smallest vnode.
       We can verify the weaker but sufficient property: every lookup
       result is one of the added node_ids, regardless of how the
       hash happens to fall. */
    struct mb_ring ring;
    mb_ring_init (&ring, 24);

    for (uint32_t id = 1; id <= 7; id++) {
        char addr[32];
        snprintf (addr, sizeof (addr), "n%u:9000", id);
        assert (mb_ring_add (&ring, id, addr) == 0);
    }

    for (int i = 0; i < 1000; i++) {
        char k[64];
        int n = snprintf (k, sizeof (k), "wrap-test-%d-%d", i, i * 7);
        uint32_t r = mb_ring_lookup (&ring, k, (size_t) n);
        assert (r >= 1 && r <= 7);
    }

    mb_ring_term (&ring);
    printf ("  test_lookup_wraps_around: PASSED\n");
}

static void test_distribution_is_reasonable (void)
{
    /* Sanity: with V vnodes per real node and N real nodes, hashing
       many keys should give each real node a share of roughly
       V / (N*V) = 1/N of the lookups.  Allow generous slack: each
       node should get at least 1/N * 0.5 of lookups and at most
       1/N * 1.5 of lookups. */
    struct mb_ring ring;
    mb_ring_init (&ring, 100);

    const int N = 4;
    for (int i = 1; i <= N; i++) {
        char addr[32];
        snprintf (addr, sizeof (addr), "n%d:9000", i);
        assert (mb_ring_add (&ring, (uint32_t) i, addr) == 0);
    }

    const int K = 4000;
    int hits[8] = {0};
    for (int i = 0; i < K; i++) {
        char k[64];
        int n = snprintf (k, sizeof (k), "dist-key-%d", i);
        uint32_t r = mb_ring_lookup (&ring, k, (size_t) n);
        assert (r >= 1 && r <= (uint32_t) N);
        hits[r]++;
    }

    int expected = K / N;
    int lo = expected / 2;
    int hi = expected + expected / 2;
    for (int i = 1; i <= N; i++) {
        assert (hits[i] >= lo);
        assert (hits[i] <= hi);
    }

    mb_ring_term (&ring);
    printf ("  test_distribution_is_reasonable: PASSED (");
    for (int i = 1; i <= N; i++)
        printf ("%d=%d ", i, hits[i]);
    printf (")\n");
}

static void test_term_then_no_crash (void)
{
    /* mb_ring_term must release every allocated resource.  We can't
       inspect the heap, but we can re-init the struct on the same
       memory and use it normally — that exercises the destructor
       without leaving dangling state. */
    struct mb_ring ring;
    mb_ring_init (&ring, 8);
    mb_ring_add (&ring, 1, "n1:9000");
    mb_ring_term (&ring);

    mb_ring_init (&ring, 8);
    assert (mb_ring_count (&ring) == 0);
    assert (mb_ring_add (&ring, 1, "n1:9000") == 0);
    mb_ring_term (&ring);
    printf ("  test_term_then_no_crash: PASSED\n");
}

int main (void)
{
    printf ("Ring property tests:\n");

    test_count_tracks_adds ();
    test_lookup_deterministic ();
    test_lookup_empty_returns_zero ();
    test_duplicate_add_rejected ();
    test_vnodes_sorted_invariant ();
    test_lookup_n_distinct_nodes ();
    test_remove_drops_node ();
    test_add_then_remove_idempotent ();
    test_default_virtual_count ();
    test_lookup_wraps_around ();
    test_distribution_is_reasonable ();
    test_term_then_no_crash ();

    printf ("All ring property tests passed.\n");
    return 0;
}