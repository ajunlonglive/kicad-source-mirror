/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2022 Mark Roszko <mark.roszko@gmail.com>
 * Copyright (C) 1992-2022 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "command_export_pcb_pos.h"
#include <cli/exit_codes.h>
#include "jobs/job_export_pcb_pos.h"
#include <kiface_base.h>
#include <layer_ids.h>
#include <wx/crt.h>

#include <macros.h>

#include <locale_io.h>

#define ARG_SIDE "--side"
#define ARG_FORMAT "--format"
#define ARG_UNITS "--units"
#define ARG_NEGATE_BOTTOM_X "--bottom-negate-x"
#define ARG_USE_DRILL_FILE_ORIGIN "--use-drill-file-origin"
#define ARG_SMD_ONLY "--smd-only"
#define ARG_EXCLUDE_FOOTPRINTS_TH "--exclude-fp-th"
#define ARG_GERBER_BOARD_EDGE "--gerber-board-edge"


CLI::EXPORT_PCB_POS_COMMAND::EXPORT_PCB_POS_COMMAND() : EXPORT_PCB_BASE_COMMAND( "pos" )
{
    m_argParser.add_argument( ARG_SIDE )
            .default_value( std::string( "both" ) )
            .help( "valid options: front,back,both" );

    m_argParser.add_argument( ARG_FORMAT )
            .default_value( std::string( "ascii" ) )
            .help( "valid options: ascii,csv,gerber" );

    m_argParser.add_argument( ARG_UNITS )
            .default_value( std::string( "in" ) )
            .help( "output units, valid options are in or mm (ascii or csv only)" );

    m_argParser.add_argument( ARG_NEGATE_BOTTOM_X )
            .help( "Use negative X coordinates for footprints on bottom layer (ascii or csv only)" )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_USE_DRILL_FILE_ORIGIN )
            .help( "Use drill/place file origin (ascii or csv only)" )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_SMD_ONLY )
            .help( "Include only SMD footprints (ascii or csv only)" )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_EXCLUDE_FOOTPRINTS_TH )
            .help( "Exclue all footprints with through-hole pads (ascii or csv only)" )
            .implicit_value( true )
            .default_value( false );

    m_argParser.add_argument( ARG_GERBER_BOARD_EDGE )
            .help( "Include board edge layer (gerber only)" )
            .implicit_value( true )
            .default_value( false );
}


int CLI::EXPORT_PCB_POS_COMMAND::Perform( KIWAY& aKiway )
{
    int baseExit = EXPORT_PCB_BASE_COMMAND::Perform( aKiway );
    if( baseExit != EXIT_CODES::OK )
        return baseExit;

    std::unique_ptr<JOB_EXPORT_PCB_POS> aPosJob( new JOB_EXPORT_PCB_POS( true ) );

    aPosJob->m_filename = FROM_UTF8( m_argParser.get<std::string>( ARG_INPUT ).c_str() );
    aPosJob->m_outputFile = FROM_UTF8( m_argParser.get<std::string>( ARG_OUTPUT ).c_str() );

    if( !wxFile::Exists( aPosJob->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    aPosJob->m_negateBottomX = m_argParser.get<bool>( ARG_NEGATE_BOTTOM_X );
    aPosJob->m_smdOnly = m_argParser.get<bool>( ARG_SMD_ONLY );
    aPosJob->m_excludeFootprintsWithTh = m_argParser.get<bool>( ARG_EXCLUDE_FOOTPRINTS_TH );
    aPosJob->m_useDrillPlaceFileOrigin = m_argParser.get<bool>( ARG_USE_DRILL_FILE_ORIGIN );
    aPosJob->m_useDrillPlaceFileOrigin = m_argParser.get<bool>( ARG_GERBER_BOARD_EDGE );

    wxString format = FROM_UTF8( m_argParser.get<std::string>( ARG_FORMAT ).c_str() );
    if( format == wxS( "ascii" ) )
    {
        aPosJob->m_format = JOB_EXPORT_PCB_POS::FORMAT::ASCII;
    }
    else if( format == wxS( "csv" ) )
    {
        aPosJob->m_format = JOB_EXPORT_PCB_POS::FORMAT::CSV;
    }
    else if( format == wxS( "gerber" ) )
    {
        aPosJob->m_format = JOB_EXPORT_PCB_POS::FORMAT::GERBER;
    }
    else
    {
        wxFprintf( stderr, _( "Invalid format\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    wxString units = FROM_UTF8( m_argParser.get<std::string>( ARG_UNITS ).c_str() );

    if( units == wxS( "mm" ) )
    {
        aPosJob->m_units = JOB_EXPORT_PCB_POS::UNITS::MILLIMETERS;
    }
    else if( units == wxS( "in" ) )
    {
        aPosJob->m_units = JOB_EXPORT_PCB_POS::UNITS::INCHES;
    }
    else
    {
        wxFprintf( stderr, _( "Invalid units specified\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    wxString side = FROM_UTF8( m_argParser.get<std::string>( ARG_SIDE ).c_str() );

    if( units == wxS( "both" ) )
    {
        if( aPosJob->m_format == JOB_EXPORT_PCB_POS::FORMAT::GERBER )
        {
            wxFprintf( stderr, _( "\"both\" not supported for gerber format\n" ) );
            return EXIT_CODES::ERR_ARGS;
        }

        aPosJob->m_side = JOB_EXPORT_PCB_POS::SIDE::BOTH;
    }
    else if( units == wxS( "front" ) )
    {
        aPosJob->m_side = JOB_EXPORT_PCB_POS::SIDE::FRONT;
    }
    else if( units == wxS( "back" ) )
    {
        aPosJob->m_side = JOB_EXPORT_PCB_POS::SIDE::BACK;
    }
    else
    {
        wxFprintf( stderr, _( "Invalid side specified\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }


    LOCALE_IO dummy;
    int       exitCode = aKiway.ProcessJob( KIWAY::FACE_PCB, aPosJob.get() );

    return exitCode;
}