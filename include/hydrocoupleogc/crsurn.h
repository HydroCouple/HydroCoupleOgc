// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   crsurn.h
 * \author Caleb Buahin
 * \brief  Naming the same coordinate system the several ways OGC does.
 *
 * `EPSG:3857`, `urn:ogc:def:crs:EPSG::3857`,
 * `urn:ogc:def:crs:EPSG:6.18.3:3857` and
 * `http://www.opengis.net/def/crs/EPSG/0/3857` are one system under four
 * spellings, and a client meets all four in the wild — sometimes in one
 * document. Every service this library speaks to has the same problem, so
 * the answer lives in one place.
 */

#ifndef HYDROCOUPLEOGC_CRSURN_H
#define HYDROCOUPLEOGC_CRSURN_H

#include "hydrocoupleogc/export.h"

#include <QString>

namespace HydroCouple::Ogc
{
  /*!
   * \brief An authority and the code it assigns, e.g. EPSG and 3857.
   */
  struct HYDROCOUPLEOGC_EXPORT CrsIdentifier
  {
      QString authority;
      QString code;

      [[nodiscard]] bool isValid() const
      {
        return !authority.isEmpty() && !code.isEmpty();
      }

      //! \returns The `AUTHORITY:CODE` form, or an empty string when invalid.
      [[nodiscard]] QString toString() const
      {
        return isValid() ? authority + QLatin1Char(':') + code : QString();
      }

      [[nodiscard]] bool operator==(const CrsIdentifier &other) const
      {
        return authority == other.authority && code == other.code;
      }

      /*!
       * \brief Whether this is CRS84 — longitude first, by definition.
       *
       * Worth asking separately because it is the one geographic system
       * whose axis order is settled by its *name* rather than by a
       * coordinate database: OGC defined CRS84 precisely so there would be
       * a lon/lat spelling of WGS 84.
       */
      [[nodiscard]] bool isCrs84() const;
  };

  /*!
   * \brief Reads any of the spellings OGC services use for a system.
   *
   * Version segments are skipped wherever they appear: `EPSG::3857`,
   * `EPSG:6.18:3857` and `EPSG:6.18.3:3857` all name EPSG 3857, and the
   * version says which edition of the register was consulted rather than
   * which system is meant.
   *
   * \param text The identifier as the document spells it.
   * \returns The authority and code; invalid when \a text names neither.
   *
   * \note Axis order is deliberately **not** answered here. Whether
   *       `EPSG:4326` is read latitude-first depends on the authority's
   *       definition of it, which needs a coordinate database this library
   *       does not carry — the hosts have one and should be asked. The
   *       single exception is CrsIdentifier::isCrs84().
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT CrsIdentifier
  parseCrsIdentifier(const QString &text);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_CRSURN_H
