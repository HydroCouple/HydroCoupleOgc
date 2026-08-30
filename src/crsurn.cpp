// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

#include "hydrocoupleogc/crsurn.h"

#include <QStringList>

namespace HydroCouple::Ogc
{
  bool CrsIdentifier::isCrs84() const
  {
    if (authority.compare(QLatin1String("OGC"), Qt::CaseInsensitive) != 0)
    {
      return false;
    }

    // OGC spells it both as CRS84 and, in the URN form
    // urn:ogc:def:crs:OGC:2:84, as a bare 84.
    return code.compare(QLatin1String("CRS84"), Qt::CaseInsensitive) == 0
           || code == QLatin1String("84");
  }

  CrsIdentifier parseCrsIdentifier(const QString &text)
  {
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty())
    {
      return {};
    }

    // The http form: .../def/crs/EPSG/0/3857. Located from the "crs"
    // marker exactly as the URN form is, because the segment between the
    // authority and the code is a register version whose spelling varies --
    // "0" here, "6.18.3" in a URN -- and counting backwards from the end
    // would read that version as the authority.
    if (trimmed.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
    {
      const QStringList parts =
        trimmed.split(QLatin1Char('/'), Qt::SkipEmptyParts);

      for (int index = 0; index + 1 < parts.size(); ++index)
      {
        if (parts.at(index).compare(QLatin1String("crs"),
                                    Qt::CaseInsensitive) == 0)
        {
          CrsIdentifier identifier;
          identifier.authority = parts.at(index + 1).trimmed();
          identifier.code = parts.last().trimmed();

          return identifier.isValid() ? identifier : CrsIdentifier{};
        }
      }

      return {};
    }

    const QStringList tokens = trimmed.split(QLatin1Char(':'));

    // The plain form, EPSG:3857.
    if (tokens.size() == 2)
    {
      CrsIdentifier identifier;
      identifier.authority = tokens.at(0).trimmed();
      identifier.code = tokens.at(1).trimmed();

      return identifier.isValid() ? identifier : CrsIdentifier{};
    }

    // The URN form: urn:ogc:def:crs:AUTHORITY[:VERSION]:CODE. The code is
    // last and the authority is the first token after the "crs" marker, so
    // however many version segments sit between them, neither moves.
    if (tokens.size() >= 5 && tokens.at(0).compare(QLatin1String("urn"),
                                                   Qt::CaseInsensitive) == 0)
    {
      int crsMarker = -1;

      for (int index = 0; index < tokens.size(); ++index)
      {
        if (tokens.at(index).compare(QLatin1String("crs"),
                                     Qt::CaseInsensitive) == 0)
        {
          crsMarker = index;
          break;
        }
      }

      if (crsMarker >= 0 && crsMarker + 2 < tokens.size())
      {
        CrsIdentifier identifier;
        identifier.authority = tokens.at(crsMarker + 1).trimmed();
        identifier.code = tokens.last().trimmed();

        return identifier.isValid() ? identifier : CrsIdentifier{};
      }
    }

    return {};
  }

} // namespace HydroCouple::Ogc
