/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * SPDX-License-Identifier:	MIT
 */

#ifndef LIBAVB_AB_H_
#define LIBAVB_AB_H_

#include <avb/libavb.h>

/* The AVB_INSIDE_LIBAVB_AB_H preprocessor symbol is used to enforce
 * library users to include only this file. All public interfaces, and
 * only public interfaces, must be included here.
 */

#define AVB_INSIDE_LIBAVB_AB_H
#include "avb_ab_flow.h"
#include "avb_ab_ops.h"
#undef AVB_INSIDE_LIBAVB_AB_H

#endif /* LIBAVB_AB_H_ */
