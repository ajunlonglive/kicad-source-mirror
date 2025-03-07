/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
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

#include <pybind11/pybind11.h>

#include <wx/debug.h>
#include <wx/msgdlg.h>
#include <i18n_utility.h>
#include <macros.h>
#include <board.h>
#include <board_design_settings.h>
#include <pcb_group.h>


BOARD_ITEM::~BOARD_ITEM()
{
    wxASSERT( m_group == nullptr );
}


const BOARD* BOARD_ITEM::GetBoard() const
{
    if( Type() == PCB_T )
        return static_cast<const BOARD*>( this );

    BOARD_ITEM* parent = GetParent();

    if( parent )
        return parent->GetBoard();

    return nullptr;
}


BOARD* BOARD_ITEM::GetBoard()
{
    if( Type() == PCB_T )
        return static_cast<BOARD*>( this );

    BOARD_ITEM* parent = GetParent();

    if( parent )
        return parent->GetBoard();

    return nullptr;
}


bool BOARD_ITEM::IsLocked() const
{
    if( GetParentGroup() )
        return GetParentGroup()->IsLocked();

    const BOARD* board = GetBoard();

    return board && board->GetBoardUse() != BOARD_USE::FPHOLDER && m_isLocked;
}


STROKE_PARAMS BOARD_ITEM::GetStroke() const
{
    wxCHECK( false, STROKE_PARAMS( pcbIUScale.mmToIU( DEFAULT_LINE_WIDTH ) ) );
}


void BOARD_ITEM::SetStroke( const STROKE_PARAMS& aStroke )
{
    wxCHECK( false, /* void */ );
}


wxString BOARD_ITEM::GetLayerName() const
{
    const BOARD* board = GetBoard();

    if( board )
        return board->GetLayerName( m_layer );

    // If no parent, return standard name
    return BOARD::GetStandardLayerName( m_layer );
}


wxString BOARD_ITEM::layerMaskDescribe() const
{
    const BOARD* board = GetBoard();
    LSET         layers = GetLayerSet();

    // Try to be smart and useful.  Check all copper first.
    if( layers[F_Cu] && layers[B_Cu] )
        return _( "all copper layers" );

    LSET copperLayers = layers & board->GetEnabledLayers().AllCuMask();
    LSET techLayers = layers & board->GetEnabledLayers().AllTechMask();

    for( LSET testLayers : { copperLayers, techLayers, layers } )
    {
        for( int bit = PCBNEW_LAYER_ID_START; bit < PCB_LAYER_ID_COUNT; ++bit )
        {
            if( testLayers[ bit ] )
            {
                wxString layerInfo = board->GetLayerName( static_cast<PCB_LAYER_ID>( bit ) );

                if( testLayers.count() > 1 )
                    layerInfo << wxS( " " ) + _( "and others" );

                return layerInfo;
            }
        }
    }

    // No copper, no technicals: no layer
    return _( "no layers" );
}


void BOARD_ITEM::ViewGetLayers( int aLayers[], int& aCount ) const
{
    // Basic fallback
    aCount = 1;
    aLayers[0] = m_layer;

    if( IsLocked() )
        aLayers[aCount++] = LAYER_LOCKED_ITEM_SHADOW;
}


void BOARD_ITEM::DeleteStructure()
{
    BOARD_ITEM_CONTAINER* parent = GetParent();

    if( parent )
        parent->Remove( this );

    delete this;
}


void BOARD_ITEM::swapData( BOARD_ITEM* aImage )
{
}


void BOARD_ITEM::SwapItemData( BOARD_ITEM* aImage )
{
    if( aImage == nullptr )
        return;

    wxASSERT( Type() == aImage->Type() );
    wxASSERT( m_Uuid == aImage->m_Uuid );

    EDA_ITEM*  parent = GetParent();
    PCB_GROUP* group = GetParentGroup();

    SetParentGroup( nullptr );
    aImage->SetParentGroup( nullptr );
    swapData( aImage );

    // Restore pointers to be sure they are not broken
    SetParent( parent );
    SetParentGroup( group );
}


BOARD_ITEM* BOARD_ITEM::Duplicate() const
{
    BOARD_ITEM* dupe = static_cast<BOARD_ITEM*>( Clone() );
    const_cast<KIID&>( dupe->m_Uuid ) = KIID();

    if( dupe->GetParentGroup() )
        dupe->GetParentGroup()->AddItem( dupe );

    return static_cast<BOARD_ITEM*>( dupe );
}


