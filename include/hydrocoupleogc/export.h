// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   export.h
 * \author Caleb Buahin
 * \brief  Symbol visibility for the shared library.
 */

#ifndef HYDROCOUPLEOGC_EXPORT_H
#define HYDROCOUPLEOGC_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef HYDROCOUPLEOGC_LIBRARY
#    define HYDROCOUPLEOGC_EXPORT __declspec(dllexport)
#  else
#    define HYDROCOUPLEOGC_EXPORT __declspec(dllimport)
#  endif
#else
#  define HYDROCOUPLEOGC_EXPORT __attribute__((visibility("default")))
#endif

#endif // HYDROCOUPLEOGC_EXPORT_H
