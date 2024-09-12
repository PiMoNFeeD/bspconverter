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
#include "KeyValues.h"

bool g_bSpewMissingAssets = false;
char g_szOutputFile[MAX_PATH] = { 0 };
int ParseCommandLine( int argc, char** argv )
{
	int i;
	for ( i = 1; i < argc; i++ )
	{
		if ( !V_stricmp( argv[i], "-nolightdata" ) )
		{
			g_BSPConverterOptions.m_bSaveLightData = false;
		}
		else if ( !V_stricmp( argv[i], "-keeplightmapalpha" ) )
		{
			g_BSPConverterOptions.m_bSaveLightmapAlpha = true;
		}
		else if ( !V_stricmp( argv[i], "-spewmissingassets" ) )
		{
			g_bSpewMissingAssets = true;
		}
		else if ( !V_stricmp( argv[i], "-o" ) || !V_stricmp( argv[i], "-output" ) )
		{
			if ( i + 1 < argc )
			{
				V_StripExtension( argv[i + 1], g_szOutputFile, sizeof( g_szOutputFile ) );
				i++;
			}
		}
		else if ( !V_stricmp( argv[i], "-v" ) || !V_stricmp( argv[i], "-verbose" ) )
		{
			verbose = true;
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
			i++;
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
		"  -o (or -output)    : Specifies output file name, defaults to <mapname>_fixed.bsp\n"
		"  -nolightdata       : Doesn't save any light information in the fixed file\n"
		"  -keeplightmapalpha : Doesn't strip unused lightmap alpha data\n"
		"  -spewmissingassets : Logs every missing brush material and static prop model\n"
		"  -v (or -verbose)   : Turn on verbose output\n"
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

		char szMapName[MAX_PATH];
		V_strcpy_safe( szMapName, V_GetFileName( szMapFile ) );

		// Setup the logfile.
		char szLogFile[MAX_PATH];
		V_snprintf( szLogFile, sizeof( szLogFile ), "%s.log", szMapFile );
		SetSpewFunctionLogFile( szLogFile );

		g_BSPConverterOptions.m_bEnabled = true;

		V_strcat_safe( szMapFile, ".bsp" );
		Msg( "Leading %s\n", szMapFile );
		LoadBSPFile( szMapFile );
		if ( numnodes == 0 || numfaces == 0 )
			Error( "Empty map" );
		//ParseEntities(); -- only needed to check max entities, but not critical
		PrintBSPFileSizes();

		char szMapFileFixed[MAX_PATH];
		if ( g_szOutputFile[0] )
		{
			V_strcpy_safe( szMapFileFixed, qdir );
			V_strcat_safe( szMapFileFixed, g_szOutputFile );
			V_strcat_safe( szMapFileFixed, ".bsp" );
		}
		else
		{
			V_FileBase( argv[argc - 1], szMapFileFixed, sizeof( szMapFileFixed ) );
			V_strcpy_safe( szMapFileFixed, ExpandPath( szMapFileFixed ) );
			V_strcat_safe( szMapFileFixed, "_fixed.bsp" );
			V_StripExtension( V_GetFileName( szMapFileFixed ), g_szOutputFile, sizeof( g_szOutputFile ) );
		}

		if ( V_strcmp( szMapFile, szMapFileFixed ) )
		{
			// if input and output file names don't match, we need to fix up bundled cubemaps and patched vmts

			IZip* pNewPakFile = IZip::CreateZip( NULL );
			int iID = -1;
			while ( 1 )
			{
				int iFileSize;
				char szRelativeFileName[MAX_PATH];
				iID = GetNextFilename( GetPakFile(), iID, szRelativeFileName, sizeof( szRelativeFileName ), iFileSize );
				if ( iID == -1 )
					break;

				CUtlBuffer bufFile;
				bool bOK = ReadFileFromPak( GetPakFile(), szRelativeFileName, false, bufFile );
				if ( !bOK )
				{
					Warning( "Failed to load '%s' from lump pak in '%s'.\n", szRelativeFileName, szMapName );
					continue;
				}
				
				// no idea if this is necessary, but not a bad practice to make sure slashes are the same!
				V_FixSlashes( szRelativeFileName, '/' );

				// have to copy extension to a temp buffer, because path fixup below will make pointer invalid
				char szExtension[16] = { 0 };
				V_strcpy_safe( szExtension, V_GetFileExtension( szRelativeFileName ) );
				if ( szExtension[0] )
				{
					// do path fixup for materials and textures that are loaded by bsp name
					if ( !V_strcmp( szExtension, "vtf" ) || !V_strcmp( szExtension, "vmt" ) )
					{
						// we only need to fix up files that are inside materials/maps/<mapname> folder, so disregard anything that isn't that
						char szMaterialsFolder[MAX_PATH];
						V_snprintf( szMaterialsFolder, sizeof( szMaterialsFolder ), "materials/maps/%s/", szMapName );
						int iMaterialsFolderLength = V_strlen( szMaterialsFolder );

						// oh boy...
						if ( !V_strncasecmp( szMaterialsFolder, szRelativeFileName, iMaterialsFolderLength ) )
						{
							// this vtf does indeed live inside a map-named subfolder, fix it!
							if ( !V_strcmp( szExtension, "vtf" ) )
							{
								// read vtf version to see if it will load in game...
								VTFFileBaseHeader_t vtfHeader;
								bufFile.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
								bufFile.Get( &vtfHeader, sizeof( vtfHeader ) );
								if ( !V_strcmp( vtfHeader.fileTypeString, "VTF" ) )
								{
									if ( vtfHeader.version[0] != VTF_MAJOR_VERSION || vtfHeader.version[1] < 0 || vtfHeader.version[1] > VTF_MINOR_VERSION )
										Warning( "Cubemap '%s' has version %d.%d, will not load in game! Rebuild cubemaps manually!\n", szRelativeFileName, vtfHeader.version[0], vtfHeader.version[1] );
								}
							}

							// need to store fixed name in a temporary buffer, or memory will go kaboom
							char szFixedFileName[MAX_PATH];
							V_snprintf( szFixedFileName, sizeof( szFixedFileName ), "materials/maps/%s/%s", g_szOutputFile, &szRelativeFileName[iMaterialsFolderLength] );

							qprintf( "Fixed embedded file path: '%s'\n", szFixedFileName );

							V_strcpy_safe( szRelativeFileName, szFixedFileName );
						}
					}

					// do path fixup for patched materials (vbsp patches some vmts to replace env_cubemap with full path to cubemap file)
					if ( !V_strcmp( szExtension, "vmt" ) )
					{
						bufFile.SetBufferType( true, true );

						// here's hoping this is the only thing vbsp does...
						KeyValues* pkvMaterial = new KeyValues( "patch" );
						if ( pkvMaterial && pkvMaterial->LoadFromBuffer( szRelativeFileName, bufFile ) )
						{
							KeyValues* pkvMaterialReplaceBlock = pkvMaterial->FindKey( "replace" );
							if ( pkvMaterialReplaceBlock )
							{
								const char* pszEnvmapName = pkvMaterialReplaceBlock->GetString( "$envmap", NULL );
								if ( pszEnvmapName )
								{
									// we only need to fix up strings that point inside maps/<mapname> folder, so disregard anything that isn't that
									char szMaterialsFolder[MAX_PATH];
									V_snprintf( szMaterialsFolder, sizeof( szMaterialsFolder ), "maps/%s/", szMapName );
									int iMaterialsFolderLength = V_strlen( szMaterialsFolder );

									// oh boy...
									if ( !V_strncasecmp( szMaterialsFolder, pszEnvmapName, iMaterialsFolderLength ) )
									{
										// this envmap does indeed live inside a map-named subfolder, fix it!
										char szFixedEnvmapPath[MAX_PATH];
										V_snprintf( szFixedEnvmapPath, sizeof( szFixedEnvmapPath ), "maps/%s/%s", g_szOutputFile, &pszEnvmapName[iMaterialsFolderLength] );
										pkvMaterialReplaceBlock->SetString( "$envmap", szFixedEnvmapPath );

										// PiMoN: unfortunately, KV doesn't have any way to save itself to a buffer, so I will have to commit an insane hack:
										// save KV to a temp file first, then load that file to buffer and hope it doesn't shit itself :facepalm:
										if ( pkvMaterial->SaveToFile( g_pFileSystem, "bspconverter_temp.txt", "GAME" ) )
										{
											bufFile.Clear(); // if I don't clear the buffer, it will crash when trying to grow existing buffer...
											if ( g_pFileSystem->ReadFile( "bspconverter_temp.txt", "GAME", bufFile ) )
											{
												g_pFullFileSystem->RemoveFile( "bspconverter_temp.txt", "GAME" );
												qprintf( "Fixed embedded material cubemap patch: '%s'\n", szFixedEnvmapPath );
											}
										}
									}
								}
							}
						}
					}
				}

				AddBufferToPak( pNewPakFile, szRelativeFileName, bufFile.Base(), bufFile.TellMaxPut(), false, IZip::eCompressionType_None );
			}

			// discard old pak in favor of new pak
			SetPakFile( pNewPakFile );
		}

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

