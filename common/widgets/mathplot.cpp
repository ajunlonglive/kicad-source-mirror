/////////////////////////////////////////////////////////////////////////////
// Name:            mathplot.cpp
// Purpose:         Framework for plotting in wxWindows
// Original Author: David Schalig
// Maintainer:      Davide Rondini
// Contributors:    Jose Luis Blanco, Val Greene, Maciej Suminski, Tomasz Wlostowski
// Created:         21/07/2003
// Last edit:       25/08/2016
// Copyright:       (c) David Schalig, Davide Rondini
// Licence:         wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <wx/window.h>

// Comment out for release operation:
// (Added by J.L.Blanco, Aug 2007)
//#define MATHPLOT_DO_LOGGING

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/object.h"
#include "wx/font.h"
#include "wx/colour.h"
#include "wx/sizer.h"
#include "wx/intl.h"
#include "wx/dcclient.h"
#include "wx/cursor.h"
#endif

#include <widgets/mathplot.h>
#include <wx/module.h>
#include <wx/image.h>

#include <cmath>
#include <cstdio>   // used only for debug
#include <ctime>    // used for representation of x axes involving date
#include <set>

// Memory leak debugging
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Legend margins
#define mpLEGEND_MARGIN     5
#define mpLEGEND_LINEWIDTH  10

// Number of pixels to scroll when scrolling by a line
#define mpSCROLL_NUM_PIXELS_PER_LINE 10

// See doxygen comments.
double mpWindow::zoomIncrementalFactor = 1.1;

// -----------------------------------------------------------------------------
// mpLayer
// -----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS( mpLayer, wxObject )

mpLayer::mpLayer() : m_type( mpLAYER_UNDEF )
{
    SetPen( (wxPen&) *wxBLACK_PEN );
    SetFont( (wxFont&) *wxNORMAL_FONT );
    m_continuous = false;   // Default
    m_showName = true;      // Default
    m_drawOutsideMargins = false;
    m_visible = true;
}


wxBitmap mpLayer::GetColourSquare( int side ) const
{
    wxBitmap   square( side, side, -1 );
    wxColour   filler = m_pen.GetColour();
    wxBrush    brush( filler, wxBRUSHSTYLE_SOLID );
    wxMemoryDC dc;

    dc.SelectObject( square );
    dc.SetBackground( brush );
    dc.Clear();
    dc.SelectObject( wxNullBitmap );
    return square;
}


// -----------------------------------------------------------------------------
// mpInfoLayer
// -----------------------------------------------------------------------------
IMPLEMENT_DYNAMIC_CLASS( mpInfoLayer, mpLayer )

mpInfoLayer::mpInfoLayer()
{
    m_dim = wxRect( 0, 0, 1, 1 );
    m_brush = *wxTRANSPARENT_BRUSH;
    m_reference.x = 0; m_reference.y = 0;
    m_winX  = 1;    // parent->GetScrX();
    m_winY  = 1;    // parent->GetScrY();
    m_type  = mpLAYER_INFO;
}


mpInfoLayer::mpInfoLayer( wxRect rect, const wxBrush* brush ) : m_dim( rect )
{
    m_brush = *brush;
    m_reference.x   = rect.x;
    m_reference.y   = rect.y;
    m_winX  = 1;    // parent->GetScrX();
    m_winY  = 1;    // parent->GetScrY();
    m_type  = mpLAYER_INFO;
}


mpInfoLayer::~mpInfoLayer()
{
}


void mpInfoLayer::UpdateInfo( mpWindow& w, wxEvent& event )
{
}


bool mpInfoLayer::Inside( wxPoint& point )
{
    return m_dim.Contains( point );
}


void mpInfoLayer::Move( wxPoint delta )
{
    m_dim.SetX( m_reference.x + delta.x );
    m_dim.SetY( m_reference.y + delta.y );
}


void mpInfoLayer::UpdateReference()
{
    m_reference.x   = m_dim.x;
    m_reference.y   = m_dim.y;
}


void mpInfoLayer::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        // Adjust relative position inside the window
        int scrx    = w.GetScrX();
        int scry    = w.GetScrY();

        // Avoid dividing by 0
        if( scrx == 0 )
            scrx = 1;

        if( scry == 0 )
            scry = 1;

        if( (m_winX != scrx) || (m_winY != scry) )
        {
            if( m_winX > 1 )
                m_dim.x = (int) floor( (double) (m_dim.x * scrx / m_winX) );

            if( m_winY > 1 )
            {
                m_dim.y = (int) floor( (double) (m_dim.y * scry / m_winY) );
                UpdateReference();
            }

            // Finally update window size
            m_winX  = scrx;
            m_winY  = scry;
        }

        dc.SetPen( m_pen );
        // wxImage image0(wxT("pixel.png"), wxBITMAP_TYPE_PNG);
        // wxBitmap image1(image0);
        // wxBrush semiWhite(image1);
        dc.SetBrush( m_brush );
        dc.DrawRectangle( m_dim.x, m_dim.y, m_dim.width, m_dim.height );
    }
}


wxPoint mpInfoLayer::GetPosition() const
{
    return m_dim.GetPosition();
}


wxSize mpInfoLayer::GetSize() const
{
    return m_dim.GetSize();
}


mpInfoCoords::mpInfoCoords() : mpInfoLayer()
{
}


mpInfoCoords::mpInfoCoords( wxRect rect, const wxBrush* brush ) : mpInfoLayer( rect, brush )
{
}


mpInfoCoords::~mpInfoCoords()
{
}


void mpInfoCoords::UpdateInfo( mpWindow& w, wxEvent& event )
{
    if( event.GetEventType() == wxEVT_MOTION )
    {
        /* It seems that Windows port of wxWidgets don't support multi-line test to be drawn in a wxDC.
         *  wxGTK instead works perfectly with it.
         *  Info on wxForum: http://wxforum.shadonet.com/viewtopic.php?t=3451&highlight=drawtext+eol */
#ifdef _WINDOWS
        // FIXME m_content.Printf(wxT("x = %f y = %f"), XScale().P2x(w, mouseX), YScale().P2x(w, mouseY));
#else
        // FIXME m_content.Printf(wxT("x = %f\ny = %f"), XScale().P2x(w, mouseX), YScale().P2x(w, mouseY));
#endif
    }
}


void mpInfoCoords::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        // Adjust relative position inside the window
        int scrx    = w.GetScrX();
        int scry    = w.GetScrY();

        if( (m_winX != scrx) || (m_winY != scry) )
        {
            if( m_winX > 1 )
                m_dim.x = (int) floor( (double) (m_dim.x * scrx / m_winX) );

            if( m_winY > 1 )
            {
                m_dim.y = (int) floor( (double) (m_dim.y * scry / m_winY) );
                UpdateReference();
            }

            // Finally update window size
            m_winX  = scrx;
            m_winY  = scry;
        }

        dc.SetPen( m_pen );
        // wxImage image0(wxT("pixel.png"), wxBITMAP_TYPE_PNG);
        // wxBitmap image1(image0);
        // wxBrush semiWhite(image1);
        dc.SetBrush( m_brush );
        dc.SetFont( m_font );
        int textX, textY;
        dc.GetTextExtent( m_content, &textX, &textY );

        if( m_dim.width < textX + 10 )
            m_dim.width = textX + 10;

        if( m_dim.height < textY + 10 )
            m_dim.height = textY + 10;

        dc.DrawRectangle( m_dim.x, m_dim.y, m_dim.width, m_dim.height );
        dc.DrawText( m_content, m_dim.x + 5, m_dim.y + 5 );
    }
}


mpInfoLegend::mpInfoLegend() : mpInfoLayer()
{
}


mpInfoLegend::mpInfoLegend( wxRect rect, const wxBrush* brush ) : mpInfoLayer( rect, brush )
{
}


mpInfoLegend::~mpInfoLegend()
{
}


void mpInfoLegend::UpdateInfo( mpWindow& w, wxEvent& event )
{
}


void mpInfoLegend::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        // Adjust relative position inside the window
        int scrx    = w.GetScrX();
        int scry    = w.GetScrY();

        if( (m_winX != scrx) || (m_winY != scry) )
        {
            if( m_winX > 1 )
                m_dim.x = (int) floor( (double) (m_dim.x * scrx / m_winX) );

            if( m_winY > 1 )
            {
                m_dim.y = (int) floor( (double) (m_dim.y * scry / m_winY) );
                UpdateReference();
            }

            // Finally update window size
            m_winX  = scrx;
            m_winY  = scry;
        }

        // wxImage image0(wxT("pixel.png"), wxBITMAP_TYPE_PNG);
        // wxBitmap image1(image0);
        // wxBrush semiWhite(image1);
        dc.SetBrush( m_brush );
        dc.SetFont( m_font );
        const int baseWidth = (mpLEGEND_MARGIN * 2 + mpLEGEND_LINEWIDTH);
        int textX = baseWidth, textY = mpLEGEND_MARGIN;
        int plotCount = 0;
        int posY    = 0;
        int tmpX    = 0, tmpY = 0;
        mpLayer* ly = NULL;
        wxPen lpen;
        wxString label;

        for( unsigned int p = 0; p < w.CountAllLayers(); p++ )
        {
            ly = w.GetLayer( p );

            if( (ly->GetLayerType() == mpLAYER_PLOT) && ( ly->IsVisible() ) )
            {
                label = ly->GetName();
                dc.GetTextExtent( label, &tmpX, &tmpY );
                textX =
                    ( textX > (tmpX + baseWidth) ) ? textX : (tmpX + baseWidth + mpLEGEND_MARGIN);
                textY += (tmpY);
            }
        }

        dc.SetPen( m_pen );
        dc.SetBrush( m_brush );
        m_dim.width = textX;

        if( textY != mpLEGEND_MARGIN )    // Don't draw any thing if there are no visible layers
        {
            textY += mpLEGEND_MARGIN;
            m_dim.height = textY;
            dc.DrawRectangle( m_dim.x, m_dim.y, m_dim.width, m_dim.height );

            for( unsigned int p2 = 0; p2 < w.CountAllLayers(); p2++ )
            {
                ly = w.GetLayer( p2 );

                if( (ly->GetLayerType() == mpLAYER_PLOT) && ( ly->IsVisible() ) )
                {
                    label   = ly->GetName();
                    lpen    = ly->GetPen();
                    dc.GetTextExtent( label, &tmpX, &tmpY );
                    dc.SetPen( lpen );
                    // textX = (textX > (tmpX + baseWidth)) ? textX : (tmpX + baseWidth);
                    // textY += (tmpY + mpLEGEND_MARGIN);
                    posY = m_dim.y + mpLEGEND_MARGIN + plotCount * tmpY + (tmpY >> 1);
                    dc.DrawLine( m_dim.x + mpLEGEND_MARGIN,                 // X start coord
                            posY,                                           // Y start coord
                            m_dim.x + mpLEGEND_LINEWIDTH + mpLEGEND_MARGIN, // X end coord
                            posY );
                    // dc.DrawRectangle(m_dim.x + 5, m_dim.y + 5 + plotCount*tmpY, 5, 5);
                    dc.DrawText( label,
                            m_dim.x + baseWidth,
                            m_dim.y + mpLEGEND_MARGIN + plotCount * tmpY );
                    plotCount++;
                }
            }
        }
    }
}


// -----------------------------------------------------------------------------
// mpLayer implementations - functions
// -----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS( mpFX, mpLayer )

mpFX::mpFX( const wxString& name, int flags )
{
    SetName( name );
    m_flags = flags;
    m_type  = mpLAYER_PLOT;
}


void mpFX::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );

        wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
        wxCoord endPx   = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

        wxCoord iy = 0;

        if( m_pen.GetWidth() <= 1 )
        {
            for( wxCoord i = startPx; i < endPx; ++i )
            {
                iy = w.y2p( GetY( w.p2x( i ) ) );

                // Draw the point only if you can draw outside margins or if the point is inside margins
                if( m_drawOutsideMargins || ( (iy >= minYpx) && (iy <= maxYpx) ) )
                    dc.DrawPoint( i, iy );    // (wxCoord) ((w.GetPosY() - GetY( (double)i / w.GetScaleX() + w.GetPosX()) ) * w.GetScaleY()));
            }
        }
        else
        {
            for( wxCoord i = startPx; i < endPx; ++i )
            {
                iy = w.y2p( GetY( w.p2x( i ) ) );

                // Draw the point only if you can draw outside margins or if the point is inside margins
                if( m_drawOutsideMargins || ( (iy >= minYpx) && (iy <= maxYpx) ) )
                    dc.DrawLine( i, iy, i, iy );

                // wxCoord c = YScale().X2p( GetY(XScale().P2x(i)) );
                //(wxCoord) ((w.GetPosY() - GetY( (double)i / w.GetScaleX() + w.GetPosX()) ) * w.GetScaleY());
            }
        }

        if( !m_name.IsEmpty() && m_showName )
        {
            dc.SetFont( m_font );

            wxCoord tx, ty;
            dc.GetTextExtent( m_name, &tx, &ty );

            /*if ((m_flags & mpALIGNMASK) == mpALIGN_RIGHT)
             *  tx = (w.GetScrX()>>1) - tx - 8;
             *  else if ((m_flags & mpALIGNMASK) == mpALIGN_CENTER)
             *  tx = -tx/2;
             *  else
             *  tx = -(w.GetScrX()>>1) + 8;
             */
            if( (m_flags & mpALIGNMASK) == mpALIGN_RIGHT )
                tx = (w.GetScrX() - tx) - w.GetMarginRight() - 8;
            else if( (m_flags & mpALIGNMASK) == mpALIGN_CENTER )
                tx = ( (w.GetScrX() - w.GetMarginRight() - w.GetMarginLeft() - tx) / 2 ) +
                     w.GetMarginLeft();
            else
                tx = w.GetMarginLeft() + 8;

            dc.DrawText( m_name, tx, w.y2p( GetY( w.p2x( tx ) ) ) );
        }
    }
}


IMPLEMENT_ABSTRACT_CLASS( mpFY, mpLayer )

