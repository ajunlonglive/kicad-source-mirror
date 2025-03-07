/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 CERN
 * Copyright (C) 2019-2022 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <sch_line_wire_bus_tool.h>

#include <wx/debug.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/stringimpl.h>
#include <wx/translation.h>
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <eda_item.h>
#include <bitmaps.h>
#include <core/typeinfo.h>
#include <layer_ids.h>
#include <math/vector2d.h>
#include <advanced_config.h>
#include <tool/actions.h>
#include <tool/conditional_menu.h>
#include <tool/selection.h>
#include <tool/selection_conditions.h>
#include <tool/tool_event.h>
#include <trigo.h>
#include <undo_redo_container.h>
#include <connection_graph.h>
#include <eeschema_id.h>
#include <sch_bus_entry.h>
#include <sch_connection.h>
#include <sch_edit_frame.h>
#include <sch_item.h>
#include <sch_line.h>
#include <sch_screen.h>
#include <sch_sheet.h>
#include <sch_sheet_pin.h>
#include <schematic.h>
#include <ee_actions.h>
#include <ee_grid_helper.h>
#include <ee_selection.h>
#include <ee_selection_tool.h>

class BUS_UNFOLD_MENU : public ACTION_MENU
{
public:
    BUS_UNFOLD_MENU() :
        ACTION_MENU( true ),
        m_showTitle( false )
    {
        SetIcon( BITMAPS::add_line2bus );
        SetTitle( _( "Unfold from Bus" ) );
    }

    void SetShowTitle()
    {
        m_showTitle = true;
    }

    bool PassHelpTextToHandler() override { return true; }

protected:
    ACTION_MENU* create() const override
    {
        return new BUS_UNFOLD_MENU();
    }

private:
    void update() override
    {
        SCH_EDIT_FRAME*    frame = (SCH_EDIT_FRAME*) getToolManager()->GetToolHolder();
        EE_SELECTION_TOOL* selTool = getToolManager()->GetTool<EE_SELECTION_TOOL>();
        EE_SELECTION&      selection = selTool->RequestSelection( { SCH_ITEM_LOCATE_BUS_T } );
        SCH_LINE*          bus = (SCH_LINE*) selection.Front();

        Clear();

        // TODO(JE) remove once real-time is enabled
        if( !ADVANCED_CFG::GetCfg().m_RealTimeConnectivity || !CONNECTION_GRAPH::m_allowRealTime )
        {
            frame->RecalculateConnections( NO_CLEANUP );

            // Pick up the pointer again because it may have been changed by SchematicCleanUp
            selection = selTool->RequestSelection( { SCH_ITEM_LOCATE_BUS_T } );
            bus = (SCH_LINE*) selection.Front();
        }

        if( !bus )
        {
            Append( ID_POPUP_SCH_UNFOLD_BUS, _( "No bus selected" ), wxEmptyString );
            Enable( ID_POPUP_SCH_UNFOLD_BUS, false );
            return;
        }

        SCH_CONNECTION* connection = bus->Connection();

        if( !connection || !connection->IsBus() || connection->Members().empty() )
        {
            Append( ID_POPUP_SCH_UNFOLD_BUS, _( "Bus has no members" ), wxEmptyString );
            Enable( ID_POPUP_SCH_UNFOLD_BUS, false );
            return;
        }

        int idx = 0;

        if( m_showTitle )
        {
            Append( ID_POPUP_SCH_UNFOLD_BUS, _( "Unfold from Bus" ), wxEmptyString );
            Enable( ID_POPUP_SCH_UNFOLD_BUS, false );
        }

        for( const std::shared_ptr<SCH_CONNECTION>& member : connection->Members() )
        {
            int id = ID_POPUP_SCH_UNFOLD_BUS + ( idx++ );
            wxString name = member->FullLocalName();

            if( member->Type() == CONNECTION_TYPE::BUS )
            {
                ACTION_MENU* submenu = new ACTION_MENU( true, m_tool );
                AppendSubMenu( submenu, SCH_CONNECTION::PrintBusForUI( name ), name );

                for( const std::shared_ptr<SCH_CONNECTION>& sub_member : member->Members() )
                {
                    id = ID_POPUP_SCH_UNFOLD_BUS + ( idx++ );
                    name = sub_member->FullLocalName();
                    submenu->Append( id, SCH_CONNECTION::PrintBusForUI( name ), name );
                }
            }
            else
            {
                Append( id, name, wxEmptyString );
            }
        }
    }

    bool m_showTitle;
};


SCH_LINE_WIRE_BUS_TOOL::SCH_LINE_WIRE_BUS_TOOL() :
    EE_TOOL_BASE<SCH_EDIT_FRAME>( "eeschema.InteractiveDrawingLineWireBus" ),
    m_inDrawingTool( false )
{
    m_busUnfold = {};
    m_wires.reserve( 16 );
}


SCH_LINE_WIRE_BUS_TOOL::~SCH_LINE_WIRE_BUS_TOOL()
{
}


