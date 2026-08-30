// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/axisorder.h"

#include <QStringList>

namespace HydroCouple::Ogc
{
  bool authorityWritesLatitudeFirst(const QString &crs)
  {
    const CrsIdentifier identifier = parseCrsIdentifier(crs);

    // Only EPSG says so. CRS84 needs no case of its own: it is OGC's, not
    // EPSG's, and being longitude first is the whole reason it exists.
    if (identifier.authority.compare(QLatin1String("EPSG"),
                                     Qt::CaseInsensitive) != 0)
    {
      return false;
    }

    static const QStringList latitudeFirst = {
      QStringLiteral("4326"),  // WGS 84
      QStringLiteral("4258"),  // ETRS89
      QStringLiteral("4269"),  // NAD83
      QStringLiteral("4979")}; // WGS 84 3D

    // Projected systems are not on this list and must not be: EPSG defines
    // Web Mercator, the national grids and the state planes easting first,
    // whatever hemisphere they are in.

    return latitudeFirst.contains(identifier.code);
  }

} // namespace HydroCouple::Ogc
