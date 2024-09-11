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

	char szMaterialPath[MAX_PATH];
	sprintf( szMaterialPath, "%smaterials", gamedir );
	InitMaterialSystem( szMaterialPath, CmdLib_GetFileSystemFactory() );
	Msg( "materialPath: %s\n", szMaterialPath );

	float flStart = Plat_FloatTime();

	{
		char szMapFile[MAX_PATH];
		V_FileBase( argv[argc - 1], szMapFile, sizeof( szMapFile ) );
		V_strcpy_safe( szMapFile, ExpandPath( szMapFile ) );

		// Setup the logfile.
		char szLogFile[MAX_PATH];
		V_snprintf( szLogFile, sizeof( szLogFile ), "%s.log", szMapFile );
		SetSpewFunctionLogFile( szLogFile );

		g_bBSPConverterMode = true;

		V_strcat_safe( szMapFile, ".bsp" );
		Msg( "Leading %s\n", szMapFile );
		LoadBSPFile( szMapFile );
		if ( numnodes == 0 || numfaces == 0 )
			Error( "Empty map" );
		//ParseEntities(); -- only needed to check max entities, but not critical
		PrintBSPFileSizes();

		char szMapFileFixed[MAX_PATH];
		V_FileBase( argv[argc - 1], szMapFileFixed, sizeof( szMapFileFixed ) );
		V_strcpy_safe( szMapFileFixed, ExpandPath( szMapFileFixed ) );
		V_strcat_safe( szMapFileFixed, "_fixed.bsp" );
		Msg( "Writing %s\n", szMapFileFixed );
		WriteBSPFile( szMapFileFixed );

		g_pFullFileSystem->AddSearchPath( szMapFileFixed, "GAME", PATH_ADD_TO_HEAD );
		g_pFullFileSystem->AddSearchPath( szMapFileFixed, "MOD", PATH_ADD_TO_HEAD );

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

	float flEnd = Plat_FloatTime();

	char szElapsed[128];
	GetHourMinuteSecondsString( (int)(flEnd - flStart), szElapsed, sizeof( szElapsed ) );
	Msg( "%s elapsed\n", szElapsed );

	ShutdownMaterialSystem();
	CmdLib_Cleanup();
	return 0;
}