bool SCH_LINE_WIRE_BUS_TOOL::Init()
{
    EE_TOOL_BASE::Init();

    std::shared_ptr<BUS_UNFOLD_MENU> busUnfoldMenu = std::make_shared<BUS_UNFOLD_MENU>();
    busUnfoldMenu->SetTool( this );
    m_menu.RegisterSubMenu( busUnfoldMenu );

    std::shared_ptr<BUS_UNFOLD_MENU> selBusUnfoldMenu = std::make_shared<BUS_UNFOLD_MENU>();
    selBusUnfoldMenu->SetTool( m_selectionTool );
    m_selectionTool->GetToolMenu().RegisterSubMenu( selBusUnfoldMenu );

    auto wireOrBusTool =
            [this]( const SELECTION& aSel )
            {
                return ( m_frame->IsCurrentTool( EE_ACTIONS::drawWire )
                      || m_frame->IsCurrentTool( EE_ACTIONS::drawBus ) );
            };

    auto lineTool =
            [this]( const SELECTION& aSel )
            {
                return m_frame->IsCurrentTool( EE_ACTIONS::drawLines );
            };

    auto belowRootSheetCondition =
            [&]( const SELECTION& aSel )
            {
                return m_frame->GetCurrentSheet().Last() != &m_frame->Schematic().Root();
            };

    auto busSelection = EE_CONDITIONS::MoreThan( 0 )
                            && EE_CONDITIONS::OnlyTypes( { SCH_ITEM_LOCATE_BUS_T } );

    auto haveHighlight =
            [&]( const SELECTION& sel )
            {
                SCH_EDIT_FRAME* editFrame = dynamic_cast<SCH_EDIT_FRAME*>( m_frame );

                return editFrame && editFrame->GetHighlightedConnection() != nullptr;
            };

    auto& ctxMenu = m_menu.GetMenu();

    // Build the tool menu
    //
    ctxMenu.AddItem( EE_ACTIONS::clearHighlight,       haveHighlight && EE_CONDITIONS::Idle, 1 );
    ctxMenu.AddSeparator(                              haveHighlight && EE_CONDITIONS::Idle, 1 );

    ctxMenu.AddItem( EE_ACTIONS::leaveSheet,           belowRootSheetCondition, 2 );

    ctxMenu.AddSeparator( 10 );
    ctxMenu.AddItem( EE_ACTIONS::drawWire,             wireOrBusTool && EE_CONDITIONS::Idle, 10 );
    ctxMenu.AddItem( EE_ACTIONS::drawBus,              wireOrBusTool && EE_CONDITIONS::Idle, 10 );
    ctxMenu.AddItem( EE_ACTIONS::drawLines,            lineTool && EE_CONDITIONS::Idle, 10 );

    ctxMenu.AddItem( EE_ACTIONS::undoLastSegment,      EE_CONDITIONS::ShowAlways, 10 );
    ctxMenu.AddItem( EE_ACTIONS::switchSegmentPosture, EE_CONDITIONS::ShowAlways, 10 );

    ctxMenu.AddItem( EE_ACTIONS::finishWire,           IsDrawingWire, 10 );
    ctxMenu.AddItem( EE_ACTIONS::finishBus,            IsDrawingBus, 10 );
    ctxMenu.AddItem( EE_ACTIONS::finishLine,           IsDrawingLine, 10 );

    ctxMenu.AddMenu( busUnfoldMenu.get(),              EE_CONDITIONS::Idle, 10 );

    ctxMenu.AddSeparator( 100 );
    ctxMenu.AddItem( EE_ACTIONS::placeJunction,        wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::placeLabel,           wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::placeClassLabel,      wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::placeGlobalLabel,     wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::placeHierLabel,       wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::breakWire,            wireOrBusTool && EE_CONDITIONS::Idle, 100 );
    ctxMenu.AddItem( EE_ACTIONS::breakBus,             wireOrBusTool && EE_CONDITIONS::Idle, 100 );

    ctxMenu.AddSeparator( 200 );
    ctxMenu.AddItem( EE_ACTIONS::selectNode,           wireOrBusTool && EE_CONDITIONS::Idle, 200 );
    ctxMenu.AddItem( EE_ACTIONS::selectConnection,     wireOrBusTool && EE_CONDITIONS::Idle, 200 );

    // Add bus unfolding to the selection tool
    //
    CONDITIONAL_MENU& selToolMenu = m_selectionTool->GetToolMenu().GetMenu();

    selToolMenu.AddMenu( selBusUnfoldMenu.get(),       busSelection && EE_CONDITIONS::Idle, 100 );

    return true;
}


bool SCH_LINE_WIRE_BUS_TOOL::IsDrawingLine( const SELECTION& aSelection )
{
    return IsDrawingLineWireOrBus( aSelection )
                && aSelection.Front()->IsType( { SCH_ITEM_LOCATE_GRAPHIC_LINE_T } );
}


bool SCH_LINE_WIRE_BUS_TOOL::IsDrawingWire( const SELECTION& aSelection )
{
    return IsDrawingLineWireOrBus( aSelection )
                && aSelection.Front()->IsType( { SCH_ITEM_LOCATE_WIRE_T } );
}


bool SCH_LINE_WIRE_BUS_TOOL::IsDrawingBus( const SELECTION& aSelection )
{
    return IsDrawingLineWireOrBus( aSelection )
                && aSelection.Front()->IsType( { SCH_ITEM_LOCATE_BUS_T } );
}


bool SCH_LINE_WIRE_BUS_TOOL::IsDrawingLineWireOrBus( const SELECTION& aSelection )
{
    // NOTE: for immediate hotkeys, it is NOT required that the line, wire or bus tool
    // be selected
    SCH_ITEM* item = (SCH_ITEM*) aSelection.Front();
    return item && item->IsNew() && item->Type() == SCH_LINE_T;
}


int SCH_LINE_WIRE_BUS_TOOL::DrawSegments( const TOOL_EVENT& aEvent )
{
    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    DRAW_SEGMENT_EVENT_PARAMS* params = aEvent.Parameter<DRAW_SEGMENT_EVENT_PARAMS*>();

    m_frame->PushTool( aEvent );
    m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

    if( aEvent.HasPosition() )
    {
        EE_GRID_HELPER   grid( m_toolMgr );
        grid.SetSnap( !aEvent.Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !aEvent.DisableGridSnapping() );

        VECTOR2D cursorPos = grid.BestSnapAnchor( aEvent.Position(), LAYER_CONNECTABLE, nullptr );
        startSegments( params->layer, cursorPos, params->sourceSegment );
    }

    return doDrawSegments( aEvent, params->layer, params->quitOnDraw );
}


