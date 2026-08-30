// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   axisorder.h
 * \author Caleb Buahin
 * \brief  Which of a coordinate pair comes first.
 *
 * The single most common way an OGC client draws a map that is plausible,
 * sideways and somewhere else. Every service that takes a bounding box has
 * the same rule under a different name: before some version, coordinates
 * are always longitude first; from that version on they are written in the
 * authority's own order, and EPSG's order for a geographic system is
 * latitude first.
 *
 * The version half of that rule differs per service and stays with each
 * service. The authority half is the same question everywhere, so it is
 * asked here once.
 */

#ifndef HYDROCOUPLEOGC_AXISORDER_H
#define HYDROCOUPLEOGC_AXISORDER_H

#include "hydrocoupleogc/crsurn.h"
#include "hydrocoupleogc/export.h"

#include <QString>

namespace HydroCouple::Ogc
{
  /*!
   * \brief Whether the authority defines \a crs latitude first.
   *
   * A short list rather than the register: these are the geographic systems
   * a service in this application is actually asked for, and every
   * projected system it uses — Web Mercator above all — is easting first.
   * The hosts carry a coordinate database and can answer this properly;
   * this exists so that the parsers, which carry none, are not left
   * guessing.
   *
   * \param crs The system, in any of the spellings services use.
   * \returns Whether latitude is written first. False for anything not
   *          recognised, which is the safer wrong answer: it is the order
   *          every pre-1.3.0 service used and the order every projected
   *          system uses.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT bool authorityWritesLatitudeFirst(
    const QString &crs);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_AXISORDER_H
