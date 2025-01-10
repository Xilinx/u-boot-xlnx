# SPDX-License-Identifier:  GPL-2.0+
#
# Tests for OP-TEE RPMB read/write support

"""
This tests optee_rpmb cmd in U-Boot
"""

import pytest
import u_boot_utils as util

@pytest.mark.buildconfigspec('cmd_optee_rpmb')
def test_optee_rpmb_read_write(u_boot_console):
    """Test OP-TEE RPMB cmd read/write
    """
    response = u_boot_console.run_command('optee_rpmb write_pvalue test_variable test_value')
    assert response == 'Wrote 11 bytes'

    response = u_boot_console.run_command('optee_rpmb read_pvalue test_variable 11')
    assert response == 'Read 11 bytes, value = test_value'