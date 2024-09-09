//========= Copyright PiMoNFeeD, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// bspconverter.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "tools_minidump.h"
#include "tier0/icommandline.h"
#include "mathlib/mathlib.h"
#include "materialsystem/imaterialsystem.h"
#include "cmdlib.h"
#include "bsplib.h"
#include "utilmatlib.h"
#include "gamebspfile.h"
#include "utlbuffer.h"

bool g_bSpewMissingAssets = false;
int ParseCommandLine( int argc, char** argv )
{
	int i;
	for ( i = 1; i < argc; i++ )
	{
		if ( !V_stricmp( argv[i], "-nolightdata" ) )
		{
			g_bSaveLightData = false;
		}
		else if ( !V_stricmp( argv[i], "-spewmissingassets" ) )
		{
			g_bSpewMissingAssets = true;
		}
		else if ( V_stricmp( argv[i], "-steam" ) == 0 )
		{
		}
		else if ( V_stricmp( argv[i], "-allowdebug" ) == 0 )
		{
			// Don't need to do anything, just don't error out.
		}
		else if ( !V_stricmp( argv[i], CMDLINEOPTION_NOVCONFIG ) )
		{
		}
		else if ( !V_stricmp( argv[i], "-vproject" ) || !V_stricmp( argv[i], "-game" ) || !V_stricmp( argv[i], "-insert_search_path" ) )
		{
			++i;
		}
		else if ( !V_stricmp( argv[i], "-FullMinidumps" ) )
		{
			EnableFullMinidumps( true );
		}
		else
		{
			break;
		}
	}
	return i;
}

void PrintCommandLine( int argc, char** argv )
{
	Warning( "Command line: " );
	for ( int z = 0; z < argc; z++ )
	{
		Warning( "\"%s\" ", argv[z] );
	}
	Warning( "\n\n" );
}

void PrintUsage( int argc, char** argv )
{
	PrintCommandLine( argc, argv );

	Warning(
		"usage  : bspconverter [options...] bspfile\n"
		"example: bspconverter -nolightdata c:\\hl2\\hl2\\maps\\test\n"
		"\n"
		"Common options:\n"
		"\n"
		"  -nolightdata       : Doesn't save any light information in the fixed file\n"
		"  -spewmissingassets : Logs every missing brush texture and static prop model\n"
		"\n"
	);
}

int main(int argc, char* argv[])
{
	SetupDefaultToolsMinidumpHandler();
	CommandLine()->CreateCmdLine( argc, argv );
	MathLib_Init( 2.2f, 2.2f, 0.0f, OVERBRIGHT, false, false, false, false );
	InstallSpewFunction();
	SpewActivate( "developer", 1 );

	int i = ParseCommandLine( argc, argv );
	if ( i != argc - 1 )
	{
		PrintUsage( argc, argv );
		CmdLib_Exit( 1 );
	}

	CmdLib_InitFileSystem( argv[argc - 1] );

	char materialPath[1024];
	sprintf( materialPath, "%smaterials", gamedir );
	InitMaterialSystem( materialPath, CmdLib_GetFileSystemFactory() );
	Msg( "materialPath: %s\n", materialPath );

	float start = Plat_FloatTime();

	{
		char mapFile[1024];
		V_FileBase( argv[argc - 1], mapFile, sizeof( mapFile ) );
		V_strncpy( mapFile, ExpandPath( mapFile ), sizeof( mapFile ) );

		// Setup the logfile.
		char logFile[512];
		_snprintf( logFile, sizeof( logFile ), "%s.log", mapFile );
		SetSpewFunctionLogFile( logFile );

		g_bBSPConverterMode = true;

		V_strncat( mapFile, ".bsp", sizeof( mapFile ) );
		Msg( "Leading %s\n", mapFile );
		LoadBSPFile( mapFile );
		if ( numnodes == 0 || numfaces == 0 )
			Error( "Empty map" );
		//ParseEntities(); -- only needed to check max entities, but not critical
		PrintBSPFileSizes();

		char mapFileFixed[1024];
		V_FileBase( argv[argc - 1], mapFileFixed, sizeof( mapFileFixed ) );
		V_strncpy( mapFileFixed, ExpandPath( mapFileFixed ), sizeof( mapFileFixed ) );
		V_strncat( mapFileFixed, "_fixed.bsp", sizeof( mapFileFixed ) );
		Msg( "Writing %s\n", mapFileFixed );
		WriteBSPFile( mapFileFixed );

		g_pFullFileSystem->AddSearchPath( mapFileFixed, "GAME", PATH_ADD_TO_HEAD );
		g_pFullFileSystem->AddSearchPath( mapFileFixed, "MOD", PATH_ADD_TO_HEAD );

		// check if assets exist
		// do it after writing the new file because of embedded files, because filesystem ignores bsps with invalid version!
		if ( g_bSpewMissingAssets )
		{
			// look for static prop lump...
			GameLumpHandle_t h = g_GameLumps.GetGameLumpHandle( GAMELUMP_STATIC_PROPS );
			if ( h != g_GameLumps.InvalidGameLump() )
			{
				int iSize = g_GameLumps.GameLumpSize( h );
				if ( iSize > 0 )
				{
					CUtlBuffer buf( g_GameLumps.GetGameLump( h ), iSize, CUtlBuffer::READ_ONLY );
					int iDictCount = buf.GetInt();
					CUtlVector<StaticPropDictLump_t> vecDict;
					vecDict.EnsureCount( iDictCount );
					buf.Get( vecDict.Base(), sizeof( StaticPropDictLump_t ) * iDictCount );

					FOR_EACH_VEC( vecDict, j )
					{
						if ( !g_pFileSystem->FileExists( vecDict[j].m_Name, NULL ) )
							Warning( "Error loading studio model \"%s\"!\n", vecDict[j].m_Name );
					}
				}
			}

			FOR_EACH_VEC( g_TexDataStringTable, i )
			{
				const char* pszMaterialName = TexDataStringTable_GetString( i );
				g_pMaterialSystem->FindMaterial( pszMaterialName, TEXTURE_GROUP_OTHER, true );
			}
		}
	}

	float end = Plat_FloatTime();

	char str[512];
	GetHourMinuteSecondsString( (int)(end - start), str, sizeof( str ) );
	Msg( "%s elapsed\n", str );

	ShutdownMaterialSystem();
	CmdLib_Cleanup();
	return 0;
}

