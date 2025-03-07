/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 CERN
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

#include <symbol_library.h>
#include <confirm.h>
#include <connection_graph.h>
#include <dialogs/dialog_symbol_fields_table.h>
#include <dialogs/dialog_eeschema_page_settings.h>
#include <dialogs/dialog_paste_special.h>
#include <dialogs/dialog_plot_schematic.h>
#include <dialogs/dialog_symbol_remap.h>
#include <dialogs/dialog_assign_netclass.h>
#include <project_rescue.h>
#include <erc.h>
#include <fmt.h>
#include <invoke_sch_dialog.h>
#include <string_utils.h>
#include <kiway.h>
#include <kiway_player.h>
#include <netlist_exporters/netlist_exporter_spice.h>
#include <paths.h>
#include <project/project_file.h>
#include <project/net_settings.h>
#include <sch_edit_frame.h>
#include <sch_plugins/kicad/sch_sexpr_plugin.h>
#include <sch_line.h>
#include <sch_shape.h>
#include <sch_painter.h>
#include <sch_sheet.h>
#include <sch_sheet_pin.h>
#include <sch_view.h>
#include <schematic.h>
#include <advanced_config.h>
#include <sim/sim_plot_frame.h>
#include <sim/spice_generator.h>
#include <sim/sim_lib_mgr.h>
#include "symbol_library_manager.h"
#include <symbol_viewer_frame.h>
#include <status_popup.h>
#include <tool/picker_tool.h>
#include <tool/tool_manager.h>
#include <tools/ee_actions.h>
#include <tools/ee_selection.h>
#include <tools/ee_selection_tool.h>
#include <tools/sch_editor_control.h>
#include <drawing_sheet/ds_proxy_undo_item.h>
#include <dialog_update_from_pcb.h>
#include <eda_list_dialog.h>
#include <wildcards_and_files_ext.h>
#include <wx_filename.h>
#include <sch_sheet_path.h>
#include <wx/filedlg.h>


int SCH_EDITOR_CONTROL::New( const TOOL_EVENT& aEvent )
{
    m_frame->NewProject();
    return 0;
}


int SCH_EDITOR_CONTROL::Open( const TOOL_EVENT& aEvent )
{
    m_frame->LoadProject();
    return 0;
}


int SCH_EDITOR_CONTROL::Save( const TOOL_EVENT& aEvent )
{
    m_frame->SaveProject();
    return 0;
}


int SCH_EDITOR_CONTROL::SaveAs( const TOOL_EVENT& aEvent )
{
    m_frame->SaveProject( true );
    return 0;
}


int SCH_EDITOR_CONTROL::SaveCurrSheetCopyAs( const TOOL_EVENT& aEvent )
{
    SCH_SHEET* curr_sheet = m_frame->GetCurrentSheet().Last();
    wxFileName curr_fn = curr_sheet->GetFileName();
    wxFileDialog dlg( m_frame, _( "Schematic Files" ), curr_fn.GetPath(),
                      curr_fn.GetFullName(), KiCadSchematicFileWildcard(),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( dlg.ShowModal() == wxID_CANCEL )
        return false;

    wxFileName newFileName = dlg.GetPath();
    newFileName.SetExt( KiCadSchematicFileExtension );

    m_frame->saveSchematicFile( curr_sheet, newFileName.GetFullPath() );
    return 0;
}


int SCH_EDITOR_CONTROL::Revert( const TOOL_EVENT& aEvent )
{
    SCHEMATIC& schematic = m_frame->Schematic();
    SCH_SHEET& root = schematic.Root();

    if( m_frame->GetCurrentSheet().Last() != &root )
    {
        m_toolMgr->RunAction( ACTIONS::cancelInteractive, true );
        m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );

        // Store the current zoom level into the current screen before switching
        m_frame->GetScreen()->m_LastZoomLevel = m_frame->GetCanvas()->GetView()->GetScale();

        SCH_SHEET_PATH rootSheetPath;
        rootSheetPath.push_back( &root );
        m_frame->SetCurrentSheet( rootSheetPath );
        m_frame->DisplayCurrentSheet();

        wxSafeYield();
    }

    wxString msg;
    msg.Printf( _( "Revert '%s' (and all sub-sheets) to last version saved?" ),
                schematic.GetFileName() );

    if( !IsOK( m_frame, msg ) )
        return false;

    SCH_SCREENS screenList( schematic.Root() );

    for( SCH_SCREEN* screen = screenList.GetFirst(); screen; screen = screenList.GetNext() )
        screen->SetContentModified( false );    // do not prompt the user for changes

    m_frame->ReleaseFile();
    m_frame->OpenProjectFiles( std::vector<wxString>( 1, schematic.GetFileName() ) );

    return 0;
}


int SCH_EDITOR_CONTROL::ShowSchematicSetup( const TOOL_EVENT& aEvent )
{
    m_frame->ShowSchematicSetupDialog();
    return 0;
}


int SCH_EDITOR_CONTROL::PageSetup( const TOOL_EVENT& aEvent )
{
    PICKED_ITEMS_LIST   undoCmd;
    DS_PROXY_UNDO_ITEM* undoItem = new DS_PROXY_UNDO_ITEM( m_frame );
    ITEM_PICKER         wrapper( m_frame->GetScreen(), undoItem, UNDO_REDO::PAGESETTINGS );

    undoCmd.PushItem( wrapper );
    m_frame->SaveCopyInUndoList( undoCmd, UNDO_REDO::PAGESETTINGS, false, false );

    DIALOG_EESCHEMA_PAGE_SETTINGS dlg( m_frame, wxSize( MAX_PAGE_SIZE_EESCHEMA_MILS,
                                                        MAX_PAGE_SIZE_EESCHEMA_MILS ) );
    dlg.SetWksFileName( BASE_SCREEN::m_DrawingSheetFileName );

    if( dlg.ShowModal() )
    {
        // Update text variables
        m_frame->GetCanvas()->GetView()->MarkDirty();
        m_frame->GetCanvas()->GetView()->UpdateAllItems( KIGFX::REPAINT );
        m_frame->GetCanvas()->Refresh();

        m_frame->OnModify();
    }
    else
    {
        m_frame->RollbackSchematicFromUndo();
    }

    return 0;
}


int SCH_EDITOR_CONTROL::RescueSymbols( const TOOL_EVENT& aEvent )
{
    SCH_SCREENS schematic( m_frame->Schematic().Root() );

    if( schematic.HasNoFullyDefinedLibIds() )
        RescueLegacyProject( true );
    else
        RescueSymbolLibTableProject( true );

    return 0;
}


bool SCH_EDITOR_CONTROL::RescueLegacyProject( bool aRunningOnDemand )
{
    LEGACY_RESCUER rescuer( m_frame->Prj(), &m_frame->Schematic(), &m_frame->GetCurrentSheet(),
                            m_frame->GetCanvas()->GetBackend() );

    return rescueProject( rescuer, aRunningOnDemand );
}


bool SCH_EDITOR_CONTROL::RescueSymbolLibTableProject( bool aRunningOnDemand )
{
    SYMBOL_LIB_TABLE_RESCUER rescuer( m_frame->Prj(), &m_frame->Schematic(),
                                      &m_frame->GetCurrentSheet(),
                                      m_frame->GetCanvas()->GetBackend() );

    return rescueProject( rescuer, aRunningOnDemand );
}


bool SCH_EDITOR_CONTROL::rescueProject( RESCUER& aRescuer, bool aRunningOnDemand )
{
    if( !RESCUER::RescueProject( m_frame, aRescuer, aRunningOnDemand ) )
        return false;

    if( aRescuer.GetCandidateCount() )
    {
        KIWAY_PLAYER* viewer = m_frame->Kiway().Player( FRAME_SCH_VIEWER, false );

        if( viewer )
            static_cast<SYMBOL_VIEWER_FRAME*>( viewer )->ReCreateLibList();

        if( aRunningOnDemand )
        {
            SCH_SCREENS schematic( m_frame->Schematic().Root() );

            schematic.UpdateSymbolLinks();
            m_frame->RecalculateConnections( GLOBAL_CLEANUP );
        }

        m_frame->ClearUndoRedoList();
        m_frame->SyncView();
        m_frame->GetCanvas()->Refresh();
        m_frame->OnModify();
    }

    return true;
}


int SCH_EDITOR_CONTROL::RemapSymbols( const TOOL_EVENT& aEvent )
{
    DIALOG_SYMBOL_REMAP dlgRemap( m_frame );

    dlgRemap.ShowQuasiModal();

    m_frame->GetCanvas()->Refresh( true );

    return 0;
}


int SCH_EDITOR_CONTROL::Print( const TOOL_EVENT& aEvent )
{
    if( !ADVANCED_CFG::GetCfg().m_RealTimeConnectivity || !CONNECTION_GRAPH::m_allowRealTime )
        m_frame->RecalculateConnections( NO_CLEANUP );

    InvokeDialogPrintUsingPrinter( m_frame );

    wxFileName fn = m_frame->Prj().AbsolutePath( m_frame->Schematic().RootScreen()->GetFileName() );

    if( fn.GetName() != NAMELESS_PROJECT )
        m_frame->SaveProjectSettings();

    return 0;
}


int SCH_EDITOR_CONTROL::Plot( const TOOL_EVENT& aEvent )
{
    if( !ADVANCED_CFG::GetCfg().m_RealTimeConnectivity || !CONNECTION_GRAPH::m_allowRealTime )
        m_frame->RecalculateConnections( NO_CLEANUP );

    DIALOG_PLOT_SCHEMATIC dlg( m_frame );

    dlg.ShowModal();

    // save project config if the prj config has changed:
    if( dlg.PrjConfigChanged() )
        m_frame->SaveProjectSettings();

    return 0;
}


int SCH_EDITOR_CONTROL::Quit( const TOOL_EVENT& aEvent )
{
    m_frame->Close( false );
    return 0;
}


// A dummy wxFindReplaceData signaling any marker should be found
static EDA_SEARCH_DATA g_markersOnly;


int SCH_EDITOR_CONTROL::FindAndReplace( const TOOL_EVENT& aEvent )
{
    m_frame->ShowFindReplaceDialog( aEvent.IsAction( &ACTIONS::findAndReplace ) );
    return UpdateFind( aEvent );
}


