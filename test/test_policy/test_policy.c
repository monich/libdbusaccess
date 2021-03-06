/*
 * Copyright (C) 2017-2019 Jolla Ltd.
 * Copyright (C) 2017-2019 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_common.h"

#include "dbusaccess_parser_p.h"
#include "dbusaccess_policy.h"

static TestOpt test_opt;

#define V DA_POLICY_VERSION
#define VPLUS "2"

int
da_system_uid(
    const char* user)
{
    if (!g_strcmp0(user, "user")) {
        return 1;
    } else {
        return -1;
    }
}

int
da_system_gid(
    const char* group)
{
    if (!g_strcmp0(group, "group")) {
        return 1;
    } else {
        return -1;
    }
}

/*==========================================================================*
 * Null
 *==========================================================================*/

static
void
test_policy_null(
    void)
{
    static const DACred root = { 0, 0, NULL, 0, 0, 0 };
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    da_policy_unref(NULL);
    g_assert(!da_policy_ref(NULL));
    g_assert(da_policy_check(NULL, NULL, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(NULL, NULL, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(NULL, &user, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(NULL, &user, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_DENY);
    /* root access is allowed even if policy is NULL! */
    g_assert(da_policy_check(NULL, &root, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
 }

/*==========================================================================*
 * Broken
 *==========================================================================*/

static
void
test_policy_broken(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    static const DA_ACTION bar [] = {
        { "bar", 1, 0 },
        { NULL }
    };
    g_assert(!da_policy_new(NULL));
    g_assert(!da_policy_new(""));
    g_assert(!da_policy_new(" "));
    g_assert(!da_policy_new("0|"));
    g_assert(!da_policy_new("0"));
    g_assert(!da_policy_new("0.0"));
    g_assert(!da_policy_new(" 0 "));
    g_assert(!da_policy_new("x"));
    g_assert(!da_policy_new("0123456789;"));
    g_assert(!da_policy_new(V "|+"));
    g_assert(!da_policy_new(V ";+?=*"));
    g_assert(!da_policy_new(VPLUS ";user(user:group)"));
    g_assert(!da_policy_new_full(V ";foo()", foo));
    g_assert(!da_policy_new_full(V ";bar()", foo));
    g_assert(!da_policy_new_full(V ";bar(*)", bar));
}

/*==========================================================================*
 * Basic
 *==========================================================================*/

static
void
test_policy_basic(
    void)
{
    static const DA_ACTION actions [] = {
        { "call", 1, 1 },
        { "arg", 1, 1 },
        { NULL }
    };
    /* Trivial case */
    DAPolicy* policy = da_policy_new(V);
    g_assert(policy);
    g_assert(da_policy_ref(policy) == policy);
    da_policy_unref(policy);
    da_policy_unref(policy);

#define BASIC_RULE1 "user(user:group) & call(foo) & " \
        "(arg('\\'a\\'')|arg(\"\\\"b\\\"\"))=deny; user(baduser)=allow; " \
        "group(badgroup)=allow"

    da_policy_unref(policy = da_policy_new_full(V ";" BASIC_RULE1, actions));
    g_assert(policy);

    da_policy_unref(policy = da_policy_new_full(BASIC_RULE1, actions));
    g_assert(policy);

#define BASIC_RULE2 "user(user) & call('foo')=deny;" \
        "group(*) & call(call) = deny;" \
        "user(22) & call(call)"

    da_policy_unref(policy = da_policy_new_full(V ";" BASIC_RULE2, actions));
    g_assert(policy);

    da_policy_unref(policy = da_policy_new_full(BASIC_RULE2, actions));
    g_assert(policy);

#define BASIC_RULE3 "user(*) & !group(222) = deny; "

    da_policy_unref(policy = da_policy_new_full(V ";" BASIC_RULE3, actions));
    g_assert(policy);

    da_policy_unref(policy = da_policy_new_full(BASIC_RULE3, actions));
    g_assert(policy);
}

/*==========================================================================*
 * Groups
 *==========================================================================*/

static
void
test_policy_groups(
    void)
{
    static const gid_t g21 [] = { 2, 1 };
    static const gid_t g23 [] = { 2, 3 };
    static const gid_t g43 [] = { 4, 3 };
    static const DACred user123 = { 1, 1, g23, G_N_ELEMENTS(g23), 0, 0 };
    static const DACred user321 = { 1, 3, g21, G_N_ELEMENTS(g21), 0, 0 };
    static const DACred user543 = { 1, 5, g43, G_N_ELEMENTS(g43), 0, 0 };
    DAPolicy* policy = da_policy_new(V "; group(1) | group(2) | "
        "user(baduser:badgroup) = deny");

    g_assert(policy);
    g_assert(da_policy_check(policy, &user123, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user321, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user543, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user543, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_DENY);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Equal1
 *==========================================================================*/

static
void
test_policy_equal1(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(*)&bar()=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";bar()&foo(*)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(da_policy_equal(NULL, NULL));
    g_assert(!da_policy_equal(p1, NULL));
    g_assert(!da_policy_equal(NULL, p2));
    g_assert(da_policy_equal(p1, p1));
    g_assert(da_policy_equal(p1, p2));
    g_assert(da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal2
 *==========================================================================*/

static
void
test_policy_equal2(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(*)&bar()=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";bar()&foo(*)=deny", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal3
 *==========================================================================*/

static
void
test_policy_equal3(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(*)&bar()=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";bar()&foo(*)=allow;*=deny", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal4
 *==========================================================================*/

static
void
test_policy_equal4(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(a)&bar()=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";bar()&foo(b)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal5
 *==========================================================================*/

static
void
test_policy_equal5(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(a)|bar()=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";foo(a)|foo(b)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal6
 *==========================================================================*/

static
void
test_policy_equal6(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 1 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(*)|bar(b)=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";foo(a)|bar(*)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal7
 *==========================================================================*/

static
void
test_policy_equal7(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { "bar", 2, 0 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(*)=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";bar()=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal8
 *==========================================================================*/

static
void
test_policy_equal8(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";foo(a)=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";!foo(a)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal9
 *==========================================================================*/

static
void
test_policy_equal9(
    void)
{
    static const DA_ACTION actions [] = {
        { "foo", 1, 1 },
        { NULL }
    };

    DAPolicy* p1 = da_policy_new_full(V ";!foo(a)=allow", actions);
    DAPolicy* p2 = da_policy_new_full(V ";!foo(b)=allow", actions);
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal10
 *==========================================================================*/

static
void
test_policy_equal10(
    void)
{
    DAPolicy* p1 = da_policy_new(V ";user(user)=allow");
    DAPolicy* p2 = da_policy_new(V ";group(group)=allow");
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal11
 *==========================================================================*/

static
void
test_policy_equal11(
    void)
{
    DAPolicy* p1 = da_policy_new(V ";*=allow");
    DAPolicy* p2 = da_policy_new(V ";group(group)=allow");
    g_assert(p1);
    g_assert(p2);
    g_assert(!da_policy_equal(p1, p2));
    g_assert(!da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Equal12
 *==========================================================================*/

static
void
test_policy_equal12(
    void)
{
    DAPolicy* p1 = da_policy_new(V ";*=allow");
    DAPolicy* p2 = da_policy_new(V ";*=allow");
    g_assert(p1);
    g_assert(p2);
    g_assert(da_policy_equal(p1, p2));
    g_assert(da_policy_equal(p2, p1));
    da_policy_unref(p1);
    da_policy_unref(p2);
}

/*==========================================================================*
 * Check 1
 *==========================================================================*/

static
void
test_policy_check1(
    void)
{
    DAPolicy* policy = da_policy_new(V ";*=allow");
    static const DACred root = { 0, 0, NULL, 0, 0, 0 };
    static const DACred user = { 100, 100, NULL, 0, 0, 0 };
    g_assert(policy);
    /* Root should still have the access */
    g_assert(da_policy_check(policy, &root, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, NULL, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 2
 *==========================================================================*/

static
void
test_policy_check2(
    void)
{
    DAPolicy* policy = da_policy_new(V ";*=deny");
    static const DACred root = { 0, 0, NULL, 0, 0, 0 };
    static const DACred user = { 100, 100, NULL, 0, 0, 0 };
    g_assert(policy);
    /* Root should still have the access */
    g_assert(da_policy_check(policy, &root, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 3
 *==========================================================================*/

static
void
test_policy_check3(
    void)
{
    DAPolicy* policy = da_policy_new(V ";*=deny;user(1:1)=allow");
    static const DACred user11 = { 1, 1, NULL, 0, 0, 0 };
    static const DACred user12 = { 1, 2, NULL, 0, 0, 0 };
    static const DACred user21 = { 2, 1, NULL, 0, 0, 0 };
    static const DACred user22 = { 2, 2, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user11, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user12, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user21, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user22, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 4
 *==========================================================================*/

static
void
test_policy_check4(
    void)
{
    DAPolicy* policy = da_policy_new(V ";*=deny;user(1)=allow");
    static const DACred user11 = { 1, 1, NULL, 0, 0, 0 };
    static const DACred user22 = { 2, 2, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user11, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user22, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, NULL, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 5
 *==========================================================================*/

static
void
test_policy_check5(
    void)
{
    DAPolicy* policy = da_policy_new(V ";*=deny;group(1)=allow");
    static const DACred user11 = { 1, 1, NULL, 0, 0, 0 };
    static const DACred user22 = { 2, 2, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user11, 0, NULL, DA_ACCESS_DENY) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user22, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, NULL, 0, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 6
 *==========================================================================*/

static
void
test_policy_check6(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 0 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";foo()=deny", foo);
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user, 1, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 2, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 7
 *==========================================================================*/

static
void
test_policy_check7(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";foo(a)=deny", foo);
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user, 1, "a", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "b", DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user, 2, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    da_policy_unref(policy);
}

/*==========================================================================*
 * Check 8
 *==========================================================================*/

static
void
test_policy_check8(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";foo(*)=deny", foo);
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user, 1, "a", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "b", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, NULL, DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
     da_policy_unref(policy);
}

/*==========================================================================*
 * Check 9
 *==========================================================================*/

static
void
test_policy_check9(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";foo(a*)|foo(b*)=deny", foo);
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user, 1, "aa", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "ba", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "c", DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
     da_policy_unref(policy);
}

/*==========================================================================*
 * Check 10
 *==========================================================================*/

static
void
test_policy_check10(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";foo(a*)|foo(b*)=deny", foo);
    static const DACred user = { 1, 1, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user, 1, "aa", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "ba", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user, 1, "c", DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
     da_policy_unref(policy);
}

/*==========================================================================*
 * Check 11
 *==========================================================================*/

static
void
test_policy_check11(
    void)
{
    static const DA_ACTION foo [] = {
        { "foo", 1, 1 },
        { NULL }
    };
    DAPolicy* policy = da_policy_new_full(V ";user(1) & !foo(a)=deny", foo);
    static const DACred user1 = { 1, 1, NULL, 0, 0, 0 };
    static const DACred user2 = { 2, 2, NULL, 0, 0, 0 };
    g_assert(policy);
    g_assert(da_policy_check(policy, &user1, 1, "a", DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
    g_assert(da_policy_check(policy, &user1, 1, "b", DA_ACCESS_ALLOW) ==
        DA_ACCESS_DENY);
    g_assert(da_policy_check(policy, &user2, 1, "a", DA_ACCESS_ALLOW) ==
        DA_ACCESS_ALLOW);
     da_policy_unref(policy);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/policy/"

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_PREFIX "null", test_policy_null);
    g_test_add_func(TEST_PREFIX "broken", test_policy_broken);
    g_test_add_func(TEST_PREFIX "basic", test_policy_basic);
    g_test_add_func(TEST_PREFIX "groups", test_policy_groups);
    g_test_add_func(TEST_PREFIX "equal1", test_policy_equal1);
    g_test_add_func(TEST_PREFIX "equal2", test_policy_equal2);
    g_test_add_func(TEST_PREFIX "equal3", test_policy_equal3);
    g_test_add_func(TEST_PREFIX "equal4", test_policy_equal4);
    g_test_add_func(TEST_PREFIX "equal5", test_policy_equal5);
    g_test_add_func(TEST_PREFIX "equal6", test_policy_equal6);
    g_test_add_func(TEST_PREFIX "equal7", test_policy_equal7);
    g_test_add_func(TEST_PREFIX "equal8", test_policy_equal8);
    g_test_add_func(TEST_PREFIX "equal9", test_policy_equal9);
    g_test_add_func(TEST_PREFIX "equal10", test_policy_equal10);
    g_test_add_func(TEST_PREFIX "equal11", test_policy_equal11);
    g_test_add_func(TEST_PREFIX "equal12", test_policy_equal12);
    g_test_add_func(TEST_PREFIX "check1", test_policy_check1);
    g_test_add_func(TEST_PREFIX "check2", test_policy_check2);
    g_test_add_func(TEST_PREFIX "check3", test_policy_check3);
    g_test_add_func(TEST_PREFIX "check4", test_policy_check4);
    g_test_add_func(TEST_PREFIX "check5", test_policy_check5);
    g_test_add_func(TEST_PREFIX "check6", test_policy_check6);
    g_test_add_func(TEST_PREFIX "check7", test_policy_check7);
    g_test_add_func(TEST_PREFIX "check8", test_policy_check8);
    g_test_add_func(TEST_PREFIX "check9", test_policy_check9);
    g_test_add_func(TEST_PREFIX "check10", test_policy_check10);
    g_test_add_func(TEST_PREFIX "check11", test_policy_check11);
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