mpFY::mpFY( const wxString& name, int flags )
{
    SetName( name );
    m_flags = flags;
    m_type  = mpLAYER_PLOT;
}


void mpFY::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );

        wxCoord i, ix;

        wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
        wxCoord endPx   = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

        if( m_pen.GetWidth() <= 1 )
        {
            for( i = minYpx; i < maxYpx; ++i )
            {
                ix = w.x2p( GetX( w.p2y( i ) ) );

                if( m_drawOutsideMargins || ( (ix >= startPx) && (ix <= endPx) ) )
                    dc.DrawPoint( ix, i );
            }
        }
        else
        {
            for( i = 0; i< w.GetScrY(); ++i )
            {
                ix = w.x2p( GetX( w.p2y( i ) ) );

                if( m_drawOutsideMargins || ( (ix >= startPx) && (ix <= endPx) ) )
                    dc.DrawLine( ix, i, ix, i );

                // wxCoord c =  XScale().X2p(GetX(YScale().P2x(i)));
                //(wxCoord) ((GetX( (double)i / w.GetScaleY() + w.GetPosY()) - w.GetPosX()) * w.GetScaleX());
                // dc.DrawLine(c, i, c, i);
            }
        }

        if( !m_name.IsEmpty() && m_showName )
        {
            dc.SetFont( m_font );

            wxCoord tx, ty;
            dc.GetTextExtent( m_name, &tx, &ty );

            if( (m_flags & mpALIGNMASK) == mpALIGN_TOP )
                ty = w.GetMarginTop() + 8;
            else if( (m_flags & mpALIGNMASK) == mpALIGN_CENTER )
                ty = ( (w.GetScrY() - w.GetMarginTop() - w.GetMarginBottom() - ty) / 2 ) +
                     w.GetMarginTop();
            else
                ty = w.GetScrY() - 8 - ty - w.GetMarginBottom();

            dc.DrawText( m_name, w.x2p( GetX( w.p2y( ty ) ) ), ty );    // (wxCoord) ((GetX( (double)i / w.GetScaleY() + w.GetPosY()) - w.GetPosX()) * w.GetScaleX()), -ty);
        }
    }
}


IMPLEMENT_ABSTRACT_CLASS( mpFXY, mpLayer )

mpFXY::mpFXY( const wxString& name, int flags )
{
    SetName( name );
    m_flags     = flags;
    m_type      = mpLAYER_PLOT;
    m_scaleX    = NULL;
    m_scaleY    = NULL;

    // Avoid not initialized members:
    maxDrawX = minDrawX = maxDrawY = minDrawY = 0;
}


void mpFXY::UpdateViewBoundary( wxCoord xnew, wxCoord ynew )
{
    // Keep track of how many points have been drawn and the bounding box
    maxDrawX    = (xnew > maxDrawX) ? xnew : maxDrawX;
    minDrawX    = (xnew < minDrawX) ? xnew : minDrawX;
    maxDrawY    = (maxDrawY > ynew) ? maxDrawY : ynew;
    minDrawY    = (minDrawY < ynew) ? minDrawY : ynew;
    // drawnPoints++;
}


void mpFXY::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );

        double x, y;
        // Do this to reset the counters to evaluate bounding box for label positioning
        Rewind(); GetNextXY( x, y );
        maxDrawX = x; minDrawX = x; maxDrawY = y; minDrawY = y;
        // drawnPoints = 0;
        Rewind();

        wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
        wxCoord endPx   = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

        dc.SetClippingRegion( startPx, minYpx, endPx - startPx + 1, maxYpx - minYpx + 1 );

        if( !m_continuous )
        {
            bool first = true;
            wxCoord ix = 0;
            std::set<wxCoord> ys;

            while( GetNextXY( x, y ) )
            {
                double px = m_scaleX->TransformToPlot( x );
                double py = m_scaleY->TransformToPlot( y );
                wxCoord newX = w.x2p( px );

                if( first )
                {
                    ix = newX;
                    first = false;
                }

                if( newX == ix )    // continue until a new X coordinate is reached
                {
                    // collect all unique points
                    ys.insert( w.y2p( py ) );
                    continue;
                }

                for( auto& iy: ys )
                {
                    if( m_drawOutsideMargins
                        || ( (ix >= startPx) && (ix <= endPx) && (iy >= minYpx)
                             && (iy <= maxYpx) ) )
                    {
                        // for some reason DrawPoint does not use the current pen,
                        // so we use DrawLine for fat pens
                        if( m_pen.GetWidth() <= 1 )
                        {
                            dc.DrawPoint( ix, iy );
                        }
                        else
                        {
                            dc.DrawLine( ix, iy, ix, iy );
                        }

                        UpdateViewBoundary( ix, iy );
                    }
                }

                ys.clear();
                ix = newX;
                ys.insert( w.y2p( py ) );
            }
        }
        else
        {
            int count = 0;
            int x0=0;               // X position of merged current vertical line
            int ymin0=0;            // y min coord of merged current vertical line
            int ymax0=0;            // y max coord of merged current vertical line
            int dupx0 = 0;          // count of currently merged vertical lines
            wxPoint line_start;     // starting point of the current line to draw

            // A buffer to store coordinates of lines to draw
            std::vector<wxPoint>pointList;
            pointList.reserve( endPx - startPx + 1 );

            // Note: we can use dc.DrawLines() only for a reasonable number or points (<10000),
            // because at least on Windows dc.DrawLines() can hang for a lot of points.
            // (> 10000 points) (can happens when a lot of points is calculated)
            // To avoid long draw time (and perhaps hanging) one plot only not redundant lines.
            // To avoid artifacts when skipping points to the same x coordinate, for each
            // group of points at a give, x coordinate we also draw a vertical line at this coord,
            // from the ymin to the ymax vertical coordinates of skipped points
            while( GetNextXY( x, y ) )
            {
                double px = m_scaleX->TransformToPlot( x );
                double py = m_scaleY->TransformToPlot( y );

                wxCoord x1 = w.x2p( px );
                wxCoord y1 = w.y2p( py );

                // Store only points on the drawing area, to speed up the drawing time
                // Note: x1 is a value truncated from px by w.x2p(). So to be sure the
                // first point is drawn, the x1 low limit is startPx-1 in plot coordinates
                if( x1 >= startPx-1 && x1 <= endPx )
                {
                    if( !count || line_start.x != x1 )
                    {
                        if( count && dupx0 > 1 && ymin0 != ymax0 )
                        {
                            // Vertical points are merged, draw the pending vertical line
                            // However, if the line is one pixel length, it is not drawn,
                            // because the main trace show this point
                            dc.DrawLine( x0, ymin0, x0, ymax0 );
                        }

                        x0 = x1;
                        ymin0 = ymax0 = y1;
                        dupx0 = 0;

                        pointList.emplace_back( wxPoint( x1, y1 ) );

                        line_start.x = x1;
                        line_start.y = y1;
                        count++;
                    }
                    else
                    {
                        ymin0 = std::min( ymin0, y1 );
                        ymax0 = std::max( ymax0, y1 );
                        x0 = x1;
                        dupx0++;
                    }
                }
            }

            if( pointList.size() > 1 )
            {
                // For a better look (when using dashed lines) and more optimization,
                // try to merge horizontal segments, in order to plot longer lines
                // we are merging horizontal segments because this is easy,
                // and horizontal segments are a frequent cases
                std::vector<wxPoint> drawPoints;
                drawPoints.reserve( endPx - startPx + 1 );

                drawPoints.push_back( pointList[0] );   // push the first point in list

                for( size_t ii = 1; ii < pointList.size()-1; ii++ )
                {
                    // Skip intermediate points between the first point and the last
                    // point of the segment candidate
                    if( drawPoints.back().y == pointList[ii].y &&
                        drawPoints.back().y == pointList[ii+1].y )
                        continue;
                    else
                        drawPoints.push_back( pointList[ii] );
                }

                // push the last point to draw in list
                if( drawPoints.back() != pointList.back() )
                    drawPoints.push_back( pointList.back() );

                dc.DrawLines( drawPoints.size(), &drawPoints[0] );
            }
        }

        if( !m_name.IsEmpty() && m_showName )
        {
            dc.SetFont( m_font );

            wxCoord tx, ty;
            dc.GetTextExtent( m_name, &tx, &ty );

            // xxx implement else ... if (!HasBBox())
            {
                // const int sx = w.GetScrX();
                // const int sy = w.GetScrY();

                if( (m_flags & mpALIGNMASK) == mpALIGN_NW )
                {
                    tx  = minDrawX + 8;
                    ty  = maxDrawY + 8;
                }
                else if( (m_flags & mpALIGNMASK) == mpALIGN_NE )
                {
                    tx  = maxDrawX - tx - 8;
                    ty  = maxDrawY + 8;
                }
                else if( (m_flags & mpALIGNMASK) == mpALIGN_SE )
                {
                    tx  = maxDrawX - tx - 8;
                    ty  = minDrawY - ty - 8;
                }
                else
                {
                    // mpALIGN_SW
                    tx  = minDrawX + 8;
                    ty  = minDrawY - ty - 8;
                }
            }

            dc.DrawText( m_name, tx, ty );
        }
    }

    dc.DestroyClippingRegion();
}


// -----------------------------------------------------------------------------
// mpProfile implementation
// -----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS( mpProfile, mpLayer )

mpProfile::mpProfile( const wxString& name, int flags )
{
    SetName( name );
    m_flags = flags;
    m_type  = mpLAYER_PLOT;
}


void mpProfile::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );

        wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
        wxCoord endPx   = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

        // Plot profile linking subsequent point of the profile, instead of mpFY, which plots simple points.
        for( wxCoord i = startPx; i < endPx; ++i )
        {
            wxCoord c0  = w.y2p( GetY( w.p2x( i ) ) );      // (wxCoord) ((w.GetYpos() - GetY( (double)i / w.GetXscl() + w.GetXpos()) ) * w.GetYscl());
            wxCoord c1  = w.y2p( GetY( w.p2x( i + 1 ) ) );  // (wxCoord) ((w.GetYpos() - GetY( (double)(i+1) / w.GetXscl() + (w.GetXpos() ) ) ) * w.GetYscl());

            // c0 = (c0 <= maxYpx) ? ((c0 >= minYpx) ? c0 : minYpx) : maxYpx;
            // c1 = (c1 <= maxYpx) ? ((c1 >= minYpx) ? c1 : minYpx) : maxYpx;
            if( !m_drawOutsideMargins )
            {
                c0  = (c0 <= maxYpx) ? ( (c0 >= minYpx) ? c0 : minYpx ) : maxYpx;
                c1  = (c1 <= maxYpx) ? ( (c1 >= minYpx) ? c1 : minYpx ) : maxYpx;
            }

            dc.DrawLine( i, c0, i + 1, c1 );
        }

        ;

        if( !m_name.IsEmpty() )
        {
            dc.SetFont( m_font );

            wxCoord tx, ty;
            dc.GetTextExtent( m_name, &tx, &ty );

            if( (m_flags & mpALIGNMASK) == mpALIGN_RIGHT )
                tx = (w.GetScrX() - tx) - w.GetMarginRight() - 8;
            else if( (m_flags & mpALIGNMASK) == mpALIGN_CENTER )
                tx = ( (w.GetScrX() - w.GetMarginRight() - w.GetMarginLeft() - tx) / 2 ) +
                     w.GetMarginLeft();
            else
                tx = w.GetMarginLeft() + 8;

            dc.DrawText( m_name, tx, w.y2p( GetY( w.p2x( tx ) ) ) );    // (wxCoord) ((w.GetPosY() - GetY( (double)tx / w.GetScaleX() + w.GetPosX())) * w.GetScaleY()) );
        }
    }
}


// -----------------------------------------------------------------------------
// mpLayer implementations - furniture (scales, ...)
// -----------------------------------------------------------------------------

#define mpLN10 2.3025850929940456840179914546844

void mpScaleX::recalculateTicks( wxDC& dc, mpWindow& w )
{
    double minV, maxV, minVvis, maxVvis;

    GetDataRange( minV, maxV );
    getVisibleDataRange( w, minVvis, maxVvis );

    m_absVisibleMaxV = std::max( std::abs( minVvis ), std::abs( maxVvis ) );

    m_tickValues.clear();
    m_tickLabels.clear();

    double  minErr = 1000000000000.0;
    double  bestStep = 1.0;

    for( int i = 10; i <= 20; i += 2 )
    {
        double  curr_step    = fabs( maxVvis - minVvis ) / (double) i;
        double  base    = pow( 10, floor( log10( curr_step ) ) );
        double  stepInt = floor( curr_step / base ) * base;
        double  err = fabs( curr_step - stepInt );

        if( err < minErr )
        {
            minErr = err;
            bestStep = stepInt;
        }
    }


    double v = floor( minVvis / bestStep ) * bestStep;

    double zeroOffset = 100000000.0;

    while( v < maxVvis )
    {
        m_tickValues.push_back( v );

        if( fabs( v ) < zeroOffset )
            zeroOffset = fabs( v );

        v += bestStep;
    }

    if( zeroOffset <= bestStep )
    {
        for( double& t : m_tickValues )
            t -= zeroOffset;
    }

    for( double t : m_tickValues )
    {
        m_tickLabels.emplace_back( t );
    }

    updateTickLabels( dc, w );
}


mpScaleBase::mpScaleBase()
{
    m_rangeSet = false;
    m_nameFlags = mpALIGN_BORDER_BOTTOM;

    // initialize these members mainly to avoid not initialized values
    m_offset = 0.0;
    m_scale = 1.0;
    m_absVisibleMaxV = 0.0;
    m_flags = 0;            // Flag for axis alignment
    m_ticks = true;         // Flag to toggle between ticks or grid
    m_minV = 0.0;
    m_maxV = 0.0;
    m_maxLabelHeight = 1;
    m_maxLabelWidth = 1;
}


