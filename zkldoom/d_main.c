//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//


#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "deh_main.h"
#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "doomfeatures.h"
#include "sounds.h"

#include "d_iwad.h"

#include "z_zone.h"
#include "w_main.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "p_saveg.h"

#include "i_endoom.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"

#include "g_game.h"

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"

#include "p_setup.h"
#include "r_local.h"
#include "statdump.h"

#include "d_main.h"

#include "doomreplay.h"
#include "d_event.h"


//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);
void D_CheckNetGame (void);

// Location where savegames are stored

char *          savegamedir;

// location of IWAD and WAD files

char *          iwadfile;


boolean		devparm;	// started game with -devparm
boolean         nomonsters;	// checkparm of -nomonsters
boolean         respawnparm;	// checkparm of -respawn
boolean         fastparm;	// checkparm of -fast

//extern int soundVolume;
//extern  int	sfxVolume;
//extern  int	musicVolume;

extern  boolean	inhelpscreens;

skill_t		startskill;
int             startepisode;
int		startmap;
boolean		autostart;
int             startloadgame;

boolean		advancedemo;

// Store demo, do not accept any inputs
boolean         storedemo;

// "BFG Edition" version of doom2.wad does not include TITLEPIC.
boolean         bfgedition;

// If true, the main game loop has started.
boolean         main_loop_started = false;

char		wadfile[1024];		// primary wad file
char		mapdir[1024];           // directory of development maps

int             show_endoom = 1;

int REPLAY_EVENTS = 1; // always == 1 in zkldoom


__uint128_t mock_inputs[64];

// BBBBBBBBBBBBBBBBBBBBBBBBBB
__uint128_t mock_input1;
__uint128_t mock_input2;

unsigned int lasttic = 0;

void print_tick_packed_input(__uint128_t v) {
    printf("tics_inputs[%d] = *(__uint128_t*) \"", lasttic++);
    for (int i=0; i<16; i++) {
        unsigned char b = (v >> (8*i)) & 0xff;
        printf("\\x%02x", b);
    } 
    printf("\";\n");
}
void print_128(__uint128_t v) {
    printf("*(__uint128_t*) \"");
    for (int i=0; i<16; i++) {
        unsigned char b = (v >> (8*i)) & 0xff;
        printf("\\x%02x", b);
    } 
    printf("\";\n");
}

unsigned int extract_ticnum(__uint128_t v) {
    unsigned int type_and_ticnum = v >> 96;
    return type_and_ticnum >> 3;
}

unsigned int last_event_idx = 0;

void pop_mocked_event_for_tic(unsigned int ticnum, __uint128_t inputs[]) {
    
    event_t ev = {{'type',0}, {'data1',0}, {'data2',0}, {'data3',0}, {'data4',0}};
    event_t *evt = &ev;

    // BBBBBBBBBBBBBBBBBBBBBBBBBBB
    if (ticnum > 160) {
        return;
    }

    if (ticnum % 40 == 0) {
        unsigned int t = unpack_event(mock_input2, evt);           
    } else {
        unsigned int t = unpack_event(mock_input1, evt);           
    }
    D_PostEvent(evt);
    return;
    // EEEBBBBBBBBBBBBBBBBBBBBBBBBBBB

    /*
    int r = 0; // temp restrict
    unsigned int event_ticnum = 0;
    while (last_event_idx < 64) {
        unsigned int event_ticnum = extract_ticnum(inputs[last_event_idx]);
        
        if (event_ticnum == ticnum) {
            unsigned int t = unpack_event(mock_inputs[last_event_idx], evt);
            //printf("Post: Curtic: %d, last_evt_idx: %d, extrtic: %d\n", ticnum, last_event_idx, event_ticnum);
            D_PostEvent(evt);
            last_event_idx++;
            continue;
        }

        if (event_ticnum < ticnum) {
            //printf("Less: Curtic: %d, last_evt_idx: %d, extrtic: %d\n", ticnum, last_event_idx, event_ticnum);
            last_event_idx++;
            continue;
        }
        if (event_ticnum > ticnum) {
            //printf("More: Curtic: %d, last_evt_idx: %d, extrtic: %d\n", ticnum, last_event_idx, event_ticnum);
            return;
        }
           
    }
    */
    
    return;
}



void D_ProcessEvents(void)
{
    event_t *ev;

    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if (storedemo) return;

    
    // AAAAAAAAAAAAAAAAAAAA
    // here we can pop event from our own inputs (packed with "pack_event" array,
    // where index = current gametic) or capture it from user
    if (REPLAY_EVENTS > 0) {
        pop_mocked_event_for_tic(gametic, mock_inputs);
    }
    while ((ev = D_PopEvent()) != NULL) {
        
        if (REPLAY_EVENTS == 0) {
            __uint128_t v = pack_event(ev, gametic);
            print_tick_packed_input(v);
        }
        
        if (M_Responder(ev)) continue; // menu ate the event
        G_Responder(ev);
    }
}


//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_DEMOSCREEN;
extern  boolean setsizeneeded;
extern  int             showMessages;
void R_ExecuteSetViewSize (void);