int SCH_EDITOR_CONTROL::UpdateFind( const TOOL_EVENT& aEvent )
{
    EDA_SEARCH_DATA& data = m_frame->GetFindReplaceData();

    auto visit =
            [&]( EDA_ITEM* aItem, SCH_SHEET_PATH* aSheet )
            {
                // We may get triggered when the dialog is not opened due to binding
                // SelectedItemsModified we also get triggered when the find dialog is
                // closed....so we need to double check the dialog is open.
                if( m_frame->m_findReplaceDialog != nullptr
                    && !data.findString.IsEmpty()
                    && aItem->Matches( data, aSheet ) )
                {
                    aItem->SetForceVisible( true );
                    m_selectionTool->BrightenItem( aItem );
                }
                else if( aItem->IsBrightened() )
                {
                    aItem->SetForceVisible( false );
                    m_selectionTool->UnbrightenItem( aItem );
                }
            };

    if( aEvent.IsAction( &ACTIONS::find ) || aEvent.IsAction( &ACTIONS::findAndReplace )
        || aEvent.IsAction( &ACTIONS::updateFind ) )
    {
        m_selectionTool->ClearSelection();

        for( SCH_ITEM* item : m_frame->GetScreen()->Items() )
        {
            visit( item, &m_frame->GetCurrentSheet() );

            item->RunOnChildren(
                    [&]( SCH_ITEM* aChild )
                    {
                        visit( aChild, &m_frame->GetCurrentSheet() );
                    } );
        }
    }
    else if( aEvent.Matches( EVENTS::SelectedItemsModified ) )
    {
        for( EDA_ITEM* item : m_selectionTool->GetSelection() )
            visit( item, &m_frame->GetCurrentSheet() );
    }

    getView()->UpdateItems();
    m_frame->GetCanvas()->Refresh();
    m_frame->UpdateTitle();

    return 0;
}


SCH_ITEM* SCH_EDITOR_CONTROL::nextMatch( SCH_SCREEN* aScreen, SCH_SHEET_PATH* aSheet,
                                         SCH_ITEM* aAfter, EDA_SEARCH_DATA& aData )
{
    bool past_item = true;

    if( aAfter != nullptr )
    {
        past_item = false;

        if( aAfter->Type() == SCH_PIN_T || aAfter->Type() == SCH_FIELD_T )
            aAfter = static_cast<SCH_ITEM*>( aAfter->GetParent() );
    }

    std::vector<SCH_ITEM*> sorted_items;

    for( SCH_ITEM* item : aScreen->Items() )
        sorted_items.push_back( item );

    std::sort( sorted_items.begin(), sorted_items.end(),
            [&]( SCH_ITEM* a, SCH_ITEM* b )
            {
                if( a->GetPosition().x == b->GetPosition().x )
                {
                    // Ensure deterministic sort
                    if( a->GetPosition().y == b->GetPosition().y )
                        return a->m_Uuid < b->m_Uuid;

                    return a->GetPosition().y < b->GetPosition().y;
                }
                else
                    return a->GetPosition().x < b->GetPosition().x;
            }
        );

    for( SCH_ITEM* item : sorted_items )
    {
        if( item == aAfter )
        {
            past_item = true;
        }
        else if( past_item )
        {
            if( &aData == &g_markersOnly && item->Type() == SCH_MARKER_T )
                return item;

            if( item->Matches( aData, aSheet ) )
                return item;

            if( item->Type() == SCH_SYMBOL_T )
            {
                SCH_SYMBOL* cmp = static_cast<SCH_SYMBOL*>( item );

                for( SCH_FIELD& field : cmp->GetFields() )
                {
                    if( field.Matches( aData, aSheet ) )
                        return &field;
                }

                for( SCH_PIN* pin : cmp->GetPins() )
                {
                    if( pin->Matches( aData, aSheet ) )
                        return pin;
                }
            }

            if( item->Type() == SCH_SHEET_T )
            {
                SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );

                for( SCH_FIELD& field : sheet->GetFields() )
                {
                    if( field.Matches( aData, aSheet ) )
                        return &field;
                }

                for( SCH_SHEET_PIN* pin : sheet->GetPins() )
                {
                    if( pin->Matches( aData, aSheet ) )
                        return pin;
                }
            }
        }
    }

    return nullptr;
}


int SCH_EDITOR_CONTROL::FindNext( const TOOL_EVENT& aEvent )
{
    EDA_SEARCH_DATA& data = m_frame->GetFindReplaceData();
    bool searchAllSheets = false;
    try
    {
        const SCH_SEARCH_DATA& schSearchData = dynamic_cast<const SCH_SEARCH_DATA&>( data );
        searchAllSheets = !( schSearchData.searchCurrentSheetOnly );
    }
    catch( const std::bad_cast& )
    {
    }

    // A timer during which a subsequent FindNext will result in a wrap-around
    static wxTimer wrapAroundTimer;

    if( aEvent.IsAction( &ACTIONS::findNextMarker ) )
    {
       // g_markersOnly.SetFlags( data.GetFlags() );

       // data = g_markersOnly;
    }
    else if( data.findString.IsEmpty() )
    {
        return FindAndReplace( ACTIONS::find.MakeEvent() );
    }

    EE_SELECTION& selection       = m_selectionTool->GetSelection();
    SCH_ITEM*     afterItem       = dynamic_cast<SCH_ITEM*>( selection.Front() );
    SCH_ITEM*     item            = nullptr;

    SCH_SHEET_PATH* afterSheet    = &m_frame->GetCurrentSheet();

    if( wrapAroundTimer.IsRunning() )
    {
        afterSheet = nullptr;
        afterItem = nullptr;
        wrapAroundTimer.Stop();
        m_frame->ClearFindReplaceStatus();
    }

    m_selectionTool->ClearSelection();

    if( afterSheet || !searchAllSheets )
        item = nextMatch( m_frame->GetScreen(), &m_frame->GetCurrentSheet(), afterItem, data );

    if( !item && searchAllSheets )
    {
        SCH_SCREENS                  screens( m_frame->Schematic().Root() );
        std::vector<SCH_SHEET_PATH*> paths;

        screens.BuildClientSheetPathList();

        for( SCH_SCREEN* screen = screens.GetFirst(); screen; screen = screens.GetNext() )
        {
            for( SCH_SHEET_PATH& sheet : screen->GetClientSheetPaths() )
                paths.push_back( &sheet );
        }

        std::sort( paths.begin(), paths.end(), [] ( const SCH_SHEET_PATH* lhs,
                                                    const SCH_SHEET_PATH* rhs ) -> bool
                {
                    int retval = lhs->ComparePageNum( *rhs );

                    if( retval < 0 )
                        return true;
                    else if( retval > 0 )
                        return false;
                    else /// Enforce strict ordering.  If the page numbers are the same, use UUIDs
                        return lhs->GetCurrentHash() < rhs->GetCurrentHash();
                } );

        for( SCH_SHEET_PATH* sheet : paths )
        {
            if( afterSheet )
            {
                if( afterSheet->GetPageNumber() == sheet->GetPageNumber() )
                    afterSheet = nullptr;

                continue;
            }

            item = nextMatch( sheet->LastScreen(), sheet, nullptr, data );

            if( item )
            {
                m_frame->Schematic().SetCurrentSheet( *sheet );
                m_frame->DisplayCurrentSheet();
                UpdateFind( ACTIONS::updateFind.MakeEvent() );

                break;
            }
        }
    }

    if( item )
    {
        m_selectionTool->AddItemToSel( item );
        m_frame->FocusOnLocation( item->GetBoundingBox().GetCenter() );
        m_frame->GetCanvas()->Refresh();
    }
    else
    {
        wxString msg = searchAllSheets ? _( "Reached end of schematic." )
                                       : _( "Reached end of sheet." );

       // Show the popup during the time period the user can wrap the search
        m_frame->ShowFindReplaceStatus( msg + wxS( " " ) +
                                        _( "Find again to wrap around to the start." ), 4000 );
        wrapAroundTimer.StartOnce( 4000 );
    }

    return 0;
}


bool SCH_EDITOR_CONTROL::HasMatch()
{
    EDA_SEARCH_DATA& data = m_frame->GetFindReplaceData();
    EDA_ITEM*          item = m_selectionTool->GetSelection().Front();

    return item && item->Matches( data, &m_frame->GetCurrentSheet() );
}


int SCH_EDITOR_CONTROL::ReplaceAndFindNext( const TOOL_EVENT& aEvent )
{
    EDA_SEARCH_DATA& data = m_frame->GetFindReplaceData();
    EDA_ITEM*          item = m_selectionTool->GetSelection().Front();
    SCH_SHEET_PATH*    sheet = &m_frame->GetCurrentSheet();

    if( data.findString.IsEmpty() )
        return FindAndReplace( ACTIONS::find.MakeEvent() );

    if( item && item->Matches( data, sheet ) )
    {
        SCH_ITEM* sch_item = static_cast<SCH_ITEM*>( item );

        m_frame->SaveCopyInUndoList( sheet->LastScreen(), sch_item, UNDO_REDO::CHANGED, false );

        if( item->Replace( data, sheet ) )
        {
            m_frame->UpdateItem( item, false, true );
            m_frame->GetCurrentSheet().UpdateAllScreenReferences();
            m_frame->OnModify();
        }

        FindNext( ACTIONS::findNext.MakeEvent() );
    }

    return 0;
}


int SCH_EDITOR_CONTROL::ReplaceAll( const TOOL_EVENT& aEvent )
{
    EDA_SEARCH_DATA& data = m_frame->GetFindReplaceData();
    bool             currentSheetOnly = false;

    try
    {
        const SCH_SEARCH_DATA& schSearchData = dynamic_cast<const SCH_SEARCH_DATA&>( data );
        currentSheetOnly = schSearchData.searchCurrentSheetOnly;
    }
    catch( const std::bad_cast& )
    {
    }

    bool modified = false;

    if( data.findString.IsEmpty() )
        return FindAndReplace( ACTIONS::find.MakeEvent() );

    auto doReplace =
            [&]( SCH_ITEM* aItem, SCH_SHEET_PATH* aSheet, EDA_SEARCH_DATA& aData )
            {
                m_frame->SaveCopyInUndoList( aSheet->LastScreen(), aItem, UNDO_REDO::CHANGED,
                                             modified );

                if( aItem->Replace( aData, aSheet ) )
                {
                    m_frame->UpdateItem( aItem, false, true );
                    modified = true;
                }
            };

    if( currentSheetOnly )
    {
        SCH_SHEET_PATH* currentSheet = &m_frame->GetCurrentSheet();

        SCH_ITEM* item = nextMatch( m_frame->GetScreen(), currentSheet, nullptr, data );

        while( item )
        {
            doReplace( item, currentSheet, data );
            item = nextMatch( m_frame->GetScreen(), currentSheet, item, data );
        }
    }
    else
    {
        SCH_SHEET_LIST allSheets = m_frame->Schematic().GetSheets();
        SCH_SCREENS    screens( m_frame->Schematic().Root() );

        for( SCH_SCREEN* screen = screens.GetFirst(); screen; screen = screens.GetNext() )
        {
            SCH_SHEET_LIST sheets = allSheets.FindAllSheetsForScreen( screen );

            for( unsigned ii = 0; ii < sheets.size(); ++ii )
            {
                SCH_ITEM* item = nextMatch( screen, &sheets[ii], nullptr, data );

                while( item )
                {
                    if( ii == 0 )
                    {
                        doReplace( item, &sheets[0], data );
                    }
                    else if( item->Type() == SCH_FIELD_T )
                    {
                        SCH_FIELD* field = static_cast<SCH_FIELD*>( item );

                        if( field->GetParent() && field->GetParent()->Type() == SCH_SYMBOL_T )
                        {
                            switch( field->GetId() )
                            {
                            case REFERENCE_FIELD:
                            case VALUE_FIELD:
                            case FOOTPRINT_FIELD:
                                // must be handled for each distinct sheet
                                doReplace( field, &sheets[ii], data );
                                break;

                            default:
                                // handled in first iteration
                                break;
                            }
                        }
                    }

                    item = nextMatch( screen, &sheets[ii], item, data );
                }
            }
        }
    }

    if( modified )
    {
        m_frame->GetCurrentSheet().UpdateAllScreenReferences();
        m_frame->OnModify();
    }

    return 0;
}