void mpScaleBase::computeLabelExtents( wxDC& dc, mpWindow& w )
{
    m_maxLabelHeight    = 0;
    m_maxLabelWidth     = 0;

    for( int n = 0; n < labelCount(); n++ )
    {
        int tx, ty;
        const wxString s = getLabel( n );

        dc.GetTextExtent( s, &tx, &ty );
        m_maxLabelHeight    = std::max( ty, m_maxLabelHeight );
        m_maxLabelWidth     = std::max( tx, m_maxLabelWidth );
    }
}


void mpScaleBase::updateTickLabels( wxDC& dc, mpWindow& w )
{
    formatLabels();
    computeLabelExtents( dc, w );

    // int gap = IsHorizontal() ? m_maxLabelWidth + 10 : m_maxLabelHeight + 5;

    // if ( m_tickLabels.size() <= 2)
    // return;

    /*
     *  fixme!
     *
     *  for ( auto &l : m_tickLabels )
     *  {
     *  double p = TransformToPlot ( l.pos );
     *
     *  if ( !IsHorizontal() )
     *  l.pixelPos = (int)(( w.GetPosY() - p ) * w.GetScaleY());
     *  else
     *  l.pixelPos = (int)(( p - w.GetPosX()) * w.GetScaleX());
     *  }
     *
     *
     *  for (int i = 1; i  < m_tickLabels.size() - 1; i++)
     *  {
     *  int dist_prev;
     *
     *  for(int j = i-1; j >= 1; j--)
     *  {
     *  if( m_tickLabels[j].visible)
     *  {
     *  dist_prev = abs( m_tickLabels[j].pixelPos - m_tickLabels[i].pixelPos );
     *  break;
     *  }
     *  }
     *
     *  if (dist_prev < gap)
     *  m_tickLabels[i].visible = false;
     *  }
     */
}


void mpScaleY::getVisibleDataRange( mpWindow& w, double& minV, double& maxV )
{
    wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
    wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

    double  pymin   = w.p2y( minYpx );
    double  pymax   = w.p2y( maxYpx );

    minV    = TransformFromPlot( pymax );
    maxV    = TransformFromPlot( pymin );
}


void mpScaleY::computeSlaveTicks( mpWindow& w )
{
    if( m_masterScale->m_tickValues.size() == 0 )
        return;

    m_tickValues.clear();
    m_tickLabels.clear();

    double  p0 = m_masterScale->TransformToPlot( m_masterScale->m_tickValues[0] );
    double  p1 = m_masterScale->TransformToPlot( m_masterScale->m_tickValues[1] );

    m_scale     = 1.0 / ( m_maxV - m_minV );
    m_offset    = -m_minV;

    double  y_slave0    = p0 / m_scale;
    double  y_slave1    = p1 / m_scale;

    double  dy_slave    = (y_slave1 - y_slave0);
    double  exponent    = floor( log10( dy_slave ) );
    double  base = dy_slave / pow( 10.0, exponent );

    double dy_scaled = ceil( 2.0 * base ) / 2.0 * pow( 10.0, exponent );

    double minvv, maxvv;

    getVisibleDataRange( w, minvv, maxvv );

    minvv = floor( minvv / dy_scaled ) * dy_scaled;

    m_scale = 1.0 / ( m_maxV - m_minV );
    m_scale *= dy_slave / dy_scaled;

    m_offset = p0 / m_scale - minvv;

    m_tickValues.clear();

    double m;

    m_absVisibleMaxV = 0;

    for( unsigned int i = 0; i < m_masterScale->m_tickValues.size(); i++ )
    {
        m = TransformFromPlot( m_masterScale->TransformToPlot( m_masterScale->m_tickValues[i] ) );
        m_tickValues.push_back( m );
        m_tickLabels.emplace_back( m );
        m_absVisibleMaxV = std::max( m_absVisibleMaxV, fabs( m ) );
    }
}


void mpScaleY::recalculateTicks( wxDC& dc, mpWindow& w )
{
    if( m_masterScale )
    {
        computeSlaveTicks( w );
        updateTickLabels( dc, w );

        return;
    }

    double minV, maxV, minVvis, maxVvis;
    GetDataRange( minV, maxV );
    getVisibleDataRange( w, minVvis, maxVvis );

    m_absVisibleMaxV = std::max( std::abs( minVvis ), std::abs( maxVvis ) );
    m_tickValues.clear();
    m_tickLabels.clear();

    double  minErr = 1000000000000.0;
    double  bestStep = 1.0;

    for( int i = 10; i <= 20; i += 2 )
    {
        double  curr_step    = fabs( maxVvis - minVvis ) / (double) i;
        double  base    = pow( 10, floor( log10( curr_step ) ) );
        double  stepInt = floor( curr_step / base ) * base;
        double  err = fabs( curr_step - stepInt );

        if( err< minErr )
        {
            minErr = err;
            bestStep = stepInt;
        }
    }


    double v = floor( minVvis / bestStep ) * bestStep;

    double zeroOffset = 100000000.0;

    const int iterLimit = 1000;
    int i = 0;

    while( v < maxVvis && i < iterLimit )
    {
        m_tickValues.push_back( v );

        if( fabs( v ) < zeroOffset )
            zeroOffset = fabs( v );

        v += bestStep;
        i++;
    }


    // something weird happened...
    if( i == iterLimit )
    {
        m_tickValues.clear();
    }

    if( zeroOffset <= bestStep )
    {
        for( double& t : m_tickValues )
            t -= zeroOffset;
    }

    for( double t : m_tickValues )
        m_tickLabels.emplace_back( t );


    // n0 = floor(minVvis / bestStep) * bestStep;
    // end = n0 +

    // n0 = floor( (w.GetPosX() ) / step ) * step ;
    updateTickLabels( dc, w );

    // labelStep = ceil(((double) m_maxLabelWidth + mpMIN_X_AXIS_LABEL_SEPARATION)/(w.GetScaleX()*step))*step;
}


void mpScaleXBase::getVisibleDataRange( mpWindow& w, double& minV, double& maxV )
{
    wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
    wxCoord endPx = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
    double  pxmin   = w.p2x( startPx );
    double  pxmax   = w.p2x( endPx );

    minV    = TransformFromPlot( pxmin );
    maxV    = TransformFromPlot( pxmax );
}


void mpScaleXLog::recalculateTicks( wxDC& dc, mpWindow& w )
{
    double minV, maxV, minVvis, maxVvis;

    GetDataRange( minV, maxV );
    getVisibleDataRange( w, minVvis, maxVvis );

    // double decades = log( maxV / minV ) / log(10);
    double  minDecade   = pow( 10, floor( log10( minV ) ) );
    double  maxDecade   = pow( 10, ceil( log10( maxV ) ) );
    double visibleDecades = log( maxVvis / minVvis ) / log( 10 );

    double d;

    m_tickValues.clear();
    m_tickLabels.clear();

    if( minDecade == 0.0 )
        return;


    for( d = minDecade; d<=maxDecade; d *= 10.0 )
    {
        m_tickLabels.emplace_back( d );

        for( double dd = d; dd < d * 10; dd += d )
        {
            if( visibleDecades < 2 )
                m_tickLabels.emplace_back( dd );

            m_tickValues.push_back( dd );
        }
    }

    updateTickLabels( dc, w );
}


IMPLEMENT_ABSTRACT_CLASS( mpScaleXBase, mpLayer )
IMPLEMENT_DYNAMIC_CLASS( mpScaleX, mpScaleXBase )
IMPLEMENT_DYNAMIC_CLASS( mpScaleXLog, mpScaleXBase )

mpScaleXBase::mpScaleXBase( const wxString& name, int flags, bool ticks, unsigned int type )
{
    SetName( name );
    SetFont( (wxFont&) *wxSMALL_FONT );
    SetPen( (wxPen&) *wxGREY_PEN );
    m_flags = flags;
    m_ticks = ticks;
    // m_labelType = type;
    m_type = mpLAYER_AXIS;
}


mpScaleX::mpScaleX( const wxString& name, int flags, bool ticks, unsigned int type ) :
    mpScaleXBase( name, flags, ticks, type )
{
}


mpScaleXLog::mpScaleXLog( const wxString& name, int flags, bool ticks, unsigned int type ) :
    mpScaleXBase( name, flags, ticks, type )
{
}


void mpScaleXBase::Plot( wxDC& dc, mpWindow& w )
{
    int tx, ty;

    m_offset    = -m_minV;
    m_scale     = 1.0 / ( m_maxV - m_minV );

    recalculateTicks( dc, w );

    if( m_visible )
    {
        dc.SetPen( m_pen );
        dc.SetFont( m_font );
        int orgy = 0;

        const int extend = w.GetScrX();    ///2;

        if( m_flags == mpALIGN_CENTER )
            orgy = w.y2p( 0 );    // (int)(w.GetPosY() * w.GetScaleY());

        if( m_flags == mpALIGN_TOP )
        {
            if( m_drawOutsideMargins )
                orgy = X_BORDER_SEPARATION;
            else
                orgy = w.GetMarginTop();
        }

        if( m_flags == mpALIGN_BOTTOM )
        {
            if( m_drawOutsideMargins )
                orgy = X_BORDER_SEPARATION;
            else
                orgy = w.GetScrY() - w.GetMarginBottom();
        }

        if( m_flags == mpALIGN_BORDER_BOTTOM )
            orgy = w.GetScrY() - 1;    // dc.LogicalToDeviceY(0) - 1;

        if( m_flags == mpALIGN_BORDER_TOP )
            orgy = 1;    // -dc.LogicalToDeviceY(0);

        // dc.DrawLine( 0, orgy, w.GetScrX(), orgy);

        wxCoord startPx = m_drawOutsideMargins ? 0 : w.GetMarginLeft();
        wxCoord endPx   = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();

        // int tmp=-65535;
        int labelH = m_maxLabelHeight;    // Control labels height to decide where to put axis name (below labels or on top of axis)

        // int maxExtent = tc.MaxLabelWidth();
        for( int n = 0; n < tickCount(); n++ )
        {
            double tp = getTickPos( n );

            // double xlogmin = log10 ( m_minV );
            // double xlogmax = log10 ( m_maxV );

            double px = TransformToPlot( tp );    // ( log10 ( tp ) - xlogmin) / (xlogmax - xlogmin);

            const int p = (int) ( ( px - w.GetPosX() ) * w.GetScaleX() );

            if( (p >= startPx) && (p <= endPx) )
            {
                if( m_ticks )    // draw axis ticks
                {
                    if( m_flags == mpALIGN_BORDER_BOTTOM )
                        dc.DrawLine( p, orgy, p, orgy - 4 );
                    else
                        dc.DrawLine( p, orgy, p, orgy + 4 );
                }
                else     // draw grid dotted lines
                {
                    m_pen.SetStyle( wxPENSTYLE_DOT );
                    dc.SetPen( m_pen );

                    if( (m_flags == mpALIGN_BOTTOM) && !m_drawOutsideMargins )
                    {
                        m_pen.SetStyle( wxPENSTYLE_DOT );
                        dc.SetPen( m_pen );
                        dc.DrawLine( p, orgy + 4, p, minYpx );
                        m_pen.SetStyle( wxPENSTYLE_SOLID );
                        dc.SetPen( m_pen );
                        dc.DrawLine( p, orgy + 4, p, orgy - 4 );
                    }
                    else
                    {
                        if( (m_flags == mpALIGN_TOP) && !m_drawOutsideMargins )
                        {
                            dc.DrawLine( p, orgy - 4, p, maxYpx );
                        }
                        else
                        {
                            dc.DrawLine( p, minYpx, p, maxYpx );    // 0/*-w.GetScrY()*/, p, w.GetScrY() );
                        }
                    }

                    m_pen.SetStyle( wxPENSTYLE_SOLID );
                    dc.SetPen( m_pen );
                }
            }
        }

        m_pen.SetStyle( wxPENSTYLE_SOLID );
        dc.SetPen( m_pen );
        dc.DrawLine( startPx, minYpx, endPx, minYpx );
        dc.DrawLine( startPx, maxYpx, endPx, maxYpx );

        // Actually draw labels, taking care of not overlapping them, and distributing them
        // regularly
        for( int n = 0; n < labelCount(); n++ )
        {
            double tp = getLabelPos( n );

            if( !m_tickLabels[n].visible )
                continue;

            // double xlogmin = log10 ( m_minV );
            // double xlogmax = log10 ( m_maxV );

            double px = TransformToPlot( tp );  // ( log10 ( tp ) - xlogmin) / (xlogmax - xlogmin);

            const int p = (int) ( ( px - w.GetPosX() ) * w.GetScaleX() );

            if( (p >= startPx) && (p <= endPx) )
            {
                // Write ticks labels in s string
                wxString s = m_tickLabels[n].label;

                dc.GetTextExtent( s, &tx, &ty );

                if( (m_flags == mpALIGN_BORDER_BOTTOM) || (m_flags == mpALIGN_TOP) )
                {
                    dc.DrawText( s, p - tx / 2, orgy - 4 - ty );
                }
                else
                {
                    dc.DrawText( s, p - tx / 2, orgy + 4 );
                }
            }
        }

        // Draw axis name
        dc.GetTextExtent( m_name, &tx, &ty );

        switch( m_nameFlags )
        {
        case mpALIGN_BORDER_BOTTOM:
            dc.DrawText( m_name, extend - tx - 4, orgy - 8 - ty - labelH );
            break;

        case mpALIGN_BOTTOM:
        {
            dc.DrawText( m_name, (endPx + startPx) / 2 - tx / 2, orgy + 6 + labelH );
        }
        break;

        case mpALIGN_CENTER:
            dc.DrawText( m_name, extend - tx - 4, orgy - 4 - ty );
            break;

        case mpALIGN_TOP:
        {
            if( (!m_drawOutsideMargins) && ( w.GetMarginTop() > (ty + labelH + 8) ) )
            {
                dc.DrawText( m_name, (endPx - startPx - tx) >> 1, orgy - 6 - ty - labelH );
            }
            else
            {
                dc.DrawText( m_name, extend - tx - 4, orgy + 4 );
            }
        }
        break;

        case mpALIGN_BORDER_TOP:
            dc.DrawText( m_name, extend - tx - 4, orgy + 6 + labelH );
            break;

        default:
            break;
        }
    }
}


