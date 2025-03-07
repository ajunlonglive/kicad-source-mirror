/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2011 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/// @todo The Boost entropy exception does not exist prior to 1.67. Once the minimum Boost
///       version is raise to 1.67 or greater, this version check can be removed.
#include <boost/version.hpp>

#if BOOST_VERSION >= 106700
#include <boost/uuid/entropy_error.hpp>
#endif

#include <kiface_base.h>
#include <confirm.h>
#include <pcb_base_frame.h>
#include <base_units.h>
#include <widgets/msgpanel.h>
#include <pgm_base.h>
#include <3d_viewer/eda_3d_viewer_frame.h>          // To include VIEWER3D_FRAMENAME
#include <footprint_editor_settings.h>
#include <pcbnew_settings.h>
#include <fp_lib_table.h>
#include <pcbnew_id.h>
#include <board.h>
#include <footprint.h>
#include <collectors.h>
#include <pcb_draw_panel_gal.h>
#include <math/vector2d.h>
#include <pcb_group.h>

#include <pcb_painter.h>
#include <settings/settings_manager.h>
#include <settings/cvpcb_settings.h>
#include <tool/tool_manager.h>
#include <tool/tool_dispatcher.h>
#include <tools/pcb_actions.h>
#include <tool/grid_menu.h>
#include "cleanup_item.h"
#include <zoom_defines.h>
#include <ratsnest/ratsnest_view_item.h>

#if defined( KICAD_USE_3DCONNEXION )
#include <navlib/nl_pcbnew_plugin.h>
#endif

using KIGFX::RENDER_SETTINGS;
using KIGFX::PCB_RENDER_SETTINGS;

wxDEFINE_EVENT( BOARD_CHANGED, wxCommandEvent );

PCB_BASE_FRAME::PCB_BASE_FRAME( KIWAY* aKiway, wxWindow* aParent, FRAME_T aFrameType,
                                const wxString& aTitle, const wxPoint& aPos, const wxSize& aSize,
                                long aStyle, const wxString& aFrameName ) :
        EDA_DRAW_FRAME( aKiway, aParent, aFrameType, aTitle, aPos, aSize, aStyle, aFrameName, pcbIUScale ),
        m_pcb( nullptr ),
        m_originTransforms( *this ),
        m_spaceMouse( nullptr )
{
}


PCB_BASE_FRAME::~PCB_BASE_FRAME()
{
#if defined( KICAD_USE_3DCONNEXION )
    if( m_spaceMouse != nullptr )
        delete m_spaceMouse;
#endif

    // Ensure m_canvasType is up to date, to save it in config
    m_canvasType = GetCanvas()->GetBackend();

    delete m_pcb;
}


bool PCB_BASE_FRAME::canCloseWindow( wxCloseEvent& aEvent )
{
    // Close modeless dialogs.  They're trouble when they get destroyed after the frame and/or
    // board.
    wxWindow* viewer3D = Get3DViewerFrame();

    if( viewer3D )
        viewer3D->Close( true );

    return true;
}


void PCB_BASE_FRAME::handleActivateEvent( wxActivateEvent& aEvent )
{
    EDA_DRAW_FRAME::handleActivateEvent( aEvent );

#if defined( KICAD_USE_3DCONNEXION )
    if( m_spaceMouse != nullptr )
    {
        m_spaceMouse->SetFocus( aEvent.GetActive() );
    }
#endif
}


void PCB_BASE_FRAME::handleIconizeEvent( wxIconizeEvent& aEvent )
{
    EDA_DRAW_FRAME::handleIconizeEvent( aEvent );

#if defined( KICAD_USE_3DCONNEXION )
    if( m_spaceMouse != nullptr && aEvent.IsIconized() )
    {
        m_spaceMouse->SetFocus( false );
    }
#endif
}


EDA_3D_VIEWER_FRAME* PCB_BASE_FRAME::Get3DViewerFrame()
{
    wxWindow* frame = FindWindowByName( QUALIFIED_VIEWER3D_FRAMENAME( this ) );
    return dynamic_cast<EDA_3D_VIEWER_FRAME*>( frame );
}


void PCB_BASE_FRAME::Update3DView( bool aMarkDirty, bool aRefresh, const wxString* aTitle )
{
    EDA_3D_VIEWER_FRAME* draw3DFrame = Get3DViewerFrame();

    if( draw3DFrame )
    {
        if( aTitle )
            draw3DFrame->SetTitle( *aTitle );

        if( aMarkDirty )
            draw3DFrame->ReloadRequest();

        if( aRefresh )
            draw3DFrame->Redraw();
    }
}