void D_Display (void)
{
    static  boolean		viewactivestate = false;
    static  boolean		menuactivestate = false;
    static  boolean		inhelpscreensstate = false;
    static  boolean		fullscreen = false;
    static  gamestate_t		oldgamestate = -1;
    static  int			borderdrawcount;
    int				nowtime;
    int				tics;
    int				wipestart;
    int				y;
    boolean			done;
    boolean			wipe;
    boolean			redrawsbar;

    redrawsbar = false;
    
    wipe = false;
    
    /* AAAAAAAAAAAAAAAAA
    // do buffered drawing
    switch (gamestate)
    {
      case GS_LEVEL:
		if (!gametic)
			break;
		ST_Drawer (viewheight == 200, redrawsbar );
		fullscreen = viewheight == 200;
		break;

    }
    
    // draw buffered stuff to screen
    I_UpdateNoBlit ();
    
    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
    	R_RenderPlayerView (&players[displayplayer]);

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
    	I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
		viewactivestate = false;        // view was not active
		R_FillBackScreen ();    // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320)
    {
		if (menuactive || menuactivestate || !viewactivestate)
			borderdrawcount = 3;
		if (borderdrawcount)
		{
			R_DrawViewBorder ();    // erase old menu stuff
			borderdrawcount--;
		}
    }

    if (testcontrols)
    {
        // Box showing current mouse speed

        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;
    
    // draw pause pic
    if (paused)
    {
		if (automapactive)
			y = 4;
		else
			y = viewwindowy+4;
		V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y,
							  W_CacheLumpName (DEH_String("M_PAUSE"), PU_CACHE));
    }


    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    */

    NetUpdate ();         // send out any new accumulation

    // normal update
    if (!wipe)
    {
    // AAAAAAAAAA - always executed (wipe == false)
    // I_FinishUpdate - without it endless loop with new tics

	I_FinishUpdate ();              // page flip or blit buffer
	return;
    }
    
    // wipe update
    /* AAAAAAAAAAAAAAA
    
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    */
    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
                DR_UpdateTime();
                nowtime = I_GetTime ();
                tics = nowtime - wipestart;
                I_Sleep(1);
        } while (tics <= 0);
                
	wipestart = nowtime;

    /* AAAAAAAAAAAAAAAA
	done = wipe_ScreenWipe(wipe_Melt
			       , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
	I_UpdateNoBlit ();
	M_Drawer ();                            // menu is drawn even on top of wipes
	I_FinishUpdate ();                      // page flip or blit buffer
    */

    } while (!done);
}

//
//  D_DoomLoop
//
void D_DoomLoop (void)
{

    main_loop_started = true;

    TryRunTics();

    I_InitGraphics();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    while (1)
    {
		// frame syncronous IO operations
		I_StartFrame ();

		// AAAAAAAAAA
        DR_ProcessInput ();
		TryRunTics (); // will run at least one tic

		// Update display, next frame, with current state.
		if (screenvisible)
		{
		    D_Display ();
		}
    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
char                    *pagename;


//
// D_PageDrawer
//
void D_PageDrawer (void)
{
    V_DrawPatch (0, 0, W_CacheLumpName(pagename, PU_CACHE));
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
}


//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void)
{
    gamemode = shareware;
}


static void D_AddFile(char *filename)
{
    wad_file_t *handle;

    handle = W_AddFile(filename);

    return;
}


// Initialize the game version
static void InitGameVersion(void)
{
    gameversion = exe_doom_1_9;
}

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void)
{
    byte *endoom;

    endoom = W_CacheLumpName(DEH_String("ENDOOM"), PU_STATIC);

    I_Endoom(endoom);

	//exit(0);
}




//
// D_DoomMain
//
// BBBBBBBBBBBBBBBBBBBBBBBBBBBb
//void D_DoomMain (__uint128_t tics_inputs[64])
void D_DoomMain (__uint128_t input1, __uint128_t input2)
{

     // BBBBBBBBBBBBBBBB
     mock_input1 = input1;
     mock_input2 = input2;

    //    for (int i =0; i< 64; i++) {                                                                                                       
    //        mock_inputs[i] = tics_inputs[i];                                                                                               
    //    }
    
    Z_Init ();

    nomonsters = 0;
    respawnparm = 0;
    fastparm = 0;
    devparm = 0;
	deathmatch = 0;
    
    modifiedgame = false;
    D_AddFile("../doom1.wad");
    // Generate the WAD hash table.  Speed things up a bit.
    // HZ CCCCCCCCCCC check
    //W_GenerateHashTable();

    I_InitTimer();

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    timelimit = 0;

    // Not loading a game
    startloadgame = -1;
    
    // AAAAAAAAA not-needed in zkllvm version
    // R_Init a();

    // AAAAAAAAA not-needed in zkllvm version
    // P_Init ();

    D_CheckNetGame ();

    ST_Init ();

    // [AAAA] autostart
    autostart = 1;
    G_InitNew (startskill, startepisode, startmap);

    D_DoomLoop ();  // never returns
}

