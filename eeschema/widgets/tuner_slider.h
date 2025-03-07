/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2016 CERN
 * Copyright (C) 2021 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * https://www.gnu.org/licenses/gpl-3.0.html
 * or you may search the http://www.gnu.org website for the version 3 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef TUNER_SLIDER_H
#define TUNER_SLIDER_H

#include "tuner_slider_base.h"

#include <sim/spice_value.h>
#include <sim/spice_generator.h>

#include <wx/timer.h>

class SIM_PLOT_FRAME;
class SCH_SYMBOL;

/**
 * Custom widget to handle quick component values modification and simulation on the fly.
 */
class TUNER_SLIDER : public TUNER_SLIDER_BASE
{
public:
    TUNER_SLIDER( SIM_PLOT_FRAME *aFrame, wxWindow* aParent, SCH_SYMBOL* aSymbol );

    wxString GetComponentName() const
    {
        return m_name->GetLabel();
    }

    const SPICE_VALUE& GetMin() const
    {
        return m_min;
    }

    const SPICE_VALUE& GetMax() const
    {
        return m_max;
    }

    std::string GetTunerCommand() const;

    bool SetValue( const SPICE_VALUE& aVal );
    bool SetMin( const SPICE_VALUE& aVal );
    bool SetMax( const SPICE_VALUE& aVal );

private:
    void updateComponentValue();
    void updateSlider();
    void updateValueText();

    void updateMax();
    void updateValue();
    void updateMin();

    void onClose( wxCommandEvent& event ) override;
    void onSave( wxCommandEvent& event ) override;
    void onSliderChanged( wxScrollEvent& event ) override;

    void onMaxKillFocus( wxFocusEvent& event ) override;
    void onValueKillFocus( wxFocusEvent& event ) override;
    void onMinKillFocus( wxFocusEvent& event ) override;

    void onMaxTextEnter( wxCommandEvent& event ) override;
    void onValueTextEnter( wxCommandEvent& event ) override;
    void onMinTextEnter( wxCommandEvent& event ) override;

    void onSimTimer( wxTimerEvent& event );

    ///< Timer that restarts the simulation after the slider value has changed
    wxTimer m_simTimer;

    SCH_SYMBOL* m_symbol;
    const SPICE_ITEM* m_item;

    SPICE_VALUE m_min, m_max, m_value;
    bool m_changed;

    SIM_PLOT_FRAME* m_frame;
};

#endif /* TUNER_SLIDER_H */
