///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
class TEXT_CTRL_EVAL;
class WX_GRID;

#include "dialog_shim.h"
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/font.h>
#include <wx/grid.h>
#include <wx/gdicmn.h>
#include <wx/bmpbuttn.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

#define ID_NOTEBOOK 1000

///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_FOOTPRINT_PROPERTIES_FP_EDITOR_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_FOOTPRINT_PROPERTIES_FP_EDITOR_BASE : public DIALOG_SHIM
{
	private:
		wxBoxSizer* m_GeneralBoxSizer;

	protected:
		wxNotebook* m_NoteBook;
		wxPanel* m_PanelGeneral;
		WX_GRID* m_itemsGrid;
		wxBitmapButton* m_bpAdd;
		wxBitmapButton* m_bpDelete;
		wxTextCtrl* m_FootprintNameCtrl;
		wxTextCtrl* m_DocCtrl;
		wxStaticText* staticKeywordsLabel;
		wxTextCtrl* m_KeywordCtrl;
		WX_GRID* m_privateLayersGrid;
		wxBitmapButton* m_bpAddLayer;
		wxBitmapButton* m_bpDeleteLayer;
		wxStaticText* m_componentTypeLabel;
		wxChoice* m_componentType;
		wxCheckBox* m_boardOnly;
		wxCheckBox* m_excludeFromPosFiles;
		wxCheckBox* m_excludeFromBOM;
		wxCheckBox* m_noCourtyards;
		wxPanel* m_PanelClearances;
		wxStaticText* m_staticTextInfo;
		wxStaticText* m_NetClearanceLabel;
		wxTextCtrl* m_NetClearanceCtrl;
		wxStaticText* m_NetClearanceUnits;
		wxStaticText* m_SolderMaskMarginLabel;
		wxTextCtrl* m_SolderMaskMarginCtrl;
		wxStaticText* m_SolderMaskMarginUnits;
		wxCheckBox* m_allowBridges;
		wxStaticText* m_SolderPasteMarginLabel;
		wxTextCtrl* m_SolderPasteMarginCtrl;
		wxStaticText* m_SolderPasteMarginUnits;
		wxStaticText* m_PasteMarginRatioLabel;
		TEXT_CTRL_EVAL* m_PasteMarginRatioCtrl;
		wxStaticText* m_PasteMarginRatioUnits;
		wxStaticText* m_staticTextInfoCopper;
		wxStaticText* m_staticTextInfoPaste;
		wxStaticText* m_staticText16;
		wxChoice* m_ZoneConnectionChoice;
		wxStaticText* m_padGroupsLabel;
		WX_GRID* m_padGroupsGrid;
		wxBitmapButton* m_bpAddPadGroup;
		wxBitmapButton* m_bpRemovePadGroup;
		wxStdDialogButtonSizer* m_sdbSizerStdButtons;
		wxButton* m_sdbSizerStdButtonsOK;
		wxButton* m_sdbSizerStdButtonsCancel;

		// Virtual event handlers, override them in your derived class
		virtual void OnInitDlg( wxInitDialogEvent& event ) { event.Skip(); }
		virtual void OnUpdateUI( wxUpdateUIEvent& event ) { event.Skip(); }
		virtual void OnGridSize( wxSizeEvent& event ) { event.Skip(); }
		virtual void OnAddField( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDeleteField( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFootprintNameText( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAddLayer( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDeleteLayer( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAddPadGroup( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRemovePadGroup( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_FOOTPRINT_PROPERTIES_FP_EDITOR_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Footprint Properties"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );

		~DIALOG_FOOTPRINT_PROPERTIES_FP_EDITOR_BASE();

};