IMPLEMENT_DYNAMIC_CLASS( mpScaleY, mpLayer )

mpScaleY::mpScaleY( const wxString& name, int flags, bool ticks )
{
    SetName( name );
    SetFont( (wxFont&) *wxSMALL_FONT );
    SetPen( (wxPen&) *wxGREY_PEN );
    m_flags = flags;
    m_ticks = ticks;
    m_type  = mpLAYER_AXIS;
    m_masterScale = NULL;
    m_nameFlags = mpALIGN_BORDER_LEFT;
}


void mpScaleY::Plot( wxDC& dc, mpWindow& w )
{
    m_offset    = -m_minV;
    m_scale     = 1.0 / ( m_maxV - m_minV );

    recalculateTicks( dc, w );

    if( m_visible )
    {
        dc.SetPen( m_pen );
        dc.SetFont( m_font );

        int orgx = 0;

        // const int extend = w.GetScrY(); // /2;
        if( m_flags == mpALIGN_CENTER )
            orgx = w.x2p( 0 );    // (int)(w.GetPosX() * w.GetScaleX());

        if( m_flags == mpALIGN_LEFT )
        {
            if( m_drawOutsideMargins )
                orgx = Y_BORDER_SEPARATION;
            else
                orgx = w.GetMarginLeft();
        }

        if( m_flags == mpALIGN_RIGHT )
        {
            if( m_drawOutsideMargins )
                orgx = w.GetScrX() - Y_BORDER_SEPARATION;
            else
                orgx = w.GetScrX() - w.GetMarginRight();
        }

        if( m_flags == mpALIGN_BORDER_RIGHT )
            orgx = w.GetScrX() - 1;    // dc.LogicalToDeviceX(0) - 1;

        if( m_flags == mpALIGN_BORDER_LEFT )
            orgx = 1;    // -dc.LogicalToDeviceX(0);

        wxCoord endPx = m_drawOutsideMargins ? w.GetScrX() : w.GetScrX() - w.GetMarginRight();
        wxCoord minYpx  = m_drawOutsideMargins ? 0 : w.GetMarginTop();
        wxCoord maxYpx  = m_drawOutsideMargins ? w.GetScrY() : w.GetScrY() - w.GetMarginBottom();
        // Draw line
        dc.DrawLine( orgx, minYpx, orgx, maxYpx );


        wxCoord tx, ty;
        wxString    s;
        wxString    fmt;
        int n = 0;


        int labelW = 0;
        // Before staring cycle, calculate label height
        int labelHeight = 0;
        s.Printf( fmt, n );
        dc.GetTextExtent( s, &tx, &labelHeight );

        for( n = 0; n < tickCount(); n++ )
        {
            double tp = getTickPos( n );

            double py = TransformToPlot( tp );  // ( log10 ( tp ) - xlogmin) / (xlogmax - xlogmin);
            const int p = (int) ( ( w.GetPosY() - py ) * w.GetScaleY() );


            if( (p >= minYpx) && (p <= maxYpx) )
            {
                if( m_ticks )    // Draw axis ticks
                {
                    if( m_flags == mpALIGN_BORDER_LEFT )
                    {
                        dc.DrawLine( orgx, p, orgx + 4, p );
                    }
                    else
                    {
                        dc.DrawLine( orgx - 4, p, orgx, p );    // ( orgx, p, orgx+4, p);
                    }
                }
                else
                {
                    dc.DrawLine( orgx - 4, p, orgx + 4, p );

                    m_pen.SetStyle( wxPENSTYLE_DOT );
                    dc.SetPen( m_pen );

                    dc.DrawLine( orgx - 4, p, endPx, p );

                    m_pen.SetStyle( wxPENSTYLE_SOLID );
                    dc.SetPen( m_pen );
                }

                // Print ticks labels
            }
        }

        for( n = 0; n < labelCount(); n++ )
        {
            double tp = getLabelPos( n );

            double py = TransformToPlot( tp );  // ( log10 ( tp ) - xlogmin) / (xlogmax - xlogmin);
            const int p = (int) ( ( w.GetPosY() - py ) * w.GetScaleY() );

            if( !m_tickLabels[n].visible )
                continue;

            if( (p >= minYpx) && (p <= maxYpx) )
            {
                s = getLabel( n );
                dc.GetTextExtent( s, &tx, &ty );

                if( (m_flags == mpALIGN_BORDER_LEFT) || (m_flags == mpALIGN_RIGHT) )
                    dc.DrawText( s, orgx + 4, p - ty / 2 );
                else
                    dc.DrawText( s, orgx - 4 - tx, p - ty / 2 );    // ( s, orgx+4, p-ty/2);
            }
        }

        // Draw axis name
        // Draw axis name

        dc.GetTextExtent( m_name, &tx, &ty );

        switch( m_nameFlags )
        {
        case mpALIGN_BORDER_LEFT:
            dc.DrawText( m_name, labelW + 8, 4 );
            break;

        case mpALIGN_LEFT:
        {
            // if ((!m_drawOutsideMargins) && (w.GetMarginLeft() > (ty + labelW + 8))) {
            // dc.DrawRotatedText( m_name, orgx - 6 - labelW - ty, (maxYpx + minYpx) / 2 + tx / 2, 90);
            // } else {
            dc.DrawText( m_name, orgx + 4, minYpx - ty - 4 );
            // }
        }
        break;

        case mpALIGN_CENTER:
            dc.DrawText( m_name, orgx + 4, 4 );
            break;

        case mpALIGN_RIGHT:
        {
            // dc.DrawRotatedText( m_name, orgx + 6, (maxYpx + minYpx) / 2 + tx / 2, 90);

            /*if ((!m_drawOutsideMargins) && (w.GetMarginRight() > (ty + labelW + 8))) {
             *  dc.DrawRotatedText( m_name, orgx + 6 + labelW, (maxYpx - minYpx + tx)>>1, 90);
             *  } else {*/
            dc.DrawText( m_name, orgx - tx - 4, minYpx - ty - 4 );
            // }
        }
        break;

        case mpALIGN_BORDER_RIGHT:
            dc.DrawText( m_name, orgx - 6 - tx - labelW, 4 );
            break;

        default:
            break;
        }
    }
}


// -----------------------------------------------------------------------------
// mpWindow
// -----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS( mpWindow, wxWindow )

BEGIN_EVENT_TABLE( mpWindow, wxWindow )
EVT_PAINT( mpWindow::OnPaint )
EVT_SIZE( mpWindow::OnSize )
EVT_SCROLLWIN_THUMBTRACK( mpWindow::OnScrollThumbTrack )
EVT_SCROLLWIN_PAGEUP( mpWindow::OnScrollPageUp )
EVT_SCROLLWIN_PAGEDOWN( mpWindow::OnScrollPageDown )
EVT_SCROLLWIN_LINEUP( mpWindow::OnScrollLineUp )
EVT_SCROLLWIN_LINEDOWN( mpWindow::OnScrollLineDown )
EVT_SCROLLWIN_TOP( mpWindow::OnScrollTop )
EVT_SCROLLWIN_BOTTOM( mpWindow::OnScrollBottom )

EVT_MIDDLE_DOWN( mpWindow::OnMouseMiddleDown )  // JLB
EVT_RIGHT_UP( mpWindow::OnShowPopupMenu )
EVT_MOUSEWHEEL( mpWindow::OnMouseWheel )        // JLB
#if wxCHECK_VERSION( 3, 1, 0 ) || defined( USE_OSX_MAGNIFY_EVENT )
EVT_MAGNIFY( mpWindow::OnMagnify )
#endif
EVT_MOTION( mpWindow::OnMouseMove )             // JLB
EVT_LEFT_DOWN( mpWindow::OnMouseLeftDown )
EVT_LEFT_UP( mpWindow::OnMouseLeftRelease )

EVT_MENU( mpID_CENTER, mpWindow::OnCenter )
EVT_MENU( mpID_FIT, mpWindow::OnFit )
EVT_MENU( mpID_ZOOM_IN, mpWindow::OnZoomIn )
EVT_MENU( mpID_ZOOM_OUT, mpWindow::OnZoomOut )
EVT_MENU( mpID_LOCKASPECT, mpWindow::OnLockAspect )
END_EVENT_TABLE()

mpWindow::mpWindow() : wxWindow(),
        m_lockaspect( false ),
        m_minX( 0.0 ),
        m_maxX( 0.0 ),
        m_minY( 0.0 ),
        m_maxY( 0.0 ),
        m_scaleX( 1.0 ),
        m_scaleY( 1.0 ),
        m_posX( 0.0 ),
        m_posY( 0.0 ),
        m_scrX( 64 ),
        m_scrY( 64 ),
        m_clickedX( 0 ),
        m_clickedY( 0 ),
        m_desiredXmin( 0.0 ),
        m_desiredXmax( 1.0 ),
        m_desiredYmin( 0.0 ),
        m_desiredYmax( 1.0 ),
        m_marginTop( 0 ),
        m_marginRight( 0 ),
        m_marginBottom( 0 ),
        m_marginLeft( 0 ),
        m_last_lx( 0 ),
        m_last_ly( 0 ),
        m_buff_bmp( nullptr ),
        m_enableDoubleBuffer( false ),
        m_enableMouseNavigation( true ),
        m_enableMouseWheelPan( false ),
        m_enableLimitedView( false ),
        m_enableScrollBars( false ),
        m_movingInfoLayer( nullptr ),
        m_zooming( false )
{
}

mpWindow::mpWindow( wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        long flag )
    : wxWindow( parent, id, pos, size, flag, wxT( "mathplot" ) )
{
    m_zooming   = false;
    m_scaleX    = m_scaleY = 1.0;
    m_posX = m_posY = 0;
    m_desiredXmin   = m_desiredYmin = 0;
    m_desiredXmax   = m_desiredYmax = 1;
    m_scrX  = m_scrY = 64;    // Fixed from m_scrX = m_scrX = 64;
    m_minX  = m_minY = 0;
    m_maxX  = m_maxY = 0;
    m_last_lx   = m_last_ly = 0;
    m_buff_bmp  = NULL;
    m_enableDoubleBuffer = false;
    m_enableMouseNavigation = true;
    m_enableLimitedView = false;
    m_movingInfoLayer = NULL;
    // Set margins to 0
    m_marginTop = 0; m_marginRight = 0; m_marginBottom = 0; m_marginLeft = 0;


    m_lockaspect = false;

    m_popmenu.Append( mpID_CENTER, _( "Center on Cursor" ), _( "Center plot view to this position" ) );
    m_popmenu.Append( mpID_FIT, _( "Fit on Screen" ), _( "Set plot view to show all items" ) );
    m_popmenu.Append( mpID_ZOOM_IN, _( "Zoom In" ), _( "Zoom in plot view." ) );
    m_popmenu.Append( mpID_ZOOM_OUT, _( "Zoom Out" ), _( "Zoom out plot view." ) );
    // m_popmenu.AppendCheckItem( mpID_LOCKASPECT, _("Lock aspect"), _("Lock horizontal and vertical zoom aspect."));
    // m_popmenu.Append( mpID_HELP_MOUSE,   _("Show mouse commands..."),    _("Show help about the mouse commands."));

    m_layers.clear();
    SetBackgroundColour( *wxWHITE );
    m_bgColour  = *wxWHITE;
    m_fgColour  = *wxBLACK;

    m_enableScrollBars = false;
    SetSizeHints( 128, 128 );

    // J.L.Blanco: Eliminates the "flick" with the double buffer.
    SetBackgroundStyle( wxBG_STYLE_CUSTOM );

    UpdateAll();
}


mpWindow::~mpWindow()
{
    // Free all the layers:
    DelAllLayers( true, false );

    if( m_buff_bmp )
    {
        delete m_buff_bmp;
        m_buff_bmp = NULL;
    }
}


// Mouse handler, for detecting when the user drag with the right button or just "clicks" for the menu
// JLB
void mpWindow::OnMouseMiddleDown( wxMouseEvent& event )
{
    m_mouseMClick.x = event.GetX();
    m_mouseMClick.y = event.GetY();
}


#if wxCHECK_VERSION( 3, 1, 0 ) || defined( USE_OSX_MAGNIFY_EVENT )
void mpWindow::OnMagnify( wxMouseEvent& event )
{
    if( !m_enableMouseNavigation )
    {
        event.Skip();
        return;
    }

    float   zoom = event.GetMagnification() + 1.0f;
    wxPoint pos( event.GetX(), event.GetY() );
    if( zoom > 1.0f )
        ZoomIn( pos, zoom );
    else if( zoom < 1.0f )
        ZoomOut( pos, 1.0f / zoom );
}
#endif


// Process mouse wheel events
// JLB
void mpWindow::OnMouseWheel( wxMouseEvent& event )
{
    if( !m_enableMouseNavigation )
    {
        event.Skip();
        return;
    }

    int       change = event.GetWheelRotation();
    const int axis = event.GetWheelAxis();
    double    changeUnitsX = change / m_scaleX;
    double    changeUnitsY = change / m_scaleY;

    if( ( !m_enableMouseWheelPan && ( event.ControlDown() || event.ShiftDown() ) )
            || ( m_enableMouseWheelPan && !event.ControlDown() ) )
    {
        // Scrolling
        if( m_enableMouseWheelPan )
        {
            if( axis == wxMOUSE_WHEEL_HORIZONTAL || event.ShiftDown() )
                SetXView( m_posX + changeUnitsX, m_desiredXmax + changeUnitsX,
                        m_desiredXmin + changeUnitsX );
            else
                SetYView( m_posY + changeUnitsY, m_desiredYmax + changeUnitsY,
                        m_desiredYmin + changeUnitsY );
        }
        else
        {
            if( event.ControlDown() )
                SetXView( m_posX + changeUnitsX, m_desiredXmax + changeUnitsX,
                        m_desiredXmin + changeUnitsX );
            else
                SetYView( m_posY + changeUnitsY, m_desiredYmax + changeUnitsY,
                        m_desiredYmin + changeUnitsY );
        }

        UpdateAll();
    }
    else
    {
        // zoom in/out
        wxPoint clickPt( event.GetX(), event.GetY() );

        if( event.GetWheelRotation() > 0 )
            ZoomIn( clickPt );
        else
            ZoomOut( clickPt );

        return;
    }
}


