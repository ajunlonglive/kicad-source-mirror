/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Jon Evans <jon@craftyjon.com>
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

#include <bitmaps.h>
#include <dialog_shim.h>
#include <wx/statline.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <board.h>
#include <board_commit.h>
#include <board_design_settings.h>
#include <collectors.h>
#include <confirm.h>
#include <convert_basic_shapes_to_polygon.h>
#include <footprint.h>
#include <footprint_edit_frame.h>
#include <fp_shape.h>
#include <geometry/shape_compound.h>
#include <pcb_edit_frame.h>
#include <pcb_shape.h>
#include <pcb_track.h>
#include <tool/tool_manager.h>
#include <tools/edit_tool.h>
#include <tools/pcb_actions.h>
#include <tools/pcb_selection_tool.h>
#include <trigo.h>
#include <zone.h>

#include "convert_tool.h"


class CONVERT_SETTINGS_DIALOG : public DIALOG_SHIM
{
public:
    CONVERT_SETTINGS_DIALOG( wxWindow* aParent, CONVERT_SETTINGS* aSettings ) :
            DIALOG_SHIM( aParent, wxID_ANY, _( "Conversion Settings" ), wxDefaultPosition,
                         wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
            m_settings( aSettings ),
            m_cbIgnoreLineWidths( nullptr ),
            m_cbDeleteOriginals( nullptr )
    {
        wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );
        wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
        SetSizer( mainSizer );

        m_cbIgnoreLineWidths = new wxCheckBox( this, wxID_ANY,
                                               _( "Ignore source object line widths" )  );
        topSizer->Add( m_cbIgnoreLineWidths, 0, wxLEFT|wxRIGHT, 5 );

        m_cbDeleteOriginals = new wxCheckBox( this, wxID_ANY,
                                              _( "Delete source objects after conversion" ) );
        topSizer->Add( m_cbDeleteOriginals, 0, wxALL, 5 );

        wxStaticLine* line =  new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                wxLI_HORIZONTAL );
        topSizer->Add( line, 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 5 );

        mainSizer->Add( topSizer, 1, wxALL|wxEXPAND, 10 );

        wxBoxSizer* buttonsSizer = new wxBoxSizer( wxHORIZONTAL );
        buttonsSizer->AddStretchSpacer();

        wxStdDialogButtonSizer* sdbSizer = new wxStdDialogButtonSizer();
        wxButton* sdbSizerOK = new wxButton( this, wxID_OK );
        sdbSizer->AddButton( sdbSizerOK );
        wxButton* sdbSizerCancel = new wxButton( this, wxID_CANCEL );
        sdbSizer->AddButton( sdbSizerCancel );
        sdbSizer->Realize();

        buttonsSizer->Add( sdbSizer, 1, 0, 5 );
        mainSizer->Add( buttonsSizer, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 5 );

        SetupStandardButtons();

        finishDialogSettings();
    }

    ~CONVERT_SETTINGS_DIALOG()
    {};

protected:
    bool TransferDataToWindow() override
    {
        m_cbIgnoreLineWidths->SetValue( m_settings->m_IgnoreLineWidths );
        m_cbDeleteOriginals->SetValue( m_settings->m_DeleteOriginals );
        return true;
    }

    bool TransferDataFromWindow() override
    {
        m_settings->m_IgnoreLineWidths = m_cbIgnoreLineWidths->GetValue();
        m_settings->m_DeleteOriginals = m_cbDeleteOriginals->GetValue();
        return true;
    }

private:
    CONVERT_SETTINGS* m_settings;
    wxCheckBox*       m_cbIgnoreLineWidths;
    wxCheckBox*       m_cbDeleteOriginals;
};


CONVERT_TOOL::CONVERT_TOOL() :
    TOOL_INTERACTIVE( "pcbnew.Convert" ),
    m_selectionTool( nullptr ),
    m_menu( nullptr ),
    m_frame( nullptr )
{
}


CONVERT_TOOL::~CONVERT_TOOL()
{
    delete m_menu;
}


using S_C   = SELECTION_CONDITIONS;
using P_S_C = PCB_SELECTION_CONDITIONS;