int SCH_LINE_WIRE_BUS_TOOL::UnfoldBus( const TOOL_EVENT& aEvent )
{
    if( m_inDrawingTool )
        return 0;

    REENTRANCY_GUARD guard( &m_inDrawingTool );

    wxString* netPtr = aEvent.Parameter<wxString*>();
    wxString  net;
    SCH_LINE* segment = nullptr;

    m_frame->PushTool( aEvent );
    Activate();

    if( netPtr )
    {
        net = *netPtr;
        delete netPtr;
    }
    else
    {
        BUS_UNFOLD_MENU unfoldMenu;
        unfoldMenu.SetTool( this );
        unfoldMenu.SetShowTitle();

        SetContextMenu( &unfoldMenu, CMENU_NOW );

        while( TOOL_EVENT* evt = Wait() )
        {
            if( evt->Action() == TA_CHOICE_MENU_CHOICE )
            {
                std::optional<int> id = evt->GetCommandId();

                if( id && ( *id > 0 ) )
                    net = *evt->Parameter<wxString*>();

                break;
            }
            else if( evt->Action() == TA_CHOICE_MENU_CLOSED )
            {
                break;
            }
            else
            {
                evt->SetPassEvent();
            }
        }
    }

    // Break a wire for the given net out of the bus
    if( !net.IsEmpty() )
        segment = doUnfoldBus( net );

    // If we have an unfolded wire to draw, then draw it
    if( segment )
    {
        return doDrawSegments( aEvent, LAYER_WIRE, false );
    }
    else
    {
        m_frame->PopTool( aEvent );
        return 0;
    }
}


SCH_LINE* SCH_LINE_WIRE_BUS_TOOL::doUnfoldBus( const wxString& aNet, const VECTOR2I& aPos )
{
    SCHEMATIC_SETTINGS& cfg = getModel<SCHEMATIC>()->Settings();

    VECTOR2I pos = aPos;

    if( aPos == VECTOR2I( 0, 0 ) )
        pos = static_cast<VECTOR2I>( getViewControls()->GetCursorPosition() );

    m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

    m_busUnfold.entry = new SCH_BUS_WIRE_ENTRY( pos );
    m_busUnfold.entry->SetParent( m_frame->GetScreen() );
    m_frame->AddToScreen( m_busUnfold.entry, m_frame->GetScreen() );

    m_busUnfold.label = new SCH_LABEL( m_busUnfold.entry->GetEnd(), aNet );
    m_busUnfold.label->SetTextSize( wxSize( cfg.m_DefaultTextSize, cfg.m_DefaultTextSize ) );
    m_busUnfold.label->SetTextSpinStyle( TEXT_SPIN_STYLE::RIGHT );
    m_busUnfold.label->SetParent( m_frame->GetScreen() );
    m_busUnfold.label->SetFlags( IS_NEW | IS_MOVING );

    m_busUnfold.in_progress = true;
    m_busUnfold.origin = pos;
    m_busUnfold.net_name = aNet;

    getViewControls()->SetCrossHairCursorPosition( m_busUnfold.entry->GetEnd(), false );

    return startSegments( LAYER_WIRE, m_busUnfold.entry->GetEnd() );
}


const SCH_SHEET_PIN* SCH_LINE_WIRE_BUS_TOOL::getSheetPin( const VECTOR2I& aPosition )
{
    SCH_SCREEN* screen = m_frame->GetScreen();

    for( SCH_ITEM* item : screen->Items().Overlapping( SCH_SHEET_T, aPosition ) )
    {
        SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );

        for( SCH_SHEET_PIN* pin : sheet->GetPins() )
        {
            if( pin->GetPosition() == aPosition )
                return pin;
        }
    }

    return nullptr;
}