void BOARD_ITEM::TransformShapeToPolygon( SHAPE_POLY_SET& aBuffer, PCB_LAYER_ID aLayer,
                                          int aClearance, int aError, ERROR_LOC aErrorLoc,
                                          bool ignoreLineWidth ) const
{
    wxASSERT_MSG( false, wxT( "Called TransformShapeToPolygon() on unsupported BOARD_ITEM." ) );
};


bool BOARD_ITEM::ptr_cmp::operator() ( const BOARD_ITEM* a, const BOARD_ITEM* b ) const
{
    if( a->Type() != b->Type() )
        return a->Type() < b->Type();

    if( a->GetLayerSet() != b->GetLayerSet() )
        return a->GetLayerSet().Seq() < b->GetLayerSet().Seq();

    if( a->m_Uuid != b->m_Uuid )       // UUIDs *should* always be unique (for valid boards anyway)
        return a->m_Uuid < b->m_Uuid;

    return a < b;                      // But just in case; ptrs are guaranteed to be different
}


std::shared_ptr<SHAPE> BOARD_ITEM::GetEffectiveShape( PCB_LAYER_ID aLayer, FLASHING aFlash ) const
{
    static std::shared_ptr<SHAPE> shape;

    UNIMPLEMENTED_FOR( GetClass() );

    return shape;
}


std::shared_ptr<SHAPE_SEGMENT> BOARD_ITEM::GetEffectiveHoleShape() const
{
    static std::shared_ptr<SHAPE_SEGMENT> slot;

    UNIMPLEMENTED_FOR( GetClass() );

    return slot;
}


BOARD_ITEM_CONTAINER* BOARD_ITEM::GetParentFootprint() const
{
    BOARD_ITEM_CONTAINER* ancestor = GetParent();

    while( ancestor && ancestor->Type() == PCB_GROUP_T )
        ancestor = ancestor->GetParent();

    return ( ancestor && ancestor->Type() == PCB_FOOTPRINT_T ) ? ancestor : nullptr;
}


void BOARD_ITEM::Rotate( const VECTOR2I& aRotCentre, const EDA_ANGLE& aAngle )
{
    wxMessageBox( wxT( "virtual BOARD_ITEM::Rotate used, should not occur" ), GetClass() );
}


void BOARD_ITEM::Flip( const VECTOR2I& aCentre, bool aFlipLeftRight )
{
    wxMessageBox( wxT( "virtual BOARD_ITEM::Flip used, should not occur" ), GetClass() );
}


static struct BOARD_ITEM_DESC
{
    BOARD_ITEM_DESC()
    {
        ENUM_MAP<PCB_LAYER_ID>& layerEnum = ENUM_MAP<PCB_LAYER_ID>::Instance();

        if( layerEnum.Choices().GetCount() == 0 )
        {
            layerEnum.Undefined( UNDEFINED_LAYER );

            for( LSEQ seq = LSET::AllLayersMask().Seq(); seq; ++seq )
                layerEnum.Map( *seq, LSET::Name( *seq ) );
        }

        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( BOARD_ITEM );
        propMgr.InheritsAfter( TYPE_HASH( BOARD_ITEM ), TYPE_HASH( EDA_ITEM ) );

        propMgr.AddProperty( new PROPERTY<BOARD_ITEM, int>( _HKI( "Position X" ),
                    &BOARD_ITEM::SetX, &BOARD_ITEM::GetX, PROPERTY_DISPLAY::PT_COORD,
                    ORIGIN_TRANSFORMS::ABS_X_COORD) );
        propMgr.AddProperty( new PROPERTY<BOARD_ITEM, int>( _HKI( "Position Y" ),
                    &BOARD_ITEM::SetY, &BOARD_ITEM::GetY, PROPERTY_DISPLAY::PT_COORD,
                    ORIGIN_TRANSFORMS::ABS_Y_COORD) );
        propMgr.AddProperty( new PROPERTY_ENUM<BOARD_ITEM, PCB_LAYER_ID>( _HKI( "Layer" ),
                    &BOARD_ITEM::SetLayer, &BOARD_ITEM::GetLayer ) );
        propMgr.AddProperty( new PROPERTY<BOARD_ITEM, bool>( _HKI( "Locked" ),
                    &BOARD_ITEM::SetLocked, &BOARD_ITEM::IsLocked ) );
    }
} _BOARD_ITEM_DESC;

IMPLEMENT_ENUM_TO_WXANY( PCB_LAYER_ID )