// If the user "drags" with the right button pressed, do "pan"
// JLB
void mpWindow::OnMouseMove( wxMouseEvent& event )
{
    if( !m_enableMouseNavigation )
    {
        event.Skip();
        return;
    }

    if( event.m_middleDown )
    {
        // The change:
        int Ax  = m_mouseMClick.x - event.GetX();
        int Ay  = m_mouseMClick.y - event.GetY();

        // For the next event, use relative to this coordinates.
        m_mouseMClick.x = event.GetX();
        m_mouseMClick.y = event.GetY();

        double  Ax_units    = Ax / m_scaleX;
        double  Ay_units    = -Ay / m_scaleY;

        bool updateRequired = false;
        updateRequired |= SetXView( m_posX + Ax_units,
                                    m_desiredXmax + Ax_units,
                                    m_desiredXmin + Ax_units );
        updateRequired |= SetYView( m_posY + Ay_units,
                                    m_desiredYmax + Ay_units,
                                    m_desiredYmin + Ay_units );

        if( updateRequired )
            UpdateAll();
    }
    else
    {
        if( event.m_leftDown )
        {
            if( m_movingInfoLayer == NULL )
            {
                wxClientDC dc( this );
                wxPen pen( m_fgColour, 1, wxPENSTYLE_DOT );
                dc.SetPen( pen );
                dc.SetBrush( *wxTRANSPARENT_BRUSH );
                dc.DrawRectangle( m_mouseLClick.x, m_mouseLClick.y,
                        event.GetX() - m_mouseLClick.x, event.GetY() - m_mouseLClick.y );
                m_zooming = true;
                m_zoomRect.x    = m_mouseLClick.x;
                m_zoomRect.y    = m_mouseLClick.y;
                m_zoomRect.width    = event.GetX() - m_mouseLClick.x;
                m_zoomRect.height   = event.GetY() - m_mouseLClick.y;
            }
            else
            {
                wxPoint moveVector( event.GetX() - m_mouseLClick.x, event.GetY() - m_mouseLClick.y );
                m_movingInfoLayer->Move( moveVector );
                m_zooming = false;
            }

            UpdateAll();
        }
        else
        {
#if 0
            wxLayerList::iterator li;

            for( li = m_layers.begin(); li != m_layers.end(); li++ )
            {
                if( (*li)->IsInfo() && (*li)->IsVisible() )
                {
                    mpInfoLayer* tmpLyr = (mpInfoLayer*) (*li);
                    tmpLyr->UpdateInfo( *this, event );
                    // UpdateAll();
                    RefreshRect( tmpLyr->GetRectangle() );
                }
            }

#endif
            /* if (m_coordTooltip) {
             *  wxString toolTipContent;
             *  toolTipContent.Printf( "X = %f\nY = %f", p2x(event.GetX()), p2y(event.GetY()));
             *  wxTipWindow** ptr = NULL;
             *  wxRect rectBounds(event.GetX(), event.GetY(), 5, 5);
             *  wxTipWindow* tip = new wxTipWindow(this, toolTipContent, 100, ptr, &rectBounds);
             *
             *  } */
        }
    }

    event.Skip();
}


void mpWindow::OnMouseLeftDown( wxMouseEvent& event )
{
    m_mouseLClick.x = event.GetX();
    m_mouseLClick.y = event.GetY();
    m_zooming = true;
    wxPoint pointClicked = event.GetPosition();
    m_movingInfoLayer = IsInsideInfoLayer( pointClicked );

    event.Skip();
}


void mpWindow::OnMouseLeftRelease( wxMouseEvent& event )
{
    wxPoint release( event.GetX(), event.GetY() );
    wxPoint press( m_mouseLClick.x, m_mouseLClick.y );

    m_zooming = false;

    if( m_movingInfoLayer != NULL )
    {
        m_movingInfoLayer->UpdateReference();
        m_movingInfoLayer = NULL;
    }
    else
    {
        if( release != press )
            ZoomRect( press, release );
    }

    event.Skip();
}


void mpWindow::Fit()
{
    if( UpdateBBox() )
        Fit( m_minX, m_maxX, m_minY, m_maxY );
}


// JL
void mpWindow::Fit( double xMin, double xMax, double yMin, double yMax,
                    wxCoord* printSizeX, wxCoord* printSizeY )
{
    // Save desired borders:
    m_desiredXmin   = xMin; m_desiredXmax = xMax;
    m_desiredYmin   = yMin; m_desiredYmax = yMax;

    // Give a small margin to plot area
    double  xExtra = fabs( xMax - xMin ) * 0.00;
    double  yExtra = fabs( yMax - yMin ) * 0.03;

    xMin    -= xExtra;
    xMax    += xExtra;
    yMin    -= yExtra;
    yMax    += yExtra;

    if( printSizeX != NULL && printSizeY != NULL )
    {
        // Printer:
        m_scrX  = *printSizeX;
        m_scrY  = *printSizeY;
    }
    else
    {
        // Normal case (screen):
        GetClientSize( &m_scrX, &m_scrY );
    }

    double Ax, Ay;

    Ax  = xMax - xMin;
    Ay  = yMax - yMin;

    m_scaleX    = (Ax != 0) ? (m_scrX - m_marginLeft - m_marginRight)  / Ax : 1;   // m_scaleX = (Ax != 0) ? m_scrX / Ax : 1;
    m_scaleY    = (Ay != 0) ? (m_scrY - m_marginTop  - m_marginBottom) / Ay : 1;   // m_scaleY = (Ay != 0) ? m_scrY / Ay : 1;

    if( m_lockaspect )
    {
        // Keep the lowest "scale" to fit the whole range required by that axis (to actually
        // "fit"!):
        double s = m_scaleX < m_scaleY ? m_scaleX : m_scaleY;
        m_scaleX    = s;
        m_scaleY    = s;
    }

    // Adjusts corner coordinates: This should be simply:
    // m_posX = m_minX;
    // m_posY = m_maxY;
    // But account for centering if we have lock aspect:
    m_posX = (xMin + xMax) / 2 - ( (m_scrX - m_marginLeft - m_marginRight) / 2 + m_marginLeft ) /
             m_scaleX;                                                                             // m_posX = (xMin+xMax)/2 - (m_scrX/2)/m_scaleX;
    // m_posY = (yMin+yMax)/2 + ((m_scrY - m_marginTop - m_marginBottom)/2 - m_marginTop)/m_scaleY;  // m_posY = (yMin+yMax)/2 + (m_scrY/2)/m_scaleY;
    m_posY = (yMin + yMax) / 2 + ( (m_scrY - m_marginTop - m_marginBottom) / 2 + m_marginTop ) /
             m_scaleY;                                                                            // m_posY = (yMin+yMax)/2 + (m_scrY/2)/m_scaleY;

    // It is VERY IMPORTANT to DO NOT call Refresh if we are drawing to the printer!!
    // Otherwise, the DC dimensions will be those of the window instead of the printer device
    if( printSizeX == NULL || printSizeY == NULL )
        UpdateAll();
}


// Patch ngpaton
void mpWindow::DoZoomInXCalc( const int staticXpixel )
{
    // Preserve the position of the clicked point:
    double staticX = p2x( staticXpixel );

    // Zoom in:
    m_scaleX = m_scaleX * zoomIncrementalFactor;
    // Adjust the new m_posx
    m_posX = staticX - (staticXpixel / m_scaleX);
    // Adjust desired
    m_desiredXmin   = m_posX;
    m_desiredXmax   = m_posX + ( m_scrX - (m_marginLeft + m_marginRight) ) / m_scaleX;
}


void mpWindow::AdjustLimitedView()
{
    if( !m_enableLimitedView )
        return;

    // m_min and m_max are plot limits for curves
    // xMin, xMax, yMin, yMax are the full limits (plot limit + margin)
    const double    xMin    = m_minX - m_marginLeft / m_scaleX;
    const double    xMax    = m_maxX + m_marginRight / m_scaleX;
    const double    yMin    = m_minY - m_marginTop / m_scaleY;
    const double    yMax    = m_maxY + m_marginBottom / m_scaleY;

    if( m_desiredXmin < xMin )
    {
        double diff = xMin - m_desiredXmin;
        m_posX += diff;
        m_desiredXmax   += diff;
        m_desiredXmin   = xMin;
    }

    if( m_desiredXmax > xMax )
    {
        double diff = m_desiredXmax - xMax;
        m_posX -= diff;
        m_desiredXmin   -= diff;
        m_desiredXmax   = xMax;
    }

    if( m_desiredYmin < yMin )
    {
        double diff = yMin - m_desiredYmin;
        m_posY += diff;
        m_desiredYmax   += diff;
        m_desiredYmin   = yMin;
    }

    if( m_desiredYmax > yMax )
    {
        double diff = m_desiredYmax - yMax;
        m_posY -= diff;
        m_desiredYmin   -= diff;
        m_desiredYmax   = yMax;
    }
}


bool mpWindow::SetXView( double pos, double desiredMax, double desiredMin )
{
    // if(!CheckXLimits(desiredMax, desiredMin))
    // return false;

    m_posX = pos;
    m_desiredXmax   = desiredMax;
    m_desiredXmin   = desiredMin;
    AdjustLimitedView();

    return true;
}


bool mpWindow::SetYView( double pos, double desiredMax, double desiredMin )
{
    // if(!CheckYLimits(desiredMax, desiredMin))
    // return false;

    m_posY = pos;
    m_desiredYmax   = desiredMax;
    m_desiredYmin   = desiredMin;
    AdjustLimitedView();

    return true;
}


void mpWindow::ZoomIn( const wxPoint& centerPoint )
{
    ZoomIn( centerPoint, zoomIncrementalFactor );
}


void mpWindow::ZoomIn( const wxPoint& centerPoint, double zoomFactor )
{
    wxPoint c( centerPoint );

    if( c == wxDefaultPosition )
    {
        GetClientSize( &m_scrX, &m_scrY );
        c.x = (m_scrX - m_marginLeft - m_marginRight) / 2 + m_marginLeft;   // c.x = m_scrX/2;
        c.y = (m_scrY - m_marginTop - m_marginBottom) / 2 - m_marginTop;    // c.y = m_scrY/2;
    }
    else
    {
        c.x = std::max( c.x, m_marginLeft );
        c.x = std::min( c.x, m_scrX - m_marginRight );
        c.y = std::max( c.y, m_marginTop );
        c.y = std::min( c.y, m_scrY - m_marginBottom );
    }

    // Preserve the position of the clicked point:
    double  prior_layer_x   = p2x( c.x );
    double  prior_layer_y   = p2y( c.y );

    // Zoom in:
    const double MAX_SCALE = 1e6;
    double       newScaleX = m_scaleX * zoomFactor;
    double       newScaleY = m_scaleY * zoomFactor;

    // Baaaaad things happen when you zoom in too much..
    if( newScaleX <= MAX_SCALE && newScaleY <= MAX_SCALE )
    {
        m_scaleX    = newScaleX;
        m_scaleY    = newScaleY;
    }
    else
    {
        return;
    }

    // Adjust the new m_posx/y:
    m_posX  = prior_layer_x - c.x / m_scaleX;
    m_posY  = prior_layer_y + c.y / m_scaleY;

    m_desiredXmin   = m_posX;
    m_desiredXmax   = m_posX + (m_scrX - m_marginLeft - m_marginRight) / m_scaleX;  // m_desiredXmax = m_posX + m_scrX / m_scaleX;
    m_desiredYmax   = m_posY;
    m_desiredYmin   = m_posY - (m_scrY - m_marginTop - m_marginBottom) / m_scaleY;  // m_desiredYmin = m_posY - m_scrY / m_scaleY;
    AdjustLimitedView();
    UpdateAll();
}


void mpWindow::ZoomOut( const wxPoint& centerPoint )
{
    ZoomOut( centerPoint, zoomIncrementalFactor );
}


void mpWindow::ZoomOut( const wxPoint& centerPoint, double zoomFactor )
{
    wxPoint c( centerPoint );

    if( c == wxDefaultPosition )
    {
        GetClientSize( &m_scrX, &m_scrY );
        c.x = (m_scrX - m_marginLeft - m_marginRight) / 2 + m_marginLeft;   // c.x = m_scrX/2;
        c.y = (m_scrY - m_marginTop - m_marginBottom) / 2 - m_marginTop;    // c.y = m_scrY/2;
    }

    // Preserve the position of the clicked point:
    double  prior_layer_x   = p2x( c.x );
    double  prior_layer_y   = p2y( c.y );

    // Zoom out:
    m_scaleX = m_scaleX / zoomFactor;
    m_scaleY = m_scaleY / zoomFactor;

    // Adjust the new m_posx/y:
    m_posX  = prior_layer_x - c.x / m_scaleX;
    m_posY  = prior_layer_y + c.y / m_scaleY;

    m_desiredXmin   = m_posX;
    m_desiredXmax   = m_posX + (m_scrX - m_marginLeft - m_marginRight) / m_scaleX;  // m_desiredXmax = m_posX + m_scrX / m_scaleX;
    m_desiredYmax   = m_posY;
    m_desiredYmin   = m_posY - (m_scrY - m_marginTop - m_marginBottom) / m_scaleY;  // m_desiredYmin = m_posY - m_scrY / m_scaleY;

    if( !CheckXLimits( m_desiredXmax,
                m_desiredXmin ) || !CheckYLimits( m_desiredYmax, m_desiredYmin ) )
    {
        Fit();
    }

    UpdateAll();
}


void mpWindow::ZoomInX()
{
    m_scaleX = m_scaleX * zoomIncrementalFactor;
    UpdateAll();
}