bool CONVERT_TOOL::Init()
{
    m_selectionTool = m_toolMgr->GetTool<PCB_SELECTION_TOOL>();
    m_frame         = getEditFrame<PCB_BASE_FRAME>();

    // Create a context menu and make it available through selection tool
    m_menu = new CONDITIONAL_MENU( this );
    m_menu->SetIcon( BITMAPS::convert );
    m_menu->SetTitle( _( "Create from Selection" ) );

    auto graphicLines = S_C::OnlyTypes( { PCB_SHAPE_LOCATE_SEGMENT_T, PCB_SHAPE_LOCATE_RECT_T,
                                          PCB_SHAPE_LOCATE_CIRCLE_T, PCB_SHAPE_LOCATE_ARC_T,
                                          PCB_SHAPE_LOCATE_BEZIER_T } )
                                && P_S_C::SameLayer();

    auto graphicToTrack = S_C::OnlyTypes( { PCB_SHAPE_LOCATE_SEGMENT_T, PCB_SHAPE_LOCATE_ARC_T } );

    auto trackLines = S_C::MoreThan( 1 ) && S_C::OnlyTypes( { PCB_TRACE_T, PCB_ARC_T } )
                            && P_S_C::SameLayer();

    auto anyLines = graphicLines || trackLines;
    auto anyPolys = S_C::OnlyTypes( { PCB_ZONE_T, PCB_FP_ZONE_T,
                                      PCB_SHAPE_LOCATE_POLY_T, PCB_SHAPE_LOCATE_RECT_T } );

    auto lineToArc = S_C::Count( 1 ) && S_C::OnlyTypes( { PCB_TRACE_T, PCB_SHAPE_LOCATE_SEGMENT_T } );

    auto canCreateArray = S_C::MoreThan( 0 );

    auto showConvertMenu   = anyPolys || anyLines || lineToArc || canCreateArray;

    auto canCreatePolyType = anyLines || anyPolys;
    auto canCreateTracks   = anyPolys || graphicToTrack;

    m_menu->AddItem( PCB_ACTIONS::convertToPoly, canCreatePolyType );
    m_menu->AddItem( PCB_ACTIONS::convertToZone, canCreatePolyType );
    m_menu->AddItem( PCB_ACTIONS::convertToKeepout, canCreatePolyType );
    m_menu->AddItem( PCB_ACTIONS::convertToLines, anyPolys );
    m_menu->AppendSeparator();

    // Currently the code exists, but tracks are not really existing in footprints
    // only segments on copper layers
    if( m_frame->IsType( FRAME_PCB_EDITOR ) )
        m_menu->AddItem( PCB_ACTIONS::convertToTracks, canCreateTracks );

    m_menu->AddItem( PCB_ACTIONS::convertToArc, lineToArc );

    m_menu->AppendSeparator();
    m_menu->AddItem( PCB_ACTIONS::createArray, canCreateArray );

    CONDITIONAL_MENU& selToolMenu = m_selectionTool->GetToolMenu().GetMenu();
    selToolMenu.AddMenu( m_menu, showConvertMenu, 100 );

    return true;
}


