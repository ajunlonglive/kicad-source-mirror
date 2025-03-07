/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jaen-pierre.charras at wanadoo.fr
 * Copyright (C) 2015 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 2004-2021 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef _LIB_ITEM_H_
#define _LIB_ITEM_H_

#include <eda_item.h>
#include <eda_shape.h>
#include <transform.h>
#include <render_settings.h>

class LINE_READER;
class OUTPUTFORMATTER;
class LIB_SYMBOL;
class PLOTTER;
class LIB_PIN;
class MSG_PANEL_ITEM;

using KIGFX::RENDER_SETTINGS;

extern const int fill_tab[];


#define MINIMUM_SELECTION_DISTANCE 2 // Minimum selection distance in internal units


/**
 * Helper for defining a list of pin object pointers.  The list does not
 * use a Boost pointer class so the object pointers do not accidentally get
 * deleted when the container is deleted.
 */
typedef std::vector< LIB_PIN* > LIB_PINS;


/**
 * The base class for drawable items used by schematic library symbols.
 */
class LIB_ITEM : public EDA_ITEM
{
public:
    LIB_ITEM( KICAD_T aType, LIB_SYMBOL* aSymbol = nullptr, int aUnit = 0, int aConvert = 0 );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    virtual ~LIB_ITEM() { }

    // Define the enums for basic
    enum LIB_CONVERT : int  { BASE = 1, DEMORGAN = 2 };

    /**
     * The list of flags used by the #compare function.
     *
     * - UNIT This flag relaxes unit, conversion and pin number constraints.  It is used for
     *        #LIB_ITEM object unit comparisons.
     * - EQUALITY This flag relaxes ordering contstraints so that fields, etc. don't have to
     *            appear in the same order to be considered equal.
     * - ERC This flag relaxes constraints on data that is settable in the schematic editor.  It
     *       compares only symbol-editor-only data.
     */
    enum COMPARE_FLAGS : int
    {
        UNIT     = 0x01,
        EQUALITY = 0x02,
        ERC      = 0x04
    };

    /**
     * Provide a user-consumable name of the object type.  Perform localization when
     * called so that run-time language selection works.
     */
    virtual wxString GetTypeName() const = 0;

    /**
     * Begin drawing a symbol library draw item at \a aPosition.
     *
     * It typically would be called on a left click when a draw tool is selected in
     * the symbol library editor and one of the graphics tools is selected.
     *
     * @param aPosition The position in drawing coordinates where the drawing was started.
     *                  May or may not be required depending on the item being drawn.
     */
    virtual void BeginEdit( const VECTOR2I& aPosition ) {}

    /**
     * Continue an edit in progress at \a aPosition.
     *
     * This is used to perform the next action while drawing an item.  This would be
     * called for each additional left click when the mouse is captured while the item
     * is being drawn.
     *
     * @param aPosition The position of the mouse left click in drawing coordinates.
     * @return True if additional mouse clicks are required to complete the edit in progress.
     */
    virtual bool ContinueEdit( const VECTOR2I& aPosition ) { return false; }

    /**
     * End an object editing action.
     *
     * This is used to end or abort an edit action in progress initiated by BeginEdit().
     */
    virtual void EndEdit() {}

    /**
     * Calculate the attributes of an item at \a aPosition when it is being edited.
     *
     * This method gets called by the Draw() method when the item is being edited.  This
     * probably should be a pure virtual method but bezier curves are not yet editable in
     * the symbol library editor.  Therefore, the default method does nothing.
     *
     * @param aPosition The current mouse position in drawing coordinates.
     */
    virtual void CalcEdit( const VECTOR2I& aPosition ) {}

    /**
     * Draw an item
     *
     * @param aDC Device Context (can be null)
     * @param aOffset Offset to draw
     * @param aData Value or pointer used to pass others parameters, depending on body items.
     *              Used for some items to force to force no fill mode ( has meaning only for
     *              items what can be filled ). used in printing or moving objects mode or to
     *              pass reference to the lib symbol for pins.
     * @param aTransform Transform Matrix (rotation, mirror ..)
     * @param aDimmed Dim the color on the printout
     */
    virtual void Print( const RENDER_SETTINGS* aSettings, const VECTOR2I& aOffset, void* aData,
                        const TRANSFORM& aTransform, bool aDimmed );

    virtual int GetPenWidth() const = 0;

    const wxString& GetDefaultFont() const;

    virtual int GetEffectivePenWidth( const RENDER_SETTINGS* aSettings ) const
    {
        // For historical reasons, a stored value of 0 means "default width" and negative
        // numbers meant "don't stroke".

        if( GetPenWidth() < 0 )
            return 0;
        else if( GetPenWidth() == 0 )
            return std::max( aSettings->GetDefaultPenWidth(), aSettings->GetMinPenWidth() );
        else
            return std::max( GetPenWidth(), aSettings->GetMinPenWidth() );
    }

    LIB_SYMBOL* GetParent() const
    {
        return (LIB_SYMBOL*) m_parent;
    }

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    bool HitTest( const VECTOR2I& aPosition, int aAccuracy = 0 ) const override
    {
        // This is just here to prevent annoying compiler warnings about hidden overloaded
        // virtual functions
        return EDA_ITEM::HitTest( aPosition, aAccuracy );
    }