void mpWindow::ZoomRect( wxPoint p0, wxPoint p1 )
{
    // Compute the 2 corners in graph coordinates:
    double  p0x = p2x( p0.x );
    double  p0y = p2y( p0.y );
    double  p1x = p2x( p1.x );
    double  p1y = p2y( p1.y );

    // Order them:
    double  zoom_x_min = p0x<p1x ? p0x : p1x;
    double  zoom_x_max = p0x>p1x ? p0x : p1x;
    double  zoom_y_min = p0y<p1y ? p0y : p1y;
    double  zoom_y_max = p0y>p1y ? p0y : p1y;

    Fit( zoom_x_min, zoom_x_max, zoom_y_min, zoom_y_max );
    AdjustLimitedView();
}


void mpWindow::LockAspect( bool enable )
{
    m_lockaspect = enable;
    m_popmenu.Check( mpID_LOCKASPECT, enable );

    // Try to fit again with the new config:
    Fit( m_desiredXmin, m_desiredXmax, m_desiredYmin, m_desiredYmax );
}


void mpWindow::OnShowPopupMenu( wxMouseEvent& event )
{
    m_clickedX  = event.GetX();
    m_clickedY  = event.GetY();
    PopupMenu( &m_popmenu, event.GetX(), event.GetY() );
}


void mpWindow::OnLockAspect( wxCommandEvent& WXUNUSED( event ) )
{
    LockAspect( !m_lockaspect );
}


void mpWindow::OnFit( wxCommandEvent& WXUNUSED( event ) )
{
    Fit();
}


void mpWindow::OnCenter( wxCommandEvent& WXUNUSED( event ) )
{
    GetClientSize( &m_scrX, &m_scrY );
    int centerX = (m_scrX - m_marginLeft - m_marginRight) / 2;      // + m_marginLeft; // c.x = m_scrX/2;
    int centerY = (m_scrY - m_marginTop - m_marginBottom) / 2;      // - m_marginTop; // c.y = m_scrY/2;
    SetPos( p2x( m_clickedX - centerX ), p2y( m_clickedY - centerY ) );
    // SetPos( p2x(m_clickedX-m_scrX/2), p2y(m_clickedY-m_scrY/2) );  //SetPos( (double)(m_clickedX-m_scrX/2) / m_scaleX + m_posX, (double)(m_scrY/2-m_clickedY) / m_scaleY + m_posY);
}


void mpWindow::OnZoomIn( wxCommandEvent& WXUNUSED( event ) )
{
    ZoomIn( wxPoint( m_mouseMClick.x, m_mouseMClick.y ) );
}


void mpWindow::OnZoomOut( wxCommandEvent& WXUNUSED( event ) )
{
    ZoomOut();
}


void mpWindow::OnSize( wxSizeEvent& WXUNUSED( event ) )
{
    // Try to fit again with the new window size:
    Fit( m_desiredXmin, m_desiredXmax, m_desiredYmin, m_desiredYmax );
}


bool mpWindow::AddLayer( mpLayer* layer, bool refreshDisplay )
{
    if( layer != NULL )
    {
        m_layers.push_back( layer );

        if( refreshDisplay )
            UpdateAll();

        return true;
    }

    ;
    return false;
}


bool mpWindow::DelLayer( mpLayer* layer,
        bool alsoDeleteObject,
        bool refreshDisplay )
{
    wxLayerList::iterator layIt;

    for( layIt = m_layers.begin(); layIt != m_layers.end(); layIt++ )
    {
        if( *layIt == layer )
        {
            // Also delete the object?
            if( alsoDeleteObject )
                delete *layIt;

            m_layers.erase( layIt );    // this deleted the reference only

            if( refreshDisplay )
                UpdateAll();

            return true;
        }
    }

    return false;
}


void mpWindow::DelAllLayers( bool alsoDeleteObject, bool refreshDisplay )
{
    while( m_layers.size()>0 )
    {
        // Also delete the object?
        if( alsoDeleteObject )
            delete m_layers[0];

        m_layers.erase( m_layers.begin() );    // this deleted the reference only
    }

    if( refreshDisplay )
        UpdateAll();
}


void mpWindow::OnPaint( wxPaintEvent& WXUNUSED( event ) )
{
    wxPaintDC dc( this );

    dc.GetSize( &m_scrX, &m_scrY );    // This is the size of the visible area only!

    // Selects direct or buffered draw:
    wxDC* trgDc;

    // J.L.Blanco @ Aug 2007: Added double buffer support
    if( m_enableDoubleBuffer )
    {
        if( m_last_lx != m_scrX || m_last_ly != m_scrY )
        {
            if( m_buff_bmp )
                delete m_buff_bmp;

            m_buff_bmp = new wxBitmap( m_scrX, m_scrY );
            m_buff_dc.SelectObject( *m_buff_bmp );
            m_last_lx   = m_scrX;
            m_last_ly   = m_scrY;
        }

        trgDc = &m_buff_dc;
    }
    else
    {
        trgDc = &dc;
    }

    // Draw background:
    // trgDc->SetDeviceOrigin(0,0);
    trgDc->SetPen( *wxTRANSPARENT_PEN );
    wxBrush brush( GetBackgroundColour() );
    trgDc->SetBrush( brush );
    trgDc->SetTextForeground( m_fgColour );
    trgDc->DrawRectangle( 0, 0, m_scrX, m_scrY );

    // Draw all the layers:
    // trgDc->SetDeviceOrigin( m_scrX>>1, m_scrY>>1);  // Origin at the center
    wxLayerList::iterator li;

    for( li = m_layers.begin(); li != m_layers.end(); li++ )
        (*li)->Plot( *trgDc, *this );

    if( m_zooming )
    {
        wxPen pen( m_fgColour, 1, wxPENSTYLE_DOT );
        trgDc->SetPen( pen );
        trgDc->SetBrush( *wxTRANSPARENT_BRUSH );
        trgDc->DrawRectangle( m_zoomRect );
    }

    // If doublebuffer, draw now to the window:
    if( m_enableDoubleBuffer )
    {
        // trgDc->SetDeviceOrigin(0,0);
        // dc.SetDeviceOrigin(0,0);  // Origin at the center
        dc.Blit( 0, 0, m_scrX, m_scrY, trgDc, 0, 0 );
    }

    // If scrollbars are enabled, refresh them
    if( m_enableScrollBars )
    {
        /*       m_scroll.x = (int) floor((m_posX - m_minX)*m_scaleX);
         *        m_scroll.y = (int) floor((m_maxY - m_posY )*m_scaleY);
         *        Scroll(m_scroll.x, m_scroll.y);*/
        // Scroll(x2p(m_posX), y2p(m_posY));
        // SetVirtualSize((int) ((m_maxX - m_minX)*m_scaleX), (int) ((m_maxY - m_minY)*m_scaleY));
        // int centerX = (m_scrX - m_marginLeft - m_marginRight)/2; // + m_marginLeft; // c.x = m_scrX/2;
        // int centerY = (m_scrY - m_marginTop - m_marginBottom)/2; // - m_marginTop; // c.y = m_scrY/2;
        /*SetScrollbars(1, 1, (int) ((m_maxX - m_minX)*m_scaleX), (int) ((m_maxY - m_minY)*m_scaleY));*/    // , x2p(m_posX + centerX/m_scaleX), y2p(m_posY - centerY/m_scaleY), true);
    }
}


bool mpWindow::UpdateBBox()
{
    m_minX  = 0.0;
    m_maxX  = 1.0;
    m_minY  = 0.0;
    m_maxY  = 1.0;

    return true;
}


void mpWindow::UpdateAll()
{
    if( UpdateBBox() )
    {
        if( m_enableScrollBars )
        {
            int cx, cy;
            GetClientSize( &cx, &cy );
            // Do x scroll bar
            {
                // Convert margin sizes from pixels to coordinates
                double leftMargin = m_marginLeft / m_scaleX;
                // Calculate the range in coords that we want to scroll over
                double  maxX    = (m_desiredXmax > m_maxX) ? m_desiredXmax : m_maxX;
                double  minX    = (m_desiredXmin < m_minX) ? m_desiredXmin : m_minX;

                if( (m_posX + leftMargin) < minX )
                    minX = m_posX + leftMargin;

                // Calculate scroll bar size and thumb position
                int sizeX   = (int) ( (maxX - minX) * m_scaleX );
                int thumbX  = (int) ( ( (m_posX + leftMargin) - minX ) * m_scaleX );
                SetScrollbar( wxHORIZONTAL, thumbX, cx - (m_marginRight + m_marginLeft), sizeX );
            }
            // Do y scroll bar
            {
                // Convert margin sizes from pixels to coordinates
                double topMargin = m_marginTop / m_scaleY;
                // Calculate the range in coords that we want to scroll over
                double maxY = (m_desiredYmax > m_maxY) ? m_desiredYmax : m_maxY;

                if( (m_posY - topMargin) > maxY )
                    maxY = m_posY - topMargin;

                double minY = (m_desiredYmin < m_minY) ? m_desiredYmin : m_minY;
                // Calculate scroll bar size and thumb position
                int sizeY   = (int) ( (maxY - minY) * m_scaleY );
                int thumbY  = (int) ( ( maxY - (m_posY - topMargin) ) * m_scaleY );
                SetScrollbar( wxVERTICAL, thumbY, cy - (m_marginTop + m_marginBottom), sizeY );
            }
        }
    }

    Refresh( false );
}


void mpWindow::DoScrollCalc( const int position, const int orientation )
{
    if( orientation == wxVERTICAL )
    {
        // Y axis
        // Get top margin in coord units
        double topMargin = m_marginTop / m_scaleY;
        // Calculate maximum Y coord to be shown in the graph
        double maxY = m_desiredYmax > m_maxY ? m_desiredYmax  : m_maxY;
        // Set new position
        SetPosY( ( maxY - (position / m_scaleY) ) + topMargin );
    }
    else
    {
        // X Axis
        // Get left margin in coord units
        double leftMargin = m_marginLeft / m_scaleX;
        // Calculate minimum X coord to be shown in the graph
        double minX = (m_desiredXmin < m_minX) ? m_desiredXmin : m_minX;
        // Set new position
        SetPosX( ( minX + (position / m_scaleX) ) - leftMargin );
    }
}


void mpWindow::OnScrollThumbTrack( wxScrollWinEvent& event )
{
    DoScrollCalc( event.GetPosition(), event.GetOrientation() );
}


void mpWindow::OnScrollPageUp( wxScrollWinEvent& event )
{
    int scrollOrientation = event.GetOrientation();
    // Get position before page up
    int position = GetScrollPos( scrollOrientation );
    // Get thumb size
    int thumbSize = GetScrollThumb( scrollOrientation );

    // Need to adjust position by a page
    position -= thumbSize;

    if( position < 0 )
        position = 0;

    DoScrollCalc( position, scrollOrientation );
}


void mpWindow::OnScrollPageDown( wxScrollWinEvent& event )
{
    int scrollOrientation = event.GetOrientation();
    // Get position before page up
    int position = GetScrollPos( scrollOrientation );
    // Get thumb size
    int thumbSize = GetScrollThumb( scrollOrientation );
    // Get scroll range
    int scrollRange = GetScrollRange( scrollOrientation );

    // Need to adjust position by a page
    position += thumbSize;

    if( position > (scrollRange - thumbSize) )
        position = scrollRange - thumbSize;

    DoScrollCalc( position, scrollOrientation );
}


void mpWindow::OnScrollLineUp( wxScrollWinEvent& event )
{
    int scrollOrientation = event.GetOrientation();
    // Get position before page up
    int position = GetScrollPos( scrollOrientation );

    // Need to adjust position by a line
    position -= mpSCROLL_NUM_PIXELS_PER_LINE;

    if( position < 0 )
        position = 0;

    DoScrollCalc( position, scrollOrientation );
}


void mpWindow::OnScrollLineDown( wxScrollWinEvent& event )
{
    int scrollOrientation = event.GetOrientation();
    // Get position before page up
    int position = GetScrollPos( scrollOrientation );
    // Get thumb size
    int thumbSize = GetScrollThumb( scrollOrientation );
    // Get scroll range
    int scrollRange = GetScrollRange( scrollOrientation );

    // Need to adjust position by a page
    position += mpSCROLL_NUM_PIXELS_PER_LINE;

    if( position > (scrollRange - thumbSize) )
        position = scrollRange - thumbSize;

    DoScrollCalc( position, scrollOrientation );
}


void mpWindow::OnScrollTop( wxScrollWinEvent& event )
{
    DoScrollCalc( 0, event.GetOrientation() );
}


void mpWindow::OnScrollBottom( wxScrollWinEvent& event )
{
    int scrollOrientation = event.GetOrientation();
    // Get thumb size
    int thumbSize = GetScrollThumb( scrollOrientation );
    // Get scroll range
    int scrollRange = GetScrollRange( scrollOrientation );

    DoScrollCalc( scrollRange - thumbSize, scrollOrientation );
}


// End patch ngpaton

void mpWindow::SetScaleX( double scaleX )
{
    if( scaleX != 0 )
        m_scaleX = scaleX;

    UpdateAll();
}


// New methods implemented by Davide Rondini

unsigned int mpWindow::CountLayers() const
{
    unsigned int layerNo = 0;

    for( const mpLayer* layer : m_layers )
    {
        if( layer->HasBBox() )
            layerNo++;
    }

    return layerNo;
}


mpLayer* mpWindow::GetLayer( int position ) const
{
    if( ( position >= (int) m_layers.size() ) || position < 0 )
        return NULL;

    return m_layers[position];
}


const mpLayer* mpWindow::GetLayerByName( const wxString& name ) const
{
    for( const mpLayer* layer : m_layers )
    {
        if( !layer->GetName().Cmp( name ) )
            return layer;
    }

    return NULL;    // Not found
}


void mpWindow::GetBoundingBox( double* bbox ) const
{
    bbox[0] = m_minX;
    bbox[1] = m_maxX;
    bbox[2] = m_minY;
    bbox[3] = m_maxY;
}