FP_LIB_TABLE* PROJECT::PcbFootprintLibs()
{
    // This is a lazy loading function, it loads the project specific table when
    // that table is asked for, not before.

    FP_LIB_TABLE*   tbl = (FP_LIB_TABLE*) GetElem( ELEM_FPTBL );

    // its gotta be NULL or a FP_LIB_TABLE, or a bug.
    wxASSERT( !tbl || tbl->Type() == FP_LIB_TABLE_T );

    if( !tbl )
    {
        // Stack the project specific FP_LIB_TABLE overlay on top of the global table.
        // ~FP_LIB_TABLE() will not touch the fallback table, so multiple projects may
        // stack this way, all using the same global fallback table.
        tbl = new FP_LIB_TABLE( &GFootprintTable );

        SetElem( ELEM_FPTBL, tbl );

        wxString projectFpLibTableFileName = FootprintLibTblName();

        try
        {
            tbl->Load( projectFpLibTableFileName );
        }
        catch( const IO_ERROR& ioe )
        {
            DisplayErrorMessage( nullptr, _( "Error loading project footprint libraries." ),
                                 ioe.What() );
        }
        catch( ... )
        {
            DisplayErrorMessage( nullptr, _( "Error loading project footprint library table." ) );
        }
    }

    return tbl;
}


void PCB_BASE_FRAME::SetBoard( BOARD* aBoard, PROGRESS_REPORTER* aReporter )
{
    if( m_pcb != aBoard )
    {
        delete m_pcb;
        m_pcb = aBoard;

        if( GetBoard() && GetCanvas() )
        {
            RENDER_SETTINGS* rs = GetCanvas()->GetView()->GetPainter()->GetSettings();

            if( rs )
            {
                rs->SetDashLengthRatio( GetBoard()->GetPlotOptions().GetDashedLineDashRatio() );
                rs->SetGapLengthRatio( GetBoard()->GetPlotOptions().GetDashedLineGapRatio() );
            }
        }

        wxCommandEvent e( BOARD_CHANGED );
        ProcessEventLocally( e );
    }
}


void PCB_BASE_FRAME::AddFootprintToBoard( FOOTPRINT* aFootprint )
{
    if( aFootprint )
    {
        GetBoard()->Add( aFootprint, ADD_MODE::APPEND );

        aFootprint->SetFlags( IS_NEW );
        aFootprint->SetPosition( wxPoint( 0, 0 ) ); // cursor in GAL may not be initialized yet

        // Put it on FRONT layer (note that it might be stored flipped if the lib is an archive
        // built from a board)
        if( aFootprint->IsFlipped() )
            aFootprint->Flip( aFootprint->GetPosition(), GetPcbNewSettings()->m_FlipLeftRight );

        // Place it in orientation 0 even if it is not saved with orientation 0 in lib (note that
        // it might be stored in another orientation if the lib is an archive built from a board)
        aFootprint->SetOrientation( ANGLE_0 );
    }
}


EDA_ITEM* PCB_BASE_FRAME::GetItem( const KIID& aId ) const
{
    return GetBoard()->GetItem( aId );
}


void PCB_BASE_FRAME::FocusOnItem( BOARD_ITEM* aItem, PCB_LAYER_ID aLayer )
{
    std::vector<BOARD_ITEM*> items;

    if( aItem )
        items.push_back( aItem );

    FocusOnItems( items, aLayer );
}