void SCH_LINE_WIRE_BUS_TOOL::computeBreakPoint( const std::pair<SCH_LINE*, SCH_LINE*>& aSegments,
                                                VECTOR2I&                              aPosition,
                                                LINE_MODE                              mode,
                                                bool                                   posture )
{
    wxCHECK_RET( aSegments.first && aSegments.second,
                 wxT( "Cannot compute break point of NULL line segment." ) );

    VECTOR2I  midPoint;
    SCH_LINE* segment      = aSegments.first;
    SCH_LINE* nextSegment  = aSegments.second;

    VECTOR2I delta = aPosition - segment->GetStartPoint();
    int      xDir  = delta.x > 0 ? 1 : -1;
    int      yDir  = delta.y > 0 ? 1 : -1;


    bool preferHorizontal;
    bool preferVertical;

    if( ( mode == LINE_MODE_45 ) && posture )
    {
        preferHorizontal = ( nextSegment->GetEndPoint().x - nextSegment->GetStartPoint().x ) != 0;
        preferVertical   = ( nextSegment->GetEndPoint().y - nextSegment->GetStartPoint().y ) != 0;
    }
    else
    {
        preferHorizontal = ( segment->GetEndPoint().x - segment->GetStartPoint().x ) != 0;
        preferVertical   = ( segment->GetEndPoint().y - segment->GetStartPoint().y ) != 0;
    }

    // Check for times we need to force horizontal sheet pin connections
    const SCH_SHEET_PIN* connectedPin = getSheetPin( segment->GetStartPoint() );
    SHEET_SIDE           force = connectedPin ? connectedPin->GetSide() : SHEET_SIDE::UNDEFINED;

    if( force == SHEET_SIDE::LEFT || force == SHEET_SIDE::RIGHT )
    {
        if( aPosition.x == connectedPin->GetPosition().x )  // push outside sheet boundary
        {
            int direction = ( force == SHEET_SIDE::LEFT ) ? -1 : 1;
            aPosition.x += KiROUND( getView()->GetGAL()->GetGridSize().x * direction );
        }

        preferHorizontal = true;
        preferVertical = false;
    }


    auto breakVertical = [&]() mutable
    {
        switch( mode )
        {
        case LINE_MODE_45:
            if( !posture )
            {
                midPoint.x = segment->GetStartPoint().x;
                midPoint.y = aPosition.y - yDir * abs( delta.x );
            }
            else
            {
                midPoint.x = aPosition.x;
                midPoint.y = segment->GetStartPoint().y + yDir * abs( delta.x );
            }
            break;
        default:
            midPoint.x = segment->GetStartPoint().x;
            midPoint.y = aPosition.y;
        }
    };


    auto breakHorizontal = [&]() mutable
    {
        switch( mode )
        {
        case LINE_MODE_45:
            if( !posture )
            {
                midPoint.x = aPosition.x - xDir * abs( delta.y );
                midPoint.y = segment->GetStartPoint().y;
            }
            else
            {
                midPoint.x = segment->GetStartPoint().x + xDir * abs( delta.y );
                midPoint.y = aPosition.y;
            }
            break;
        default:
            midPoint.x = aPosition.x;
            midPoint.y = segment->GetStartPoint().y;
        }
    };


    // Maintain current line shape if we can, e.g. if we were originally moving
    // vertically keep the first segment vertical
    if( preferVertical )
        breakVertical();
    else if( preferHorizontal )
        breakHorizontal();

    // Check if our 45 degree angle is one of these shapes
    //    /
    //   /
    //  /
    // /__________
    VECTOR2I deltaMidpoint = midPoint - segment->GetStartPoint();

    if( mode == LINE_MODE::LINE_MODE_45 && !posture
        && ( ( alg::signbit( deltaMidpoint.x ) != alg::signbit( delta.x ) )
             || ( alg::signbit( deltaMidpoint.y ) != alg::signbit( delta.y ) ) ) )
    {
        preferVertical = false;
        preferHorizontal = false;
    }
    else if( mode == LINE_MODE::LINE_MODE_45 && posture
             && ( ( abs( deltaMidpoint.x ) > abs( delta.x ) )
                  || ( abs( deltaMidpoint.y ) > abs( delta.y ) ) ) )
    {
        preferVertical = false;
        preferHorizontal = false;
    }

    if( !preferHorizontal && !preferVertical )
    {
        if( std::abs( delta.x ) < std::abs( delta.y ) )
            breakVertical();
        else
            breakHorizontal();
    }

    segment->SetEndPoint( midPoint );
    nextSegment->SetStartPoint( midPoint );
    nextSegment->SetEndPoint( aPosition );
}