int CONVERT_TOOL::CreatePolys( const TOOL_EVENT& aEvent )
{
    std::vector<SHAPE_POLY_SET> polys;
    CONVERT_SETTINGS            convertSettings;
    PCB_LAYER_ID                destLayer = m_frame->GetActiveLayer();
    FOOTPRINT*                  parentFootprint = nullptr;
    bool                        foundChainedSegs = false;
    bool                        foundFilledShape = false;

    PCB_SELECTION& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
            } );

    if( selection.Empty() )
        return 0;

    auto getPolys =
            [&]()
            {
                polys.clear();

                for( EDA_ITEM* item : selection )
                {
                    item->ClearTempFlags();

                    if( item->Type() == PCB_SHAPE_T || item->Type() == PCB_FP_SHAPE_T )
                        foundFilledShape = static_cast<PCB_SHAPE*>( item )->IsFilled();
                }

                SHAPE_POLY_SET polySet;

                if( convertSettings.m_IgnoreLineWidths )
                {
                    polySet.Append( makePolysFromChainedSegs( selection.GetItems() ) );
                    foundChainedSegs = polySet.OutlineCount() > 0;
                }

                polySet.Append( makePolysFromGraphics( selection.GetItems(),
                                                       convertSettings.m_IgnoreLineWidths ) );

                if( polySet.IsEmpty() )
                    return false;

                polySet.Simplify( SHAPE_POLY_SET::PM_FAST );

                for( int ii = 0; ii < polySet.OutlineCount(); ++ii )
                {
                    polys.emplace_back( SHAPE_POLY_SET( polySet.COutline( ii ) ) );

                    for( int jj = 0; jj < polySet.HoleCount( ii ); ++jj )
                        polys.back().AddHole( polySet.Hole( ii, jj ) );
                }

                return true;
            };

    // Pre-flight getPolys().  If we find any chained segments then we default m_IgnoreLineWidths
    // to true.
    // We also use the pre-flight to keep from putting up any of the dialogs if there's nothing
    // to convert.
    convertSettings.m_IgnoreLineWidths = true;
    convertSettings.m_DeleteOriginals = false;

    if( !getPolys() )
        return 0;

    convertSettings.m_IgnoreLineWidths = foundChainedSegs;

    bool isFootprint = m_frame->IsType( FRAME_FOOTPRINT_EDITOR );

    if( isFootprint )
    {
        if( FP_SHAPE* graphic = dynamic_cast<FP_SHAPE*>( selection.Front() ) )
            parentFootprint = graphic->GetParentFootprint();
        else if( FP_ZONE* zone = dynamic_cast<FP_ZONE*>( selection.Front() ) )
            parentFootprint = static_cast<FOOTPRINT*>( zone->GetParent() );
        else
            wxFAIL_MSG( wxT( "Unimplemented footprint parent in CONVERT_TOOL::CreatePolys" ) );
    }

    BOARD_COMMIT commit( m_frame );

    if( aEvent.IsAction( &PCB_ACTIONS::convertToPoly ) )
    {
        CONVERT_SETTINGS_DIALOG dlg( m_frame, &convertSettings );

        if( dlg.ShowModal() != wxID_OK )
            return 0;

        if( !getPolys() )
            return 0;

        for( const SHAPE_POLY_SET& poly : polys )
        {
            PCB_SHAPE* graphic = isFootprint ? new FP_SHAPE( parentFootprint ) : new PCB_SHAPE;

            graphic->SetShape( SHAPE_T::POLY );
            graphic->SetFilled( !convertSettings.m_IgnoreLineWidths || foundFilledShape );
            graphic->SetStroke( STROKE_PARAMS( 0, PLOT_DASH_TYPE::SOLID, COLOR4D::UNSPECIFIED ) );
            graphic->SetLayer( destLayer );
            graphic->SetPolyShape( poly );

            commit.Add( graphic );
        }
    }
    else
    {
        // Creating zone or keepout
        PCB_BASE_EDIT_FRAME*  frame    = getEditFrame<PCB_BASE_EDIT_FRAME>();
        BOARD_ITEM_CONTAINER* parent   = frame->GetModel();
        ZONE_SETTINGS         zoneInfo = frame->GetZoneSettings();

        bool nonCopper = IsNonCopperLayer( destLayer );
        zoneInfo.m_Layers.reset().set( destLayer );
        zoneInfo.m_Name.Empty();

        int ret;

        if( aEvent.IsAction( &PCB_ACTIONS::convertToKeepout ) )
        {
            zoneInfo.SetIsRuleArea( true );
            ret = InvokeRuleAreaEditor( frame, &zoneInfo, &convertSettings );
        }
        else if( nonCopper )
        {
            zoneInfo.SetIsRuleArea( false );
            ret = InvokeNonCopperZonesEditor( frame, &zoneInfo, &convertSettings );
        }
        else
        {
            zoneInfo.SetIsRuleArea( false );
            ret = InvokeCopperZonesEditor( frame, &zoneInfo, &convertSettings );
        }

        if( ret == wxID_CANCEL )
            return 0;

        if( !getPolys() )
            return 0;

        for( const SHAPE_POLY_SET& poly : polys )
        {
            ZONE* zone = isFootprint ? new FP_ZONE( parent ) : new ZONE( parent );

            *zone->Outline() = poly;
            zone->HatchBorder();

            zoneInfo.ExportSetting( *zone );

            commit.Add( zone );
        }
    }

    if( convertSettings.m_DeleteOriginals )
    {
        PCB_SELECTION selectionCopy = selection;
        m_selectionTool->ClearSelection();

        for( EDA_ITEM* item : selectionCopy )
        {
            if( item->GetFlags() & SKIP_STRUCT )
                commit.Remove( item );
        }
    }

    if( aEvent.IsAction( &PCB_ACTIONS::convertToPoly ) )
        commit.Push( _( "Convert shapes to polygon" ) );
    else
        commit.Push( _( "Convert shapes to zone" ) );

    return 0;
}