void PCB_BASE_FRAME::FocusOnItems( std::vector<BOARD_ITEM*> aItems, PCB_LAYER_ID aLayer )
{
    static std::vector<KIID> lastBrightenedItemIDs;

    BOARD_ITEM* lastItem = nullptr;

    for( KIID lastBrightenedItemID : lastBrightenedItemIDs )
    {
        /// @todo The Boost entropy exception does not exist prior to 1.67. Once the minimum Boost
        ///       version is raise to 1.67 or greater, this version check can be removed.
    #if BOOST_VERSION >= 106700
        try
        {
            lastItem = GetBoard()->GetItem( lastBrightenedItemID );
        }
        catch( const boost::uuids::entropy_error& )
        {
            wxLogError( wxT( "A Boost UUID entropy exception was thrown in %s:%s." ),
                        __FILE__, __FUNCTION__ );
        }
    #else
        lastItem = GetBoard()->GetItem( lastBrightenedItemID );
    #endif

        if( lastItem && lastItem != DELETED_BOARD_ITEM::GetInstance() )
        {
            lastItem->ClearBrightened();

            if( lastItem->Type() == PCB_FOOTPRINT_T )
            {
                static_cast<FOOTPRINT*>( lastItem )->RunOnChildren(
                        [&]( BOARD_ITEM* child )
                        {
                            child->ClearBrightened();
                        } );
            }
            else if( lastItem->Type() == PCB_GROUP_T )
            {
                static_cast<PCB_GROUP*>( lastItem )->RunOnChildren(
                        [&]( BOARD_ITEM* child )
                        {
                            child->ClearBrightened();
                        } );
            }

            GetCanvas()->GetView()->Update( lastItem );
            lastBrightenedItemID = niluuid;
            GetCanvas()->Refresh();
        }
    }

    lastBrightenedItemIDs.clear();

    if( aItems.empty() )
        return;

    VECTOR2I       focusPt;
    KIGFX::VIEW*   view = GetCanvas()->GetView();
    SHAPE_POLY_SET viewportPoly( view->GetViewport() );

    for( wxWindow* dialog : findDialogs() )
    {
        wxPoint dialogPos = GetCanvas()->ScreenToClient( dialog->GetScreenPosition() );
        SHAPE_POLY_SET dialogPoly( BOX2D( view->ToWorld( dialogPos, true ),
                                          view->ToWorld( dialog->GetSize(), false ) ) );

        try
        {
            viewportPoly.BooleanSubtract( dialogPoly, SHAPE_POLY_SET::PM_FAST );
        }
        catch( const ClipperLib::clipperException& exc )
        {
            // This may be overkill and could be an assertion but we are more likely to
            // find any clipper errors this way.
            wxLogError( wxT( "Clipper library exception '%s' occurred." ), exc.what() );
        }
    }

    SHAPE_POLY_SET itemPoly, clippedPoly;

    for( BOARD_ITEM* item : aItems )
    {
        if( item && item != DELETED_BOARD_ITEM::GetInstance() )
        {
            item->SetBrightened();

            if( item->Type() == PCB_FOOTPRINT_T )
            {
                static_cast<FOOTPRINT*>( item )->RunOnChildren(
                        [&]( BOARD_ITEM* child )
                        {
                            child->SetBrightened();
                        });
            }
            else if( item->Type() == PCB_GROUP_T )
            {
                static_cast<PCB_GROUP*>( item )->RunOnChildren(
                        [&]( BOARD_ITEM* child )
                        {
                            child->SetBrightened();
                        });
            }

            GetCanvas()->GetView()->Update( item );
            lastBrightenedItemIDs.push_back( item->m_Uuid );

            // Focus on the object's location.  Prefer a visible part of the object to its anchor
            // in order to keep from scrolling around.

            focusPt = item->GetPosition();

            if( aLayer == UNDEFINED_LAYER )
                aLayer = item->GetLayerSet().Seq()[0];

            switch( item->Type() )
            {
            case PCB_FOOTPRINT_T:
                try
                {
                    itemPoly = static_cast<FOOTPRINT*>( item )->GetBoundingHull();
                }
                catch( const ClipperLib::clipperException& exc )
                {
                    // This may be overkill and could be an assertion but we are more likely to
                    // find any clipper errors this way.
                    wxLogError( wxT( "Clipper library exception '%s' occurred." ), exc.what() );
                }

                break;

            case PCB_PAD_T:
            case PCB_MARKER_T:
            case PCB_VIA_T:
                FocusOnLocation( item->GetFocusPosition() );
                GetCanvas()->Refresh();
                return;

            case PCB_SHAPE_T:
            case PCB_TEXT_T:
            case PCB_TEXTBOX_T:
            case PCB_FP_TEXT_T:
            case PCB_FP_TEXTBOX_T:
            case PCB_FP_SHAPE_T:
            case PCB_FP_ZONE_T:
            case PCB_TRACE_T:
            case PCB_ARC_T:
            case PCB_DIM_ALIGNED_T:
            case PCB_DIM_LEADER_T:
            case PCB_DIM_CENTER_T:
            case PCB_DIM_RADIAL_T:
            case PCB_DIM_ORTHOGONAL_T:
            case PCB_FP_DIM_ALIGNED_T:
            case PCB_FP_DIM_LEADER_T:
            case PCB_FP_DIM_CENTER_T:
            case PCB_FP_DIM_RADIAL_T:
            case PCB_FP_DIM_ORTHOGONAL_T:
                item->TransformShapeToPolygon( itemPoly, aLayer, 0, pcbIUScale.mmToIU( 0.1 ),
                                               ERROR_INSIDE );
                break;

            case PCB_ZONE_T:
            {
                ZONE* zone = static_cast<ZONE*>( item );
                #if 0
                // Using the filled area shapes to find a Focus point can give good results, but
                // unfortunately the calculations are highly time consuming, even for not very
                // large areas (can be easily a few minutes for large areas).
                // so we used only the zone outline that usually do not have too many vertices.
                zone->TransformShapeToPolygon( itemPoly, aLayer, 0, pcbIUScale.mmToIU( 0.1 ),
                                               ERROR_INSIDE );

                if( itemPoly.IsEmpty() )
                    itemPoly = *zone->Outline();
                #else
                // much faster calculation time when using only the zone outlines
                itemPoly = *zone->Outline();
                #endif

                break;
            }

            default:
            {
                BOX2I item_bbox = item->GetBoundingBox();
                itemPoly.NewOutline();
                itemPoly.Append( item_bbox.GetOrigin() );
                itemPoly.Append( item_bbox.GetOrigin() + VECTOR2I( item_bbox.GetWidth(), 0 ) );
                itemPoly.Append( item_bbox.GetOrigin() + VECTOR2I( 0, item_bbox.GetHeight() ) );
                itemPoly.Append( item_bbox.GetOrigin() + VECTOR2I( item_bbox.GetWidth(),
                                                                   item_bbox.GetHeight() ) );
                break;
            }
            }

            try
            {
                clippedPoly.BooleanIntersection( itemPoly, viewportPoly, SHAPE_POLY_SET::PM_FAST );
            }
            catch( const ClipperLib::clipperException& exc )
            {
                // This may be overkill and could be an assertion but we are more likely to
                // find any clipper errors this way.
                wxLogError( wxT( "Clipper library exception '%s' occurred." ), exc.what() );
            }

            if( !clippedPoly.IsEmpty() )
                itemPoly = clippedPoly;
        }
    }

    /*
     * Perform a step-wise deflate to find the visual-center-of-mass
     */

    BOX2I    bbox = itemPoly.BBox();
    int      step = std::min( bbox.GetWidth(), bbox.GetHeight() ) / 10;

    while( !itemPoly.IsEmpty() )
    {
        focusPt = itemPoly.BBox().Centre();

        try
        {
            itemPoly.Deflate( step, 4, SHAPE_POLY_SET::CHAMFER_ACUTE_CORNERS );
        }
        catch( const ClipperLib::clipperException& exc )
        {
            // This may be overkill and could be an assertion but we are more likely to
            // find any clipper errors this way.
            wxLogError( wxT( "Clipper library exception '%s' occurred." ), exc.what() );
        }
    }

    FocusOnLocation( focusPt );

    GetCanvas()->Refresh();
}


