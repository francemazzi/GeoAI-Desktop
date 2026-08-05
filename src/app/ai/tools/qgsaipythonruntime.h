/***************************************************************************
    qgsaipythonruntime.h
    --------------------
    begin                : August 2026
    copyright            : (C) 2026 by Francesco Mazzi
    email                : francemazzi at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSAIPYTHONRUNTIME_H
#define QGSAIPYTHONRUNTIME_H

#include "qgis_app.h"

#include <QString>

namespace QgsAiPythonRuntime
{
  //! Directory in the active profile reserved for AI-installed Python packages.
  APP_EXPORT QString packageTargetPath();

  //! Persistent successful-install cache used by install_python_package.
  APP_EXPORT QString installCachePath();

  /**
   * Returns Python source which makes the isolated package directory and the
   * standard Strata PyQGIS namespace available to run_python code.
   */
  APP_EXPORT QString bootstrapSource( const QString &targetPath );

  /**
   * Builds the Python subprocess wrapper used by install_python_package.
   * The wrapper only invokes the bundled sibling interpreter and installs
   * packages under \a targetPath using pip --target.
   */
  APP_EXPORT QString pipWrapperSource( const QString &outputPath, const QString &argumentsPath, const QString &targetPath, const QString &cachePath, int timeoutSeconds );

  //! True when pip output identifies a PEP 668 externally-managed environment.
  APP_EXPORT bool isExternallyManagedError( const QString &standardError );
} // namespace QgsAiPythonRuntime

#endif // QGSAIPYTHONRUNTIME_H
