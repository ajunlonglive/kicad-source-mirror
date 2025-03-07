/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014-2017 CERN
 * Copyright (C) 2014-2022 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Maciej Suminski <maciej.suminski@cern.ch>
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
#include <cstdint>
#include <thread>
#include <zone.h>
#include <connectivity/connectivity_data.h>
#include <board_commit.h>
#include <footprint.h>
#include <pcb_track.h>
#include <pad.h>
#include <pcb_group.h>
#include <board_design_settings.h>
#include <progress_reporter.h>
#include <widgets/infobar.h>
#include <widgets/wx_progress_reporters.h>
#include <wx/event.h>
#include <wx/hyperlink.h>
#include <tool/tool_manager.h>
#include "pcb_actions.h"
#include "zone_filler_tool.h"
#include "zone_filler.h"


ZONE_FILLER_TOOL::ZONE_FILLER_TOOL() :
    PCB_TOOL_BASE( "pcbnew.ZoneFiller" ),
    m_fillInProgress( false )
{
}


ZONE_FILLER_TOOL::~ZONE_FILLER_TOOL()
{
}


void ZONE_FILLER_TOOL::Reset( RESET_REASON aReason )
{
}


void ZONE_FILLER_TOOL::CheckAllZones( wxWindow* aCaller, PROGRESS_REPORTER* aReporter )
{
    if( !getEditFrame<PCB_EDIT_FRAME>()->m_ZoneFillsDirty || m_fillInProgress )
        return;

    m_fillInProgress = true;

    std::vector<ZONE*> toFill;

    for( ZONE* zone : board()->Zones() )
        toFill.push_back( zone );

    BOARD_COMMIT                          commit( this );
    std::unique_ptr<WX_PROGRESS_REPORTER> reporter;
    ZONE_FILLER                           filler( frame()->GetBoard(), &commit );

    if( aReporter )
    {
        filler.SetProgressReporter( aReporter );
    }
    else
    {
        reporter = std::make_unique<WX_PROGRESS_REPORTER>( aCaller, _( "Checking Zones" ), 4 );
        filler.SetProgressReporter( reporter.get() );
    }

    if( filler.Fill( toFill, true, aCaller ) )
    {
        commit.Push( _( "Fill Zone(s)" ), SKIP_CONNECTIVITY | ZONE_FILL_OP );
        getEditFrame<PCB_EDIT_FRAME>()->m_ZoneFillsDirty = false;
    }
    else
    {
        commit.Revert();
    }

    board()->BuildConnectivity();

    refresh();

    m_fillInProgress = false;
}


void ZONE_FILLER_TOOL::singleShotRefocus( wxIdleEvent& )
{
    canvas()->SetFocus();
    canvas()->Unbind( wxEVT_IDLE, &ZONE_FILLER_TOOL::singleShotRefocus, this );
}


void ZONE_FILLER_TOOL::FillAllZones( wxWindow* aCaller, PROGRESS_REPORTER* aReporter )
{
    PCB_EDIT_FRAME*    frame = getEditFrame<PCB_EDIT_FRAME>();
    std::vector<ZONE*> toFill;

    if( m_fillInProgress )
        return;

    m_fillInProgress = true;

    for( ZONE* zone : board()->Zones() )
        toFill.push_back( zone );

    board()->IncrementTimeStamp();    // Clear caches

    BOARD_COMMIT                          commit( this );
    std::unique_ptr<WX_PROGRESS_REPORTER> reporter;
    ZONE_FILLER                           filler( board(), &commit );

    if( !board()->GetDesignSettings().m_DRCEngine->RulesValid() )
    {
        WX_INFOBAR* infobar = frame->GetInfoBar();
        wxHyperlinkCtrl* button = new wxHyperlinkCtrl( infobar, wxID_ANY, _( "Show DRC rules" ),
                                                       wxEmptyString );

        button->Bind( wxEVT_COMMAND_HYPERLINK,
                      std::function<void( wxHyperlinkEvent& aEvent )>(
                      [frame]( wxHyperlinkEvent& aEvent )
                      {
                          frame->ShowBoardSetupDialog( _( "Rules" ) );
                      } ) );

        infobar->RemoveAllButtons();
        infobar->AddButton( button );

        infobar->ShowMessageFor( _( "Zone fills may be inaccurate.  DRC rules contain errors." ),
                                 10000, wxICON_WARNING );
    }

    if( aReporter )
    {
        filler.SetProgressReporter( aReporter );
    }
    else
    {
        reporter = std::make_unique<WX_PROGRESS_REPORTER>( aCaller, _( "Fill All Zones" ), 5 );
        filler.SetProgressReporter( reporter.get() );
    }

    if( filler.Fill( toFill ) )
    {
        filler.GetProgressReporter()->AdvancePhase();

        commit.Push( _( "Fill Zone(s)" ), SKIP_CONNECTIVITY | ZONE_FILL_OP );
        frame->m_ZoneFillsDirty = false;
    }
    else
    {
        commit.Revert();
    }

    board()->BuildConnectivity( reporter.get() );

    if( filler.IsDebug() )
        frame->UpdateUserInterface();

    refresh();

    m_fillInProgress = false;

    // wxWidgets has keyboard focus issues after the progress reporter.  Re-setting the focus
    // here doesn't work, so we delay it to an idle event.
    canvas()->Bind( wxEVT_IDLE, &ZONE_FILLER_TOOL::singleShotRefocus, this );
}