void PCB_BASE_FRAME::HideSolderMask()
{
    KIGFX::PCB_VIEW* view = GetCanvas()->GetView();

    if( view && GetBoard()->m_SolderMask && view->HasItem( GetBoard()->m_SolderMask ) )
        view->Remove( GetBoard()->m_SolderMask );
}


void PCB_BASE_FRAME::ShowSolderMask()
{
    KIGFX::PCB_VIEW* view = GetCanvas()->GetView();

    if( view && GetBoard()->m_SolderMask )
    {
        if( view->HasItem( GetBoard()->m_SolderMask ) )
            view->Remove( GetBoard()->m_SolderMask );

        view->Add( GetBoard()->m_SolderMask );
    }
}


void PCB_BASE_FRAME::SetPageSettings( const PAGE_INFO& aPageSettings )
{
    m_pcb->SetPageSettings( aPageSettings );

    if( GetScreen() )
        GetScreen()->InitDataPoints( aPageSettings.GetSizeIU( pcbIUScale.IU_PER_MILS ) );
}


const PAGE_INFO& PCB_BASE_FRAME::GetPageSettings() const
{
    return m_pcb->GetPageSettings();
}


const wxSize PCB_BASE_FRAME::GetPageSizeIU() const
{
    // this function is only needed because EDA_DRAW_FRAME is not compiled
    // with either -DPCBNEW or -DEESCHEMA, so the virtual is used to route
    // into an application specific source file.
    return m_pcb->GetPageSettings().GetSizeIU( pcbIUScale.IU_PER_MILS );
}


const VECTOR2I& PCB_BASE_FRAME::GetGridOrigin() const
{
    return m_pcb->GetDesignSettings().GetGridOrigin();
}


void PCB_BASE_FRAME::SetGridOrigin( const VECTOR2I& aPoint )
{
    m_pcb->GetDesignSettings().SetGridOrigin( aPoint );
}


const VECTOR2I& PCB_BASE_FRAME::GetAuxOrigin() const
{
    return m_pcb->GetDesignSettings().GetAuxOrigin();
}


const VECTOR2I PCB_BASE_FRAME::GetUserOrigin() const
{
    VECTOR2I origin( 0, 0 );

    switch( GetPcbNewSettings()->m_Display.m_DisplayOrigin )
    {
    case PCB_DISPLAY_ORIGIN::PCB_ORIGIN_PAGE:                           break;
    case PCB_DISPLAY_ORIGIN::PCB_ORIGIN_AUX:  origin = GetAuxOrigin();  break;
    case PCB_DISPLAY_ORIGIN::PCB_ORIGIN_GRID: origin = GetGridOrigin(); break;
    default:                                  wxASSERT( false );        break;
    }

    return origin;
}

