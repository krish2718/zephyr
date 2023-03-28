/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <errno.h>
#include <stdlib.h>

#include <signal.h>

void sigint_handler(int signum)
{
    printk("Caught signal %d - exiting\n", signum);
    exit(0);
}

void main(void)
{

 signal(SIGINT, sigint_handler);

}