int SCH_LINE_WIRE_BUS_TOOL::doDrawSegments( const TOOL_EVENT& aTool, int aType, bool aQuitOnDraw )
{
    SCH_SCREEN*           screen = m_frame->GetScreen();
    SCH_LINE*             segment = nullptr;
    EE_GRID_HELPER        grid( m_toolMgr );
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    int                   lastMode = m_frame->eeconfig()->m_Drawing.line_mode;
    static bool           posture = false;

    auto setCursor =
            [&]()
            {
                if( aType == LAYER_WIRE )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LINE_WIRE );
                else if( aType == LAYER_BUS )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LINE_BUS );
                else if( aType == LAYER_NOTES )
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LINE_GRAPHIC );
                else
                    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::LINE_WIRE );
            };

    auto cleanup =
            [&] ()
            {
                m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

                for( SCH_LINE* wire : m_wires )
                    delete wire;

                m_wires.clear();
                segment = nullptr;

                if( m_busUnfold.entry )
                    m_frame->RemoveFromScreen( m_busUnfold.entry, screen );

                if( m_busUnfold.label && !m_busUnfold.label_placed )
                    m_selectionTool->RemoveItemFromSel( m_busUnfold.label, true );

                if( m_busUnfold.label && m_busUnfold.label_placed )
                    m_frame->RemoveFromScreen( m_busUnfold.label, screen );

                delete m_busUnfold.entry;
                delete m_busUnfold.label;
                m_busUnfold = {};

                m_view->ClearPreview();
                m_view->ShowPreview( false );
            };

    Activate();
    // Must be done after Activate() so that it gets set into the correct context
    controls->ShowCursor( true );
    // Set initial cursor
    setCursor();

    // Add the new label to the selection so the rotate command operates on it
    if( m_busUnfold.label )
        m_selectionTool->AddItemToSel( m_busUnfold.label, true );

    // Continue the existing wires if we've started (usually by immediate action preference)
    if( !m_wires.empty() )
        segment = m_wires.back();

    wxPoint contextMenuPos;

    // Main loop: keep receiving events
    while( TOOL_EVENT* evt = Wait() )
    {
        LINE_MODE currentMode = (LINE_MODE) m_frame->eeconfig()->m_Drawing.line_mode;
        bool      twoSegments = currentMode != LINE_MODE::LINE_MODE_FREE;

        // The tool hotkey is interpreted as a click when drawing
        bool isSyntheticClick = ( segment || m_busUnfold.in_progress ) && evt->IsActivate()
                                && evt->HasPosition() && evt->Matches( aTool );

        setCursor();
        grid.SetMask( GRID_HELPER::ALL );
        grid.SetSnap( !evt->Modifier( MD_SHIFT ) );
        grid.SetUseGrid( getView()->GetGAL()->GetGridSnapping() && !evt->DisableGridSnapping() );

        if( segment )
        {
            if( segment->GetStartPoint().x == segment->GetEndPoint().x )
                grid.ClearMaskFlag( GRID_HELPER::VERTICAL );

            if( segment->GetStartPoint().y == segment->GetEndPoint().y )
                grid.ClearMaskFlag( GRID_HELPER::HORIZONTAL );
        }

        VECTOR2D eventPosition = evt->HasPosition() ? evt->Position()
                                                    : controls->GetMousePosition();

        VECTOR2I cursorPos = grid.BestSnapAnchor( eventPosition, LAYER_CONNECTABLE, segment );
        controls->ForceCursorPosition( true, cursorPos );

        // Need to handle change in H/V mode while drawing
        if( currentMode != lastMode )
        {
            // Need to delete extra segment if we have one
            if( segment && currentMode == LINE_MODE::LINE_MODE_FREE && m_wires.size() >= 2 )
            {
                m_wires.pop_back();
                m_selectionTool->RemoveItemFromSel( segment );
                delete segment;

                segment = m_wires.back();
                segment->SetEndPoint( cursorPos );
            }
            // Add a segment so we can move orthogonally/45
            else if( segment && lastMode == LINE_MODE::LINE_MODE_FREE )
            {
                segment->SetEndPoint( cursorPos );

                // Create a new segment, and chain it after the current segment.
                segment = static_cast<SCH_LINE*>( segment->Duplicate() );
                segment->SetFlags( IS_NEW | IS_MOVING );
                segment->SetStartPoint( cursorPos );
                m_wires.push_back( segment );

                m_selectionTool->AddItemToSel( segment, true /*quiet mode*/ );
            }

            lastMode = currentMode;
        }

        //------------------------------------------------------------------------
        // Handle cancel:
        //
        if( evt->IsCancelInteractive() )
        {
            m_frame->GetInfoBar()->Dismiss();

            if( segment || m_busUnfold.in_progress )
            {
                cleanup();

                if( aQuitOnDraw )
                {
                    m_frame->PopTool( aTool );
                    break;
                }
            }
            else
            {
                m_frame->PopTool( aTool );
                break;
            }
        }
        else if( evt->IsActivate() && !isSyntheticClick )
        {
            if( segment || m_busUnfold.in_progress )
            {
                m_frame->ShowInfoBarMsg( _( "Press <ESC> to cancel drawing." ) );
                evt->SetPassEvent( false );
                continue;
            }

            if( evt->IsMoveTool() )
            {
                // leave ourselves on the stack so we come back after the move
                break;
            }
            else
            {
                m_frame->PopTool( aTool );
                break;
            }
        }
        //------------------------------------------------------------------------
        // Handle finish:
        //
        else if( evt->IsAction( &EE_ACTIONS::finishLineWireOrBus )
                     || evt->IsAction( &EE_ACTIONS::finishWire )
                     || evt->IsAction( &EE_ACTIONS::finishBus )
                     || evt->IsAction( &EE_ACTIONS::finishLine ) )
        {
            if( segment || m_busUnfold.in_progress )
            {
                finishSegments();
                segment = nullptr;

                if( aQuitOnDraw )
                {
                    m_frame->PopTool( aTool );
                    break;
                }
            }
        }
        //------------------------------------------------------------------------
        // Handle click:
        //
        else if( evt->IsClick( BUT_LEFT )
                || ( segment && evt->IsDblClick( BUT_LEFT ) )
                || isSyntheticClick )
        {
            // First click when unfolding places the label and wire-to-bus entry
            if( m_busUnfold.in_progress && !m_busUnfold.label_placed )
            {
                wxASSERT( aType == LAYER_WIRE );

                m_frame->AddToScreen( m_busUnfold.label, screen );
                m_selectionTool->RemoveItemFromSel( m_busUnfold.label, true );
                m_busUnfold.label_placed = true;
            }

            if( !segment )
            {
                segment = startSegments( aType, VECTOR2D( cursorPos ) );
            }
            // Create a new segment if we're out of previously-created ones
            else if( !segment->IsNull()
                     || ( twoSegments && !m_wires[m_wires.size() - 2]->IsNull() ) )
            {
                // Terminate the command if the end point is on a pin, junction, label, or another
                // wire or bus.
                if( screen->IsTerminalPoint( cursorPos, segment->GetLayer() ) )
                {
                    finishSegments();
                    segment = nullptr;

                    if( aQuitOnDraw )
                    {
                        m_frame->PopTool( aTool );
                        break;
                    }
                }
                else
                {
                    int placedSegments = 1;

                    // When placing lines with the forty-five degree end, the user is
                    // targetting the endpoint with the angled portion, so it's more
                    // intuitive to place both segments at the same time.
                    if( currentMode == LINE_MODE::LINE_MODE_45 )
                        placedSegments++;

                    segment->SetEndPoint( cursorPos );

                    for( int i = 0; i < placedSegments; i++ )
                    {
                        // Create a new segment, and chain it after the current segment.
                        segment = static_cast<SCH_LINE*>( segment->Duplicate() );
                        segment->SetFlags( IS_NEW | IS_MOVING );
                        segment->SetStartPoint( cursorPos );
                        m_wires.push_back( segment );

                        m_selectionTool->AddItemToSel( segment, true /*quiet mode*/ );
                    }
                }
            }

            if( evt->IsDblClick( BUT_LEFT ) && segment )
            {
                if( twoSegments && m_wires.size() >= 2 )
                    computeBreakPoint( { m_wires[m_wires.size() - 2], segment }, cursorPos,
                                       currentMode, posture );

                finishSegments();
                segment = nullptr;

                if( aQuitOnDraw )
                {
                    m_frame->PopTool( aTool );
                    break;
                }
            }
        }
        //------------------------------------------------------------------------
        // Handle motion:
        //
        else if( evt->IsMotion() || evt->IsAction( &ACTIONS::refreshPreview ) )
        {
            m_view->ClearPreview();

            // Update the bus unfold posture based on the mouse movement
            if( m_busUnfold.in_progress && !m_busUnfold.label_placed )
            {
                VECTOR2I cursor_delta = cursorPos - m_busUnfold.origin;
                SCH_BUS_WIRE_ENTRY* entry = m_busUnfold.entry;

                bool flipX = ( cursor_delta.x < 0 );
                bool flipY = ( cursor_delta.y < 0 );

                // Erase and redraw if necessary
                if( flipX != m_busUnfold.flipX || flipY != m_busUnfold.flipY )
                {
                    wxSize size  = entry->GetSize();
                    int    ySign = flipY ? -1 : 1;
                    int    xSign = flipX ? -1 : 1;

                    size.x = std::abs( size.x ) * xSign;
                    size.y = std::abs( size.y ) * ySign;
                    entry->SetSize( size );

                    m_busUnfold.flipY = flipY;
                    m_busUnfold.flipX = flipX;

                    m_frame->UpdateItem( entry, false, true );
                    m_wires.front()->SetStartPoint( entry->GetEnd() );
                }

                // Update the label "ghost" position
                m_busUnfold.label->SetPosition( cursorPos );
                m_view->AddToPreview( m_busUnfold.label->Clone() );

                // Ensure segment is non-null at the start of bus unfold
                if( !segment )
                    segment = m_wires.back();
            }

            if( segment )
            {
                // Coerce the line to vertical/horizontal/45 as necessary
                if( twoSegments && m_wires.size() >= 2 )
                {
                    computeBreakPoint( { m_wires[m_wires.size() - 2], segment }, cursorPos,
                                       currentMode, posture );
                }
                else
                {
                    segment->SetEndPoint( cursorPos );
                }
            }

            for( SCH_LINE* wire : m_wires )
            {
                if( !wire->IsNull() )
                    m_view->AddToPreview( wire->Clone() );
            }
        }
        else if( evt->IsAction( &EE_ACTIONS::undoLastSegment ) )
        {
            if( ( currentMode == LINE_MODE::LINE_MODE_FREE && m_wires.size() > 1 )
                || ( LINE_MODE::LINE_MODE_90 && m_wires.size() > 2 ) )
            {
                m_view->ClearPreview();

                m_wires.pop_back();
                m_selectionTool->RemoveItemFromSel( segment );
                delete segment;

                segment = m_wires.back();
                segment->SetEndPoint( cursorPos );

                // Find new bend point for current mode
                if( twoSegments && m_wires.size() >= 2 )
                {
                    computeBreakPoint( { m_wires[m_wires.size() - 2], segment }, cursorPos,
                                       currentMode, posture );
                }
                else
                {
                    segment->SetEndPoint( cursorPos );
                }

                for( SCH_LINE* wire : m_wires )
                {
                    if( !wire->IsNull() )
                        m_view->AddToPreview( wire->Clone() );
                }
            }
            else
            {
                wxBell();
            }
        }
        else if( evt->IsAction( &EE_ACTIONS::switchSegmentPosture ) && m_wires.size() >= 2 )
        {
            posture = !posture;

            // The 90 degree mode doesn't have a forced posture like
            // the 45 degree mode and computeBreakPoint maintains existing 90s' postures.
            // Instead, just swap the 90 angle here.
            if( currentMode == LINE_MODE::LINE_MODE_90 )
            {
                m_view->ClearPreview();

                SCH_LINE* line2 = m_wires[m_wires.size() - 1];
                SCH_LINE* line1 = m_wires[m_wires.size() - 2];

                VECTOR2I delta2 = line2->GetEndPoint() - line2->GetStartPoint();
                VECTOR2I delta1 = line1->GetEndPoint() - line1->GetStartPoint();

                line2->SetStartPoint(line2->GetEndPoint() - delta1);
                line1->SetEndPoint(line1->GetStartPoint() + delta2);

                for( SCH_LINE* wire : m_wires )
                {
                    if( !wire->IsNull() )
                        m_view->AddToPreview( wire->Clone() );
                }
            }
            else
            {
                computeBreakPoint( { m_wires[m_wires.size() - 2], segment }, cursorPos, currentMode,
                                   posture );

                m_toolMgr->RunAction( ACTIONS::refreshPreview );
            }
        }
        //------------------------------------------------------------------------
        // Handle context menu:
        //
        else if( evt->IsClick( BUT_RIGHT ) )
        {
            // Warp after context menu only if dragging...
            if( !segment )
                m_toolMgr->VetoContextMenuMouseWarp();

            contextMenuPos = (wxPoint) cursorPos;
            m_menu.ShowContextMenu( m_selectionTool->GetSelection() );
        }
        else if( evt->Category() == TC_COMMAND && evt->Action() == TA_CHOICE_MENU_CHOICE )
        {
            if( *evt->GetCommandId() >= ID_POPUP_SCH_UNFOLD_BUS
                && *evt->GetCommandId() <= ID_POPUP_SCH_UNFOLD_BUS_END )
            {
                wxASSERT_MSG( !segment, "Bus unfold event received when already drawing!" );

                aType = LAYER_WIRE;
                wxString net = *evt->Parameter<wxString*>();
                segment = doUnfoldBus( net, contextMenuPos );
            }
        }
        //------------------------------------------------------------------------
        // Handle TOOL_ACTION special cases
        //
        else if( evt->IsAction( &EE_ACTIONS::rotateCW ) || evt->IsAction( &EE_ACTIONS::rotateCCW ) )
        {
            if( m_busUnfold.in_progress )
            {
                m_busUnfold.label->Rotate90( evt->IsAction( &EE_ACTIONS::rotateCW ) );
                m_toolMgr->RunAction( ACTIONS::refreshPreview );
            }
            else
            {
                wxBell();
            }
        }
        else if( evt->IsAction( &ACTIONS::doDelete ) && ( segment || m_busUnfold.in_progress ) )
        {
            cleanup();
        }
        else
        {
            evt->SetPassEvent();
        }

        // Enable autopanning and cursor capture only when there is a segment to be placed
        controls->SetAutoPan( segment != nullptr );
        controls->CaptureCursor( segment != nullptr );
    }

    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    m_frame->GetCanvas()->SetCurrentCursor( KICURSOR::ARROW );
    controls->ForceCursorPosition( false );
    return 0;
}