ORIGIN_TRANSFORMS& PCB_BASE_FRAME::GetOriginTransforms()
{
    return m_originTransforms;
}


const TITLE_BLOCK& PCB_BASE_FRAME::GetTitleBlock() const
{
    return m_pcb->GetTitleBlock();
}


void PCB_BASE_FRAME::SetTitleBlock( const TITLE_BLOCK& aTitleBlock )
{
    m_pcb->SetTitleBlock( aTitleBlock );
}


BOARD_DESIGN_SETTINGS& PCB_BASE_FRAME::GetDesignSettings() const
{
    return m_pcb->GetDesignSettings();
}


void PCB_BASE_FRAME::SetDrawBgColor( const COLOR4D& aColor )
{
    m_drawBgColor= aColor;
    m_auimgr.Update();
}


const ZONE_SETTINGS& PCB_BASE_FRAME::GetZoneSettings() const
{
    return m_pcb->GetDesignSettings().GetDefaultZoneSettings();
}


void PCB_BASE_FRAME::SetZoneSettings( const ZONE_SETTINGS& aSettings )
{
    m_pcb->GetDesignSettings().SetDefaultZoneSettings( aSettings );
}


const PCB_PLOT_PARAMS& PCB_BASE_FRAME::GetPlotSettings() const
{
    return m_pcb->GetPlotOptions();
}


void PCB_BASE_FRAME::SetPlotSettings( const PCB_PLOT_PARAMS& aSettings )
{
    m_pcb->SetPlotOptions( aSettings );
}


BOX2I PCB_BASE_FRAME::GetBoardBoundingBox( bool aBoardEdgesOnly ) const
{
    BOX2I area = aBoardEdgesOnly ? m_pcb->GetBoardEdgesBoundingBox() : m_pcb->GetBoundingBox();

    if( area.GetWidth() == 0 && area.GetHeight() == 0 )
    {
        wxSize pageSize = GetPageSizeIU();

        if( m_showBorderAndTitleBlock )
        {
            area.SetOrigin( 0, 0 );
            area.SetEnd( pageSize.x, pageSize.y );
        }
        else
        {
            area.SetOrigin( -pageSize.x / 2, -pageSize.y / 2 );
            area.SetEnd( pageSize.x / 2, pageSize.y / 2 );
        }
    }

    return area;
}


// Virtual function
void PCB_BASE_FRAME::ReCreateMenuBar()
{
}


void PCB_BASE_FRAME::ShowChangedLanguage()
{
    // call my base class
    EDA_DRAW_FRAME::ShowChangedLanguage();

    // tooltips in toolbars
    RecreateToolbars();

    // status bar
    UpdateMsgPanel();
}


EDA_3D_VIEWER_FRAME* PCB_BASE_FRAME::CreateAndShow3D_Frame()
{
    EDA_3D_VIEWER_FRAME* draw3DFrame = Get3DViewerFrame();

    if( !draw3DFrame )
        draw3DFrame = new EDA_3D_VIEWER_FRAME( &Kiway(), this, _( "3D Viewer" ) );

    // Raising the window does not show the window on Windows if iconized. This should work
    // on any platform.
    if( draw3DFrame->IsIconized() )
         draw3DFrame->Iconize( false );

    draw3DFrame->Raise();
    draw3DFrame->Show( true );

    // Raising the window does not set the focus on Linux.  This should work on any platform.
    if( wxWindow::FindFocus() != draw3DFrame )
        draw3DFrame->SetFocus();

    // Allocate a slice of time to display the 3D frame
    // a call to wxSafeYield() should be enough (and better), but on Linux we need
    // to call wxYield()
    // otherwise the activity messages are not displayed during the first board loading
    wxYield();

    // Note, the caller is responsible to load/update the board 3D view.
    // after frame creation the board is not automatically created.

    return draw3DFrame;
}


void PCB_BASE_FRAME::SwitchLayer( PCB_LAYER_ID layer )
{
    PCB_LAYER_ID preslayer = GetActiveLayer();
    auto& displ_opts = GetDisplayOptions();

    // Check if the specified layer matches the present layer
    if( layer == preslayer )
        return;

    // Copper layers cannot be selected unconditionally; how many of those layers are
    // currently enabled needs to be checked.
    if( IsCopperLayer( layer ) )
    {
        // If only one copper layer is enabled, the only such layer that can be selected to
        // is the "Copper" layer (so the selection of any other copper layer is disregarded).
        if( m_pcb->GetCopperLayerCount() < 2 )
        {
            if( layer != B_Cu )
                return;
        }

        // If more than one copper layer is enabled, the "Copper" and "Component" layers
        // can be selected, but the total number of copper layers determines which internal
        // layers are also capable of being selected.
        else
        {
            if( layer != B_Cu && layer != F_Cu && layer >= ( m_pcb->GetCopperLayerCount() - 1 ) )
                return;
        }
    }

    // Is yet more checking required? E.g. when the layer to be selected is a non-copper
    // layer, or when switching between a copper layer and a non-copper layer, or vice-versa?
    // ...

    SetActiveLayer( layer );

    if( displ_opts.m_ContrastModeDisplay != HIGH_CONTRAST_MODE::NORMAL )
        GetCanvas()->Refresh();
}


