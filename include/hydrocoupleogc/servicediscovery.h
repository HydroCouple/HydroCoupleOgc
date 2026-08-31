// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 HydroCouple

/*!
 * \file   servicediscovery.h
 * \author Caleb Buahin
 * \brief  Working out what a service is from its address alone.
 *
 * A user pastes a URL. Which of the OGC services it is, and which version
 * of it, is something the server can be asked — but only in the service's
 * own dialect, so a client has to guess before it can ask. Guessing wrong
 * is cheap: the request comes back as an exception report and the next
 * dialect is tried.
 */

#ifndef HYDROCOUPLEOGC_SERVICEDISCOVERY_H
#define HYDROCOUPLEOGC_SERVICEDISCOVERY_H

#include "hydrocoupleogc/export.h"

#include <QByteArray>
#include <QString>

namespace HydroCouple::Ogc
{
  /*!
   * \brief The kinds of service this library speaks.
   */
  enum class ServiceKind
  {
      Unknown,
      Wms,
      Wmts,
      Wfs,
      Wcs
  };

  /*!
   * \brief Where to ask \a serviceUrl what it can do.
   *
   * The address the user pasted usually already carries parameters — a
   * MapServer mapfile, a workspace, a tenant — so they are kept and the
   * request is added to them. Any GetCapabilities parameters already in it
   * are replaced rather than duplicated: a URL copied out of a browser is
   * as often a whole GetCapabilities request as it is a bare endpoint, and
   * a query with two SERVICE parameters is answered by neither server the
   * user might be talking to.
   *
   * \param serviceUrl The address as the user gave it.
   * \param kind Which dialect to ask in.
   * \returns The URL to fetch, or empty when \a serviceUrl is not one.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT QString buildCapabilitiesUrl(
    const QString &serviceUrl, ServiceKind kind);

  /*!
   * \brief What kind of service answered.
   *
   * Decided from the element names, so it costs nothing and works on a
   * document this library could not otherwise parse. A WMS calls its root
   * WMS_Capabilities, or WMT_MS_Capabilities before 1.3.0; a WFS calls its
   * root WFS_Capabilities; a WMTS calls its root simply Capabilities — and
   * may wrap it in a SOAP envelope, which is why the root alone is not what
   * is looked at.
   *
   * \param xml A capabilities response.
   * \returns The kind, or Unknown for anything else — including an
   *          exception report, which is not an answer about a service.
   */
  [[nodiscard]] HYDROCOUPLEOGC_EXPORT ServiceKind detectServiceKind(
    const QByteArray &xml);

} // namespace HydroCouple::Ogc

#endif // HYDROCOUPLEOGC_SERVICEDISCOVERY_H