SHAPE_POLY_SET CONVERT_TOOL::makePolysFromChainedSegs( const std::deque<EDA_ITEM*>& aItems )
{
    // TODO: This code has a somewhat-similar purpose to ConvertOutlineToPolygon but is slightly
    // different, so this remains a separate algorithm.  It might be nice to analyze the dfiferences
    // in requirements and refactor this.

    // Very tight epsilon used here to account for rounding errors in import, not sloppy drawing
    const int chainingEpsilonSquared = SEG::Square( 100 );

    SHAPE_POLY_SET poly;

    // Stores pairs of (anchor, item) where anchor == 0 -> SEG.A, anchor == 1 -> SEG.B
    std::map<VECTOR2I, std::vector<std::pair<int, EDA_ITEM*>>> connections;
    std::deque<EDA_ITEM*> toCheck;

    auto closeEnough =
            []( VECTOR2I aLeft, VECTOR2I aRight, unsigned aLimit )
            {
                return ( aLeft - aRight ).SquaredEuclideanNorm() <= aLimit;
            };

    auto findInsertionPoint =
            [&]( VECTOR2I aPoint ) -> VECTOR2I
            {
                if( connections.count( aPoint ) )
                    return aPoint;

                for( const auto& candidatePair : connections )
                {
                    if( closeEnough( aPoint, candidatePair.first, chainingEpsilonSquared ) )
                        return candidatePair.first;
                }

                return aPoint;
            };

    for( EDA_ITEM* item : aItems )
    {
        item->ClearFlags( SKIP_STRUCT );

        if( std::optional<SEG> seg = getStartEndPoints( item, nullptr ) )
        {
            toCheck.push_back( item );
            connections[findInsertionPoint( seg->A )].emplace_back( std::make_pair( 0, item ) );
            connections[findInsertionPoint( seg->B )].emplace_back( std::make_pair( 1, item ) );
        }
    }

    while( !toCheck.empty() )
    {
        EDA_ITEM* candidate = toCheck.front();
        toCheck.pop_front();

        if( candidate->GetFlags() & SKIP_STRUCT )
            continue;

        int width = -1;
        SHAPE_LINE_CHAIN outline;

        auto insert =
                [&]( EDA_ITEM* aItem, VECTOR2I aAnchor, bool aDirection )
                {
                    if( aItem->Type() == PCB_ARC_T
                        || ( ( aItem->Type() == PCB_SHAPE_T || aItem->Type() == PCB_FP_SHAPE_T )
                             && static_cast<PCB_SHAPE*>( aItem )->GetShape() == SHAPE_T::ARC ) )
                    {
                        SHAPE_ARC arc;

                        if( aItem->Type() == PCB_ARC_T )
                        {
                            PCB_ARC* pcb_arc = static_cast<PCB_ARC*>( aItem );
                            arc = *static_cast<SHAPE_ARC*>( pcb_arc->GetEffectiveShape().get() );
                        }
                        else
                        {
                            PCB_SHAPE* pcb_shape = static_cast<PCB_SHAPE*>( aItem );
                            arc = SHAPE_ARC( pcb_shape->GetStart(), pcb_shape->GetArcMid(),
                                             pcb_shape->GetEnd(), pcb_shape->GetWidth() );
                        }

                        if( aDirection )
                            outline.Append( aAnchor == arc.GetP0() ? arc : arc.Reversed() );
                        else
                            outline.Insert( 0, aAnchor == arc.GetP0() ? arc : arc.Reversed() );
                    }
                    else if( aItem->IsType( { PCB_SHAPE_LOCATE_BEZIER_T } ) )
                    {
                        PCB_SHAPE* graphic = static_cast<PCB_SHAPE*>( aItem );

                        if( aAnchor == graphic->GetStart() )
                        {
                            for( auto it = graphic->GetBezierPoints().begin();
                                             it != graphic->GetBezierPoints().end();
                                             ++it )
                            {
                                if( aDirection )
                                    outline.Append( *it );
                                else
                                    outline.Insert( 0, *it );
                            }

                        }
                        else
                        {
                            for( auto it = graphic->GetBezierPoints().rbegin();
                                             it != graphic->GetBezierPoints().rend();
                                             ++it )
                            {
                                if( aDirection )
                                    outline.Append( *it );
                                else
                                    outline.Insert( 0, *it );
                            }
                        }
                    }
                    else
                    {
                        std::optional<SEG> nextSeg = getStartEndPoints( aItem, &width );
                        wxASSERT( nextSeg );

                        VECTOR2I& point = ( aAnchor == nextSeg->A ) ? nextSeg->B : nextSeg->A;

                        if( aDirection )
                            outline.Append( point );
                        else
                            outline.Insert( 0, point );
                    }
                };

        // aDirection == true for walking "right" and appending to the end of points
        // false for walking "left" and prepending to the beginning
        std::function<void( EDA_ITEM*, VECTOR2I, bool )> process =
                [&]( EDA_ITEM* aItem, VECTOR2I aAnchor, bool aDirection )
                {
                    if( aItem->GetFlags() & SKIP_STRUCT )
                        return;

                    aItem->SetFlags( SKIP_STRUCT );

                    insert( aItem, aAnchor, aDirection );

                    std::optional<SEG> anchors = getStartEndPoints( aItem, &width );
                    wxASSERT( anchors );

                    VECTOR2I nextAnchor = ( aAnchor == anchors->A ) ? anchors->B : anchors->A;

                    for( std::pair<int, EDA_ITEM*> pair : connections[nextAnchor] )
                    {
                        if( pair.second == aItem )
                            continue;

                        process( pair.second, nextAnchor, aDirection );
                    }
                };

        std::optional<SEG> anchors = getStartEndPoints( candidate, &width );
        wxASSERT( anchors );

        // Start with the first object and walk "right"
        // Note if the first object is an arc, we don't need to insert its first point here, the
        // whole arc will be inserted at anchor B inside process()
        if( !( candidate->Type() == PCB_ARC_T
               || ( ( candidate->Type() == PCB_SHAPE_T || candidate->Type() == PCB_FP_SHAPE_T )
                    && static_cast<PCB_SHAPE*>( candidate )->GetShape() == SHAPE_T::ARC ) ) )
        {
            insert( candidate, anchors->A, true );
        }

        process( candidate, anchors->B, true );

        // check for any candidates on the "left"
        EDA_ITEM* left = nullptr;

        for( std::pair<int, EDA_ITEM*> possibleLeft : connections[anchors->A] )
        {
            if( possibleLeft.second != candidate )
            {
                left = possibleLeft.second;
                break;
            }
        }

        if( left )
            process( left, anchors->A, false );

        if( outline.PointCount() < 3 )
            continue;

        outline.SetClosed( true );
        outline.Simplify();

        if( width >= 0 )
            outline.SetWidth( width );

        poly.AddOutline( outline );
    }

    return poly;
}