int SCH_EDITOR_CONTROL::CrossProbeToPcb( const TOOL_EVENT& aEvent )
{
    doCrossProbeSchToPcb( aEvent, false );
    return 0;
}


int SCH_EDITOR_CONTROL::ExplicitCrossProbeToPcb( const TOOL_EVENT& aEvent )
{
    doCrossProbeSchToPcb( aEvent, true );
    return 0;
}


void SCH_EDITOR_CONTROL::doCrossProbeSchToPcb( const TOOL_EVENT& aEvent, bool aForce )
{
    // Don't get in an infinite loop SCH -> PCB -> SCH -> PCB -> SCH -> ...
    if( m_probingPcbToSch || m_frame->IsSyncingSelection() )
        return;

    EE_SELECTION_TOOL*      selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();

    EE_SELECTION& selection = aForce ? selTool->RequestSelection() : selTool->GetSelection();

    m_frame->SendSelectItemsToPcb( selection.GetItemsSortedBySelectionOrder(), aForce );
}


int SCH_EDITOR_CONTROL::ExportSymbolsToLibrary( const TOOL_EVENT& aEvent )
{
    bool createNew = aEvent.IsAction( &EE_ACTIONS::exportSymbolsToNewLibrary );

    SCH_REFERENCE_LIST symbols;
    m_frame->Schematic().GetSheets().GetSymbols( symbols, false );

    std::map<LIB_ID, LIB_SYMBOL*> libSymbols;
    std::map<LIB_ID, std::vector<SCH_SYMBOL*>> symbolMap;

    for( size_t i = 0; i < symbols.GetCount(); ++i )
    {
        SCH_SYMBOL* symbol = symbols[i].GetSymbol();
        LIB_SYMBOL* libSymbol = symbol->GetLibSymbolRef().get();
        LIB_ID id = libSymbol->GetLibId();

        if( libSymbols.count( id ) )
        {
            wxASSERT_MSG( libSymbols[id]->Compare( *libSymbol, LIB_ITEM::COMPARE_FLAGS::ERC ) == 0,
                          "Two symbols have the same LIB_ID but are different!" );
        }
        else
        {
            libSymbols[id] = libSymbol;
        }

        symbolMap[id].emplace_back( symbol );
    }

    SYMBOL_LIBRARY_MANAGER mgr( *m_frame );

    wxString targetLib;

    if( createNew )
    {
        wxFileName fn;
        SYMBOL_LIB_TABLE* libTable = m_frame->SelectSymLibTable();

        if( !m_frame->LibraryFileBrowser( false, fn, KiCadSymbolLibFileWildcard(),
                                          KiCadSymbolLibFileExtension, false,
                                          ( libTable == &SYMBOL_LIB_TABLE::GetGlobalLibTable() ),
                                          PATHS::GetDefaultUserSymbolsPath() ) )
        {
            return 0;
        }

        targetLib = fn.GetName();

        if( libTable->HasLibrary( targetLib, false ) )
        {
            DisplayError( m_frame, wxString::Format( _( "Library '%s' already exists." ),
                                                     targetLib ) );
            return 0;
        }

        if( !mgr.CreateLibrary( fn.GetFullPath(), libTable ) )
        {
            DisplayError( m_frame, wxString::Format( _( "Could not add library '%s'." ),
                                                     targetLib ) );
            return 0;
        }
    }
    else
    {
        targetLib = m_frame->SelectLibraryFromList();
    }

    if( targetLib.IsEmpty() )
        return 0;

    bool map = IsOK( m_frame, _( "Update symbols in schematic to refer to new library?" ) );

    SYMBOL_LIB_TABLE_ROW* row = mgr.GetLibrary( targetLib );
    SCH_IO_MGR::SCH_FILE_T type = SCH_IO_MGR::EnumFromStr( row->GetType() );
    SCH_PLUGIN::SCH_PLUGIN_RELEASER pi( SCH_IO_MGR::FindPlugin( type ) );

    wxFileName dest = row->GetFullURI( true );
    dest.Normalize( FN_NORMALIZE_FLAGS | wxPATH_NORM_ENV_VARS );

    for( const std::pair<const LIB_ID, LIB_SYMBOL*>& it : libSymbols )
    {
        LIB_SYMBOL* origSym = it.second;
        LIB_SYMBOL* newSym = origSym->Flatten().release();

        pi->SaveSymbol( dest.GetFullPath(), newSym );

        if( map )
        {
            LIB_ID id = it.first;
            id.SetLibNickname( targetLib );

            for( SCH_SYMBOL* symbol : symbolMap[it.first] )
                symbol->SetLibId( id );
        }
    }

    return 0;
}


#ifdef KICAD_SPICE

#define HITTEST_THRESHOLD_PIXELS 5

int SCH_EDITOR_CONTROL::SimProbe( const TOOL_EVENT& aEvent )
{
    PICKER_TOOL*    picker = m_toolMgr->GetTool<PICKER_TOOL>();
    SIM_PLOT_FRAME* simFrame = (SIM_PLOT_FRAME*) m_frame->Kiway().Player( FRAME_SIMULATOR, false );

    if( !simFrame )     // Defensive coding; shouldn't happen.
        return 0;

    if( wxWindow* blocking_win = simFrame->Kiway().GetBlockingDialog() )
        blocking_win->Close( true );

    // Deactivate other tools; particularly important if another PICKER is currently running
    Activate();

    picker->SetCursor( KICURSOR::VOLTAGE_PROBE );
    picker->SetSnapping( false );

    picker->SetClickHandler(
            [this, simFrame]( const VECTOR2D& aPosition )
            {
                EE_SELECTION_TOOL* selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                EDA_ITEM*          item = selTool->GetNode( aPosition );

                if( !item )
                    return false;

                if( item->Type() == SCH_PIN_T )
                {
                    try
                    {
                        LIB_PIN* pin = static_cast<SCH_PIN*>( item )->GetLibPin();
                        SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item->GetParent() );
                        std::vector<LIB_PIN*> pins = symbol->GetLibPins();

                        SIM_LIB_MGR mgr( m_frame->Prj() );
                        SIM_MODEL&  model = mgr.CreateModel( *symbol ).model;

                        SPICE_ITEM spiceItem;
                        spiceItem.refName = std::string( symbol->GetRef( &m_frame->GetCurrentSheet() ).ToUTF8() );
                        std::vector<std::string> currentNames =
                                model.SpiceGenerator().CurrentNames( spiceItem );

                        if( currentNames.size() == 0 )
                            return true;
                        else if( currentNames.size() == 1 )
                        {
                            simFrame->AddCurrentPlot( currentNames.at( 0 ) );
                            return true;
                        }

                        int modelPinIndex =
                                model.FindModelPinIndex( std::string( pin->GetNumber().ToUTF8() ) );

                        if( modelPinIndex != SIM_MODEL::PIN::NOT_CONNECTED )
                        {
                            wxString name = currentNames.at( modelPinIndex );
                            simFrame->AddCurrentPlot( name );
                        }
                    }
                    catch( const IO_ERROR& e )
                    {
                        DisplayErrorMessage( m_frame, e.What() );
                    }
                }
                else if( item->IsType( { SCH_ITEM_LOCATE_WIRE_T } ) )
                {
                    if( SCH_CONNECTION* conn = static_cast<SCH_ITEM*>( item )->Connection() )
                    {
                        std::string spiceNet = std::string( UnescapeString( conn->Name() ).ToUTF8() );
                        NETLIST_EXPORTER_SPICE::ReplaceForbiddenChars( spiceNet );

                        simFrame->AddVoltagePlot( wxString::Format( "V(%s)", spiceNet ) );
                    }
                }

                return true;
            } );

    picker->SetMotionHandler(
            [this, picker]( const VECTOR2D& aPos )
            {
                EE_COLLECTOR collector;
                collector.m_Threshold = KiROUND( getView()->ToWorld( HITTEST_THRESHOLD_PIXELS ) );
                collector.Collect( m_frame->GetScreen(), { SCH_ITEM_LOCATE_WIRE_T,
                                                           SCH_PIN_T,
                                                           SCH_SHEET_PIN_T }, aPos );

                EE_SELECTION_TOOL* selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                selectionTool->GuessSelectionCandidates( collector, aPos );

                EDA_ITEM* item = collector.GetCount() == 1 ? collector[ 0 ] : nullptr;
                SCH_LINE* wire = dynamic_cast<SCH_LINE*>( item );

                const SCH_CONNECTION* conn = nullptr;

                if( wire )
                {
                    item = nullptr;
                    conn = wire->Connection();
                }

                if( item && item->Type() == SCH_PIN_T )
                    picker->SetCursor( KICURSOR::CURRENT_PROBE );
                else
                    picker->SetCursor( KICURSOR::VOLTAGE_PROBE );

                if( m_pickerItem != item )
                {
                    if( m_pickerItem )
                        selectionTool->UnbrightenItem( m_pickerItem );

                    m_pickerItem = item;

                    if( m_pickerItem )
                        selectionTool->BrightenItem( m_pickerItem );
                }

                if( m_frame->GetHighlightedConnection() != conn )
                {
                    m_frame->SetHighlightedConnection( conn );

                    TOOL_EVENT dummyEvent;
                    UpdateNetHighlighting( dummyEvent );
                }
            } );

    picker->SetFinalizeHandler(
            [this]( const int& aFinalState )
            {
                if( m_pickerItem )
                    m_toolMgr->GetTool<EE_SELECTION_TOOL>()->UnbrightenItem( m_pickerItem );

                if( m_frame->GetHighlightedConnection() )
                {
                    m_frame->SetHighlightedConnection( nullptr );

                    TOOL_EVENT dummyEvent;
                    UpdateNetHighlighting( dummyEvent );
                }

                // Wake the selection tool after exiting to ensure the cursor gets updated
                m_toolMgr->RunAction( EE_ACTIONS::selectionActivate, false );
            } );

    m_toolMgr->RunAction( ACTIONS::pickerTool, true );

    return 0;
}