bool mpWindow::SaveScreenshot( const wxString& filename, wxBitmapType type, wxSize imageSize,
                               bool fit )
{
    int sizeX, sizeY;
    int bk_scrX, bk_scrY;

    if( imageSize == wxDefaultSize )
    {
        sizeX   = m_scrX;
        sizeY   = m_scrY;
    }
    else
    {
        sizeX   = imageSize.x;
        sizeY   = imageSize.y;
        bk_scrX = m_scrX;
        bk_scrY = m_scrY;
        SetScr( sizeX, sizeY );
    }

    wxBitmap screenBuffer( sizeX, sizeY );
    wxMemoryDC screenDC;
    screenDC.SelectObject( screenBuffer );
    screenDC.SetPen( *wxWHITE_PEN );
    screenDC.SetTextForeground( m_fgColour );
    wxBrush brush( GetBackgroundColour() );
    screenDC.SetBrush( brush );
    screenDC.DrawRectangle( 0, 0, sizeX, sizeY );

    if( fit )
        Fit( m_minX, m_maxX, m_minY, m_maxY, &sizeX, &sizeY );
    else
        Fit( m_desiredXmin, m_desiredXmax, m_desiredYmin, m_desiredYmax, &sizeX, &sizeY );

    // Draw all the layers:
    for( mpLayer* layer : m_layers )
        layer->Plot( screenDC, *this );

    if( imageSize != wxDefaultSize )
    {
        // Restore dimensions
        SetScr( bk_scrX, bk_scrY );
        Fit( m_desiredXmin, m_desiredXmax, m_desiredYmin, m_desiredYmax, &bk_scrX, &bk_scrY );
        UpdateAll();
    }

    // Once drawing is complete, actually save screen shot
    wxImage screenImage = screenBuffer.ConvertToImage();
    return screenImage.SaveFile( filename, type );
}


void mpWindow::SetMargins( int top, int right, int bottom, int left )
{
    m_marginTop    = top;
    m_marginRight  = right;
    m_marginBottom = bottom;
    m_marginLeft   = left;
}


mpInfoLayer* mpWindow::IsInsideInfoLayer( wxPoint& point )
{
    for( mpLayer* layer : m_layers )
    {
        if( layer->IsInfo() )
        {
            mpInfoLayer* tmpLyr = static_cast<mpInfoLayer*>( layer );

            if( tmpLyr->Inside( point ) )
                return tmpLyr;
        }
    }

    return NULL;
}


void mpWindow::SetLayerVisible( const wxString& name, bool viewable )
{
    mpLayer* lx = GetLayerByName( name );

    if( lx )
    {
        lx->SetVisible( viewable );
        UpdateAll();
    }
}


bool mpWindow::IsLayerVisible( const wxString& name ) const
{
    const mpLayer* lx = GetLayerByName( name );

    return lx ? lx->IsVisible() : false;
}


void mpWindow::SetLayerVisible( const unsigned int position, bool viewable )
{
    mpLayer* lx = GetLayer( position );

    if( lx )
    {
        lx->SetVisible( viewable );
        UpdateAll();
    }
}


bool mpWindow::IsLayerVisible( unsigned int position ) const
{
    mpLayer* lx = GetLayer( position );

    return (lx) ? lx->IsVisible() : false;
}


void mpWindow::SetColourTheme( const wxColour& bgColour, const wxColour& drawColour,
                               const wxColour& axesColour )
{
    SetBackgroundColour( bgColour );
    SetForegroundColour( drawColour );
    m_bgColour  = bgColour;
    m_fgColour  = drawColour;
    m_axColour  = axesColour;

    // Cycle between layers to set colours and properties to them
    for( mpLayer* layer : m_layers )
    {
        if( layer->GetLayerType() == mpLAYER_AXIS )
        {
            wxPen axisPen = layer->GetPen();    // Get the old pen to modify only colour, not style or width
            axisPen.SetColour( axesColour );
            layer->SetPen( axisPen );
        }

        if( layer->GetLayerType() == mpLAYER_INFO )
        {
            wxPen infoPen = layer->GetPen();    // Get the old pen to modify only colour, not style or width
            infoPen.SetColour( drawColour );
            layer->SetPen( infoPen );
        }
    }
}


// -----------------------------------------------------------------------------
// mpFXYVector implementation - by Jose Luis Blanco (AGO-2007)
// -----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS( mpFXYVector, mpFXY )

// Constructor
mpFXYVector::mpFXYVector( const wxString& name, int flags ) : mpFXY( name, flags )
{
    m_index = 0;
    m_minX  = -1;
    m_maxX  = 1;
    m_minY  = -1;
    m_maxY  = 1;
    m_type  = mpLAYER_PLOT;
}


double mpScaleX::TransformToPlot( double x ) const
{
    return (x + m_offset) * m_scale;
}


double mpScaleX::TransformFromPlot( double xplot ) const
{
    return xplot / m_scale - m_offset;
}


double mpScaleY::TransformToPlot( double x ) const
{
    return (x + m_offset) * m_scale;
}


double mpScaleY::TransformFromPlot( double xplot ) const
{
    return xplot / m_scale - m_offset;
}


double mpScaleXLog::TransformToPlot( double x ) const
{
    double  xlogmin = log10( m_minV );
    double  xlogmax = log10( m_maxV );

    return ( log10( x ) - xlogmin) / (xlogmax - xlogmin);
}


double mpScaleXLog::TransformFromPlot( double xplot ) const
{
    double  xlogmin = log10( m_minV );
    double  xlogmax = log10( m_maxV );

    return pow( 10.0, xplot * (xlogmax - xlogmin) + xlogmin );
}


#if 0
mpFSemiLogXVector::mpFSemiLogXVector( wxString name, int flags ) :
    mpFXYVector( name, flags )
{
}


IMPLEMENT_DYNAMIC_CLASS( mpFSemiLogXVector, mpFXYVector )
#endif // 0

void mpFXYVector::Rewind()
{
    m_index = 0;
}

size_t mpFXYVector::GetCount() const
{
    return m_xs.size();
}


bool mpFXYVector::GetNextXY( double& x, double& y )
{
    if( m_index >= m_xs.size() )
    {
        return false;
    }
    else
    {
        x = m_xs[m_index];
        y = m_ys[m_index++];
        return m_index <= m_xs.size();
    }
}


void mpFXYVector::Clear()
{
    m_xs.clear();
    m_ys.clear();
}


void mpFXYVector::SetData( const std::vector<double>& xs, const std::vector<double>& ys )
{
    // Check if the data vectors are of the same size
    if( xs.size() != ys.size() )
        return;

    // Copy the data:
    m_xs    = xs;
    m_ys    = ys;

    // Update internal variables for the bounding box.
    if( xs.size() > 0 )
    {
        m_minX  = xs[0];
        m_maxX  = xs[0];
        m_minY  = ys[0];
        m_maxY  = ys[0];

        for( const double x : xs )
        {
            if( x < m_minX )
                m_minX = x;

            if( x > m_maxX )
                m_maxX = x;
        }

        for( const double y : ys )
        {
            if( y < m_minY )
                m_minY = y;

            if( y > m_maxY )
                m_maxY = y;
        }
    }
    else
    {
        m_minX  = -1;
        m_maxX  = 1;
        m_minY  = -1;
        m_maxY  = 1;
    }
}


// -----------------------------------------------------------------------------
// mpText - provided by Val Greene
// -----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS( mpText, mpLayer )


/** @param name text to be displayed
 *  @param offsetx x position in percentage (0-100)
 *  @param offsetx y position in percentage (0-100)
 */
mpText::mpText( const wxString& name, int offsetx, int offsety )
{
    SetName( name );

    if( offsetx >= 0 && offsetx <= 100 )
        m_offsetx = offsetx;
    else
        m_offsetx = 5;

    if( offsety >= 0 && offsety <= 100 )
        m_offsety = offsety;
    else
        m_offsety = 50;

    m_type = mpLAYER_INFO;
}


/** mpText Layer plot handler.
 *  This implementation will plot the text adjusted to the visible area.
 */

void mpText::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );
        dc.SetFont( m_font );

        wxCoord tw = 0, th = 0;
        dc.GetTextExtent( GetName(), &tw, &th );

        // int left = -dc.LogicalToDeviceX(0);
        // int width = dc.LogicalToDeviceX(0) - left;
        // int bottom = dc.LogicalToDeviceY(0);
        // int height = bottom - -dc.LogicalToDeviceY(0);

        /*    dc.DrawText( GetName(),
         *     (int)((((float)width/100.0) * m_offsety) + left - (tw/2)),
         *     (int)((((float)height/100.0) * m_offsetx) - bottom) );*/
        int px  = m_offsetx * ( w.GetScrX() - w.GetMarginLeft() - w.GetMarginRight() ) / 100;
        int py  = m_offsety * ( w.GetScrY() - w.GetMarginTop() - w.GetMarginBottom() ) / 100;
        dc.DrawText( GetName(), px, py );
    }
}


// -----------------------------------------------------------------------------
// mpPrintout - provided by Davide Rondini
// -----------------------------------------------------------------------------

mpPrintout::mpPrintout( mpWindow* drawWindow, const wxChar* title ) : wxPrintout( title )
{
    drawn = false;
    plotWindow = drawWindow;
}


bool mpPrintout::OnPrintPage( int page )
{
    wxDC* trgDc = GetDC();

    if( (trgDc) && (page == 1) )
    {
        wxCoord m_prnX, m_prnY;
        int marginX = 50;
        int marginY = 50;
        trgDc->GetSize( &m_prnX, &m_prnY );

        m_prnX  -= (2 * marginX);
        m_prnY  -= (2 * marginY);
        trgDc->SetDeviceOrigin( marginX, marginY );

        // Set the scale according to the page:
        plotWindow->Fit(
                plotWindow->GetDesiredXmin(),
                plotWindow->GetDesiredXmax(),
                plotWindow->GetDesiredYmin(),
                plotWindow->GetDesiredYmax(),
                &m_prnX,
                &m_prnY );

        // Get the colours of the plotWindow to restore them ath the end
        wxColour    oldBgColour = plotWindow->GetBackgroundColour();
        wxColour    oldFgColour = plotWindow->GetForegroundColour();
        wxColour    oldAxColour = plotWindow->GetAxesColour();

        // Draw background, ensuring to use white background for printing.
        trgDc->SetPen( *wxTRANSPARENT_PEN );
        // wxBrush brush( plotWindow->GetBackgroundColour() );
        wxBrush brush = *wxWHITE_BRUSH;
        trgDc->SetBrush( brush );
        trgDc->DrawRectangle( 0, 0, m_prnX, m_prnY );

        // Draw all the layers:
        // trgDc->SetDeviceOrigin( m_prnX>>1, m_prnY>>1);  // Origin at the center
        mpLayer* layer;

        for( unsigned int li = 0; li < plotWindow->CountAllLayers(); li++ )
        {
            layer = plotWindow->GetLayer( li );
            layer->Plot( *trgDc, *plotWindow );
        }

        ;
        // Restore device origin
        // trgDc->SetDeviceOrigin(0, 0);
        // Restore colours
        plotWindow->SetColourTheme( oldBgColour, oldFgColour, oldAxColour );
        // Restore drawing
        plotWindow->Fit( plotWindow->GetDesiredXmin(),
                plotWindow->GetDesiredXmax(), plotWindow->GetDesiredYmin(),
                plotWindow->GetDesiredYmax(), NULL, NULL );
        plotWindow->UpdateAll();
    }

    return true;
}


bool mpPrintout::HasPage( int page )
{
    return page == 1;
}


// -----------------------------------------------------------------------------
// mpMovableObject - provided by Jose Luis Blanco
// -----------------------------------------------------------------------------
void mpMovableObject::TranslatePoint( double x, double y, double& out_x, double& out_y )
{
    double  ccos    = cos( m_reference_phi ); // Avoid computing cos/sin twice.
    double  csin    = sin( m_reference_phi );

    out_x   = m_reference_x + ccos * x - csin * y;
    out_y   = m_reference_y + csin * x + ccos * y;
}


// This method updates the buffers m_trans_shape_xs/ys, and the precomputed bounding box.
void mpMovableObject::ShapeUpdated()
{
    // Just in case...
    if( m_shape_xs.size() != m_shape_ys.size() )
    {
    }
    else
    {
        double  ccos    = cos( m_reference_phi ); // Avoid computing cos/sin twice.
        double  csin    = sin( m_reference_phi );

        m_trans_shape_xs.resize( m_shape_xs.size() );
        m_trans_shape_ys.resize( m_shape_xs.size() );

        std::vector<double>::iterator   itXi, itXo;
        std::vector<double>::iterator   itYi, itYo;

        m_bbox_min_x    = 1e300;
        m_bbox_max_x    = -1e300;
        m_bbox_min_y    = 1e300;
        m_bbox_max_y    = -1e300;

        for( itXo = m_trans_shape_xs.begin(),
             itYo = m_trans_shape_ys.begin(), itXi = m_shape_xs.begin(), itYi = m_shape_ys.begin();
             itXo!=m_trans_shape_xs.end(); itXo++, itYo++, itXi++, itYi++ )
        {
            *itXo   = m_reference_x + ccos * (*itXi) - csin * (*itYi);
            *itYo   = m_reference_y + csin * (*itXi) + ccos * (*itYi);

            // Keep BBox:
            if( *itXo < m_bbox_min_x )
                m_bbox_min_x = *itXo;

            if( *itXo > m_bbox_max_x )
                m_bbox_max_x = *itXo;

            if( *itYo < m_bbox_min_y )
                m_bbox_min_y = *itYo;

            if( *itYo > m_bbox_max_y )
                m_bbox_max_y = *itYo;
        }
    }
}