SHAPE_POLY_SET CONVERT_TOOL::makePolysFromGraphics( const std::deque<EDA_ITEM*>& aItems,
                                                    bool aIgnoreLineWidths )
{
    BOARD_DESIGN_SETTINGS& bds = m_frame->GetBoard()->GetDesignSettings();
    SHAPE_POLY_SET         poly;

    for( EDA_ITEM* item : aItems )
    {
        if( item->GetFlags() & SKIP_STRUCT )
            continue;

        switch( item->Type() )
        {
        case PCB_SHAPE_T:
        case PCB_FP_SHAPE_T:
        {
            PCB_SHAPE* temp = static_cast<PCB_SHAPE*>( item->Clone() );

            if( aIgnoreLineWidths )
                temp->SetFilled( true );

            temp->TransformShapeToPolygon( poly, UNDEFINED_LAYER, 0, bds.m_MaxError, ERROR_INSIDE,
                                           aIgnoreLineWidths );
            item->SetFlags( SKIP_STRUCT );
            break;
        }

        case PCB_ZONE_T:
        case PCB_FP_ZONE_T:
            poly.Append( *static_cast<ZONE*>( item )->Outline() );
            item->SetFlags( SKIP_STRUCT );
            break;

        default:
            continue;
        }
    }

    return poly;
}