int SCH_EDITOR_CONTROL::SimTune( const TOOL_EVENT& aEvent )
{
    PICKER_TOOL* picker = m_toolMgr->GetTool<PICKER_TOOL>();

    // Deactivate other tools; particularly important if another PICKER is currently running
    Activate();

    picker->SetCursor( KICURSOR::TUNE );
    picker->SetSnapping( false );

    picker->SetClickHandler(
            [this]( const VECTOR2D& aPosition )
            {
                EE_SELECTION_TOOL* selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                EDA_ITEM*          item = nullptr;
                selTool->SelectPoint( aPosition, { SCH_SYMBOL_T, SCH_FIELD_T }, &item );

                if( !item )
                    return false;

                if( item->Type() != SCH_SYMBOL_T )
                {
                    item = item->GetParent();

                    if( item->Type() != SCH_SYMBOL_T )
                        return false;
                }

                SCH_SYMBOL*   symbol = static_cast<SCH_SYMBOL*>( item );
                KIWAY_PLAYER* simFrame = m_frame->Kiway().Player( FRAME_SIMULATOR, false );

                if( simFrame )
                {
                    if( wxWindow* blocking_win = simFrame->Kiway().GetBlockingDialog() )
                        blocking_win->Close( true );

                    static_cast<SIM_PLOT_FRAME*>( simFrame )->AddTuner( symbol );
                }

                // We do not really want to keep a symbol selected in schematic,
                // so clear the current selection
                selTool->ClearSelection();
                return true;
            } );

    picker->SetMotionHandler(
            [this]( const VECTOR2D& aPos )
            {
                EE_COLLECTOR collector;
                collector.m_Threshold = KiROUND( getView()->ToWorld( HITTEST_THRESHOLD_PIXELS ) );
                collector.Collect( m_frame->GetScreen(), { SCH_SYMBOL_T, SCH_FIELD_T }, aPos );

                EE_SELECTION_TOOL* selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                selectionTool->GuessSelectionCandidates( collector, aPos );

                EDA_ITEM* item = collector.GetCount() == 1 ? collector[ 0 ] : nullptr;

                if( m_pickerItem != item )
                {
                    if( m_pickerItem )
                        selectionTool->UnbrightenItem( m_pickerItem );

                    m_pickerItem = item;

                    if( m_pickerItem )
                        selectionTool->BrightenItem( m_pickerItem );
                }
            } );

    picker->SetFinalizeHandler(
            [this]( const int& aFinalState )
            {
                if( m_pickerItem )
                    m_toolMgr->GetTool<EE_SELECTION_TOOL>()->UnbrightenItem( m_pickerItem );

                // Wake the selection tool after exiting to ensure the cursor gets updated
                // and deselect previous selection from simulator to avoid any issue
                // ( avoid crash in some cases when the SimTune tool is deselected )
                EE_SELECTION_TOOL* selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
                selectionTool->ClearSelection();
                m_toolMgr->RunAction( EE_ACTIONS::selectionActivate, false );
            } );

    m_toolMgr->RunAction( ACTIONS::pickerTool, true );

    return 0;
}
#endif /* KICAD_SPICE */


// A singleton reference for clearing the highlight
static VECTOR2D CLEAR;


static bool highlightNet( TOOL_MANAGER* aToolMgr, const VECTOR2D& aPosition )
{
    SCH_EDIT_FRAME*     editFrame     = static_cast<SCH_EDIT_FRAME*>( aToolMgr->GetToolHolder() );
    EE_SELECTION_TOOL*  selTool       = aToolMgr->GetTool<EE_SELECTION_TOOL>();
    SCH_EDITOR_CONTROL* editorControl = aToolMgr->GetTool<SCH_EDITOR_CONTROL>();
    SCH_CONNECTION*     conn          = nullptr;
    bool                retVal        = true;

    if( aPosition != CLEAR )
    {
        ERC_TESTER erc( &editFrame->Schematic() );

        if( erc.TestDuplicateSheetNames( false ) > 0 )
        {
            wxMessageBox( _( "Error: duplicate sub-sheet names found in current sheet." ) );
            retVal = false;
        }
        else
        {
            SCH_ITEM*   item   = static_cast<SCH_ITEM*>( selTool->GetNode( aPosition ) );
            SCH_SYMBOL* symbol = dynamic_cast<SCH_SYMBOL*>( item );

            if( item )
            {
                if( item->IsConnectivityDirty() )
                {
                    editFrame->RecalculateConnections( NO_CLEANUP );
                }

                if( item->Type() == SCH_FIELD_T )
                    symbol = dynamic_cast<SCH_SYMBOL*>( item->GetParent() );

                if( symbol && symbol->GetLibSymbolRef() && symbol->GetLibSymbolRef()->IsPower() )
                {
                    std::vector<SCH_PIN*> pins = symbol->GetPins();

                    if( pins.size() == 1 )
                        conn = pins[0]->Connection();
                }
                else
                {
                    conn = item->Connection();
                }
            }
        }
    }

    if( !conn || conn == editFrame->GetHighlightedConnection() )
    {
        editFrame->SetStatusText( wxT( "" ) );
        editFrame->SendCrossProbeClearHighlight();
        editFrame->SetHighlightedConnection( nullptr );
    }
    else
    {
        editFrame->SetCrossProbeConnection( conn );
        editFrame->SetHighlightedConnection( conn );
    }

    editFrame->UpdateNetHighlightStatus();

    TOOL_EVENT dummy;
    editorControl->UpdateNetHighlighting( dummy );

    return retVal;
}


int SCH_EDITOR_CONTROL::HighlightNet( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    VECTOR2D              cursorPos = controls->GetCursorPosition( !aEvent.DisableGridSnapping() );

    highlightNet( m_toolMgr, cursorPos );

    return 0;
}


int SCH_EDITOR_CONTROL::ClearHighlight( const TOOL_EVENT& aEvent )
{
    highlightNet( m_toolMgr, CLEAR );

    return 0;
}


int SCH_EDITOR_CONTROL::AssignNetclass( const TOOL_EVENT& aEvent )
{
    EE_SELECTION_TOOL*    selectionTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    VECTOR2D              cursorPos = controls->GetCursorPosition( !aEvent.DisableGridSnapping() );
    SCHEMATIC&            schematic = m_frame->Schematic();
    SCH_SCREEN*           screen = m_frame->GetCurrentSheet().LastScreen();

    // TODO remove once real-time connectivity is a given
    if( !ADVANCED_CFG::GetCfg().m_RealTimeConnectivity || !CONNECTION_GRAPH::m_allowRealTime )
    {
        // Ensure the netlist data is up to date:
        m_frame->RecalculateConnections( NO_CLEANUP );
    }

    // Remove selection in favor of highlighting so the whole net is highlighted
    selectionTool->ClearSelection();
    highlightNet( m_toolMgr, cursorPos );

    const SCH_CONNECTION* conn = m_frame->GetHighlightedConnection();

    if( conn )
    {
        wxString netName = conn->Name();

        if( conn->IsBus() )
        {
            wxString prefix;

            if( NET_SETTINGS::ParseBusVector( netName, &prefix, nullptr ) )
            {
                netName = prefix + wxT( "*" );
            }
            else if( NET_SETTINGS::ParseBusGroup( netName, &prefix, nullptr ) )
            {
                netName = prefix + wxT( ".*" );
            }
        }
        else if( !conn->Driver() || CONNECTION_SUBGRAPH::GetDriverPriority( conn->Driver() )
                                                    < CONNECTION_SUBGRAPH::PRIORITY::SHEET_PIN )
        {
            m_frame->ShowInfoBarError( _( "Net must be labeled to assign a netclass." ) );
            highlightNet( m_toolMgr, CLEAR );
            return 0;
        }

        DIALOG_ASSIGN_NETCLASS dlg( m_frame, netName, schematic.GetNetClassAssignmentCandidates(),
                [&]( const std::vector<wxString>& aNetNames )
                {
                    for( SCH_ITEM* item : screen->Items() )
                    {
                        bool            redraw   = item->IsBrightened();
                        SCH_CONNECTION* itemConn = item->Connection();

                        if( itemConn && alg::contains( aNetNames, itemConn->Name() ) )
                            item->SetBrightened();
                        else
                            item->ClearBrightened();

                        redraw |= item->IsBrightened();

                        if( item->Type() == SCH_SYMBOL_T )
                        {
                            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

                            redraw |= symbol->HasBrightenedPins();

                            symbol->ClearBrightenedPins();

                            for( SCH_PIN* pin : symbol->GetPins() )
                            {
                                SCH_CONNECTION* pin_conn = pin->Connection();

                                if( pin_conn && alg::contains( aNetNames, pin_conn->Name() ) )
                                {
                                    pin->SetBrightened();
                                    redraw = true;
                                }
                            }
                        }
                        else if( item->Type() == SCH_SHEET_T )
                        {
                            for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( item )->GetPins() )
                            {
                                SCH_CONNECTION* pin_conn = pin->Connection();

                                redraw |= pin->IsBrightened();

                                if( pin_conn && alg::contains( aNetNames, pin_conn->Name() ) )
                                    pin->SetBrightened();
                                else
                                    pin->ClearBrightened();

                                redraw |= pin->IsBrightened();
                            }
                        }

                        if( redraw )
                            getView()->Update( item, KIGFX::VIEW_UPDATE_FLAGS::REPAINT );
                    }

                    m_frame->GetCanvas()->ForceRefresh();
                } );

        if( dlg.ShowModal() )
        {
            getView()->UpdateAllItemsConditionally( KIGFX::REPAINT,
                    []( KIGFX::VIEW_ITEM* aItem ) -> bool
                    {
                        return dynamic_cast<SCH_LINE*>( aItem );
                    } );
        }
    }

    highlightNet( m_toolMgr, CLEAR );
    return 0;
}


