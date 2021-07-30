/*****************************************************************************
The Dark Mod GPL Source Code

This file is part of the The Dark Mod Source Code, originally based
on the Doom 3 GPL Source Code as published in 2011.

The Dark Mod Source Code is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. For details, see LICENSE.TXT.

Project: The Dark Mod (http://www.thedarkmod.com/)

******************************************************************************/

#ifndef __GAME_H__
#define __GAME_H__

#include "../framework/Licensee.h"

/*
===============================================================================

	Public game interface with methods to run the game.

===============================================================================
*/

// default scripts
#define SCRIPT_DEFAULTDEFS			"script/tdm_defs.script"
#define SCRIPT_DEFAULT				"script/tdm_main.script"
#define SCRIPT_DEFAULTFUNC			"tdm_main"

typedef struct {
	char		sessionCommand[MAX_STRING_CHARS];	// "map", "disconnect", "victory", etc
	int			consistencyHash;					// used to check for network game divergence
	int			health;
	int			heartRate;
	int			stamina;
	int			combat;
	bool		syncNextGameFrame;					// used when cinematics are skipped to prevent session from simulating several game frames to
													// keep the game time in sync with real time
} gameReturn_t;

typedef enum {
	ALLOW_YES = 0,
	ALLOW_BADPASS,	// core will prompt for password and connect again
	ALLOW_NOTYET,	// core will wait with transmitted message
	ALLOW_NO		// core will abort with transmitted message
} allowReply_t;

typedef enum {
	ESC_IGNORE = 0,	// do nothing
	ESC_MAIN,		// start main menu GUI
	ESC_GUI			// set an explicit GUI
} escReply_t;

#define TIME_GROUP1		0
#define TIME_GROUP2		1

class idRenderWorld;
class idSoundWorld;
class usercmd_t;
class idUserInterface;

class idGame {
public:
	virtual						~idGame() {}

	// Initialize the game for the first time.
	virtual void				Init( void ) = 0;

	// Shut down the entire game.
	virtual void				Shutdown( void ) = 0;

	// Set the local client number. Distinguishes listen ( == 0 ) / dedicated ( == -1 )
	virtual void				SetLocalClient( int clientNum ) = 0;

	// Sets the user info for a client.
	// if canModify is true, the game can modify the user info in the returned dictionary pointer, server will forward the change back
	// canModify is never true on network client
	virtual const idDict *		SetUserInfo( int clientNum, const idDict &userInfo, bool isClient, bool canModify ) = 0;

	// Retrieve the game's userInfo dict for a client.
	virtual const idDict *		GetUserInfo( int clientNum ) = 0;

	// Sets the serverinfo at map loads and when it changes.
	virtual void				SetServerInfo( const idDict &serverInfo ) = 0;

	// The session calls this before moving the single player game to a new level.
	virtual const idDict &		GetPersistentPlayerInfo( int clientNum ) = 0;

	// The session calls this right before a new level is loaded.
	virtual void				SetPersistentPlayerInfo( int clientNum, const idDict &playerInfo ) = 0;

	// Obsttorte
	virtual idStr				triggeredSave() = 0; 
	virtual bool				savegamesDisallowed() = 0;
	virtual bool				quicksavesDisallowed() = 0;
	// <-- end
	virtual bool				PlayerReady() = 0;	// SteveL #4139. Prevent saving before player has clicked "ready" to start the map

	// Loads a map and spawns all the entities.
	virtual void				InitFromNewMap( const char *mapName, idRenderWorld *renderWorld, idSoundWorld *soundWorld, bool isServer, bool isClient, int randseed ) = 0;

	// Loads a map from a savegame file.
	virtual bool				InitFromSaveGame( const char *mapName, idRenderWorld *renderWorld, idSoundWorld *soundWorld, idFile *saveGameFile ) = 0;

	// Saves the current game state, the session may have written some data to the file already.
	virtual void				SaveGame( idFile *saveGameFile ) = 0;

	// Shut down the current map.
	virtual void				MapShutdown( void ) = 0;

	// Caches media referenced from in key/value pairs in the given dictionary.
	virtual void				CacheDictionaryMedia( const idDict *dict ) = 0;

	// Spawns the player entity to be used by the client.
	virtual void				SpawnPlayer( int clientNum ) = 0;

	// Runs a game frame, may return a session command for level changing, etc
	virtual gameReturn_t		RunFrame( const usercmd_t *clientCmds, int timestepMs = USERCMD_MSEC ) = 0;

	// Makes rendering and sound system calls to display for a given clientNum.
	virtual bool				Draw( int clientNum ) = 0;
	virtual void				DrawLightgem( int clientNum ) = 0;

	// Let the game do it's own UI when ESCAPE is used
	virtual escReply_t			HandleESC( idUserInterface **gui ) = 0;