int ZONE_FILLER_TOOL::ZoneFillDirty( const TOOL_EVENT& aEvent )
{
    PCB_EDIT_FRAME*    frame = getEditFrame<PCB_EDIT_FRAME>();
    std::vector<ZONE*> toFill;

    for( ZONE* zone : board()->Zones() )
    {
        if( m_dirtyZoneIDs.count( zone->m_Uuid ) )
            toFill.push_back( zone );
    }

    if( toFill.empty() )
        return 0;

    if( m_fillInProgress )
        return 0;

    m_fillInProgress = true;

    m_dirtyZoneIDs.clear();

    board()->IncrementTimeStamp();    // Clear caches

    BOARD_COMMIT                          commit( this );
    std::unique_ptr<WX_PROGRESS_REPORTER> reporter;
    ZONE_FILLER                           filler( board(), &commit );
    int                                   pts = 0;

    if( !board()->GetDesignSettings().m_DRCEngine->RulesValid() )
    {
        WX_INFOBAR* infobar = frame->GetInfoBar();
        wxHyperlinkCtrl* button = new wxHyperlinkCtrl( infobar, wxID_ANY, _( "Show DRC rules" ),
                                                       wxEmptyString );

        button->Bind( wxEVT_COMMAND_HYPERLINK,
                      std::function<void( wxHyperlinkEvent& aLocEvent )>(
                      [frame]( wxHyperlinkEvent& aLocEvent )
                      {
                          frame->ShowBoardSetupDialog( _( "Rules" ) );
                      } ) );

        infobar->RemoveAllButtons();
        infobar->AddButton( button );

        infobar->ShowMessageFor( _( "Zone fills may be inaccurate.  DRC rules contain errors." ),
                                 10000, wxICON_WARNING );
    }

    for( ZONE* zone : toFill )
    {
        for( PCB_LAYER_ID layer : zone->GetLayerSet().Seq() )
            pts += zone->GetFilledPolysList( layer )->FullPointCount();

        if( pts > 1000 )
        {
            wxString title = wxString::Format( _( "Refill %d Zones" ), (int) toFill.size() );

            reporter = std::make_unique<WX_PROGRESS_REPORTER>( frame, title, 5 );
            filler.SetProgressReporter( reporter.get() );
            break;
        }
    }

    if( filler.Fill( toFill ) )
        commit.Push( _( "Auto-fill Zone(s)" ), APPEND_UNDO | SKIP_CONNECTIVITY | ZONE_FILL_OP );
    else
        commit.Revert();

    board()->BuildConnectivity( reporter.get() );

    if( filler.IsDebug() )
        frame->UpdateUserInterface();

    refresh();

    m_fillInProgress = false;

    // wxWidgets has keyboard focus issues after the progress reporter.  Re-setting the focus
    // here doesn't work, so we delay it to an idle event.
    canvas()->Bind( wxEVT_IDLE, &ZONE_FILLER_TOOL::singleShotRefocus, this );

    return 0;
}


int ZONE_FILLER_TOOL::ZoneFillAll( const TOOL_EVENT& aEvent )
{
    FillAllZones( frame() );
    return 0;
}


int ZONE_FILLER_TOOL::ZoneUnfillAll( const TOOL_EVENT& aEvent )
{
    BOARD_COMMIT commit( this );

    for( ZONE* zone : board()->Zones() )
    {
        commit.Modify( zone );

        zone->UnFill();
    }

    commit.Push( _( "Unfill All Zones" ), ZONE_FILL_OP );

    refresh();

    return 0;
}


void ZONE_FILLER_TOOL::refresh()
{
    canvas()->GetView()->UpdateAllItemsConditionally( KIGFX::REPAINT,
            [&]( KIGFX::VIEW_ITEM* aItem ) -> bool
            {
                if( PCB_VIA* via = dynamic_cast<PCB_VIA*>( aItem ) )
                {
                    return via->GetRemoveUnconnected();
                }
                else if( PAD* pad = dynamic_cast<PAD*>( aItem ) )
                {
                    return pad->GetRemoveUnconnected();
                }

                return false;
            } );

    canvas()->Refresh();
}


void ZONE_FILLER_TOOL::setTransitions()
{
    // Zone actions
    Go( &ZONE_FILLER_TOOL::ZoneFillAll,    PCB_ACTIONS::zoneFillAll.MakeEvent() );
    Go( &ZONE_FILLER_TOOL::ZoneFillDirty, PCB_ACTIONS::zoneFillDirty.MakeEvent() );
    Go( &ZONE_FILLER_TOOL::ZoneUnfillAll,  PCB_ACTIONS::zoneUnfillAll.MakeEvent() );
}