int SCH_EDITOR_CONTROL::UpdateNetHighlighting( const TOOL_EVENT& aEvent )
{
    SCH_SCREEN*            screen = m_frame->GetCurrentSheet().LastScreen();
    CONNECTION_GRAPH*      connectionGraph = m_frame->Schematic().ConnectionGraph();
    std::vector<EDA_ITEM*> itemsToRedraw;
    const SCH_CONNECTION*  selectedConn = m_frame->GetHighlightedConnection();

    if( !screen )
        return 0;

    bool     selectedIsBus = selectedConn ? selectedConn->IsBus() : false;
    wxString selectedName  = selectedConn ? selectedConn->Name() : "";

    bool                 selectedIsNoNet  = false;
    CONNECTION_SUBGRAPH* selectedSubgraph = nullptr;

    if( selectedConn && selectedConn->Driver() == nullptr )
    {
        selectedIsNoNet  = true;
        selectedSubgraph = connectionGraph->GetSubgraphForItem( selectedConn->Parent() );
    }

    for( SCH_ITEM* item : screen->Items() )
    {
        bool redraw    = item->IsBrightened();
        bool highlight = false;

        if( selectedConn )
        {
            SCH_CONNECTION* itemConn  = nullptr;
            SCH_SYMBOL*     symbol    = nullptr;

            if( item->Type() == SCH_SYMBOL_T )
                symbol = static_cast<SCH_SYMBOL*>( item );

            if( symbol && symbol->GetLibSymbolRef() && symbol->GetLibSymbolRef()->IsPower() )
                itemConn = symbol->Connection();
            else
                itemConn = item->Connection();

            if( selectedIsNoNet && selectedSubgraph )
            {
                for( SCH_ITEM* subgraphItem : selectedSubgraph->m_items )
                {
                    if( item == subgraphItem )
                    {
                        highlight = true;
                        break;
                    }
                }
            }
            else if( selectedIsBus && itemConn && itemConn->IsNet() )
            {
                for( const std::shared_ptr<SCH_CONNECTION>& member : selectedConn->Members() )
                {
                    if( member->Name() == itemConn->Name() )
                    {
                        highlight = true;
                        break;
                    }
                    else if( member->IsBus() )
                    {
                        for( const std::shared_ptr<SCH_CONNECTION>& bus_member : member->Members() )
                        {
                            if( bus_member->Name() == itemConn->Name() )
                            {
                                highlight = true;
                                break;
                            }
                        }
                    }
                }
            }
            else if( selectedConn && itemConn && selectedName == itemConn->Name() )
            {
                highlight = true;
            }
        }

        if( highlight )
            item->SetBrightened();
        else
            item->ClearBrightened();

        redraw |= item->IsBrightened();

        if( item->Type() == SCH_SYMBOL_T )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            redraw |= symbol->HasBrightenedPins();

            symbol->ClearBrightenedPins();

            for( SCH_PIN* pin : symbol->GetPins() )
            {
                SCH_CONNECTION* pin_conn = pin->Connection();

                if( pin_conn && pin_conn->Name() == selectedName )
                {
                    pin->SetBrightened();
                    redraw = true;
                }
            }

            if( symbol->GetLibSymbolRef() && symbol->GetLibSymbolRef()->IsPower() )
            {
                std::vector<SCH_FIELD>& fields = symbol->GetFields();

                for( int id : { REFERENCE_FIELD, VALUE_FIELD } )
                {
                    if( item->IsBrightened() && fields[id].IsVisible() )
                        fields[id].SetBrightened();
                    else
                        fields[id].ClearBrightened();
                }
            }
        }
        else if( item->Type() == SCH_SHEET_T )
        {
            for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( item )->GetPins() )
            {
                SCH_CONNECTION* pin_conn = pin->Connection();
                bool            redrawPin = pin->IsBrightened();

                if( pin_conn && pin_conn->Name() == selectedName )
                    pin->SetBrightened();
                else
                    pin->ClearBrightened();

                redrawPin ^= pin->IsBrightened();
                redraw    |= redrawPin;
            }
        }

        if( redraw )
            itemsToRedraw.push_back( item );
    }

    // Be sure highlight change will be redrawn
    KIGFX::VIEW* view = getView();

    for( EDA_ITEM* redrawItem : itemsToRedraw )
        view->Update( (KIGFX::VIEW_ITEM*)redrawItem, KIGFX::VIEW_UPDATE_FLAGS::REPAINT );

    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::HighlightNetCursor( const TOOL_EVENT& aEvent )
{
    PICKER_TOOL* picker = m_toolMgr->GetTool<PICKER_TOOL>();

    // Deactivate other tools; particularly important if another PICKER is currently running
    Activate();

    picker->SetCursor( KICURSOR::BULLSEYE );
    picker->SetSnapping( false );

    picker->SetClickHandler(
        [this] ( const VECTOR2D& aPos )
        {
            return highlightNet( m_toolMgr, aPos );
        } );

    m_toolMgr->RunAction( ACTIONS::pickerTool, true );

    return 0;
}


int SCH_EDITOR_CONTROL::Undo( const TOOL_EVENT& aEvent )
{
    if( m_frame->GetUndoCommandCount() <= 0 )
        return 0;

    // Inform tools that undo command was issued
    m_toolMgr->ProcessEvent( { TC_MESSAGE, TA_UNDO_REDO_PRE, AS_GLOBAL } );

    // Get the old list
    PICKED_ITEMS_LIST* List = m_frame->PopCommandFromUndoList();
    size_t num_undos = m_frame->m_undoList.m_CommandsList.size();

    // The cleanup routines normally run after an operation and so attempt to append their
    // undo items onto the operation's list.  However, in this case that's going be the list
    // under us, which we don't want, so we push a dummy list onto the stack.
    PICKED_ITEMS_LIST* dummy = new PICKED_ITEMS_LIST();
    m_frame->PushCommandToUndoList( dummy );

    m_frame->PutDataInPreviousState( List );

    m_frame->SetSheetNumberAndCount();
    m_frame->TestDanglingEnds();
    m_frame->OnPageSettingsChange();

    // The cleanup routines *should* have appended to our dummy list, but just to be doubly
    // sure pop any other new lists off the stack as well
    while( m_frame->m_undoList.m_CommandsList.size() > num_undos )
        delete m_frame->PopCommandFromUndoList();

    // Now push the old command to the RedoList
    List->ReversePickersListOrder();
    m_frame->PushCommandToRedoList( List );

    m_toolMgr->GetTool<EE_SELECTION_TOOL>()->RebuildSelection();

    m_frame->SyncView();
    m_frame->GetCanvas()->Refresh();
    m_frame->OnModify();

    return 0;
}


int SCH_EDITOR_CONTROL::Redo( const TOOL_EVENT& aEvent )
{
    if( m_frame->GetRedoCommandCount() == 0 )
        return 0;

    // Inform tools that undo command was issued
    m_toolMgr->ProcessEvent( { TC_MESSAGE, TA_UNDO_REDO_PRE, AS_GLOBAL } );

    /* Get the old list */
    PICKED_ITEMS_LIST* list = m_frame->PopCommandFromRedoList();

    /* Redo the command: */
    m_frame->PutDataInPreviousState( list );

    /* Put the old list in UndoList */
    list->ReversePickersListOrder();
    m_frame->PushCommandToUndoList( list );

    m_frame->SetSheetNumberAndCount();
    m_frame->TestDanglingEnds();
    m_frame->OnPageSettingsChange();

    m_toolMgr->GetTool<EE_SELECTION_TOOL>()->RebuildSelection();

    m_frame->SyncView();
    m_frame->GetCanvas()->Refresh();
    m_frame->OnModify();

    return 0;
}


bool SCH_EDITOR_CONTROL::doCopy( bool aUseLocalClipboard )
{
    EE_SELECTION_TOOL* selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
    EE_SELECTION&      selection = selTool->RequestSelection();
    SCHEMATIC&         schematic = m_frame->Schematic();

    if( !selection.GetSize() )
        return false;

    selection.SetScreen( m_frame->GetScreen() );
    m_supplementaryClipboard.clear();

    for( EDA_ITEM* item : selection )
    {
        if( item->Type() == SCH_SHEET_T )
        {
            SCH_SHEET* sheet = (SCH_SHEET*) item;
            m_supplementaryClipboard[ sheet->GetFileName() ] = sheet->GetScreen();
        }
    }

    STRING_FORMATTER formatter;
    SCH_SEXPR_PLUGIN plugin;
    SCH_SHEET_LIST   hierarchy = schematic.GetSheets();
    SCH_SHEET_PATH   selPath = m_frame->GetCurrentSheet();

    plugin.Format( &selection, &selPath, schematic, &formatter, true );

    if( aUseLocalClipboard )
    {
        m_localClipboard = formatter.GetString();
        return true;
    }

    return m_toolMgr->SaveClipboard( formatter.GetString() );
}


bool SCH_EDITOR_CONTROL::searchSupplementaryClipboard( const wxString& aSheetFilename,
                                                       SCH_SCREEN** aScreen )
{
    if( m_supplementaryClipboard.count( aSheetFilename ) > 0 )
    {
        *aScreen = m_supplementaryClipboard[ aSheetFilename ];
        return true;
    }

    return false;
}


int SCH_EDITOR_CONTROL::Duplicate( const TOOL_EVENT& aEvent )
{
    doCopy( true ); // Use the local clipboard
    Paste( aEvent );

    return 0;
}


int SCH_EDITOR_CONTROL::Cut( const TOOL_EVENT& aEvent )
{
    wxTextEntry* textEntry = dynamic_cast<wxTextEntry*>( wxWindow::FindFocus() );

    if( textEntry )
    {
        textEntry->Cut();
        return 0;
    }

    if( doCopy() )
        m_toolMgr->RunAction( ACTIONS::doDelete, true );

    return 0;
}


int SCH_EDITOR_CONTROL::Copy( const TOOL_EVENT& aEvent )
{
    wxTextEntry* textEntry = dynamic_cast<wxTextEntry*>( wxWindow::FindFocus() );

    if( textEntry )
    {
        textEntry->Copy();
        return 0;
    }

    doCopy();

    return 0;
}


void SCH_EDITOR_CONTROL::updatePastedSymbol( SCH_SYMBOL* aSymbol, SCH_SCREEN* aPasteScreen,
                                             const SCH_SHEET_PATH& aPastePath,
                                             const KIID_PATH& aClipPath,
                                             bool aForceKeepAnnotations )
{
    KIID_PATH clipItemPath = aClipPath;
    clipItemPath.push_back( aSymbol->m_Uuid );

    wxString reference, value, footprint;
    int      unit;

    if( m_clipboardSymbolInstances.count( clipItemPath ) > 0 )
    {
        SYMBOL_INSTANCE_REFERENCE instance = m_clipboardSymbolInstances.at( clipItemPath );

        unit = instance.m_Unit;
        reference = instance.m_Reference;
        value = instance.m_Value;
        footprint = instance.m_Footprint;
    }
    else
    {
        // Some legacy versions saved value fields escaped.  While we still do in the symbol
        // editor, we don't anymore in the schematic, so be sure to unescape them.
        SCH_FIELD* valueField = aSymbol->GetField( VALUE_FIELD );
        valueField->SetText( UnescapeString( valueField->GetText() ) );

        // Pasted from notepad or an older instance of eeschema.  Use the values in the fields
        // instead.
        reference = aSymbol->GetField( REFERENCE_FIELD )->GetText();
        value = aSymbol->GetField( VALUE_FIELD )->GetText();
        footprint = aSymbol->GetField( FOOTPRINT_FIELD )->GetText();
        unit = aSymbol->GetUnit();
    }

    if( aForceKeepAnnotations && !reference.IsEmpty() )
        aSymbol->SetRef( &aPastePath, reference );
    else
        aSymbol->ClearAnnotation( &aPastePath, false );

    // We might clear annotations but always leave the original unit number, value and footprint
    // from the paste
    aSymbol->SetUnitSelection( &aPastePath, unit );
    aSymbol->SetUnit( unit );
    aSymbol->SetValue( &aPastePath, value );
    aSymbol->SetFootprint( &aPastePath, footprint );
}