GENERAL_COLLECTORS_GUIDE PCB_BASE_FRAME::GetCollectorsGuide()
{
    GENERAL_COLLECTORS_GUIDE guide( m_pcb->GetVisibleLayers(), GetActiveLayer(),
                                    GetCanvas()->GetView() );

    // account for the globals
    guide.SetIgnoreMTextsMarkedNoShow( ! m_pcb->IsElementVisible( LAYER_MOD_TEXT_INVISIBLE ) );
    guide.SetIgnoreMTextsOnBack( ! m_pcb->IsElementVisible( LAYER_MOD_TEXT ) );
    guide.SetIgnoreMTextsOnFront( ! m_pcb->IsElementVisible( LAYER_MOD_TEXT ) );
    guide.SetIgnoreModulesOnBack( ! m_pcb->IsElementVisible( LAYER_MOD_BK ) );
    guide.SetIgnoreModulesOnFront( ! m_pcb->IsElementVisible( LAYER_MOD_FR ) );
    guide.SetIgnorePadsOnBack( ! m_pcb->IsElementVisible( LAYER_PAD_BK ) );
    guide.SetIgnorePadsOnFront( ! m_pcb->IsElementVisible( LAYER_PAD_FR ) );
    guide.SetIgnoreThroughHolePads( ! m_pcb->IsElementVisible( LAYER_PADS_TH ) );
    guide.SetIgnoreModulesVals( ! m_pcb->IsElementVisible( LAYER_MOD_VALUES ) );
    guide.SetIgnoreModulesRefs( ! m_pcb->IsElementVisible( LAYER_MOD_REFERENCES ) );
    guide.SetIgnoreThroughVias( ! m_pcb->IsElementVisible( LAYER_VIAS ) );
    guide.SetIgnoreBlindBuriedVias( ! m_pcb->IsElementVisible( LAYER_VIAS ) );
    guide.SetIgnoreMicroVias( ! m_pcb->IsElementVisible( LAYER_VIAS ) );
    guide.SetIgnoreTracks( ! m_pcb->IsElementVisible( LAYER_TRACKS ) );

    return guide;
}


void PCB_BASE_FRAME::DisplayGridMsg()
{
    VECTOR2D gridSize = GetCanvas()->GetGAL()->GetGridSize();
    wxString line;

    line.Printf( wxT( "grid X %s  Y %s" ),
                 MessageTextFromValue( gridSize.x, false ),
                 MessageTextFromValue( gridSize.y, false ) );

    SetStatusText( line, 4 );
}


void PCB_BASE_FRAME::UpdateStatusBar()
{
    EDA_DRAW_FRAME::UpdateStatusBar();

    BASE_SCREEN* screen = GetScreen();

    if( !screen )
        return;

    wxString line;
    VECTOR2D cursorPos = GetCanvas()->GetViewControls()->GetCursorPosition();

    if( GetShowPolarCoords() )  // display polar coordinates
    {
        double   dx = cursorPos.x - screen->m_LocalOrigin.x;
        double   dy = cursorPos.y - screen->m_LocalOrigin.y;
        double   theta = RAD2DEG( atan2( -dy, dx ) );
        double   ro = hypot( dx, dy );

        line.Printf( wxT( "r %s  theta %.3f" ),
                     MessageTextFromValue( ro, false ),
                     theta );

        SetStatusText( line, 3 );
    }

    // Transform absolute coordinates for user origin preferences
    double userXpos = m_originTransforms.ToDisplayAbsX( static_cast<double>( cursorPos.x ) );
    double userYpos = m_originTransforms.ToDisplayAbsY( static_cast<double>( cursorPos.y ) );

    // Display absolute coordinates:
    line.Printf( wxT( "X %s  Y %s" ),
                 MessageTextFromValue( userXpos, false ),
                 MessageTextFromValue( userYpos, false ) );
    SetStatusText( line, 2 );

    if( !GetShowPolarCoords() )  // display relative cartesian coordinates
    {
        // Calculate relative coordinates
        double relXpos = cursorPos.x - screen->m_LocalOrigin.x;
        double relYpos = cursorPos.y - screen->m_LocalOrigin.y;

        // Transform relative coordinates for user origin preferences
        userXpos = m_originTransforms.ToDisplayRelX( relXpos );
        userYpos = m_originTransforms.ToDisplayRelY( relYpos );

        line.Printf( wxT( "dx %s  dy %s  dist %s" ),
                     MessageTextFromValue( userXpos, false ),
                     MessageTextFromValue( userYpos, false ),
                     MessageTextFromValue( hypot( userXpos, userYpos ), false ) );
        SetStatusText( line, 3 );
    }

    DisplayGridMsg();
}


