/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2011-2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <check.h>

#include "copse/cps.h"
#include "copse/round-robin.h"

#include "helpers.h"


/*-----------------------------------------------------------------------
 * Test continuations
 */

struct save_int {
    struct cps_cont  parent;
    unsigned int  *dest;
    unsigned int  value;
    unsigned int  run_count;
};

static int
save_int__resume(struct cps_cont *cont, struct cps_cont *next)
{
    struct save_int  *self = cps_container_of(cont, struct save_int, parent);
    *self->dest = self->value;
    self->run_count++;
    return cps_return(next);
}

#define SAVE_INT_INIT(d, v)  { { save_int__resume }, (d), (v), 0 }

static void
save_int_verify(struct save_int *i)
{
    fail_unless_equal("Continuation result", "%u", i->value, *i->dest);
    fail_unless_equal("Continuation run count", "%u", 1, i->run_count);
}


struct save_int2 {
    struct cps_cont  step1;
    unsigned int  *dest1;
    unsigned int  value1;

    struct cps_cont  step2;
    unsigned int  *dest2;
    unsigned int  value2;

    unsigned int  run_count;
};

static int
save_int2__step1(struct cps_cont *cont, struct cps_cont *next)
{
    struct save_int2  *self = cps_container_of(cont, struct save_int2, step1);
    *self->dest1 = self->value1;
    self->run_count++;
    return cps_resume(next, &self->step2);
}

static int
save_int2__step2(struct cps_cont *cont, struct cps_cont *next)
{
    struct save_int2  *self = cps_container_of(cont, struct save_int2, step2);
    *self->dest2 = self->value2;
    self->run_count++;
    return cps_return(next);
}

#define SAVE_INT2_INIT(d1, v1, d2, v2) \
    { { save_int2__step1 }, (d1), (v1), { save_int2__step2 }, (d2), (v2), 0 }

static void
save_int2_verify1(struct save_int2 *i)
{
    fail_unless_equal("Continuation result #1", "%u", i->value1, *i->dest1);
    fail_unless_equal("Continuation run count", "%u", 1, i->run_count);
}

static void
save_int2_verify2(struct save_int2 *i)
{
    fail_unless_equal("Continuation result #1", "%u", i->value1, *i->dest1);
    fail_unless_equal("Continuation result #2", "%u", i->value2, *i->dest2);
    fail_unless_equal("Continuation run count", "%u", 2, i->run_count);
}


/*-----------------------------------------------------------------------
 * Simple continuation passing
 */

START_TEST(test_cps_01)
{
    DESCRIBE_TEST;
    unsigned int  result = 0;
    struct save_int  i = SAVE_INT_INIT(&result, 10);
    fail_if(cps_run(&i.parent), "Error running continuations");
    save_int_verify(&i);
}
END_TEST

START_TEST(test_cps_02)
{
    DESCRIBE_TEST;
    unsigned int  result1 = 0;
    unsigned int  result2 = 0;
    struct save_int  i1 = SAVE_INT_INIT(&result1, 10);
    struct save_int  i2 = SAVE_INT_INIT(&result2, 20);
    fail_if(cps_resume(&i1.parent, &i2.parent), "Error running continuations");
    save_int_verify(&i1);
    save_int_verify(&i2);
}
END_TEST

START_TEST(test_cps_03)
{
    DESCRIBE_TEST;
    unsigned int  result1 = 0;
    unsigned int  result2 = 0;
    unsigned int  result3 = 0;
    struct save_int  i1 = SAVE_INT_INIT(&result1, 10);
    struct save_int  i2 = SAVE_INT_INIT(&result2, 20);
    struct save_int  i3 = SAVE_INT_INIT(&result3, 30);
    struct cps_rr  *rr = cps_rr_new();
    cps_rr_add(rr, &i1.parent);
    cps_rr_add(rr, &i2.parent);
    cps_rr_add(rr, &i3.parent);
    fail_if(cps_rr_drain(rr), "Error running continuations");
    save_int_verify(&i1);
    save_int_verify(&i2);
    save_int_verify(&i3);
    cps_rr_free(rr);
}
END_TEST