SCH_LINE* SCH_LINE_WIRE_BUS_TOOL::startSegments( int aType, const VECTOR2D& aPos,
                                                 SCH_LINE* aSegment )
{
    // If a segment isn't provided to copy properties from, we need to create one
    if( aSegment == nullptr )
    {
        switch( aType )
        {
        default:         aSegment = new SCH_LINE( aPos, LAYER_NOTES ); break;
        case LAYER_WIRE: aSegment = new SCH_LINE( aPos, LAYER_WIRE );  break;
        case LAYER_BUS:  aSegment = new SCH_LINE( aPos, LAYER_BUS );   break;
        }

        // Give segments a parent so they find the default line/wire/bus widths
        aSegment->SetParent( &m_frame->Schematic() );
    }
    else
    {
        aSegment = static_cast<SCH_LINE*>( aSegment->Duplicate() );
        aSegment->SetStartPoint( aPos );
    }


    aSegment->SetFlags( IS_NEW | IS_MOVING );
    m_wires.push_back( aSegment );

    m_selectionTool->AddItemToSel( aSegment, true /*quiet mode*/ );

    // We need 2 segments to go from a given start pin to an end point when the
    // horizontal and vertical lines only switch is on.
    if( m_frame->eeconfig()->m_Drawing.line_mode )
    {
        aSegment = static_cast<SCH_LINE*>( aSegment->Duplicate() );
        aSegment->SetFlags( IS_NEW | IS_MOVING );
        m_wires.push_back( aSegment );

        m_selectionTool->AddItemToSel( aSegment, true /*quiet mode*/ );
    }

    return aSegment;
}


