// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log10.c
/* Unit name unconfirmed: fdlibm w_log10.c, as in the public decomps; no DOL string names the file. */

/* @(#)w_log10.c 1.2 95/01/04 */
/* $Id: w_log10.c,v 1.3 2004/02/26 17:03:30 ceciliar Exp $ */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* 
 * wrapper log10(X)
 */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * log10
 *
 * Wrapper: base-10 logarithm.
 */
double log10(double x) /* wrapper log10 */
{
	return __ieee754_log10(x);
} // 0x801A5E8C