SCH_SHEET_PATH SCH_EDITOR_CONTROL::updatePastedSheet( const SCH_SHEET_PATH& aPastePath,
                                                      const KIID_PATH& aClipPath, SCH_SHEET* aSheet,
                                                      bool aForceKeepAnnotations,
                                                      SCH_SHEET_LIST* aPastedSheetsSoFar,
                                                      SCH_REFERENCE_LIST* aPastedSymbolsSoFar )
{
    SCH_SHEET_PATH sheetPath = aPastePath;
    sheetPath.push_back( aSheet );

    aSheet->AddInstance( sheetPath );

    wxString pageNum;

    if( m_clipboardSheetInstances.count( aClipPath ) > 0 )
        pageNum = m_clipboardSheetInstances.at( aClipPath ).m_PageNumber;
    else
        pageNum = wxString::Format( "%d", static_cast<int>( aPastedSheetsSoFar->size() ) );

    aSheet->SetPageNumber( sheetPath, pageNum );
    aPastedSheetsSoFar->push_back( sheetPath );

    if( aSheet->GetScreen() == nullptr )
        return sheetPath; // We can only really set the page number but not load any items

    for( SCH_ITEM* item : aSheet->GetScreen()->Items() )
    {
        if( item->Type() == SCH_SYMBOL_T )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            updatePastedSymbol( symbol, aSheet->GetScreen(), sheetPath, aClipPath,
                                aForceKeepAnnotations );
        }
        else if( item->Type() == SCH_SHEET_T )
        {
            SCH_SHEET* subsheet = static_cast<SCH_SHEET*>( item );

            KIID_PATH newClipPath = aClipPath;
            newClipPath.push_back( subsheet->m_Uuid );

            updatePastedSheet( sheetPath, newClipPath, subsheet, aForceKeepAnnotations,
                               aPastedSheetsSoFar, aPastedSymbolsSoFar );

            SCH_SHEET_PATH subSheetPath = sheetPath;
            subSheetPath.push_back( subsheet );

            subSheetPath.GetSymbols( *aPastedSymbolsSoFar );
        }
    }

    return sheetPath;
}


void SCH_EDITOR_CONTROL::setClipboardInstances( const SCH_SCREEN* aPastedScreen )
{
    m_clipboardSheetInstances.clear();

    for( const SCH_SHEET_INSTANCE& sheet : aPastedScreen->GetSheetInstances() )
        m_clipboardSheetInstances[sheet.m_Path] = sheet;

    m_clipboardSymbolInstances.clear();

    for( const SYMBOL_INSTANCE_REFERENCE& symbol : aPastedScreen->GetSymbolInstances() )
        m_clipboardSymbolInstances[symbol.m_Path] = symbol;
}


int SCH_EDITOR_CONTROL::Paste( const TOOL_EVENT& aEvent )
{
    wxTextEntry* textEntry = dynamic_cast<wxTextEntry*>( wxWindow::FindFocus() );

    if( textEntry )
    {
        textEntry->Paste();
        return 0;
    }

    EE_SELECTION_TOOL* selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
    std::string        content;
    VECTOR2I           eventPos;

    if( aEvent.IsAction( &ACTIONS::duplicate ) )
        content = m_localClipboard;
    else
        content = m_toolMgr->GetClipboardUTF8();

    if( content.empty() )
        return 0;

    if( aEvent.IsAction( &ACTIONS::duplicate ) )
        eventPos = getViewControls()->GetCursorPosition( false );

    STRING_LINE_READER reader( content, "Clipboard" );
    SCH_SEXPR_PLUGIN   plugin;

    SCH_SHEET          tempSheet;
    SCH_SCREEN*        tempScreen = new SCH_SCREEN( &m_frame->Schematic() );

    EESCHEMA_SETTINGS::PANEL_ANNOTATE& annotate = m_frame->eeconfig()->m_AnnotatePanel;
    int annotateStartNum = m_frame->Schematic().Settings().m_AnnotateStartNum;

    // Screen object on heap is owned by the sheet.
    tempSheet.SetScreen( tempScreen );

    try
    {
        plugin.LoadContent( reader, &tempSheet );
    }
    catch( IO_ERROR& )
    {
        // If it wasn't content, then paste as content
        SCH_TEXT* text_item = new SCH_TEXT( wxPoint( 0, 0 ), content );
        text_item->SetTextSpinStyle( TEXT_SPIN_STYLE::RIGHT ); // Left alignment
        tempScreen->Append( text_item );
    }

    // Save loaded screen instances to m_clipboardSheetInstances
    setClipboardInstances( tempScreen );

    PASTE_MODE pasteMode = annotate.automatic ? PASTE_MODE::RESPECT_OPTIONS
                                              : PASTE_MODE::REMOVE_ANNOTATIONS;

    if( aEvent.IsAction( &ACTIONS::pasteSpecial ) )
    {
        DIALOG_PASTE_SPECIAL dlg( m_frame, &pasteMode );

        if( dlg.ShowModal() == wxID_CANCEL )
            return 0;
    }

    bool forceKeepAnnotations = pasteMode != PASTE_MODE::REMOVE_ANNOTATIONS;

    // SCH_SEXP_PLUGIN added the items to the paste screen, but not to the view or anything
    // else.  Pull them back out to start with.
    //
    EDA_ITEMS       loadedItems;
    bool            sheetsPasted = false;
    SCH_SHEET_LIST  hierarchy = m_frame->Schematic().GetSheets();
    SCH_SHEET_PATH& pasteRoot = m_frame->GetCurrentSheet();
    wxFileName      destFn = pasteRoot.Last()->GetFileName();

    if( destFn.IsRelative() )
        destFn.MakeAbsolute( m_frame->Prj().GetProjectPath() );

    // List of paths in the hierarchy that refer to the destination sheet of the paste
    SCH_SHEET_LIST pasteInstances = hierarchy.FindAllSheetsForScreen( pasteRoot.LastScreen() );
    pasteInstances.SortByPageNumbers();

    // Build a list of screens from the current design (to avoid loading sheets that already exist)
    std::map<wxString, SCH_SCREEN*> loadedScreens;

    for( const SCH_SHEET_PATH& item : hierarchy )
    {
        if( item.LastScreen() )
            loadedScreens[item.Last()->GetFileName()] = item.LastScreen();
    }

    // Build symbol list for reannotation of duplicates
    SCH_REFERENCE_LIST existingRefs;
    hierarchy.GetSymbols( existingRefs );
    existingRefs.SortByReferenceOnly();

    // Build UUID map for fetching last-resolved-properties
    std::map<KIID, EDA_ITEM*> itemMap;
    hierarchy.FillItemMap( itemMap );

    // Keep track of pasted sheets and symbols for the different
    // paths to the hierarchy
    std::map<SCH_SHEET_PATH, SCH_REFERENCE_LIST> pastedSymbols;
    std::map<SCH_SHEET_PATH, SCH_SHEET_LIST>     pastedSheets;

    for( SCH_ITEM* item : tempScreen->Items() )
    {
        loadedItems.push_back( item );

        //@todo: we might want to sort the sheets by page number before adding to loadedItems
        if( item->Type() == SCH_SHEET_T )
        {
            SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );
            wxFileName srcFn = sheet->GetFileName();

            if( srcFn.IsRelative() )
                srcFn.MakeAbsolute( m_frame->Prj().GetProjectPath() );

            SCH_SHEET_LIST sheetHierarchy( sheet );

            if( hierarchy.TestForRecursion( sheetHierarchy, destFn.GetFullPath( wxPATH_UNIX ) ) )
            {
                auto msg = wxString::Format( _( "The pasted sheet '%s'\n"
                                                "was dropped because the destination already has "
                                                "the sheet or one of its subsheets as a parent." ),
                                             sheet->GetFileName() );
                DisplayError( m_frame, msg );
                loadedItems.pop_back();
            }
        }
    }

    // Remove the references from our temporary screen to prevent freeing on the DTOR
    tempScreen->Clear( false );

    for( unsigned i = 0; i < loadedItems.size(); ++i )
    {
        EDA_ITEM* item = loadedItems[i];
        KIID_PATH clipPath( wxT("/") ); // clipboard is at root

        if( item->Type() == SCH_SYMBOL_T )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            // The library symbol gets set from the cached library symbols in the current
            // schematic not the symbol libraries.  The cached library symbol may have
            // changed from the original library symbol which would cause the copy to
            // be incorrect.
            SCH_SCREEN* currentScreen = m_frame->GetScreen();

            wxCHECK2( currentScreen, continue );

            auto it = currentScreen->GetLibSymbols().find( symbol->GetSchSymbolLibraryName() );
            auto end = currentScreen->GetLibSymbols().end();

            if( it == end )
            {
                // If can't find library definition in the design, use the pasted library
                it = tempScreen->GetLibSymbols().find( symbol->GetSchSymbolLibraryName() );
                end = tempScreen->GetLibSymbols().end();
            }

            LIB_SYMBOL* libSymbol = nullptr;

            if( it != end )
            {
                libSymbol = new LIB_SYMBOL( *it->second );
                symbol->SetLibSymbol( libSymbol );
            }

            for( SCH_SHEET_PATH& instance : pasteInstances )
                updatePastedSymbol( symbol, tempScreen, instance, clipPath, forceKeepAnnotations );

            // Assign a new KIID
            const_cast<KIID&>( item->m_Uuid ) = KIID();

            // Make sure pins get a new UUID
            for( SCH_PIN* pin : symbol->GetPins() )
                const_cast<KIID&>( pin->m_Uuid ) = KIID();

            for( SCH_SHEET_PATH& instance : pasteInstances )
            {
                // Ignore pseudo-symbols (e.g. power symbols) and symbols from a non-existant
                // library.
                if( libSymbol && symbol->GetRef( &instance )[0] != wxT( '#' ) )
                {
                    SCH_REFERENCE schReference( symbol, libSymbol, instance );
                    schReference.SetSheetNumber( instance.GetVirtualPageNumber() );
                    pastedSymbols[instance].AddItem( schReference );
                }
            }
        }
        else if( item->Type() == SCH_SHEET_T )
        {
            SCH_SHEET*  sheet          = (SCH_SHEET*) item;
            SCH_FIELD&  nameField      = sheet->GetFields()[SHEETNAME];
            wxString    baseName       = nameField.GetText();
            wxString    candidateName  = baseName;
            wxString    number;

            while( !baseName.IsEmpty() && wxIsdigit( baseName.Last() ) )
            {
                number = baseName.Last() + number;
                baseName.RemoveLast();
            }
            // Update hierarchy to include any other sheets we already added, avoiding
            // duplicate sheet names
            hierarchy = m_frame->Schematic().GetSheets();

            //@todo: it might be better to just iterate through the sheet names
            // in this screen instead of the whole hierarchy.
            int uniquifier = std::max( 0, wxAtoi( number ) ) + 1;

            while( hierarchy.NameExists( candidateName ) )
                candidateName = wxString::Format( wxT( "%s%d" ), baseName, uniquifier++ );

            nameField.SetText( candidateName );

            wxFileName     fn = sheet->GetFileName();
            SCH_SCREEN*    existingScreen = nullptr;

            sheet->SetParent( pasteRoot.Last() );
            sheet->SetScreen( nullptr );

            if( !fn.IsAbsolute() )
            {
                wxFileName currentSheetFileName = pasteRoot.LastScreen()->GetFileName();
                fn.Normalize(  FN_NORMALIZE_FLAGS | wxPATH_NORM_ENV_VARS,
                               currentSheetFileName.GetPath() );
            }

            // Try to find the screen for the pasted sheet by several means
            if( !m_frame->Schematic().Root().SearchHierarchy( fn.GetFullPath( wxPATH_UNIX ),
                                                              &existingScreen ) )
            {
                if( loadedScreens.count( sheet->GetFileName() ) > 0 )
                    existingScreen = loadedScreens.at( sheet->GetFileName() );
                else
                    searchSupplementaryClipboard( sheet->GetFileName(), &existingScreen );
            }

            if( existingScreen )
            {
                sheet->SetScreen( existingScreen );
            }
            else
            {
                if( !m_frame->LoadSheetFromFile( sheet, &pasteRoot, fn.GetFullPath() ) )
                    m_frame->InitSheet( sheet, sheet->GetFileName() );
            }

            sheetsPasted = true;

            // Push it to the clipboard path while it still has its old KIID
            clipPath.push_back( sheet->m_Uuid );

            // Assign a new KIID to the pasted sheet
            const_cast<KIID&>( sheet->m_Uuid ) = KIID();

            // Make sure pins get a new UUID
            for( SCH_SHEET_PIN* pin : sheet->GetPins() )
                const_cast<KIID&>( pin->m_Uuid ) = KIID();

            // Once we have our new KIID we can update all pasted instances. This will either
            // reset the annotations or copy "kept" annotations from the supplementary clipboard.
            for( SCH_SHEET_PATH& instance : pasteInstances )
            {
                SCH_SHEET_PATH sheetPath = updatePastedSheet( instance, clipPath, sheet,
                                                              ( forceKeepAnnotations && annotate.recursive ),
                                                              &pastedSheets[instance],
                                                              &pastedSymbols[instance] );

                sheetPath.GetSymbols( pastedSymbols[instance] );
            }
        }
        else
        {
            SCH_ITEM* srcItem = dynamic_cast<SCH_ITEM*>( itemMap[ item->m_Uuid ] );
            SCH_ITEM* destItem = dynamic_cast<SCH_ITEM*>( item );

            // Everything gets a new KIID
            const_cast<KIID&>( item->m_Uuid ) = KIID();

            if( srcItem && destItem )
            {
                destItem->SetConnectivityDirty( true );
                destItem->SetLastResolvedState( srcItem );
            }
        }

        // Lines need both ends selected for a move after paste so the whole
        // line moves
        if( item->Type() == SCH_LINE_T )
            item->SetFlags( STARTPOINT | ENDPOINT );

        item->SetFlags( IS_NEW | IS_PASTED | IS_MOVING );
        m_frame->AddItemToScreenAndUndoList( m_frame->GetScreen(), (SCH_ITEM*) item, i > 0 );

        // Reset flags for subsequent move operation
        item->SetFlags( IS_NEW | IS_PASTED | IS_MOVING );
        // Start out hidden so the pasted items aren't "ghosted" in their original location
        // before being moved to the current location.
        getView()->Hide( item, true );
    }

    pasteInstances.SortByPageNumbers();

    if( sheetsPasted )
    {
        // Update page numbers: Find next free numeric page number
        for( SCH_SHEET_PATH& instance : pasteInstances )
        {
            pastedSheets[instance].SortByPageNumbers();

            for( SCH_SHEET_PATH& pastedSheet : pastedSheets[instance] )
            {
                int      page = 1;
                wxString pageNum = wxString::Format( "%d", page );

                while( hierarchy.PageNumberExists( pageNum ) )
                    pageNum = wxString::Format( "%d", ++page );

                pastedSheet.SetPageNumber( pageNum );
                hierarchy.push_back( pastedSheet );
            }
        }

        m_frame->SetSheetNumberAndCount();
        m_frame->UpdateHierarchyNavigator();

        // Get a version with correct sheet numbers since we've pasted sheets,
        // we'll need this when annotating next
        hierarchy = m_frame->Schematic().GetSheets();
    }

    if( pasteMode == PASTE_MODE::UNIQUE_ANNOTATIONS || pasteMode == PASTE_MODE::RESPECT_OPTIONS )
    {
        for( SCH_SHEET_PATH& instance : pasteInstances )
        {
            pastedSymbols[instance].SortByReferenceOnly();

            if( pasteMode == PASTE_MODE::UNIQUE_ANNOTATIONS )
                pastedSymbols[instance].ReannotateDuplicates( existingRefs );
            else
                pastedSymbols[instance].ReannotateByOptions( (ANNOTATE_ORDER_T) annotate.sort_order,
                                                             (ANNOTATE_ALGO_T) annotate.method,
                                                             annotateStartNum, existingRefs, true,
                                                             &hierarchy );

            pastedSymbols[instance].UpdateAnnotation();

            // Update existing refs for next iteration
            for( size_t i = 0; i < pastedSymbols[instance].GetCount(); i++ )
                existingRefs.AddItem( pastedSymbols[instance][i] );
        }
    }

    m_frame->GetCurrentSheet().UpdateAllScreenReferences();

    // Now clear the previous selection, select the pasted items, and fire up the "move"
    // tool.
    //
    m_toolMgr->RunAction( EE_ACTIONS::clearSelection, true );
    m_toolMgr->RunAction( EE_ACTIONS::addItemsToSel, true, &loadedItems );

    EE_SELECTION& selection = selTool->GetSelection();

    if( !selection.Empty() )
    {
        if( aEvent.IsAction( &ACTIONS::duplicate ) )
        {
            int closest_dist = INT_MAX;

            auto processPt =
                    [&]( const VECTOR2I& pt )
                    {
                        int dist = ( eventPos - pt ).EuclideanNorm();

                        if( dist < closest_dist )
                        {
                            selection.SetReferencePoint( pt );
                            closest_dist = dist;
                        }
                    };

            // Prefer connection points (which should remain on grid)

            for( EDA_ITEM* item : selection.Items() )
            {
                SCH_ITEM* sch_item = dynamic_cast<SCH_ITEM*>( item );
                LIB_PIN*  lib_pin = dynamic_cast<LIB_PIN*>( item );

                if( sch_item && sch_item->IsConnectable() )
                {
                    for( const VECTOR2I& pt : sch_item->GetConnectionPoints() )
                        processPt( pt );
                }
                else if( lib_pin )
                {
                    processPt( lib_pin->GetPosition() );
                }
            }

            // Only process other points if we didn't find any connection points

            if( closest_dist == INT_MAX )
            {
                for( EDA_ITEM* item : selection.Items() )
                {
                    switch( item->Type() )
                    {
                    case SCH_LINE_T:
                        processPt( static_cast<SCH_LINE*>( item )->GetStartPoint() );
                        processPt( static_cast<SCH_LINE*>( item )->GetEndPoint() );
                        break;

                    case SCH_SHAPE_T:
                    {
                        SCH_SHAPE* shape = static_cast<SCH_SHAPE*>( item );

                        switch( shape->GetShape() )
                        {
                        case SHAPE_T::RECT:
                            for( const VECTOR2I& pt : shape->GetRectCorners() )
                                processPt( pt );

                            break;

                        case SHAPE_T::CIRCLE:
                            processPt( shape->GetCenter() );
                            break;

                        case SHAPE_T::POLY:
                            for( int ii = 0; ii < shape->GetPolyShape().TotalVertices(); ++ii )
                                processPt( shape->GetPolyShape().CVertex( ii ) );

                            break;

                        default:
                            processPt( shape->GetStart() );
                            processPt( shape->GetEnd() );
                            break;
                        }

                        break;
                    }

                    default:
                        processPt( item->GetPosition() );
                        break;
                    }
                }
            }
        }
        else
        {
            SCH_ITEM* item = static_cast<SCH_ITEM*>( selection.GetTopLeftItem() );

            selection.SetReferencePoint( item->GetPosition() );
        }

        m_toolMgr->RunAction( EE_ACTIONS::move, false );
    }

    return 0;
}


