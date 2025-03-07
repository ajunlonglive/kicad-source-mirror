///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "widgets/font_choice.h"

#include "dialog_global_edit_text_and_graphics_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bMainSizer;
	bMainSizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizerTop;
	bSizerTop = new wxBoxSizer( wxHORIZONTAL );

	wxStaticBoxSizer* sbScope;
	sbScope = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Scope") ), wxVERTICAL );

	m_references = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Reference designators"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_references, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_values = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Values"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_values, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_otherFields = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Other symbol fields"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_otherFields, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );


	sbScope->Add( 0, 0, 0, wxEXPAND|wxTOP|wxBOTTOM, 5 );

	m_wires = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Wires && wire labels"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_wires, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_buses = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Buses && bus labels"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_buses, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_globalLabels = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Global labels"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_globalLabels, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_hierLabels = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Hierarchical labels"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_hierLabels, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );


	sbScope->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 5 );

	m_sheetTitles = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Sheet titles"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_sheetTitles, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_sheetFields = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Other sheet fields"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_sheetFields, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_sheetPins = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Sheet pins"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_sheetPins, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );

	m_sheetBorders = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Sheet borders && backgrounds"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_sheetBorders, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );


	sbScope->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 5 );

	m_schTextAndGraphics = new wxCheckBox( sbScope->GetStaticBox(), wxID_ANY, _("Schematic text && graphics"), wxDefaultPosition, wxDefaultSize, 0 );
	sbScope->Add( m_schTextAndGraphics, 0, wxBOTTOM|wxRIGHT|wxLEFT, 4 );


	bSizerTop->Add( sbScope, 0, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );

	wxStaticBoxSizer* sbFilters;
	sbFilters = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Filters") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer2;
	fgSizer2 = new wxFlexGridSizer( 0, 2, 4, 0 );
	fgSizer2->AddGrowableCol( 1 );
	fgSizer2->SetFlexibleDirection( wxBOTH );
	fgSizer2->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_fieldnameFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Filter other symbol fields by name:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_fieldnameFilterOpt, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_fieldnameFilter = new wxTextCtrl( sbFilters->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_fieldnameFilter, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxEXPAND, 5 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 3 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 3 );

	m_referenceFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Filter items by parent reference designator:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_referenceFilterOpt, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	m_referenceFilter = new wxTextCtrl( sbFilters->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_referenceFilter->SetMinSize( wxSize( 150,-1 ) );

	fgSizer2->Add( m_referenceFilter, 0, wxEXPAND|wxLEFT, 5 );

	m_symbolFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Filter items by parent symbol library id:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_symbolFilterOpt, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_symbolFilter = new wxTextCtrl( sbFilters->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_symbolFilter, 0, wxEXPAND|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	m_typeFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Filter items by parent symbol type:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_typeFilterOpt, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	wxString m_typeFilterChoices[] = { _("Non-power symbols"), _("Power symbols") };
	int m_typeFilterNChoices = sizeof( m_typeFilterChoices ) / sizeof( wxString );
	m_typeFilter = new wxChoice( sbFilters->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, m_typeFilterNChoices, m_typeFilterChoices, 0 );
	m_typeFilter->SetSelection( 0 );
	fgSizer2->Add( m_typeFilter, 0, wxRIGHT|wxLEFT|wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 5 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND, 5 );

	m_netFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Filter items by net:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_netFilterOpt, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_netFilter = new wxTextCtrl( sbFilters->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_netFilter, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxLEFT, 5 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND|wxTOP|wxBOTTOM, 5 );


	fgSizer2->Add( 0, 0, 1, wxEXPAND, 5 );

	m_selectedFilterOpt = new wxCheckBox( sbFilters->GetStaticBox(), wxID_ANY, _("Only include selected items"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2->Add( m_selectedFilterOpt, 0, wxRIGHT|wxLEFT, 5 );


	sbFilters->Add( fgSizer2, 1, wxEXPAND|wxRIGHT, 5 );


	bSizerTop->Add( sbFilters, 1, wxEXPAND|wxTOP|wxRIGHT|wxLEFT, 10 );


	bMainSizer->Add( bSizerTop, 0, wxEXPAND, 5 );


	bMainSizer->Add( 0, 0, 0, wxTOP, 5 );


	bMainSizer->Add( 0, 0, 0, wxTOP, 5 );

	wxStaticBoxSizer* sbAction;
	sbAction = new wxStaticBoxSizer( new wxStaticBox( this, wxID_ANY, _("Set To") ), wxVERTICAL );

	m_specifiedValues = new wxPanel( sbAction->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxVERTICAL );

	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 0, 6, 2, 0 );
	fgSizer1->AddGrowableCol( 1 );
	fgSizer1->AddGrowableCol( 3 );
	fgSizer1->AddGrowableCol( 5 );
	fgSizer1->SetFlexibleDirection( wxBOTH );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_fontLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Font:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_fontLabel->Wrap( -1 );
	fgSizer1->Add( m_fontLabel, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	wxString m_fontCtrlChoices[] = { _("KiCad Font") };
	int m_fontCtrlNChoices = sizeof( m_fontCtrlChoices ) / sizeof( wxString );
	m_fontCtrl = new FONT_CHOICE( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_fontCtrlNChoices, m_fontCtrlChoices, 0 );
	m_fontCtrl->SetSelection( 0 );
	fgSizer1->Add( m_fontCtrl, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_setTextColor = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Text color:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_setTextColor, 0, wxALL, 5 );

	m_textColorSwatch = new COLOR_SWATCH( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_textColorSwatch->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_textColorSwatch->SetMinSize( wxSize( 48,24 ) );

	fgSizer1->Add( m_textColorSwatch, 0, wxALL, 5 );

	m_textSizeLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Text size:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_textSizeLabel->Wrap( -1 );
	fgSizer1->Add( m_textSizeLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_textSizeCtrl = new wxTextCtrl( m_specifiedValues, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_textSizeCtrl->SetMinSize( wxSize( 120,-1 ) );

	fgSizer1->Add( m_textSizeCtrl, 0, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );

	m_textSizeUnits = new wxStaticText( m_specifiedValues, wxID_ANY, _("unit"), wxDefaultPosition, wxDefaultSize, 0 );
	m_textSizeUnits->Wrap( -1 );
	fgSizer1->Add( m_textSizeUnits, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_bold = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Bold"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER );
	fgSizer1->Add( m_bold, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	orientationLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Orientation:"), wxDefaultPosition, wxDefaultSize, 0 );
	orientationLabel->Wrap( -1 );
	fgSizer1->Add( orientationLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	wxString m_orientationChoices[] = { _("Right"), _("Up"), _("Left"), _("Down"), _("-- leave unchanged --") };
	int m_orientationNChoices = sizeof( m_orientationChoices ) / sizeof( wxString );
	m_orientation = new wxChoice( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_orientationNChoices, m_orientationChoices, 0 );
	m_orientation->SetSelection( 4 );
	fgSizer1->Add( m_orientation, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxTOP|wxBOTTOM, 4 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 0, wxEXPAND|wxRIGHT|wxLEFT, 25 );

	m_italic = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Italic"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER );
	fgSizer1->Add( m_italic, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 5, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	hAlignLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("H Align:"), wxDefaultPosition, wxDefaultSize, 0 );
	hAlignLabel->Wrap( -1 );
	fgSizer1->Add( hAlignLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	wxString m_hAlignChoices[] = { _("Left"), _("Center"), _("Right"), _("-- leave unchanged --") };
	int m_hAlignNChoices = sizeof( m_hAlignChoices ) / sizeof( wxString );
	m_hAlign = new wxChoice( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_hAlignNChoices, m_hAlignChoices, 0 );
	m_hAlign->SetSelection( 3 );
	fgSizer1->Add( m_hAlign, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxTOP|wxBOTTOM, 4 );

	m_staticText14 = new wxStaticText( m_specifiedValues, wxID_ANY, _("(fields only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText14->Wrap( -1 );
	fgSizer1->Add( m_staticText14, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_visible = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Visible"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER );
	fgSizer1->Add( m_visible, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_staticText12 = new wxStaticText( m_specifiedValues, wxID_ANY, _("(fields only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText12->Wrap( -1 );
	fgSizer1->Add( m_staticText12, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 15 );

	vAlignLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("V Align:"), wxDefaultPosition, wxDefaultSize, 0 );
	vAlignLabel->Wrap( -1 );
	fgSizer1->Add( vAlignLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	wxString m_vAlignChoices[] = { _("Top"), _("Center"), _("Bottom"), _("-- leave unchanged --") };
	int m_vAlignNChoices = sizeof( m_vAlignChoices ) / sizeof( wxString );
	m_vAlign = new wxChoice( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_vAlignNChoices, m_vAlignChoices, 0 );
	m_vAlign->SetSelection( 3 );
	fgSizer1->Add( m_vAlign, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	m_staticText15 = new wxStaticText( m_specifiedValues, wxID_ANY, _("(fields only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText15->Wrap( -1 );
	fgSizer1->Add( m_staticText15, 0, wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_showFieldNames = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Show field name"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE );
	fgSizer1->Add( m_showFieldNames, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_staticText13 = new wxStaticText( m_specifiedValues, wxID_ANY, _("(fields only)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText13->Wrap( -1 );
	fgSizer1->Add( m_staticText13, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 15 );

	m_staticline1 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline1, 0, wxEXPAND|wxTOP|wxBOTTOM, 7 );

	m_staticline2 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline2, 0, wxEXPAND|wxTOP|wxBOTTOM, 7 );

	m_staticline21 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline21, 0, wxEXPAND|wxTOP|wxBOTTOM, 7 );

	m_staticline3 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline3, 0, wxEXPAND|wxTOP|wxBOTTOM, 7 );

	m_staticline4 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline4, 0, wxEXPAND|wxTOP|wxBOTTOM, 7 );

	m_staticline5 = new wxStaticLine( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	fgSizer1->Add( m_staticline5, 0, wxEXPAND|wxTOP|wxBOTTOM|wxRIGHT, 7 );

	m_lineWidthLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Line width:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_lineWidthLabel->Wrap( -1 );
	fgSizer1->Add( m_lineWidthLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_LineWidthCtrl = new wxTextCtrl( m_specifiedValues, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_LineWidthCtrl, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	m_lineWidthUnits = new wxStaticText( m_specifiedValues, wxID_ANY, _("unit"), wxDefaultPosition, wxDefaultSize, 0 );
	m_lineWidthUnits->Wrap( -1 );
	fgSizer1->Add( m_lineWidthUnits, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_setColor = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Line color:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_setColor, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_colorSwatch = new COLOR_SWATCH( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_colorSwatch->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_colorSwatch->SetMinSize( wxSize( 48,24 ) );

	fgSizer1->Add( m_colorSwatch, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	lineStyleLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Line style:"), wxDefaultPosition, wxDefaultSize, 0 );
	lineStyleLabel->Wrap( -1 );
	fgSizer1->Add( lineStyleLabel, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	wxString m_lineStyleChoices[] = { _("Solid"), _("Dashed"), _("Dotted"), _("Dash-Dot"), _("Dash-Dot-Dot") };
	int m_lineStyleNChoices = sizeof( m_lineStyleChoices ) / sizeof( wxString );
	m_lineStyle = new wxChoice( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_lineStyleNChoices, m_lineStyleChoices, 0 );
	m_lineStyle->SetSelection( 0 );
	fgSizer1->Add( m_lineStyle, 0, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND, 5 );

	m_setFillColor = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Fill color:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_setFillColor, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_fillColorSwatch = new COLOR_SWATCH( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_fillColorSwatch->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_fillColorSwatch->SetMinSize( wxSize( 48,24 ) );

	fgSizer1->Add( m_fillColorSwatch, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 5 );

	m_dotSizeLabel = new wxStaticText( m_specifiedValues, wxID_ANY, _("Junction size:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_dotSizeLabel->Wrap( -1 );
	fgSizer1->Add( m_dotSizeLabel, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	m_dotSizeCtrl = new wxTextCtrl( m_specifiedValues, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_dotSizeCtrl, 0, wxALIGN_CENTER_VERTICAL|wxEXPAND, 5 );

	m_dotSizeUnits = new wxStaticText( m_specifiedValues, wxID_ANY, _("unit"), wxDefaultPosition, wxDefaultSize, 0 );
	m_dotSizeUnits->Wrap( -1 );
	fgSizer1->Add( m_dotSizeUnits, 0, wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );


	fgSizer1->Add( 0, 0, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );

	m_setDotColor = new wxCheckBox( m_specifiedValues, wxID_ANY, _("Junction color:"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer1->Add( m_setDotColor, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );

	m_dotColorSwatch = new COLOR_SWATCH( m_specifiedValues, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_dotColorSwatch->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );
	m_dotColorSwatch->SetMinSize( wxSize( 48,24 ) );

	fgSizer1->Add( m_dotColorSwatch, 0, wxRIGHT|wxLEFT|wxALIGN_CENTER_VERTICAL, 5 );


	bSizer2->Add( fgSizer1, 1, wxEXPAND|wxTOP, 2 );


	m_specifiedValues->SetSizer( bSizer2 );
	m_specifiedValues->Layout();
	bSizer2->Fit( m_specifiedValues );
	sbAction->Add( m_specifiedValues, 1, wxEXPAND|wxBOTTOM, 12 );


	bMainSizer->Add( sbAction, 1, wxEXPAND|wxRIGHT|wxLEFT, 10 );

	m_sdbSizerButtons = new wxStdDialogButtonSizer();
	m_sdbSizerButtonsOK = new wxButton( this, wxID_OK );
	m_sdbSizerButtons->AddButton( m_sdbSizerButtonsOK );
	m_sdbSizerButtonsApply = new wxButton( this, wxID_APPLY );
	m_sdbSizerButtons->AddButton( m_sdbSizerButtonsApply );
	m_sdbSizerButtonsCancel = new wxButton( this, wxID_CANCEL );
	m_sdbSizerButtons->AddButton( m_sdbSizerButtonsCancel );
	m_sdbSizerButtons->Realize();

	bMainSizer->Add( m_sdbSizerButtons, 0, wxALL|wxEXPAND, 5 );


	this->SetSizer( bMainSizer );
	this->Layout();
	bMainSizer->Fit( this );

	// Connect Events
	this->Connect( wxEVT_UPDATE_UI, wxUpdateUIEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnUpdateUI ) );
	m_fieldnameFilter->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnFieldNameFilterText ), NULL, this );
	m_referenceFilter->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnReferenceFilterText ), NULL, this );
	m_symbolFilter->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnSymbolFilterText ), NULL, this );
	m_netFilter->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnNetFilterText ), NULL, this );
	m_fontCtrl->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::onFontSelected ), NULL, this );
}

DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::~DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE()
{
	// Disconnect Events
	this->Disconnect( wxEVT_UPDATE_UI, wxUpdateUIEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnUpdateUI ) );
	m_fieldnameFilter->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnFieldNameFilterText ), NULL, this );
	m_referenceFilter->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnReferenceFilterText ), NULL, this );
	m_symbolFilter->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnSymbolFilterText ), NULL, this );
	m_netFilter->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::OnNetFilterText ), NULL, this );
	m_fontCtrl->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( DIALOG_GLOBAL_EDIT_TEXT_AND_GRAPHICS_BASE::onFontSelected ), NULL, this );

}