START_TEST(test_cps_04)
{
    DESCRIBE_TEST;
    unsigned int  result1a = 0;
    unsigned int  result1b = 0;
    unsigned int  result2a = 0;
    unsigned int  result2b = 0;
    unsigned int  result3a = 0;
    unsigned int  result3b = 0;
    struct save_int2  i1 = SAVE_INT2_INIT(&result1a, 10, &result1b, 15);
    struct save_int2  i2 = SAVE_INT2_INIT(&result2a, 20, &result2b, 25);
    struct save_int2  i3 = SAVE_INT2_INIT(&result3a, 30, &result3b, 35);
    struct cps_rr  *rr = cps_rr_new();
    cps_rr_add(rr, &i1.step1);
    cps_rr_add(rr, &i2.step1);
    cps_rr_add(rr, &i3.step1);
    fail_if(cps_rr_drain(rr), "Error running continuations");
    save_int2_verify2(&i1);
    save_int2_verify2(&i2);
    save_int2_verify2(&i3);
    cps_rr_free(rr);
}
END_TEST

START_TEST(test_cps_05)
{
    DESCRIBE_TEST;
    unsigned int  result1 = 0;
    unsigned int  result2a = 0;
    unsigned int  result2b = 0;
    unsigned int  result3 = 0;
    struct save_int  i1 = SAVE_INT_INIT(&result1, 10);
    struct save_int2  i2 = SAVE_INT2_INIT(&result2a, 20, &result2b, 25);
    struct save_int  i3 = SAVE_INT_INIT(&result3, 30);
    struct cps_rr  *rr = cps_rr_new();
    cps_rr_add(rr, &i1.parent);
    cps_rr_add(rr, &i2.step1);
    cps_rr_add(rr, &i3.parent);
    fail_if(cps_rr_drain(rr), "Error running continuations");
    save_int_verify(&i1);
    save_int2_verify2(&i2);
    save_int_verify(&i3);
    cps_rr_free(rr);
}
END_TEST

START_TEST(test_cps_06)
{
    DESCRIBE_TEST;
    unsigned int  result1a = 0;
    unsigned int  result1b = 0;
    unsigned int  result2a = 0;
    unsigned int  result2b = 0;
    unsigned int  result3a = 0;
    unsigned int  result3b = 0;
    struct save_int2  i1 = SAVE_INT2_INIT(&result1a, 10, &result1b, 15);
    struct save_int2  i2 = SAVE_INT2_INIT(&result2a, 20, &result2b, 25);
    struct save_int2  i3 = SAVE_INT2_INIT(&result3a, 30, &result3b, 35);
    struct cps_rr  *rr = cps_rr_new();
    cps_rr_add(rr, &i1.step1);
    cps_rr_add(rr, &i2.step1);
    cps_rr_add(rr, &i3.step1);
    fail_if(cps_rr_run_one_lap(rr), "Error running continuations");
    save_int2_verify1(&i1);
    save_int2_verify1(&i2);
    save_int2_verify1(&i3);
    fail_if(cps_rr_run_one_lap(rr), "Error running continuations");
    save_int2_verify2(&i1);
    save_int2_verify2(&i2);
    save_int2_verify2(&i3);
    cps_rr_free(rr);
}
END_TEST


/*-----------------------------------------------------------------------
 * Testing harness
 */

Suite *
test_suite()
{
    Suite  *s = suite_create("cps");

    TCase  *tc_cps = tcase_create("cps");
    tcase_add_test(tc_cps, test_cps_01);
    tcase_add_test(tc_cps, test_cps_02);
    tcase_add_test(tc_cps, test_cps_03);
    tcase_add_test(tc_cps, test_cps_04);
    tcase_add_test(tc_cps, test_cps_05);
    tcase_add_test(tc_cps, test_cps_06);
    suite_add_tcase(s, tc_cps);

    return s;
}


int
main(int argc, const char **argv)
{
    int  number_failed;
    Suite  *suite = test_suite();
    SRunner  *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0)? EXIT_SUCCESS: EXIT_FAILURE;
}