int SCH_EDITOR_CONTROL::EditWithSymbolEditor( const TOOL_EVENT& aEvent )
{
    EE_SELECTION_TOOL* selTool = m_toolMgr->GetTool<EE_SELECTION_TOOL>();
    EE_SELECTION&      selection = selTool->RequestSelection( { SCH_SYMBOL_T } );
    SCH_SYMBOL*        symbol = nullptr;
    SYMBOL_EDIT_FRAME* symbolEditor;

    if( selection.GetSize() >= 1 )
        symbol = (SCH_SYMBOL*) selection.Front();

    if( !symbol || symbol->GetEditFlags() != 0 )
        return 0;

    if( symbol->IsMissingLibSymbol() )
    {
        m_frame->ShowInfoBarError( _( "Symbols with broken library symbol links cannot "
                                      "be edited." ) );
        return 0;
    }

    m_toolMgr->RunAction( ACTIONS::showSymbolEditor, true );
    symbolEditor = (SYMBOL_EDIT_FRAME*) m_frame->Kiway().Player( FRAME_SCH_SYMBOL_EDITOR, false );

    if( symbolEditor )
    {
        if( wxWindow* blocking_win = symbolEditor->Kiway().GetBlockingDialog() )
            blocking_win->Close( true );

        if( aEvent.IsAction( &EE_ACTIONS::editWithLibEdit ) )
        {
            symbolEditor->LoadSymbolFromSchematic( symbol );
        }
        else if( aEvent.IsAction( &EE_ACTIONS::editLibSymbolWithLibEdit ) )
        {
            symbolEditor->LoadSymbol( symbol->GetLibId(), symbol->GetUnit(), symbol->GetConvert() );

            if( !symbolEditor->IsSymbolTreeShown() )
            {
                wxCommandEvent evt;
                symbolEditor->OnToggleSymbolTree( evt );
            }
        }
    }

    return 0;
}


int SCH_EDITOR_CONTROL::Annotate( const TOOL_EVENT& aEvent )
{
    wxCommandEvent dummy;
    m_frame->OnAnnotate( dummy );
    return 0;
}


int SCH_EDITOR_CONTROL::ShowCvpcb( const TOOL_EVENT& aEvent )
{
    wxCommandEvent dummy;
    m_frame->OnOpenCvpcb( dummy );
    return 0;
}


int SCH_EDITOR_CONTROL::EditSymbolFields( const TOOL_EVENT& aEvent )
{
    DIALOG_SYMBOL_FIELDS_TABLE dlg( m_frame );
    dlg.ShowQuasiModal();
    return 0;
}


int SCH_EDITOR_CONTROL::EditSymbolLibraryLinks( const TOOL_EVENT& aEvent )
{
    if( InvokeDialogEditSymbolsLibId( m_frame ) )
        m_frame->HardRedraw();

    return 0;
}


int SCH_EDITOR_CONTROL::ShowPcbNew( const TOOL_EVENT& aEvent )
{
    wxCommandEvent dummy;
    m_frame->OnOpenPcbnew( dummy );
    return 0;
}


int SCH_EDITOR_CONTROL::UpdatePCB( const TOOL_EVENT& aEvent )
{
    wxCommandEvent dummy;
    m_frame->OnUpdatePCB( dummy );
    return 0;
}


int SCH_EDITOR_CONTROL::UpdateFromPCB( const TOOL_EVENT& aEvent )
{
    DIALOG_UPDATE_FROM_PCB dlg( m_frame );
    dlg.ShowModal();
    return 0;
}