int CONVERT_TOOL::CreateLines( const TOOL_EVENT& aEvent )
{
    auto& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    switch( item->Type() )
                    {
                    case PCB_SHAPE_T:
                    case PCB_FP_SHAPE_T:
                        switch( static_cast<PCB_SHAPE*>( item )->GetShape() )
                        {
                        case SHAPE_T::SEGMENT:
                        case SHAPE_T::ARC:
                        case SHAPE_T::POLY:
                        case SHAPE_T::RECT:
                            break;

                        default:
                            aCollector.Remove( item );
                        }

                        break;

                    case PCB_ZONE_T:
                    case PCB_FP_ZONE_T:
                        break;

                    default:
                        aCollector.Remove( item );
                    }
                }
            } );

    if( selection.Empty() )
        return 0;

    auto getPolySet =
            []( EDA_ITEM* aItem )
            {
                SHAPE_POLY_SET set;

                switch( aItem->Type() )
                {
                case PCB_ZONE_T:
                case PCB_FP_ZONE_T:
                    set = *static_cast<ZONE*>( aItem )->Outline();
                    break;

                case PCB_SHAPE_T:
                case PCB_FP_SHAPE_T:
                {
                    PCB_SHAPE* graphic = static_cast<PCB_SHAPE*>( aItem );

                    if( graphic->GetShape() == SHAPE_T::POLY )
                    {
                        set = graphic->GetPolyShape();
                    }
                    else if( graphic->GetShape() == SHAPE_T::RECT )
                    {
                        SHAPE_LINE_CHAIN outline;
                        VECTOR2I start( graphic->GetStart() );
                        VECTOR2I end( graphic->GetEnd() );

                        outline.Append( start );
                        outline.Append( VECTOR2I( end.x, start.y ) );
                        outline.Append( end );
                        outline.Append( VECTOR2I( start.x, end.y ) );
                        outline.SetClosed( true );

                        set.AddOutline( outline );
                    }
                    else
                    {
                        wxFAIL_MSG( wxT( "Unhandled graphic shape type in PolyToLines - getPolySet" ) );
                    }
                    break;
                }

                default:
                    wxFAIL_MSG( wxT( "Unhandled type in PolyToLines - getPolySet" ) );
                    break;
                }

                return set;
            };

    auto getSegList =
            []( SHAPE_POLY_SET& aPoly )
            {
                std::vector<SEG> segs;

                // Our input should be valid polys, so OK to assert here
                wxASSERT( aPoly.VertexCount() >= 2 );

                for( int i = 1; i < aPoly.VertexCount(); i++ )
                    segs.emplace_back( SEG( aPoly.CVertex( i - 1 ), aPoly.CVertex( i ) ) );

                segs.emplace_back( SEG( aPoly.CVertex( aPoly.VertexCount() - 1 ),
                                        aPoly.CVertex( 0 ) ) );

                return segs;
            };

    BOARD_COMMIT          commit( m_frame );
    PCB_BASE_EDIT_FRAME*  frame       = getEditFrame<PCB_BASE_EDIT_FRAME>();
    FOOTPRINT_EDIT_FRAME* fpEditor    = dynamic_cast<FOOTPRINT_EDIT_FRAME*>( m_frame );
    FOOTPRINT*            footprint   = nullptr;
    PCB_LAYER_ID          targetLayer = m_frame->GetActiveLayer();
    BOARD_ITEM_CONTAINER* parent      = frame->GetModel();

    if( fpEditor )
        footprint = fpEditor->GetBoard()->GetFirstFootprint();

    auto handleGraphicSeg =
            [&]( EDA_ITEM* aItem )
            {
                if( aItem->Type() != PCB_SHAPE_T && aItem->Type() != PCB_FP_SHAPE_T )
                    return false;

                PCB_SHAPE* graphic = static_cast<PCB_SHAPE*>( aItem );

                if( graphic->GetShape() == SHAPE_T::SEGMENT )
                {
                    PCB_TRACK* track = new PCB_TRACK( parent );

                    track->SetLayer( targetLayer );
                    track->SetStart( graphic->GetStart() );
                    track->SetEnd( graphic->GetEnd() );
                    track->SetWidth( graphic->GetWidth() );
                    commit.Add( track );

                    return true;
                }
                else if( graphic->GetShape() == SHAPE_T::ARC )
                {
                    PCB_ARC* arc = new PCB_ARC( parent );

                    arc->SetLayer( targetLayer );
                    arc->SetStart( graphic->GetStart() );
                    arc->SetEnd( graphic->GetEnd() );
                    arc->SetMid( graphic->GetArcMid() );
                    arc->SetWidth( graphic->GetWidth() );
                    commit.Add( arc );

                    return true;
                }

                return false;
            };

    if( aEvent.IsAction( &PCB_ACTIONS::convertToTracks ) )
    {
        if( !IsCopperLayer( targetLayer ) )
        {
            targetLayer = frame->SelectOneLayer( F_Cu, LSET::AllNonCuMask() );

            if( targetLayer == UNDEFINED_LAYER )    // User canceled
                return true;
        }
    }

    for( EDA_ITEM* item : selection )
    {
        if( handleGraphicSeg( item ) )
            continue;

        SHAPE_POLY_SET   polySet = getPolySet( item );
        std::vector<SEG> segs    = getSegList( polySet );

        if( aEvent.IsAction( &PCB_ACTIONS::convertToLines ) )
        {
            for( SEG& seg : segs )
            {
                if( fpEditor )
                {
                    FP_SHAPE* graphic = new FP_SHAPE( footprint, SHAPE_T::SEGMENT );

                    graphic->SetLayer( targetLayer );
                    graphic->SetStart( VECTOR2I( seg.A ) );
                    graphic->SetStart0( VECTOR2I( seg.A ) );
                    graphic->SetEnd( VECTOR2I( seg.B ) );
                    graphic->SetEnd0( VECTOR2I( seg.B ) );
                    commit.Add( graphic );
                }
                else
                {
                    PCB_SHAPE* graphic = new PCB_SHAPE( nullptr, SHAPE_T::SEGMENT );

                    graphic->SetLayer( targetLayer );
                    graphic->SetStart( VECTOR2I( seg.A ) );
                    graphic->SetEnd( VECTOR2I( seg.B ) );
                    commit.Add( graphic );
                }
            }
        }
        else
        {
            // I am really unsure converting a polygon to "tracks" (i.e. segments on
            // copper layers) make sense for footprints, but anyway this code exists
            if( fpEditor )
            {
                // Creating segments on copper layer
                for( SEG& seg : segs )
                {
                    FP_SHAPE* graphic = new FP_SHAPE( footprint, SHAPE_T::SEGMENT );
                    graphic->SetLayer( targetLayer );
                    graphic->SetStart( VECTOR2I( seg.A ) );
                    graphic->SetStart0( VECTOR2I( seg.A ) );
                    graphic->SetEnd( VECTOR2I( seg.B ) );
                    graphic->SetEnd0( VECTOR2I( seg.B ) );
                    commit.Add( graphic );
                }
            }
            else
            {
                // Creating tracks
                for( SEG& seg : segs )
                {
                    PCB_TRACK* track = new PCB_TRACK( parent );

                    track->SetLayer( targetLayer );
                    track->SetStart( VECTOR2I( seg.A ) );
                    track->SetEnd( VECTOR2I( seg.B ) );
                    commit.Add( track );
                }
            }
        }
    }

    commit.Push( _( "Convert polygons to lines" ) );

    return 0;
}