void mpMovableObject::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible )
    {
        dc.SetPen( m_pen );


        std::vector<double>::iterator   itX = m_trans_shape_xs.begin();
        std::vector<double>::iterator   itY = m_trans_shape_ys.begin();

        if( !m_continuous )
        {
            // for some reason DrawPoint does not use the current pen,
            // so we use DrawLine for fat pens
            if( m_pen.GetWidth() <= 1 )
            {
                while( itX!=m_trans_shape_xs.end() )
                {
                    dc.DrawPoint( w.x2p( *(itX++) ), w.y2p( *(itY++) ) );
                }
            }
            else
            {
                while( itX!=m_trans_shape_xs.end() )
                {
                    wxCoord cx  = w.x2p( *(itX++) );
                    wxCoord cy  = w.y2p( *(itY++) );
                    dc.DrawLine( cx, cy, cx, cy );
                }
            }
        }
        else
        {
            wxCoord cx0 = 0, cy0 = 0;
            bool first  = true;

            while( itX != m_trans_shape_xs.end() )
            {
                wxCoord cx  = w.x2p( *(itX++) );
                wxCoord cy  = w.y2p( *(itY++) );

                if( first )
                {
                    first = false;
                    cx0 = cx; cy0 = cy;
                }

                dc.DrawLine( cx0, cy0, cx, cy );
                cx0 = cx; cy0 = cy;
            }
        }

        if( !m_name.IsEmpty() && m_showName )
        {
            dc.SetFont( m_font );

            wxCoord tx, ty;
            dc.GetTextExtent( m_name, &tx, &ty );

            if( HasBBox() )
            {
                wxCoord sx  = (wxCoord) ( ( m_bbox_max_x - w.GetPosX() ) * w.GetScaleX() );
                wxCoord sy  = (wxCoord) ( (w.GetPosY() - m_bbox_max_y ) * w.GetScaleY() );

                tx  = sx - tx - 8;
                ty  = sy - 8 - ty;
            }
            else
            {
                const int   sx = w.GetScrX() >> 1;
                const int   sy = w.GetScrY() >> 1;

                if( (m_flags & mpALIGNMASK) == mpALIGN_NE )
                {
                    tx  = sx - tx - 8;
                    ty  = -sy + 8;
                }
                else if( (m_flags & mpALIGNMASK) == mpALIGN_NW )
                {
                    tx  = -sx + 8;
                    ty  = -sy + 8;
                }
                else if( (m_flags & mpALIGNMASK) == mpALIGN_SW )
                {
                    tx  = -sx + 8;
                    ty  = sy - 8 - ty;
                }
                else
                {
                    tx  = sx - tx - 8;
                    ty  = sy - 8 - ty;
                }
            }

            dc.DrawText( m_name, tx, ty );
        }
    }
}


// -----------------------------------------------------------------------------
// mpCovarianceEllipse - provided by Jose Luis Blanco
// -----------------------------------------------------------------------------

// Called to update the m_shape_xs, m_shape_ys vectors, whenever a parameter changes.
void mpCovarianceEllipse::RecalculateShape()
{
    m_shape_xs.clear();
    m_shape_ys.clear();

    // Preliminary checks:
    if( m_quantiles < 0 )
        return;

    if( m_cov_00 < 0 )
        return;

    if( m_cov_11 < 0 )
        return;

    m_shape_xs.resize( m_segments, 0 );
    m_shape_ys.resize( m_segments, 0 );

    // Compute the two eigenvalues of the covariance:
    // -------------------------------------------------
    double  b   = -m_cov_00 - m_cov_11;
    double  c   =  m_cov_00 * m_cov_11 - m_cov_01 * m_cov_01;

    double D = b * b - 4 * c;

    if( D < 0 )
        return;

    double  eigenVal0   = 0.5 * ( -b + sqrt( D ) );
    double  eigenVal1   = 0.5 * ( -b - sqrt( D ) );

    // Compute the two corresponding eigenvectors:
    // -------------------------------------------------
    double  eigenVec0_x, eigenVec0_y;
    double  eigenVec1_x, eigenVec1_y;

    if( fabs( eigenVal0 - m_cov_00 ) > 1e-6 )
    {
        double k1x = m_cov_01 / ( eigenVal0 - m_cov_00 );
        eigenVec0_y = 1;
        eigenVec0_x = eigenVec0_y * k1x;
    }
    else
    {
        double k1y = m_cov_01 / ( eigenVal0 - m_cov_11 );
        eigenVec0_x = 1;
        eigenVec0_y = eigenVec0_x * k1y;
    }

    if( fabs( eigenVal1 - m_cov_00 ) > 1e-6 )
    {
        double k2x = m_cov_01 / ( eigenVal1 - m_cov_00 );
        eigenVec1_y = 1;
        eigenVec1_x = eigenVec1_y * k2x;
    }
    else
    {
        double k2y = m_cov_01 / ( eigenVal1 - m_cov_11 );
        eigenVec1_x = 1;
        eigenVec1_y = eigenVec1_x * k2y;
    }

    // Normalize the eigenvectors:
    double len = sqrt( eigenVec0_x * eigenVec0_x + eigenVec0_y * eigenVec0_y );
    eigenVec0_x /= len;    // It *CANNOT* be zero
    eigenVec0_y /= len;

    len = sqrt( eigenVec1_x * eigenVec1_x + eigenVec1_y * eigenVec1_y );
    eigenVec1_x /= len;    // It *CANNOT* be zero
    eigenVec1_y /= len;


    // Take the sqrt of the eigenvalues (required for the ellipse scale):
    eigenVal0   = sqrt( eigenVal0 );
    eigenVal1   = sqrt( eigenVal1 );

    // Compute the 2x2 matrix M = diag(eigVal) * (~eigVec)  (each eigen vector is a row):
    double  M_00    = eigenVec0_x * eigenVal0;
    double  M_01    = eigenVec0_y * eigenVal0;

    double  M_10    = eigenVec1_x * eigenVal1;
    double  M_11    = eigenVec1_y * eigenVal1;

    // The points of the 2D ellipse:
    double  ang;
    double  Aang = 6.283185308 / (m_segments - 1);
    int i;

    for( i = 0, ang = 0; i < m_segments; i++, ang += Aang )
    {
        double  ccos    = cos( ang );
        double  csin    = sin( ang );

        m_shape_xs[i]   = m_quantiles * (ccos * M_00 + csin * M_10 );
        m_shape_ys[i]   = m_quantiles * (ccos * M_01 + csin * M_11 );
    }    // end for points on ellipse

    ShapeUpdated();
}


// -----------------------------------------------------------------------------
// mpPolygon - provided by Jose Luis Blanco
// -----------------------------------------------------------------------------
void mpPolygon::setPoints( const std::vector<double>& points_xs,
        const std::vector<double>& points_ys,
        bool closedShape )
{
    if( points_xs.size() == points_ys.size() )
    {
        m_shape_xs  = points_xs;
        m_shape_ys  = points_ys;

        if( closedShape && !points_xs.empty() )
        {
            m_shape_xs.push_back( points_xs[0] );
            m_shape_ys.push_back( points_ys[0] );
        }

        ShapeUpdated();
    }
}


// -----------------------------------------------------------------------------
// mpBitmapLayer - provided by Jose Luis Blanco
// -----------------------------------------------------------------------------
void mpBitmapLayer::GetBitmapCopy( wxImage& outBmp ) const
{
    if( m_validImg )
        outBmp = m_bitmap;
}


void mpBitmapLayer::SetBitmap( const wxImage& inBmp, double x, double y, double lx, double ly )
{
    if( inBmp.Ok() )
    {
        m_bitmap    = inBmp; // .GetSubBitmap( wxRect(0, 0, inBmp.GetWidth(), inBmp.GetHeight()));
        m_min_x = x;
        m_min_y = y;
        m_max_x = x + lx;
        m_max_y = y + ly;
        m_validImg = true;
    }
}


void mpBitmapLayer::Plot( wxDC& dc, mpWindow& w )
{
    if( m_visible && m_validImg )
    {
        /*	1st: We compute (x0,y0)-(x1,y1), the pixel coordinates of the real outer limits
         *       of the image rectangle within the (screen) mpWindow. Note that these coordinates
         *       might fall well far away from the real view limits when the user zoom in.
         *
         *  2nd: We compute (dx0,dy0)-(dx1,dy1), the pixel coordinates the rectangle that will
         *  be actually drawn into the mpWindow, i.e. the clipped real rectangle that
         *  avoids the non-visible parts. (offset_x,offset_y) are the pixel coordinates
         *  that correspond to the window point (dx0,dy0) within the image "m_bitmap", and
         *  (b_width,b_height) is the size of the bitmap patch that will be drawn.
         *
         *  (x0,y0) .................  (x1,y0)
         *  .                          .
         *  .                          .
         *  (x0,y1) ................   (x1,y1)
         *  (In pixels!!)
         */

        // 1st step -------------------------------
        wxCoord x0  = w.x2p( m_min_x );
        wxCoord y0  = w.y2p( m_max_y );
        wxCoord x1  = w.x2p( m_max_x );
        wxCoord y1  = w.y2p( m_min_y );

        // 2nd step -------------------------------
        // Precompute the size of the actual bitmap pixel on the screen (e.g. will be >1 if zoomed in)
        double  screenPixelX    = ( x1 - x0 ) / (double) m_bitmap.GetWidth();
        double  screenPixelY    = ( y1 - y0 ) / (double) m_bitmap.GetHeight();

        // The minimum number of pixels that the stretched image will overpass the actual mpWindow borders:
        wxCoord borderMarginX   = (wxCoord) (screenPixelX + 1); // ceil
        wxCoord borderMarginY   = (wxCoord) (screenPixelY + 1); // ceil

        // The actual drawn rectangle (dx0,dy0)-(dx1,dy1) is (x0,y0)-(x1,y1) clipped:
        wxCoord dx0 = x0, dx1 = x1, dy0 = y0, dy1 = y1;

        if( dx0 < 0 )
            dx0 = -borderMarginX;

        if( dy0 < 0 )
            dy0 = -borderMarginY;

        if( dx1 > w.GetScrX() )
            dx1 = w.GetScrX() + borderMarginX;

        if( dy1 > w.GetScrY() )
            dy1 = w.GetScrY() + borderMarginY;

        // For convenience, compute the width/height of the rectangle to be actually drawn:
        wxCoord d_width     = dx1 - dx0 + 1;
        wxCoord d_height    = dy1 - dy0 + 1;

        // Compute the pixel offsets in the internally stored bitmap:
        wxCoord offset_x    = (wxCoord) ( (dx0 - x0) / screenPixelX );
        wxCoord offset_y    = (wxCoord) ( (dy0 - y0) / screenPixelY );

        // and the size in pixel of the area to be actually drawn from the internally stored bitmap:
        wxCoord b_width     = (wxCoord) ( (dx1 - dx0 + 1) / screenPixelX );
        wxCoord b_height    = (wxCoord) ( (dy1 - dy0 + 1) / screenPixelY );

        // Is there any visible region?
        if( d_width>0 && d_height>0 )
        {
            // Build the scaled bitmap from the image, only if it has changed:
            if( m_scaledBitmap.GetWidth()!=d_width
                || m_scaledBitmap.GetHeight()!=d_height
                || m_scaledBitmap_offset_x != offset_x
                || m_scaledBitmap_offset_y != offset_y  )
            {
                wxRect r( wxRect( offset_x, offset_y, b_width, b_height ) );

                // Just for the case....
                if( r.x < 0 )
                    r.x = 0;

                if( r.y < 0 )
                    r.y = 0;

                if( r.width>m_bitmap.GetWidth() )
                    r.width = m_bitmap.GetWidth();

                if( r.height>m_bitmap.GetHeight() )
                    r.height = m_bitmap.GetHeight();

                m_scaledBitmap = wxBitmap(
                        wxBitmap( m_bitmap ).GetSubBitmap( r ).ConvertToImage()
                        .Scale( d_width, d_height ) );
                m_scaledBitmap_offset_x = offset_x;
                m_scaledBitmap_offset_y = offset_y;
            }

            // Draw it:
            dc.DrawBitmap( m_scaledBitmap, dx0, dy0, true );
        }
    }

    // Draw the name label
    if( !m_name.IsEmpty() && m_showName )
    {
        dc.SetFont( m_font );

        wxCoord tx, ty;
        dc.GetTextExtent( m_name, &tx, &ty );

        if( HasBBox() )
        {
            wxCoord sx  = (wxCoord) ( ( m_max_x - w.GetPosX() ) * w.GetScaleX() );
            wxCoord sy  = (wxCoord) ( (w.GetPosY() - m_max_y ) * w.GetScaleY() );

            tx  = sx - tx - 8;
            ty  = sy - 8 - ty;
        }
        else
        {
            const int   sx = w.GetScrX() >> 1;
            const int   sy = w.GetScrY() >> 1;

            if( (m_flags & mpALIGNMASK) == mpALIGN_NE )
            {
                tx  = sx - tx - 8;
                ty  = -sy + 8;
            }
            else if( (m_flags & mpALIGNMASK) == mpALIGN_NW )
            {
                tx  = -sx + 8;
                ty  = -sy + 8;
            }
            else if( (m_flags & mpALIGNMASK) == mpALIGN_SW )
            {
                tx  = -sx + 8;
                ty  = sy - 8 - ty;
            }
            else
            {
                tx  = sx - tx - 8;
                ty  = sy - 8 - ty;
            }
        }

        dc.DrawText( m_name, tx, ty );
    }
}


void mpFXY::SetScale( mpScaleBase* scaleX, mpScaleBase* scaleY )
{
    m_scaleX    = scaleX;
    m_scaleY    = scaleY;

    UpdateScales();
}


void mpFXY::UpdateScales()
{
    if( m_scaleX )
        m_scaleX->ExtendDataRange( GetMinX(), GetMaxX() );

    if( m_scaleY )
        m_scaleY->ExtendDataRange( GetMinY(), GetMaxY() );
}


double mpFXY::s2x( double plotCoordX ) const
{
    return m_scaleX->TransformFromPlot( plotCoordX );
}


double mpFXY::s2y( double plotCoordY ) const
{
    return m_scaleY->TransformFromPlot( plotCoordY );
}


double mpFXY::x2s( double x ) const
{
    return m_scaleX->TransformToPlot( x );
}


double mpFXY::y2s( double y ) const
{
    return m_scaleY->TransformToPlot( y );
}
