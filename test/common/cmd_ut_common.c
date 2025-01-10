// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 Heinrich Schuchardt <xypron.glpk@gmx.de>
 * Copyright (c) 2021 Steffen Jaeckel <jaeckel-floss@eyet-services.de>
 *
 * Unit tests for common functions
 */

#include <command.h>
#include <test/common.h>
#include <test/suites.h>
#include <test/ut.h>

int do_ut_common(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(common_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(common_test);

	return cmd_ut_category("common", "common_test_", tests, n_ents, argc,
			       argv);
}