int CONVERT_TOOL::SegmentToArc( const TOOL_EVENT& aEvent )
{
    auto& selection = m_selectionTool->RequestSelection(
            []( const VECTOR2I& aPt, GENERAL_COLLECTOR& aCollector, PCB_SELECTION_TOOL* sTool )
            {
                for( int i = aCollector.GetCount() - 1; i >= 0; --i )
                {
                    BOARD_ITEM* item = aCollector[i];

                    if( !( item->Type() == PCB_SHAPE_T ||
                           item->Type() == PCB_TRACE_T ||
                           item->Type() == PCB_FP_SHAPE_T ) )
                    {
                        aCollector.Remove( item );
                    }
                }
            } );

    EDA_ITEM* source = selection.Front();
    VECTOR2I start, end, mid;

    // Offset the midpoint along the normal a little bit so that it's more obviously an arc
    const double offsetRatio = 0.1;

    if( std::optional<SEG> seg = getStartEndPoints( source, nullptr ) )
    {
        start = seg->A;
        end   = seg->B;

        VECTOR2I normal = ( seg->B - seg->A ).Perpendicular().Resize( offsetRatio * seg->Length() );
        mid = seg->Center() + normal;
    }
    else
    {
        return -1;
    }

    PCB_BASE_EDIT_FRAME*  frame  = getEditFrame<PCB_BASE_EDIT_FRAME>();
    BOARD_ITEM_CONTAINER* parent = frame->GetModel();

    BOARD_ITEM* boardItem = dynamic_cast<BOARD_ITEM*>( source );

    // Don't continue processing if we don't actually have a board item
    if( !boardItem )
        return 0;

    PCB_LAYER_ID layer = boardItem->GetLayer();

    BOARD_COMMIT commit( m_frame );

    if( source->Type() == PCB_SHAPE_T || source->Type() == PCB_FP_SHAPE_T )
    {
        PCB_SHAPE* line = static_cast<PCB_SHAPE*>( source );
        PCB_SHAPE* arc  = new PCB_SHAPE( parent, SHAPE_T::ARC );

        VECTOR2I center = CalcArcCenter( start, mid, end );

        arc->SetFilled( false );
        arc->SetLayer( layer );
        arc->SetStroke( line->GetStroke() );

        arc->SetCenter( VECTOR2I( center ) );
        arc->SetStart( VECTOR2I( start ) );
        arc->SetEnd( VECTOR2I( end ) );

        commit.Add( arc );
    }
    else
    {
        wxASSERT( source->Type() == PCB_TRACE_T );
        PCB_TRACK* line = static_cast<PCB_TRACK*>( source );
        PCB_ARC*   arc  = new PCB_ARC( parent );

        arc->SetLayer( layer );
        arc->SetWidth( line->GetWidth() );
        arc->SetStart( VECTOR2I( start ) );
        arc->SetMid( VECTOR2I( mid ) );
        arc->SetEnd( VECTOR2I( end ) );

        commit.Add( arc );
    }

    commit.Push( _( "Create arc from line segment" ) );

    return 0;
}


