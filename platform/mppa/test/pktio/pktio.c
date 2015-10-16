/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include "pktio.h"

int main(int argc, char* argv[])
{
	if (argc == 2) {
		setenv("ODP_PKTIO_IF0", argv[1], 1);
	} else if (argc == 3) {
		setenv("ODP_PKTIO_IF0", argv[1], 1);
		setenv("ODP_PKTIO_IF1", argv[2], 1);
	} else if (argc  > 3)
		exit(1);
	return pktio_main();
}