	// When the game is running it's own UI fullscreen, GUI commands are passed through here
	// return NULL once the fullscreen UI mode should stop, or "main" to go to main menu
	virtual const char *		HandleGuiCommands( const char *menuCommand ) = 0;

	// main menu commands not caught in the engine are passed here
	virtual void				HandleMainMenuCommands( const char *menuCommand, idUserInterface *gui ) = 0;

	// Used to manage divergent time-lines
	virtual void				SelectTimeGroup( int timeGroup ) = 0;
	virtual int					GetTimeGroupTime( int timeGroup ) = 0;

	virtual void				GetBestGameType( const char* map, const char* gametype, char buf[ MAX_STRING_CHARS ] ) = 0;

	virtual void				GetMapLoadingGUI( char gui[ MAX_STRING_CHARS ] ) = 0;

	// Lets the game know after a "reloadimages" command has been invoked
	virtual void				OnReloadImages() = 0;

	// Lets the game know after a "vid_restart" command has been invoked
	virtual void				OnVidRestart() = 0;

	// grayman #3556 - determine whether the player is underwater
	virtual bool				PlayerUnderwater() = 0;
};

extern idGame *					game;


/*
===============================================================================

	Public game interface with methods for in-game editing.

===============================================================================
*/

class idSoundEmitter;
class idSoundShader;

struct refSound_t {
	idSoundEmitter *			referenceSound;	// this is the interface to the sound system, created
												// with idSoundWorld::AllocSoundEmitter() when needed
	idVec3						origin;
	int							listenerId;		// SSF_PRIVATE_SOUND only plays if == listenerId from PlaceListener
												// no spatialization will be performed if == listenerID
	const idSoundShader *		shader;			// this really shouldn't be here, it is a holdover from single channel behavior
	float						diversity;		// 0.0 to 1.0 value used to select which
												// samples in a multi-sample list from the shader are used
	bool						waitfortrigger;	// don't start it at spawn time
	soundShaderParms_t			parms;			// override volume, flags, etc
};

enum {
	TEST_PARTICLE_MODEL = 0,
	TEST_PARTICLE_IMPACT,
	TEST_PARTICLE_MUZZLE,
	TEST_PARTICLE_FLIGHT,
	TEST_PARTICLE_SELECTED
};

class idEntity;
class idMD5Anim;
typedef struct renderLight_s renderLight_t;
typedef struct renderEntity_s renderEntity_t;
class idRenderModel;

// FIXME: this interface needs to be reworked but it properly separates code for the time being
class idGameEdit {
public:
	virtual						~idGameEdit( void ) {}

	// These are the canonical idDict to parameter parsing routines used by both the game and tools.
	virtual void				ParseSpawnArgsToRenderLight( const idDict *args, renderLight_t *renderLight );
	virtual void				ParseSpawnArgsToRenderEntity( const idDict *args, renderEntity_t *renderEntity );
	virtual void				ParseSpawnArgsToRefSound( const idDict *args, refSound_t *refSound );
	virtual void				ParseSpawnArgsToAxis( const idDict *args, idMat3 &axis );

	// Animation system calls for non-game based skeletal rendering.
	virtual idRenderModel *		ANIM_GetModelFromEntityDef( const char *classname );
	virtual const idVec3 		&ANIM_GetModelOffsetFromEntityDef( const char *classname );
	virtual idRenderModel *		ANIM_GetModelFromEntityDef( const idDict *args );
	virtual idRenderModel *		ANIM_GetModelFromName( const char *modelName );
	virtual const idMD5Anim *	ANIM_GetAnimFromEntityDef( const char *classname, const char *animname );
	virtual int					ANIM_GetNumAnimsFromEntityDef( const idDict *args );
	virtual const char *		ANIM_GetAnimNameFromEntityDef( const idDict *args, int animNum );
	virtual const idMD5Anim *	ANIM_GetAnim( const char *fileName );
	virtual int					ANIM_GetLength( const idMD5Anim *anim );
	virtual int					ANIM_GetNumFrames( const idMD5Anim *anim );
	virtual void				ANIM_CreateAnimFrame( const idRenderModel *model, const idMD5Anim *anim, int numJoints, idJointMat *frame, int time, const idVec3 &offset, bool remove_origin_offset );
	virtual idRenderModel *		ANIM_CreateMeshForAnim( idRenderModel *model, const char *classname, const char *animname, int frame, bool remove_origin_offset );

	// Articulated Figure calls for AF editor and Radiant.
	virtual bool				AF_SpawnEntity( const char *fileName );
	virtual void				AF_UpdateEntities( const char *fileName );
	virtual void				AF_UndoChanges( void );
	virtual idRenderModel *		AF_CreateMesh( const idDict &args, idVec3 &meshOrigin, idMat3 &meshAxis, bool &poseIsSet );