std::optional<SEG> CONVERT_TOOL::getStartEndPoints( EDA_ITEM* aItem, int* aWidth )
{
    switch( aItem->Type() )
    {
    case PCB_SHAPE_T:
    case PCB_FP_SHAPE_T:
    {
        PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( aItem );

        switch( shape->GetShape() )
        {
        case SHAPE_T::SEGMENT:
        case SHAPE_T::ARC:
        case SHAPE_T::POLY:
        case SHAPE_T::BEZIER:
            if( shape->GetStart() == shape->GetEnd() )
                return std::nullopt;

            if( aWidth )
                *aWidth = shape->GetWidth();

            return std::make_optional<SEG>( VECTOR2I( shape->GetStart() ),
                                            VECTOR2I( shape->GetEnd() ) );

        default:
            return std::nullopt;
        }
    }

    case PCB_TRACE_T:
    {
        PCB_TRACK* line = static_cast<PCB_TRACK*>( aItem );

        if( aWidth )
            *aWidth = line->GetWidth();

        return std::make_optional<SEG>( VECTOR2I( line->GetStart() ), VECTOR2I( line->GetEnd() ) );
    }

    case PCB_ARC_T:
    {
        PCB_ARC* arc = static_cast<PCB_ARC*>( aItem );

        if( aWidth )
            *aWidth = arc->GetWidth();

        return std::make_optional<SEG>( VECTOR2I( arc->GetStart() ), VECTOR2I( arc->GetEnd() ) );
    }

    default:
        return std::nullopt;
    }
}


void CONVERT_TOOL::setTransitions()
{
    Go( &CONVERT_TOOL::CreatePolys,    PCB_ACTIONS::convertToPoly.MakeEvent() );
    Go( &CONVERT_TOOL::CreatePolys,    PCB_ACTIONS::convertToZone.MakeEvent() );
    Go( &CONVERT_TOOL::CreatePolys,    PCB_ACTIONS::convertToKeepout.MakeEvent() );
    Go( &CONVERT_TOOL::CreateLines,    PCB_ACTIONS::convertToLines.MakeEvent() );
    Go( &CONVERT_TOOL::CreateLines,    PCB_ACTIONS::convertToTracks.MakeEvent() );
    Go( &CONVERT_TOOL::SegmentToArc,   PCB_ACTIONS::convertToArc.MakeEvent() );
}
