/***************************************************************************
                          qgspdfrenderer.cpp
                             -------------------
    begin                : December 2023
    copyright            : (C) 2023 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgspdfrenderer.h"

#ifdef HAVE_PDF4QT
#include "pdfdocumentreader.h"
#include "pdfdocumenttextflow.h"
#include "pdfrenderer.h"
#include "pdffont.h"
#include "pdfcms.h"
#else
#include "qgsexception.h"
#include <QObject>
#endif

#include <QRectF>
#include <QPainter>

#ifdef HAVE_PDF4QT
class PdfDocumentContainer
{
  public:
    PdfDocumentContainer( const QString &path, Qgis::PdfRenderFlags flags )
      : reader(
          nullptr, []( bool * ) -> QString { return QString(); }, true, false
        )
      , document( reader.readFromFile( path ) )
      , modifiedDocument( &document, nullptr )
      , fontCache( 1000, 1000 )
    {
      fontCache.setDocument( modifiedDocument );

      pdf::PDFRenderer::Features features;
      if ( flags.testFlag( Qgis::PdfRenderFlag::RenderTextAsText ) )
      {
        features.setFlag( pdf::PDFRenderer::Feature::RealText, true );
      }
      renderer = std::make_unique< pdf::PDFRenderer >( &document, &fontCache, &pdfCms, nullptr, features, meshQualitySettings );
    }
    pdf::PDFDocumentReader reader;
    pdf::PDFDocument document;
    pdf::PDFModifiedDocument modifiedDocument;
    pdf::PDFFontCache fontCache;
    pdf::PDFCMSGeneric pdfCms;
    pdf::PDFMeshQualitySettings meshQualitySettings;
    std::unique_ptr< pdf::PDFRenderer > renderer;
};
#endif

QgsPdfRenderer::QgsPdfRenderer( const QString &path, Qgis::PdfRenderFlags flags )
  : mPath( path )
  , mFlags( flags )
{
#ifdef HAVE_PDF4QT
  mDocumentContainer = std::make_unique< PdfDocumentContainer >( path, flags );
#endif
}

QgsPdfRenderer::~QgsPdfRenderer() = default;

#ifdef HAVE_PDF4QT
QString QgsPdfRenderer::extractText( qint64 maxUtf8Bytes ) const
{
  if ( maxUtf8Bytes <= 0 )
    return QString();

  pdf::PDFDocumentTextFlowFactory factory;
  QString text = factory.create( &mDocumentContainer->document, pdf::PDFDocumentTextFlowFactory::Algorithm::Auto ).getText().trimmed();
  QByteArray utf8 = text.toUtf8();
  if ( utf8.size() <= maxUtf8Bytes )
    return text;

  utf8.truncate( maxUtf8Bytes );
  text = QString::fromUtf8( utf8 );
  while ( text.endsWith( QChar::ReplacementCharacter ) )
    text.chop( 1 );
  return text.trimmed();
}
#else
QString QgsPdfRenderer::extractText( qint64 ) const
{
  throw QgsNotSupportedException( QObject::tr( "Extracting PDF text requires a QGIS build with PDF4Qt library support" ) );
}
#endif

#ifdef HAVE_PDF4QT
int QgsPdfRenderer::pageCount() const
{
  const pdf::PDFCatalog *catalog = mDocumentContainer->document.getCatalog();
  return static_cast< int >( catalog->getPageCount() );
}
#else
int QgsPdfRenderer::pageCount() const
{
  throw QgsNotSupportedException( QObject::tr( "Rendering PDF requires a QGIS build with PDF4Qt library support" ) );
}
#endif

#ifdef HAVE_PDF4QT
QRectF QgsPdfRenderer::pageMediaBox( int pageNumber ) const
{
  if ( pageNumber < 0 || pageNumber >= pageCount() )
    return QRectF();

  const pdf::PDFCatalog *catalog = mDocumentContainer->document.getCatalog();
  return catalog->getPage( pageNumber )->getMediaBox();
}
#else
QRectF QgsPdfRenderer::pageMediaBox( int ) const
{
  throw QgsNotSupportedException( QObject::tr( "Rendering PDF requires a QGIS build with PDF4Qt library support" ) );
}
#endif

#ifdef HAVE_PDF4QT
bool QgsPdfRenderer::render( QPainter *painter, const QRectF &rectangle, int pageIndex )
{
  mDocumentContainer->renderer->render( painter, rectangle, pageIndex );
  return true;
}
#else
bool QgsPdfRenderer::render( QPainter *, const QRectF &, int )
{
  throw QgsNotSupportedException( QObject::tr( "Rendering PDF requires a QGIS build with PDF4Qt library support" ) );
}

#endif