void PCB_BASE_FRAME::unitsChangeRefresh()
{
    EDA_DRAW_FRAME::unitsChangeRefresh();    // Update the status bar.

    UpdateGridSelectBox();
}


void PCB_BASE_FRAME::LoadSettings( APP_SETTINGS_BASE* aCfg )
{
    EDA_DRAW_FRAME::LoadSettings( aCfg );

    if( aCfg->m_Window.grid.sizes.empty() )
        aCfg->m_Window.grid.sizes = aCfg->DefaultGridSizeList();

    // Currently values read from config file are not used because the user cannot
    // change this config
    // if( aCfg->m_Window.zoom_factors.empty() )
    {
        aCfg->m_Window.zoom_factors = { ZOOM_LIST_PCBNEW };
    }

    // Some, but not all, derived classes have a PCBNEW_SETTINGS.
    if( PCBNEW_SETTINGS* pcbnew_cfg = dynamic_cast<PCBNEW_SETTINGS*>( aCfg ) )
        m_polarCoords = pcbnew_cfg->m_PolarCoords;

    wxASSERT( GetCanvas() );

    if( GetCanvas() )
    {
        RENDER_SETTINGS* rs = GetCanvas()->GetView()->GetPainter()->GetSettings();

        if( rs )
        {
            rs->SetHighlightFactor( aCfg->m_Graphics.highlight_factor );
            rs->SetSelectFactor( aCfg->m_Graphics.select_factor );
            rs->SetDefaultFont( wxEmptyString );    // Always the KiCad font for PCBs
        }
    }
}


SEVERITY PCB_BASE_FRAME::GetSeverity( int aErrorCode ) const
{
    if( aErrorCode >= CLEANUP_FIRST )
        return RPT_SEVERITY_ACTION;

    BOARD_DESIGN_SETTINGS& bds = GetBoard()->GetDesignSettings();

    return bds.m_DRCSeverities[ aErrorCode ];
}


void PCB_BASE_FRAME::SaveSettings( APP_SETTINGS_BASE* aCfg )
{
    EDA_DRAW_FRAME::SaveSettings( aCfg );

    // Some, but not all derived classes have a PCBNEW_SETTINGS.
    PCBNEW_SETTINGS* cfg = dynamic_cast<PCBNEW_SETTINGS*>( aCfg );

    if( cfg )
        cfg->m_PolarCoords = m_polarCoords;
}


PCBNEW_SETTINGS* PCB_BASE_FRAME::GetPcbNewSettings() const
{
    return Pgm().GetSettingsManager().GetAppSettings<PCBNEW_SETTINGS>();
}


FOOTPRINT_EDITOR_SETTINGS* PCB_BASE_FRAME::GetFootprintEditorSettings() const
{
    return Pgm().GetSettingsManager().GetAppSettings<FOOTPRINT_EDITOR_SETTINGS>();
}


PCB_VIEWERS_SETTINGS_BASE* PCB_BASE_FRAME::GetViewerSettingsBase() const
{
    switch( GetFrameType() )
    {
    case FRAME_PCB_EDITOR:
    case FRAME_FOOTPRINT_EDITOR:
    case FRAME_FOOTPRINT_WIZARD:
    case FRAME_PCB_DISPLAY3D:
    default:
        return Pgm().GetSettingsManager().GetAppSettings<PCBNEW_SETTINGS>();

    case FRAME_FOOTPRINT_VIEWER:
    case FRAME_FOOTPRINT_VIEWER_MODAL:
    case FRAME_FOOTPRINT_PREVIEW:
    case FRAME_CVPCB:
    case FRAME_CVPCB_DISPLAY:
        return Pgm().GetSettingsManager().GetAppSettings<CVPCB_SETTINGS>();
    }
}


MAGNETIC_SETTINGS* PCB_BASE_FRAME::GetMagneticItemsSettings()
{
    return &GetPcbNewSettings()->m_MagneticItems;
}