/**
 * In a contiguous list of wires, remove wires that backtrack over the previous
 * wire. Example:
 *
 * Wire is added:
 * ---------------------------------------->
 *
 * A second wire backtracks over it:
 * -------------------<====================>
 *
 * simplifyWireList is called:
 * ------------------->
 */
void SCH_LINE_WIRE_BUS_TOOL::simplifyWireList()
{
    for( auto it = m_wires.begin(); it != m_wires.end(); )
    {
        SCH_LINE* line = *it;

        if( line->IsNull() )
        {
            delete line;
            it = m_wires.erase( it );
            continue;
        }

        auto next_it = it;
        ++next_it;

        if( next_it == m_wires.end() )
            break;

        SCH_LINE* next_line = *next_it;

        if( SCH_LINE* merged = line->MergeOverlap( m_frame->GetScreen(), next_line, false ) )
        {
            delete line;
            delete next_line;
            it = m_wires.erase( it );
            *it = merged;
        }

        ++it;
    }
}


void SCH_LINE_WIRE_BUS_TOOL::finishSegments()
{
    // Clear selection when done so that a new wire can be started.
    // NOTE: this must be done before simplifyWireList is called or we might end up with
    // freed selected items.
    m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

    SCH_SCREEN*       screen = m_frame->GetScreen();
    PICKED_ITEMS_LIST itemList;

    // Remove segments backtracking over others
    simplifyWireList();

    // Collect the possible connection points for the new lines
    std::vector<VECTOR2I> connections = m_frame->GetSchematicConnections();
    std::vector<VECTOR2I> new_ends;

    // Check each new segment for possible junctions and add/split if needed
    for( SCH_LINE* wire : m_wires )
    {
        if( wire->HasFlag( SKIP_STRUCT ) )
            continue;

        std::vector<VECTOR2I> tmpends = wire->GetConnectionPoints();

        new_ends.insert( new_ends.end(), tmpends.begin(), tmpends.end() );

        for( const VECTOR2I& pt : connections )
        {
            if( IsPointOnSegment( wire->GetStartPoint(), wire->GetEndPoint(), pt ) )
                new_ends.push_back( pt );
        }

        itemList.PushItem( ITEM_PICKER( screen, wire, UNDO_REDO::NEWITEM ) );
    }

    if( m_busUnfold.in_progress && m_busUnfold.label_placed )
    {
        wxASSERT( m_busUnfold.entry && m_busUnfold.label );

        itemList.PushItem( ITEM_PICKER( screen, m_busUnfold.entry, UNDO_REDO::NEWITEM ) );
        m_frame->SaveCopyForRepeatItem( m_busUnfold.entry );

        itemList.PushItem( ITEM_PICKER( screen, m_busUnfold.label, UNDO_REDO::NEWITEM ) );
        m_frame->AddCopyForRepeatItem( m_busUnfold.label );
        m_busUnfold.label->ClearEditFlags();
    }
    else if( !m_wires.empty() )
    {
        m_frame->SaveCopyForRepeatItem( m_wires[0] );
    }

    for( size_t ii = 1; ii < m_wires.size(); ++ii )
        m_frame->AddCopyForRepeatItem( m_wires[ii] );

    // Get the last non-null wire (this is the last created segment).
    if( !m_wires.empty() )
        m_frame->AddCopyForRepeatItem( m_wires.back() );

    // Add the new wires
    for( SCH_LINE* wire : m_wires )
    {
        wire->ClearFlags( IS_NEW | IS_MOVING );
        m_frame->AddToScreen( wire, screen );
    }

    m_wires.clear();
    m_view->ClearPreview();
    m_view->ShowPreview( false );

    getViewControls()->CaptureCursor( false );
    getViewControls()->SetAutoPan( false );

    m_frame->SaveCopyInUndoList( itemList, UNDO_REDO::NEWITEM, false );

    // Correct and remove segments that need to be merged.
    m_frame->SchematicCleanUp();

    std::vector<SCH_ITEM*> symbols;

    for( SCH_ITEM* symbol : m_frame->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
        symbols.push_back( symbol );

    for( SCH_ITEM* symbol : symbols )
    {
        std::vector<VECTOR2I> pts = symbol->GetConnectionPoints();

        if( pts.size() > 2 )
            continue;

        for( auto pt = pts.begin(); pt != pts.end(); pt++ )
        {
            for( auto secondPt = pt + 1; secondPt != pts.end(); secondPt++ )
                m_frame->TrimWire( *pt, *secondPt );
        }
    }

    for( const VECTOR2I& pt : new_ends )
    {
        if( m_frame->GetScreen()->IsExplicitJunctionNeeded( pt ) )
            m_frame->AddJunction( m_frame->GetScreen(), pt, true, false );
    }

    if( m_busUnfold.in_progress )
        m_busUnfold = {};

    m_frame->TestDanglingEnds();
    m_toolMgr->PostEvent( EVENTS::SelectedItemsModified );

    m_frame->OnModify();
}


int SCH_LINE_WIRE_BUS_TOOL::TrimOverLappingWires( const TOOL_EVENT& aEvent )
{
    EE_SELECTION* aSelection = aEvent.Parameter<EE_SELECTION*>();
    SCHEMATIC* sch = getModel<SCHEMATIC>();
    SCH_SCREEN* screen = sch->CurrentSheet().LastScreen();

    std::set<SCH_LINE*> lines;
    BOX2I bb = aSelection->GetBoundingBox();

    for( EDA_ITEM* item : screen->Items().Overlapping( SCH_LINE_T, bb ) )
        lines.insert( static_cast<SCH_LINE*>( item ) );

    for( unsigned ii = 0; ii < aSelection->GetSize(); ii++ )
    {
        SCH_ITEM* item = dynamic_cast<SCH_ITEM*>( aSelection->GetItem( ii ) );

        if( !item || !item->IsConnectable() || ( item->Type() == SCH_LINE_T ) )
            continue;

        std::vector<VECTOR2I> pts = item->GetConnectionPoints();

        /// If the line intersects with an item in the selection at only two points,
        /// then we can remove the line between the two points.
        for( SCH_LINE* line : lines )
        {
            std::vector<VECTOR2I> conn_pts;

            for( VECTOR2I pt : pts )
            {
                if( IsPointOnSegment( line->GetStartPoint(), line->GetEndPoint(), pt ) )
                    conn_pts.push_back( pt );

                if( conn_pts.size() > 2 )
                    break;
            }

            if( conn_pts.size() == 2 )
                m_frame->TrimWire( conn_pts[0], conn_pts[1] );
        }
    }

    return 0;
}


int SCH_LINE_WIRE_BUS_TOOL::AddJunctionsIfNeeded( const TOOL_EVENT& aEvent )
{
    EE_SELECTION* aSelection = aEvent.Parameter<EE_SELECTION*>();

    std::vector<VECTOR2I> pts;
    std::vector<VECTOR2I> connections = m_frame->GetSchematicConnections();

    std::set<SCH_LINE*> lines;
    BOX2I bb = aSelection->GetBoundingBox();

    for( EDA_ITEM* item : m_frame->GetScreen()->Items().Overlapping( SCH_LINE_T, bb ) )
        lines.insert( static_cast<SCH_LINE*>( item ) );

    for( unsigned ii = 0; ii < aSelection->GetSize(); ii++ )
    {
        SCH_ITEM* item = dynamic_cast<SCH_ITEM*>( aSelection->GetItem( ii ) );

        if( !item || !item->IsConnectable() )
            continue;

        std::vector<VECTOR2I> new_pts = item->GetConnectionPoints();
        pts.insert( pts.end(), new_pts.begin(), new_pts.end() );

        // If the item is a line, we also add any connection points from the rest of the schematic
        // that terminate on the line after it is moved.
        if( item->Type() == SCH_LINE_T )
        {
            SCH_LINE* line = (SCH_LINE*) item;

            for( const VECTOR2I& pt : connections )
            {
                if( IsPointOnSegment( line->GetStartPoint(), line->GetEndPoint(), pt ) )
                    pts.push_back( pt );
            }
        }
    }

    // We always have some overlapping connection points.  Drop duplicates here
    std::sort( pts.begin(), pts.end(),
               []( const VECTOR2I& a, const VECTOR2I& b ) -> bool
               {
                   return a.x < b.x || ( a.x == b.x && a.y < b.y );
               } );

    pts.erase( unique( pts.begin(), pts.end() ), pts.end() );

    for( const VECTOR2I& point : pts )
    {
        if( m_frame->GetScreen()->IsExplicitJunctionNeeded( point ) )
            m_frame->AddJunction( m_frame->GetScreen(), point, true, false );
    }

    return 0;
}


void SCH_LINE_WIRE_BUS_TOOL::setTransitions()
{
    Go( &SCH_LINE_WIRE_BUS_TOOL::AddJunctionsIfNeeded, EE_ACTIONS::addNeededJunctions.MakeEvent() );
    Go( &SCH_LINE_WIRE_BUS_TOOL::TrimOverLappingWires, EE_ACTIONS::trimOverlappingWires.MakeEvent() );
    Go( &SCH_LINE_WIRE_BUS_TOOL::DrawSegments,         EE_ACTIONS::drawWire.MakeEvent() );
    Go( &SCH_LINE_WIRE_BUS_TOOL::DrawSegments,         EE_ACTIONS::drawBus.MakeEvent() );
    Go( &SCH_LINE_WIRE_BUS_TOOL::DrawSegments,         EE_ACTIONS::drawLines.MakeEvent() );

    Go( &SCH_LINE_WIRE_BUS_TOOL::UnfoldBus,            EE_ACTIONS::unfoldBus.MakeEvent() );
}