	// Entity selection.
	virtual void				ClearEntitySelection( void );
	virtual int					GetSelectedEntities( idEntity *list[], int max );
	virtual void				AddSelectedEntity( idEntity *ent );

	// Selection methods
	virtual void				TriggerSelected();

	//flags passed to SpawnEntityDef function
	enum SpawnEntityDef_Flags {
		sedRespectInhibit = 0x1,
		sedCacheMedia = 0x2,
		sedRespawn = 0x4
	};
	// Entity defs and spawning.
	virtual const idDict *		FindEntityDefDict( const char *name, bool makeDefault = true ) const;
	virtual void				SpawnEntityDef( const idDict &args, idEntity **ent, int flags = 0 );
	virtual idEntity *			FindEntity( const char *name ) const;
	virtual const char *		GetUniqueEntityName( const char *classname ) const;

	// Entity methods.
	virtual void				EntityGetOrigin( idEntity *ent, idVec3 &org ) const;
	virtual void				EntityGetAxis( idEntity *ent, idMat3 &axis ) const;
	virtual void				EntitySetOrigin( idEntity *ent, const idVec3 &org );
	virtual void				EntitySetAxis( idEntity *ent, const idMat3 &axis );
	virtual void				EntityTranslate( idEntity *ent, const idVec3 &org );
	virtual const idDict *		EntityGetSpawnArgs( idEntity *ent ) const;
	virtual void				EntityUpdateChangeableSpawnArgs( idEntity *ent, const idDict *dict );
	virtual void				EntityChangeSpawnArgs( idEntity *ent, const idDict *newArgs );
	virtual void				EntityUpdateVisuals( idEntity *ent );
	virtual void				EntitySetModel( idEntity *ent, const char *val );
	virtual void				EntityStopSound( idEntity *ent );
	virtual void				EntityDelete( idEntity *ent, bool safe = false );
	virtual void				EntitySetColor( idEntity *ent, const idVec3 color );
	virtual void				EntityUpdateLOD( idEntity *ent );
	virtual void				EntityUpdateShaderParms( idEntity *ent );

	// Player methods.
	virtual bool				PlayerIsValid() const;
	virtual void				PlayerGetOrigin( idVec3 &org ) const;
	virtual void				PlayerGetAxis( idMat3 &axis ) const;
	virtual void				PlayerGetViewAngles( idAngles &angles ) const;
	virtual void				PlayerGetEyePosition( idVec3 &org ) const;

	// In game map editing support.
	virtual const idDict *		MapGetEntityDict( const char *name ) const;
	virtual void				MapSave( const char *path = NULL ) const;
	virtual void				MapSetEntityKeyVal( const char *name, const char *key, const char *val ) const ;
	virtual void				MapCopyDictToEntity( const char *name, const idDict *dict ) const;
	virtual int					MapGetUniqueMatchingKeyVals( const char *key, const char *list[], const int max ) const;
	virtual void				MapAddEntity( const idDict *dict ) const;
	virtual int					MapGetEntitiesMatchingClassWithString( const char *classname, const char *match, const char *list[], const int max ) const;
	virtual void				MapRemoveEntity( const char *name ) const;
	virtual void				MapEntityTranslate( const char *name, const idVec3 &v ) const;

};

extern idGameEdit *				gameEdit;


/*
===============================================================================

	Game API.

===============================================================================
*/

class idCmdSystem;
class idRenderSystem;
class idSoundSystem;
class idRenderModelManager;
class idUserInterfaceManager;
class idDeclManager;
class idAASFileManager;
class idCollisionModelManager;

typedef struct {

	int							version;				// API version
	idSys *						sys;					// non-portable system services
	idCommon *					common;					// common
	idCmdSystem *				cmdSystem;				// console command system
	idCVarSystem *				cvarSystem;				// console variable system
	idFileSystem *				fileSystem;				// file system
	idRenderSystem *			renderSystem;			// render system
	idSoundSystem *				soundSystem;			// sound system
	idRenderModelManager *		renderModelManager;		// render model manager
	idUserInterfaceManager *	uiManager;				// user interface manager
	idDeclManager *				declManager;			// declaration manager
	idAASFileManager *			AASFileManager;			// AAS file manager
	idCollisionModelManager *	collisionModelManager;	// collision model manager

} gameImport_t;

typedef struct {

	int							version;				// API version
	idGame *					game;					// interface to run the game
	idGameEdit *				gameEdit;				// interface for in-game editing

} gameExport_t;

extern "C" {
typedef gameExport_t * (*GetGameAPI_t)( gameImport_t *import );
}

#endif /* !__GAME_H__ */