void PCB_BASE_FRAME::CommonSettingsChanged( bool aEnvVarsChanged, bool aTextVarsChanged )
{
    EDA_DRAW_FRAME::CommonSettingsChanged( aEnvVarsChanged, aTextVarsChanged );

    RENDER_SETTINGS*     settings = GetCanvas()->GetView()->GetPainter()->GetSettings();
    PCB_RENDER_SETTINGS* renderSettings = static_cast<KIGFX::PCB_RENDER_SETTINGS*>( settings );

    renderSettings->LoadColors( GetColorSettings( true ) );
    renderSettings->LoadDisplayOptions( GetDisplayOptions() );

    GetCanvas()->GetView()->UpdateAllItemsConditionally( KIGFX::REPAINT,
            [&]( KIGFX::VIEW_ITEM* aItem ) -> bool
            {
                if( dynamic_cast<RATSNEST_VIEW_ITEM*>( aItem ) )
                {
                    return true;    // ratsnest display
                }
                else if( dynamic_cast<PCB_TRACK*>( aItem ) )
                {
                    return true;    // track, arc & via clearance display
                }
                else if( dynamic_cast<PAD*>( aItem ) )
                {
                    return true;    // pad clearance display
                }

                return false;
            } );

    GetCanvas()->GetView()->UpdateAllItems( KIGFX::COLOR );

    RecreateToolbars();

    // The 3D viewer isn't in the Kiway, so send its update manually
    EDA_3D_VIEWER_FRAME* viewer = Get3DViewerFrame();

    if( viewer )
        viewer->CommonSettingsChanged( aEnvVarsChanged, aTextVarsChanged );
}


void PCB_BASE_FRAME::OnModify()
{
    EDA_BASE_FRAME::OnModify();

    GetScreen()->SetContentModified();
    m_autoSaveRequired = true;

    GetBoard()->IncrementTimeStamp();

    UpdateStatusBar();
    UpdateMsgPanel();
}


PCB_DRAW_PANEL_GAL* PCB_BASE_FRAME::GetCanvas() const
{
    return static_cast<PCB_DRAW_PANEL_GAL*>( EDA_DRAW_FRAME::GetCanvas() );
}


void PCB_BASE_FRAME::ActivateGalCanvas()
{
    EDA_DRAW_FRAME::ActivateGalCanvas();

    EDA_DRAW_PANEL_GAL* canvas = GetCanvas();
    KIGFX::VIEW*        view = canvas->GetView();

    if( m_toolManager )
    {
        m_toolManager->SetEnvironment( m_pcb, view, canvas->GetViewControls(), config(), this );

        m_toolManager->ResetTools( TOOL_BASE::GAL_SWITCH );
    }

    KIGFX::PCB_PAINTER*         painter = static_cast<KIGFX::PCB_PAINTER*>( view->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings = painter->GetSettings();
    const PCB_DISPLAY_OPTIONS&  displ_opts = GetDisplayOptions();

    settings->LoadDisplayOptions( displ_opts );
    settings->LoadColors( GetColorSettings() );

    view->RecacheAllItems();
    canvas->SetEventDispatcher( m_toolDispatcher );
    canvas->StartDrawing();

#if defined( KICAD_USE_3DCONNEXION )
    try
    {
        if( m_spaceMouse == nullptr )
        {
            m_spaceMouse = new NL_PCBNEW_PLUGIN( GetCanvas() );
        }
    }
    catch( const std::system_error& e )
    {
        wxLogTrace( wxT( "KI_TRACE_NAVLIB" ), e.what() );
    }
#endif
}


void PCB_BASE_FRAME::SetDisplayOptions( const PCB_DISPLAY_OPTIONS& aOptions, bool aRefresh )
{
    bool hcChanged   = m_displayOptions.m_ContrastModeDisplay != aOptions.m_ContrastModeDisplay;
    m_displayOptions = aOptions;

    EDA_DRAW_PANEL_GAL* canvas = GetCanvas();
    KIGFX::PCB_VIEW*    view   = static_cast<KIGFX::PCB_VIEW*>( canvas->GetView() );

    view->UpdateDisplayOptions( aOptions );
    canvas->SetHighContrastLayer( GetActiveLayer() );
    OnDisplayOptionsChanged();

    // Vias on a restricted layer set must be redrawn when high contrast mode is changed
    if( hcChanged )
    {
        GetCanvas()->GetView()->UpdateAllItemsConditionally( KIGFX::REPAINT,
                []( KIGFX::VIEW_ITEM* aItem ) -> bool
                {
                    if( PCB_VIA* via = dynamic_cast<PCB_VIA*>( aItem ) )
                    {
                        return via->GetViaType() == VIATYPE::BLIND_BURIED
                                || via->GetViaType() == VIATYPE::MICROVIA
                                || via->GetRemoveUnconnected();
                    }
                    else if( PAD* pad = dynamic_cast<PAD*>( aItem ) )
                    {
                        return pad->GetRemoveUnconnected();
                    }

                    return false;
                } );
    }

    if( aRefresh )
        canvas->Refresh();
}