    bool HitTest( const BOX2I& aRect, bool aContained, int aAccuracy = 0 ) const override;

    /**
     * @return the boundary box for this, in library coordinates
     */
    const BOX2I GetBoundingBox() const override { return EDA_ITEM::GetBoundingBox(); }

    /**
     * Display basic info (type, part and convert) about the current item in message panel.
     * <p>
     * This base function is used to display the information common to the
     * all library items.  Call the base class from the derived class or the
     * common information will not be updated in the message panel.
     * </p>
     * @param aList is the list to populate.
     */
    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

    /**
     * Test LIB_ITEM objects for equivalence.
     *
     * @param aOther Object to test against.
     * @return True if object is identical to this object.
     */
    bool operator==( const LIB_ITEM& aOther ) const;
    bool operator==( const LIB_ITEM* aOther ) const
    {
        return *this == *aOther;
    }

    /**
     * Test if another draw item is less than this draw object.
     *
     * @param aOther - Draw item to compare against.
     * @return - True if object is less than this object.
     */
    bool operator<( const LIB_ITEM& aOther) const;

    /**
     * Set the drawing object by \a aOffset from the current position.
     *
     * @param aOffset Coordinates to offset the item position.
     */
    virtual void Offset( const VECTOR2I& aOffset ) = 0;

    /**
     * Move a draw object to \a aPosition.
     *
     * @param aPosition Position to move draw item to.
     */
    virtual void MoveTo( const VECTOR2I& aPosition ) = 0;

    void SetPosition( const VECTOR2I& aPosition ) override { MoveTo( aPosition ); }

    /**
     * Mirror the draw object along the horizontal (X) axis about \a aCenter point.
     *
     * @param aCenter Point to mirror around.
     */
    virtual void MirrorHorizontal( const VECTOR2I& aCenter ) = 0;

    /**
     * Mirror the draw object along the MirrorVertical (Y) axis about \a aCenter point.
     *
     * @param aCenter Point to mirror around.
     */
    virtual void MirrorVertical( const VECTOR2I& aCenter ) = 0;

    /**
     * Rotate the object about \a aCenter point.
     *
     * @param aCenter Point to rotate around.
     * @param aRotateCCW True to rotate counter clockwise.  False to rotate clockwise.
     */
    virtual void Rotate( const VECTOR2I& aCenter, bool aRotateCCW = true ) = 0;

    /**
     * Plot the draw item using the plot object.
     *
     * @param aPlotter The plot object to plot to.
     * @param aBackground a poor-man's Z-order.  The routine will get called twice, first with
     *                    aBackground true and then with aBackground false.
     * @param aOffset Plot offset position.
     * @param aFill Flag to indicate whether or not the object is filled.
     * @param aTransform The plot transform.
     * @param aDimmed if true, reduce color to background
     */
    virtual void Plot( PLOTTER* aPlotter, bool aBackground, const VECTOR2I& aOffset,
                       const TRANSFORM& aTransform, bool aDimmed ) const = 0;

    void SetUnit( int aUnit ) { m_unit = aUnit; }
    int GetUnit() const { return m_unit; }

    void SetConvert( int aConvert ) { m_convert = aConvert; }
    int GetConvert() const { return m_convert; }

    void SetPrivate( bool aPrivate ) { m_private = aPrivate; }
    bool IsPrivate() const { return m_private; }

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif

protected:
    /**
     * Provide the draw object specific comparison called by the == and < operators.
     *
     * The base object sort order which always proceeds the derived object sort order
     * is as follows:
     *      - Symbol alternate part (DeMorgan) number.
     *      - Symbol part number.
     *      - KICAD_T enum value.
     *      - Result of derived classes comparison.
     *
     * @note Make sure you call down to #LIB_ITEM::compare before doing any derived object
     *       comparisons or you will break the sorting using the symbol library file format.
     *
     * @param aOther A reference to the other #LIB_ITEM to compare the arc against.
     * @param aCompareFlags The flags used to perform the comparison.
     *
     * @return An integer value less than 0 if the object is less than \a aOther object,
     *         zero if the object is equal to \a aOther object, or greater than 0 if the
     *         object is greater than \a aOther object.
     */
    virtual int compare( const LIB_ITEM& aOther, int aCompareFlags = 0 ) const;

    /**
     * Print the item to \a aDC.
     *
     * @param aOffset A reference to a wxPoint object containing the offset where to draw
     *                from the object's current position.
     * @param aData A pointer to any object specific data required to perform the draw.
     * @param aTransform A reference to a #TRANSFORM object containing drawing transform.
     */
    virtual void print( const RENDER_SETTINGS* aSettings, const VECTOR2I& aOffset, void* aData,
                        const TRANSFORM& aTransform, bool aDimmed ) = 0;

private:
    friend class LIB_SYMBOL;

protected:
    /**
     * Unit identification for multiple parts per package.  Set to 0 if the item is common
     * to all units.
     */
    int         m_unit;

    /**
     * Shape identification for alternate body styles.  Set 0 if the item is common to all
     * body styles.  This is typially used for representing DeMorgan variants in KiCad.
     */
    int         m_convert;

    /**
     * Private items are shown only in the Symbol Editor.
     */
    bool        m_private;
};


#endif  //  _LIB_ITEM_H_