int SCH_EDITOR_CONTROL::ExportNetlist( const TOOL_EVENT& aEvent )
{
    int result = NET_PLUGIN_CHANGE;

    // If a plugin is removed or added, rebuild and reopen the new dialog
    while( result == NET_PLUGIN_CHANGE )
        result = InvokeDialogNetList( m_frame );

    return 0;
}


int SCH_EDITOR_CONTROL::GenerateBOM( const TOOL_EVENT& aEvent )
{
    InvokeDialogCreateBOM( m_frame );
    return 0;
}


int SCH_EDITOR_CONTROL::DrawSheetOnClipboard( const TOOL_EVENT& aEvent )
{
    if( !ADVANCED_CFG::GetCfg().m_RealTimeConnectivity || !CONNECTION_GRAPH::m_allowRealTime )
        m_frame->RecalculateConnections( LOCAL_CLEANUP );

    m_frame->DrawCurrentSheetToClipboard();
    return 0;
}


int SCH_EDITOR_CONTROL::ShowHierarchy( const TOOL_EVENT& aEvent )
{
    getEditFrame<SCH_EDIT_FRAME>()->ToggleSchematicHierarchy();
    return 0;
}


int SCH_EDITOR_CONTROL::ToggleHiddenPins( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_Appearance.show_hidden_pins = !cfg->m_Appearance.show_hidden_pins;

    getView()->UpdateAllItems( KIGFX::REPAINT );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::ToggleHiddenFields( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_Appearance.show_hidden_fields = !cfg->m_Appearance.show_hidden_fields;

    getView()->UpdateAllItems( KIGFX::REPAINT );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::ToggleERCWarnings( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_Appearance.show_erc_warnings = !cfg->m_Appearance.show_erc_warnings;

    getView()->SetLayerVisible( LAYER_ERC_WARN, cfg->m_Appearance.show_erc_warnings );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::ToggleERCErrors( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_Appearance.show_erc_errors = !cfg->m_Appearance.show_erc_errors;

    getView()->SetLayerVisible( LAYER_ERC_ERR, cfg->m_Appearance.show_erc_errors );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::ToggleERCExclusions( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_Appearance.show_erc_exclusions = !cfg->m_Appearance.show_erc_exclusions;

    getView()->SetLayerVisible( LAYER_ERC_EXCLUSION, cfg->m_Appearance.show_erc_exclusions );
    m_frame->GetCanvas()->Refresh();

    return 0;
}


int SCH_EDITOR_CONTROL::ChangeLineMode( const TOOL_EVENT& aEvent )
{
    m_frame->eeconfig()->m_Drawing.line_mode = aEvent.Parameter<int>();
    m_toolMgr->RunAction( ACTIONS::refreshPreview );
    return 0;
}


int SCH_EDITOR_CONTROL::NextLineMode( const TOOL_EVENT& aEvent )
{
    m_frame->eeconfig()->m_Drawing.line_mode++;
    m_frame->eeconfig()->m_Drawing.line_mode %= LINE_MODE::LINE_MODE_COUNT;
    m_toolMgr->RunAction( ACTIONS::refreshPreview );
    return 0;
}


int SCH_EDITOR_CONTROL::ToggleAnnotateAuto( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_AnnotatePanel.automatic = !cfg->m_AnnotatePanel.automatic;
    return 0;
}


int SCH_EDITOR_CONTROL::ToggleAnnotateRecursive( const TOOL_EVENT& aEvent )
{
    EESCHEMA_SETTINGS* cfg = m_frame->eeconfig();
    cfg->m_AnnotatePanel.recursive = !cfg->m_AnnotatePanel.recursive;
    return 0;
}


int SCH_EDITOR_CONTROL::TogglePythonConsole( const TOOL_EVENT& aEvent )
{

    m_frame->ScriptingConsoleEnableDisable();
    return 0;
}


int SCH_EDITOR_CONTROL::RepairSchematic( const TOOL_EVENT& aEvent )
{
    int      errors = 0;
    wxString details;
    bool     quiet = aEvent.Parameter<bool>();

    // Repair duplicate IDs.
    std::map<KIID, EDA_ITEM*> ids;
    int                       duplicates = 0;

    auto processItem =
            [&]( EDA_ITEM* aItem )
            {
                auto it = ids.find( aItem->m_Uuid );

                if( it != ids.end() && it->second != aItem )
                {
                    duplicates++;
                    const_cast<KIID&>( aItem->m_Uuid ) = KIID();
                }

                ids[ aItem->m_Uuid ] = aItem;
            };

    // Symbol IDs are the most important, so give them the first crack at "claiming" a
    // particular KIID.

    for( const SCH_SHEET_PATH& sheet : m_frame->Schematic().GetSheets() )
    {
        SCH_SCREEN* screen = sheet.LastScreen();

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            processItem( item );

            for( SCH_PIN* pin : static_cast<SCH_SYMBOL*>( item )->GetPins( &sheet ) )
                processItem( pin );
        }
    }

    for( const SCH_SHEET_PATH& sheet : m_frame->Schematic().GetSheets() )
    {
        SCH_SCREEN* screen = sheet.LastScreen();

        for( SCH_ITEM* item : screen->Items() )
        {
            processItem( item );

            item->RunOnChildren(
                    [&]( SCH_ITEM* aChild )
                    {
                        processItem( item );
                    } );
        }
    }

    /*******************************
     * Your test here
     */

    /*******************************
     * Inform the user
     */

    if( duplicates )
    {
        errors += duplicates;
        details += wxString::Format( _( "%d duplicate IDs replaced.\n" ), duplicates );
    }

    if( errors )
    {
        m_frame->OnModify();

        wxString msg = wxString::Format( _( "%d potential problems repaired." ), errors );

        if( !quiet )
            DisplayInfoMessage( m_frame, msg, details );
    }
    else if( !quiet )
    {
        DisplayInfoMessage( m_frame, _( "No errors found." ) );
    }

    return 0;
}


void SCH_EDITOR_CONTROL::setTransitions()
{
    Go( &SCH_EDITOR_CONTROL::New,                   ACTIONS::doNew.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Open,                  ACTIONS::open.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Save,                  ACTIONS::save.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::SaveAs,                ACTIONS::saveAs.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::SaveCurrSheetCopyAs,   EE_ACTIONS::saveCurrSheetCopyAs.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Revert,                ACTIONS::revert.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ShowSchematicSetup,    EE_ACTIONS::schematicSetup.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::PageSetup,             ACTIONS::pageSettings.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Print,                 ACTIONS::print.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Plot,                  ACTIONS::plot.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Quit,                  ACTIONS::quit.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::RescueSymbols,         EE_ACTIONS::rescueSymbols.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::RemapSymbols,          EE_ACTIONS::remapSymbols.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::FindAndReplace,        ACTIONS::find.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::FindAndReplace,        ACTIONS::findAndReplace.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::FindNext,              ACTIONS::findNext.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::FindNext,              ACTIONS::findNextMarker.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ReplaceAndFindNext,    ACTIONS::replaceAndFindNext.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ReplaceAll,            ACTIONS::replaceAll.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::UpdateFind,            ACTIONS::updateFind.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::UpdateFind,            EVENTS::SelectedItemsModified );

    Go( &SCH_EDITOR_CONTROL::CrossProbeToPcb,       EVENTS::PointSelectedEvent );
    Go( &SCH_EDITOR_CONTROL::CrossProbeToPcb,       EVENTS::SelectedEvent );
    Go( &SCH_EDITOR_CONTROL::CrossProbeToPcb,       EVENTS::UnselectedEvent );
    Go( &SCH_EDITOR_CONTROL::CrossProbeToPcb,       EVENTS::ClearedEvent );
    Go( &SCH_EDITOR_CONTROL::ExplicitCrossProbeToPcb, EE_ACTIONS::selectOnPCB.MakeEvent() );

#ifdef KICAD_SPICE
    Go( &SCH_EDITOR_CONTROL::SimProbe,              EE_ACTIONS::simProbe.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::SimTune,               EE_ACTIONS::simTune.MakeEvent() );
#endif /* KICAD_SPICE */

    Go( &SCH_EDITOR_CONTROL::HighlightNet,          EE_ACTIONS::highlightNet.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ClearHighlight,        EE_ACTIONS::clearHighlight.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::HighlightNetCursor,    EE_ACTIONS::highlightNetTool.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::UpdateNetHighlighting, EVENTS::SelectedItemsModified );
    Go( &SCH_EDITOR_CONTROL::UpdateNetHighlighting, EE_ACTIONS::updateNetHighlighting.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::AssignNetclass,        EE_ACTIONS::assignNetclass.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::Undo,                  ACTIONS::undo.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Redo,                  ACTIONS::redo.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Cut,                   ACTIONS::cut.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Copy,                  ACTIONS::copy.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Paste,                 ACTIONS::paste.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Paste,                 ACTIONS::pasteSpecial.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Duplicate,             ACTIONS::duplicate.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::EditWithSymbolEditor,  EE_ACTIONS::editWithLibEdit.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::EditWithSymbolEditor,  EE_ACTIONS::editLibSymbolWithLibEdit.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ShowCvpcb,             EE_ACTIONS::assignFootprints.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ImportFPAssignments,   EE_ACTIONS::importFPAssignments.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::Annotate,              EE_ACTIONS::annotate.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::EditSymbolFields,      EE_ACTIONS::editSymbolFields.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::EditSymbolLibraryLinks,EE_ACTIONS::editSymbolLibraryLinks.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ShowPcbNew,            EE_ACTIONS::showPcbNew.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::UpdatePCB,             ACTIONS::updatePcbFromSchematic.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::UpdateFromPCB,         ACTIONS::updateSchematicFromPcb.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ExportNetlist,         EE_ACTIONS::exportNetlist.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::GenerateBOM,           EE_ACTIONS::generateBOM.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::DrawSheetOnClipboard,  EE_ACTIONS::drawSheetOnClipboard.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::ShowHierarchy,         EE_ACTIONS::showHierarchy.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::ToggleHiddenPins,      EE_ACTIONS::toggleHiddenPins.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ToggleHiddenFields,    EE_ACTIONS::toggleHiddenFields.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ToggleERCWarnings,     EE_ACTIONS::toggleERCWarnings.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ToggleERCErrors,       EE_ACTIONS::toggleERCErrors.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ToggleERCExclusions,   EE_ACTIONS::toggleERCExclusions.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ChangeLineMode,        EE_ACTIONS::lineModeFree.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ChangeLineMode,        EE_ACTIONS::lineMode90.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ChangeLineMode,        EE_ACTIONS::lineMode45.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::NextLineMode,          EE_ACTIONS::lineModeNext.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ToggleAnnotateAuto,    EE_ACTIONS::toggleAnnotateAuto.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::TogglePythonConsole,   EE_ACTIONS::showPythonConsole.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::RepairSchematic,       EE_ACTIONS::repairSchematic.MakeEvent() );

    Go( &SCH_EDITOR_CONTROL::ExportSymbolsToLibrary, EE_ACTIONS::exportSymbolsToLibrary.MakeEvent() );
    Go( &SCH_EDITOR_CONTROL::ExportSymbolsToLibrary, EE_ACTIONS::exportSymbolsToNewLibrary.MakeEvent() );
}
