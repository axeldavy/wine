/* Direct Play 2,3,4 Implementation
 *
 * Copyright 1998,1999,2000,2001 - Peter Hunnisett
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winerror.h"
#include "winbase.h"
#include "winnt.h"
#include "winreg.h"
#include "winnls.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#include "dpinit.h"
#include "dplayx_global.h"
#include "name_server.h"
#include "dplayx_queue.h"
#include "wine/dplaysp.h"
#include "dplay_global.h"

WINE_DEFAULT_DEBUG_CHANNEL(dplay);

/* FIXME: Should this be externed? */
extern HRESULT DPL_CreateCompoundAddress
( LPCDPCOMPOUNDADDRESSELEMENT lpElements, DWORD dwElementCount,
  LPVOID lpAddress, LPDWORD lpdwAddressSize, BOOL bAnsiInterface );


/* Local function prototypes */
static lpPlayerList DP_FindPlayer( IDirectPlay2AImpl* This, DPID dpid );
static lpPlayerData DP_CreatePlayer( IDirectPlay2Impl* iface, LPDPID lpid,
                                     LPDPNAME lpName, DWORD dwFlags,
                                     HANDLE hEvent, BOOL bAnsi );
static BOOL DP_CopyDPNAMEStruct( LPDPNAME lpDst, const DPNAME *lpSrc, BOOL bAnsi );
static void DP_SetPlayerData( lpPlayerData lpPData, DWORD dwFlags,
                              LPVOID lpData, DWORD dwDataSize );

static lpGroupData DP_CreateGroup( IDirectPlay2AImpl* iface, const DPID *lpid,
                                   const DPNAME *lpName, DWORD dwFlags,
                                   DPID idParent, BOOL bAnsi );
static void DP_SetGroupData( lpGroupData lpGData, DWORD dwFlags,
                             LPVOID lpData, DWORD dwDataSize );
static void DP_DeleteDPNameStruct( LPDPNAME lpDPName );
static void DP_DeletePlayer( IDirectPlay2Impl* This, DPID dpid );
static BOOL CALLBACK cbDeletePlayerFromAllGroups( DPID dpId,
                                                  DWORD dwPlayerType,
                                                  LPCDPNAME lpName,
                                                  DWORD dwFlags,
                                                  LPVOID lpContext );
static lpGroupData DP_FindAnyGroup( IDirectPlay2AImpl* This, DPID dpid );
static BOOL CALLBACK cbRemoveGroupOrPlayer( DPID dpId, DWORD dwPlayerType,
                                            LPCDPNAME lpName, DWORD dwFlags,
                                            LPVOID lpContext );
static void DP_DeleteGroup( IDirectPlay2Impl* This, DPID dpid );

/* Helper methods for player/group interfaces */
static HRESULT DP_IF_CreatePlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, LPDPID lpidPlayer,
            LPDPNAME lpPlayerName, HANDLE hEvent, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_DestroyGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup, BOOL bAnsi );
static HRESULT DP_IF_DestroyPlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idPlayer, BOOL bAnsi );
static HRESULT DP_IF_GetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_GetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_SetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_SetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_AddGroupToGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup );
static HRESULT DP_IF_CreateGroup
          ( IDirectPlay2AImpl* This, LPVOID lpMsgHdr, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_CreateGroupInGroup
          ( IDirectPlay3Impl* This, LPVOID lpMsgHdr, DPID idParentGroup,
            LPDPID lpidGroup, LPDPNAME lpGroupName, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_AddPlayerToGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi );
static HRESULT DP_IF_DeleteGroupFromGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup );
static HRESULT DP_SetSessionDesc
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpSessDesc,
            DWORD dwFlags, BOOL bInitial, BOOL bAnsi  );
static HRESULT DP_SecureOpen
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials,
            BOOL bAnsi );
static HRESULT DP_SendEx
          ( IDirectPlay2Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID, BOOL bAnsi );
static HRESULT DP_IF_Receive
          ( IDirectPlay2Impl* This, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize, BOOL bAnsi );
static HRESULT DP_IF_GetMessageQueue
          ( IDirectPlay4Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes, BOOL bAnsi );
static HRESULT DP_SP_SendEx
          ( IDirectPlay2Impl* This, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID );
static HRESULT DP_IF_CancelMessage
          ( IDirectPlay4Impl* This, DWORD dwMsgID, DWORD dwFlags,
            DWORD dwMinPriority, DWORD dwMaxPriority, BOOL bAnsi );
static HRESULT DP_IF_EnumGroupsInGroup
          ( IDirectPlay3AImpl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_GetGroupParent
          ( IDirectPlay3AImpl* This, DPID idGroup, LPDPID lpidGroup,
            BOOL bAnsi );
static HRESULT DP_IF_EnumSessions
          ( IDirectPlay2Impl* This, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi );
static HRESULT DP_IF_InitializeConnection
          ( IDirectPlay3Impl* This, LPVOID lpConnection, DWORD dwFlags, BOOL bAnsi );
static BOOL CALLBACK cbDPCreateEnumConnections( LPCGUID lpguidSP,
    LPVOID lpConnection, DWORD dwConnectionSize, LPCDPNAME lpName,
    DWORD dwFlags, LPVOID lpContext );
static BOOL DP_BuildSPCompoundAddr( LPGUID lpcSpGuid, LPVOID* lplpAddrBuf,
                                    LPDWORD lpdwBufSize );



static inline DPID DP_NextObjectId(void);
static DPID DP_GetRemoteNextObjectId(void);

static DWORD DP_CalcSessionDescSize( LPCDPSESSIONDESC2 lpSessDesc, BOOL bAnsi );
static void DP_CopySessionDesc( LPDPSESSIONDESC2 destSessionDesc,
                                LPCDPSESSIONDESC2 srcSessDesc, BOOL bAnsi );


static HMODULE DP_LoadSP( LPCGUID lpcGuid, LPSPINITDATA lpSpData, LPBOOL lpbIsDpSp );
static HRESULT DP_InitializeDPSP( IDirectPlay3Impl* This, HMODULE hServiceProvider );
static HRESULT DP_InitializeDPLSP( IDirectPlay3Impl* This, HMODULE hServiceProvider );






#define DPID_NOPARENT_GROUP 0 /* Magic number to indicate no parent of group */
#define DPID_SYSTEM_GROUP DPID_NOPARENT_GROUP /* If system group is supported
                                                 we don't have to change much */
#define DPID_NAME_SERVER 0x19a9d65b  /* Don't ask me why */

/* Strip out dwFlag values which cannot be sent in the CREATEGROUP msg */
#define DPMSG_CREATEGROUP_DWFLAGS(x) ( (x) & DPGROUP_HIDDEN )

/* Strip out all dwFlags values for CREATEPLAYER msg */
#define DPMSG_CREATEPLAYER_DWFLAGS(x) 0

static LONG kludgePlayerGroupId = 1000;


static inline IDirectPlayImpl *impl_from_IDirectPlay4A( IDirectPlay4A *iface )
{
    return CONTAINING_RECORD( iface, IDirectPlayImpl, IDirectPlay4A_iface );
}

static inline IDirectPlayImpl *impl_from_IDirectPlay4( IDirectPlay4 *iface )
{
    return CONTAINING_RECORD( iface, IDirectPlayImpl, IDirectPlay4_iface );
}

static BOOL DP_CreateDirectPlay2( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  This->dp2 = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->dp2) ) );
  if ( This->dp2 == NULL )
  {
    return FALSE;
  }

  This->dp2->bConnectionOpen = FALSE;

  This->dp2->hEnumSessionThread = INVALID_HANDLE_VALUE;
  This->dp2->dwEnumSessionLock = 0;

  This->dp2->bHostInterface = FALSE;

  DPQ_INIT(This->dp2->receiveMsgs);
  DPQ_INIT(This->dp2->sendMsgs);
  DPQ_INIT(This->dp2->repliesExpected);

  if( !NS_InitializeSessionCache( &This->dp2->lpNameServerData ) )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  /* Provide an initial session desc with nothing in it */
  This->dp2->lpSessionDesc = HeapAlloc( GetProcessHeap(),
                                        HEAP_ZERO_MEMORY,
                                        sizeof( *This->dp2->lpSessionDesc ) );
  if( This->dp2->lpSessionDesc == NULL )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }
  This->dp2->lpSessionDesc->dwSize = sizeof( *This->dp2->lpSessionDesc );

  /* We are emulating a dp 6 implementation */
  This->dp2->spData.dwSPVersion = DPSP_MAJORVERSION;

  This->dp2->spData.lpCB = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      sizeof( *This->dp2->spData.lpCB ) );
  This->dp2->spData.lpCB->dwSize = sizeof( *This->dp2->spData.lpCB );
  This->dp2->spData.lpCB->dwVersion = DPSP_MAJORVERSION;

  /* This is the pointer to the service provider */
  if( FAILED( DPSP_CreateInterface( &IID_IDirectPlaySP,
                                    (LPVOID*)&This->dp2->spData.lpISP, This ) )
    )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  /* Setup lobby provider information */
  This->dp2->dplspData.dwSPVersion = DPSP_MAJORVERSION;
  This->dp2->dplspData.lpCB = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof( *This->dp2->dplspData.lpCB ) );
  This->dp2->dplspData.lpCB->dwSize = sizeof(  *This->dp2->dplspData.lpCB );

  if( FAILED( DPLSP_CreateInterface( &IID_IDPLobbySP,
                                     (LPVOID*)&This->dp2->dplspData.lpISP, This ) )
    )
  {
    /* FIXME: Memory leak */
    return FALSE;
  }

  return TRUE;
}

/* Definition of the global function in dplayx_queue.h. #
 * FIXME: Would it be better to have a dplayx_queue.c for this function? */
DPQ_DECL_DELETECB( cbDeleteElemFromHeap, LPVOID )
{
  HeapFree( GetProcessHeap(), 0, elem );
}

static BOOL DP_DestroyDirectPlay2( LPVOID lpDP )
{
  IDirectPlay2AImpl *This = lpDP;

  if( This->dp2->hEnumSessionThread != INVALID_HANDLE_VALUE )
  {
    TerminateThread( This->dp2->hEnumSessionThread, 0 );
    CloseHandle( This->dp2->hEnumSessionThread );
  }

  /* Finish with the SP - have it shutdown */
  if( This->dp2->spData.lpCB->ShutdownEx )
  {
    DPSP_SHUTDOWNDATA data;

    TRACE( "Calling SP ShutdownEx\n" );

    data.lpISP = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->ShutdownEx)( &data );
  }
  else if (This->dp2->spData.lpCB->Shutdown ) /* obsolete interface */
  {
    TRACE( "Calling obsolete SP Shutdown\n" );
    (*This->dp2->spData.lpCB->Shutdown)();
  }

  /* Unload the SP (if it exists) */
  if( This->dp2->hServiceProvider != 0 )
  {
    FreeLibrary( This->dp2->hServiceProvider );
  }

  /* Unload the Lobby Provider (if it exists) */
  if( This->dp2->hDPLobbyProvider != 0 )
  {
    FreeLibrary( This->dp2->hDPLobbyProvider );
  }

  /* FIXME: Need to delete receive and send msgs queue contents */

  NS_DeleteSessionCache( This->dp2->lpNameServerData );

  HeapFree( GetProcessHeap(), 0, This->dp2->lpSessionDesc );

  IDirectPlaySP_Release( This->dp2->spData.lpISP );

  /* Delete the contents */
  HeapFree( GetProcessHeap(), 0, This->dp2 );

  return TRUE;
}

static void dplay_destroy(IDirectPlayImpl *obj)
{
     DP_DestroyDirectPlay2( obj );
     obj->lock.DebugInfo->Spare[0] = 0;
     DeleteCriticalSection( &obj->lock );
     HeapFree( GetProcessHeap(), 0, obj );
}

static inline DPID DP_NextObjectId(void)
{
  return (DPID)InterlockedIncrement( &kludgePlayerGroupId );
}

/* *lplpReply will be non NULL iff there is something to reply */
HRESULT DP_HandleMessage( IDirectPlay2Impl* This, LPCVOID lpcMessageBody,
                          DWORD  dwMessageBodySize, LPCVOID lpcMessageHeader,
                          WORD wCommandId, WORD wVersion,
                          LPVOID* lplpReply, LPDWORD lpdwMsgSize )
{
  TRACE( "(%p)->(%p,0x%08x,%p,%u,%u)\n",
         This, lpcMessageBody, dwMessageBodySize, lpcMessageHeader, wCommandId,
         wVersion );

  switch( wCommandId )
  {
    /* Name server needs to handle this request */
    case DPMSGCMD_ENUMSESSIONSREQUEST:
      /* Reply expected */
      NS_ReplyToEnumSessionsRequest( lpcMessageBody, lplpReply, lpdwMsgSize, This );
      break;

    /* Name server needs to handle this request */
    case DPMSGCMD_ENUMSESSIONSREPLY:
      /* No reply expected */
      NS_AddRemoteComputerAsNameServer( lpcMessageHeader,
                                        This->dp2->spData.dwSPHeaderSize,
                                        lpcMessageBody,
                                        This->dp2->lpNameServerData );
      break;

    case DPMSGCMD_REQUESTNEWPLAYERID:
    {
      LPCDPMSG_REQUESTNEWPLAYERID lpcMsg = lpcMessageBody;

      LPDPMSG_NEWPLAYERIDREPLY lpReply;

      *lpdwMsgSize = This->dp2->spData.dwSPHeaderSize + sizeof( *lpReply );

      *lplpReply = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, *lpdwMsgSize );

      FIXME( "Ignoring dwFlags 0x%08x in request msg\n",
             lpcMsg->dwFlags );

      /* Setup the reply */
      lpReply = (LPDPMSG_NEWPLAYERIDREPLY)( (BYTE*)(*lplpReply) +
                                            This->dp2->spData.dwSPHeaderSize );

      lpReply->envelope.dwMagic    = DPMSGMAGIC_DPLAYMSG;
      lpReply->envelope.wCommandId = DPMSGCMD_NEWPLAYERIDREPLY;
      lpReply->envelope.wVersion   = DPMSGVER_DP6;

      lpReply->dpidNewPlayerId = DP_NextObjectId();

      TRACE( "Allocating new playerid 0x%08x from remote request\n",
             lpReply->dpidNewPlayerId );
      break;
    }

    case DPMSGCMD_GETNAMETABLEREPLY:
    case DPMSGCMD_NEWPLAYERIDREPLY:
#if 0
      if( wCommandId == DPMSGCMD_NEWPLAYERIDREPLY )
        DebugBreak();
#endif
      DP_MSG_ReplyReceived( This, wCommandId, lpcMessageBody, dwMessageBodySize );
      break;

#if 1
    case DPMSGCMD_JUSTENVELOPE:
      TRACE( "GOT THE SELF MESSAGE: %p -> 0x%08x\n", lpcMessageHeader, ((const DWORD *)lpcMessageHeader)[1] );
      NS_SetLocalAddr( This->dp2->lpNameServerData, lpcMessageHeader, 20 );
      DP_MSG_ReplyReceived( This, wCommandId, lpcMessageBody, dwMessageBodySize );
#endif

    case DPMSGCMD_FORWARDADDPLAYER:
#if 0
      DebugBreak();
#endif
#if 1
      TRACE( "Sending message to self to get my addr\n" );
      DP_MSG_ToSelf( This, 1 ); /* This is a hack right now */
#endif
      break;

    case DPMSGCMD_FORWARDADDPLAYERNACK:
      DP_MSG_ErrorReceived( This, wCommandId, lpcMessageBody, dwMessageBodySize );
      break;

    default:
      FIXME( "Unknown wCommandId %u. Ignoring message\n", wCommandId );
      DebugBreak();
      break;
  }

  /* FIXME: There is code in dplaysp.c to handle dplay commands. Move to here. */

  return DP_OK;
}


static HRESULT DP_IF_AddPlayerToGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup,
            DPID idPlayer, BOOL bAnsi )
{
  lpGroupData  lpGData;
  lpPlayerList lpPList;
  lpPlayerList lpNewPList;

  TRACE( "(%p)->(%p,0x%08x,0x%08x,%u)\n",
         This, lpMsgHdr, idGroup, idPlayer, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Find the player */
  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* Create a player list (ie "shortcut" ) */
  lpNewPList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpNewPList ) );
  if( lpNewPList == NULL )
  {
    return DPERR_CANTADDPLAYER;
  }

  /* Add the shortcut */
  lpPList->lpPData->uRef++;
  lpNewPList->lpPData = lpPList->lpPData;

  /* Add the player to the list of players for this group */
  DPQ_INSERT(lpGData->players,lpNewPList,players);

  /* Let the SP know that we've added a player to the group */
  if( This->dp2->spData.lpCB->AddPlayerToGroup )
  {
    DPSP_ADDPLAYERTOGROUPDATA data;

    TRACE( "Calling SP AddPlayerToGroup\n" );

    data.idPlayer = idPlayer;
    data.idGroup  = idGroup;
    data.lpISP    = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->AddPlayerToGroup)( &data );
  }

  /* Inform all other peers of the addition of player to the group. If there are
   * no peers keep this event quiet.
   * Also, if this event was the result of another machine sending it to us,
   * don't bother rebroadcasting it.
   */
  if( ( lpMsgHdr == NULL ) &&
      This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_ADDPLAYERTOGROUP msg;
    msg.dwType = DPSYS_ADDPLAYERTOGROUP;

    msg.dpIdGroup  = idGroup;
    msg.dpIdPlayer = idPlayer;

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( This, DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_QueryInterface( IDirectPlay4A *iface, REFIID riid,
        void **ppv )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_QueryInterface( &This->IDirectPlay4_iface, riid, ppv );
}

static HRESULT WINAPI IDirectPlay4Impl_QueryInterface( IDirectPlay4 *iface, REFIID riid,
        void **ppv )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );

    if ( IsEqualGUID( &IID_IUnknown, riid ) )
    {
        TRACE( "(%p)->(IID_IUnknown %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4A_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay2A, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay2A %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4A_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay2, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay2 %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay3A, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay3A %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4A_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay3, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay3 %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay4A, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay4A %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4A_iface;
    }
    else if ( IsEqualGUID( &IID_IDirectPlay4, riid ) )
    {
        TRACE( "(%p)->(IID_IDirectPlay4 %p)\n", This, ppv );
        *ppv = &This->IDirectPlay4_iface;
    }
    else
    {
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI IDirectPlay4AImpl_AddRef(IDirectPlay4A *iface)
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    ULONG ref = InterlockedIncrement( &This->ref4A );

    TRACE( "(%p) ref4A=%d\n", This, ref );

    if ( ref == 1 )
        InterlockedIncrement( &This->numIfaces );

    return ref;
}

static ULONG WINAPI IDirectPlay4Impl_AddRef(IDirectPlay4 *iface)
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    ULONG ref = InterlockedIncrement( &This->ref4 );

    TRACE( "(%p) ref4=%d\n", This, ref );

    if ( ref == 1 )
        InterlockedIncrement( &This->numIfaces );

    return ref;
}

static ULONG WINAPI IDirectPlay4AImpl_Release(IDirectPlay4A *iface)
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    ULONG ref = InterlockedDecrement( &This->ref4A );

    TRACE( "(%p) ref4A=%d\n", This, ref );

    if ( !ref && !InterlockedDecrement( &This->numIfaces ) )
        dplay_destroy( This );

    return ref;
}

static ULONG WINAPI IDirectPlay4Impl_Release(IDirectPlay4 *iface)
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    ULONG ref = InterlockedDecrement( &This->ref4 );

    TRACE( "(%p) ref4=%d\n", This, ref );

    if ( !ref && !InterlockedDecrement( &This->numIfaces ) )
        dplay_destroy( This );

    return ref;
}

static HRESULT WINAPI IDirectPlay4AImpl_AddPlayerToGroup( IDirectPlay4A *iface, DPID idGroup,
        DPID idPlayer )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_AddPlayerToGroup( This, NULL, idGroup, idPlayer, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_AddPlayerToGroup
          ( LPDIRECTPLAY2 iface, DPID idGroup, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_AddPlayerToGroup( This, NULL, idGroup, idPlayer, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_Close( IDirectPlay4A *iface )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_Close( &This->IDirectPlay4_iface);
}

static HRESULT WINAPI IDirectPlay4Impl_Close( IDirectPlay4 *iface )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    HRESULT hr = DP_OK;

    TRACE( "(%p)", This );

    /* FIXME: Need to find a new host I assume (how?) */
    /* FIXME: Need to destroy all local groups */
    /* FIXME: Need to migrate all remotely visible players to the new host */

    /* Invoke the SP callback to inform of session close */
    if( This->dp2->spData.lpCB->CloseEx )
    {
        DPSP_CLOSEDATA data;

        TRACE( "Calling SP CloseEx\n" );
        data.lpISP = This->dp2->spData.lpISP;
        hr = (*This->dp2->spData.lpCB->CloseEx)( &data );
    }
    else if ( This->dp2->spData.lpCB->Close ) /* Try obsolete version */
    {
        TRACE( "Calling SP Close (obsolete interface)\n" );
        hr = (*This->dp2->spData.lpCB->Close)();
    }

    return hr;
}

static
lpGroupData DP_CreateGroup( IDirectPlay2AImpl* This, const DPID *lpid,
                            const DPNAME *lpName, DWORD dwFlags,
                            DPID idParent, BOOL bAnsi )
{
  lpGroupData lpGData;

  /* Allocate the new space and add to end of high level group list */
  lpGData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGData ) );

  if( lpGData == NULL )
  {
    return NULL;
  }

  DPQ_INIT(lpGData->groups);
  DPQ_INIT(lpGData->players);

  /* Set the desired player ID - no sanity checking to see if it exists */
  lpGData->dpid = *lpid;

  DP_CopyDPNAMEStruct( &lpGData->name, lpName, bAnsi );

  /* FIXME: Should we check that the parent exists? */
  lpGData->parent  = idParent;

  /* FIXME: Should we validate the dwFlags? */
  lpGData->dwFlags = dwFlags;

  TRACE( "Created group id 0x%08x\n", *lpid );

  return lpGData;
}

/* This method assumes that all links to it are already deleted */
static void
DP_DeleteGroup( IDirectPlay2Impl* This, DPID dpid )
{
  lpGroupList lpGList;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  DPQ_REMOVE_ENTRY( This->dp2->lpSysGroup->groups, groups, lpGData->dpid, ==, dpid, lpGList );

  if( lpGList == NULL )
  {
    ERR( "DPID 0x%08x not found\n", dpid );
    return;
  }

  if( --(lpGList->lpGData->uRef) )
  {
    FIXME( "Why is this not the last reference to group?\n" );
    DebugBreak();
  }

  /* Delete player */
  DP_DeleteDPNameStruct( &lpGList->lpGData->name );
  HeapFree( GetProcessHeap(), 0, lpGList->lpGData );

  /* Remove and Delete Player List object */
  HeapFree( GetProcessHeap(), 0, lpGList );

}

static lpGroupData DP_FindAnyGroup( IDirectPlay2AImpl* This, DPID dpid )
{
  lpGroupList lpGroups;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  if( dpid == DPID_SYSTEM_GROUP )
  {
    return This->dp2->lpSysGroup;
  }
  else
  {
    DPQ_FIND_ENTRY( This->dp2->lpSysGroup->groups, groups, lpGData->dpid, ==, dpid, lpGroups );
  }

  if( lpGroups == NULL )
  {
    return NULL;
  }

  return lpGroups->lpGData;
}

static HRESULT DP_IF_CreateGroup
          ( IDirectPlay2AImpl* This, LPVOID lpMsgHdr, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE( "(%p)->(%p,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, lpMsgHdr, lpidGroup, lpGroupName, lpData, dwDataSize,
         dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* If the name is not specified, we must provide one */
  if( DPID_UNKNOWN == *lpidGroup )
  {
    /* If we are the name server, we decide on the group ids. If not, we
     * must ask for one before attempting a creation.
     */
    if( This->dp2->bHostInterface )
    {
      *lpidGroup = DP_NextObjectId();
    }
    else
    {
      *lpidGroup = DP_GetRemoteNextObjectId();
    }
  }

  lpGData = DP_CreateGroup( This, lpidGroup, lpGroupName, dwFlags,
                            DPID_NOPARENT_GROUP, bAnsi );

  if( lpGData == NULL )
  {
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  if( DPID_SYSTEM_GROUP == *lpidGroup )
  {
    This->dp2->lpSysGroup = lpGData;
    TRACE( "Inserting system group\n" );
  }
  else
  {
    /* Insert into the system group */
    lpGroupList lpGroup = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGroup ) );
    lpGroup->lpGData = lpGData;

    DPQ_INSERT( This->dp2->lpSysGroup->groups, lpGroup, groups );
  }

  /* Something is now referencing this data */
  lpGData->uRef++;

  /* Set all the important stuff for the group */
  DP_SetGroupData( lpGData, DPSET_REMOTE, lpData, dwDataSize );

  /* FIXME: We should only create the system group if GetCaps returns
   *        DPCAPS_GROUPOPTIMIZED.
   */

  /* Let the SP know that we've created this group */
  if( This->dp2->spData.lpCB->CreateGroup )
  {
    DPSP_CREATEGROUPDATA data;
    DWORD dwCreateFlags = 0;

    TRACE( "Calling SP CreateGroup\n" );

    if( *lpidGroup == DPID_NOPARENT_GROUP )
      dwCreateFlags |= DPLAYI_GROUP_SYSGROUP;

    if( lpMsgHdr == NULL )
      dwCreateFlags |= DPLAYI_PLAYER_PLAYERLOCAL;

    if( dwFlags & DPGROUP_HIDDEN )
      dwCreateFlags |= DPLAYI_GROUP_HIDDEN;

    data.idGroup           = *lpidGroup;
    data.dwFlags           = dwCreateFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->CreateGroup)( &data );
  }

  /* Inform all other peers of the creation of a new group. If there are
   * no peers keep this event quiet.
   * Also if this message was sent to us, don't rebroadcast.
   */
  if( ( lpMsgHdr == NULL ) &&
      This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_CREATEPLAYERORGROUP msg;
    msg.dwType = DPSYS_CREATEPLAYERORGROUP;

    msg.dwPlayerType     = DPPLAYERTYPE_GROUP;
    msg.dpId             = *lpidGroup;
    msg.dwCurrentPlayers = 0; /* FIXME: Incorrect? */
    msg.lpData           = lpData;
    msg.dwDataSize       = dwDataSize;
    msg.dpnName          = *lpGroupName;
    msg.dpIdParent       = DPID_NOPARENT_GROUP;
    msg.dwFlags          = DPMSG_CREATEGROUP_DWFLAGS( dwFlags );

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( This, DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),
               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_CreateGroup( IDirectPlay4A *iface, DPID *lpidGroup,
        DPNAME *lpGroupName, void *lpData, DWORD dwDataSize, DWORD dwFlags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

    *lpidGroup = DPID_UNKNOWN;

    return DP_IF_CreateGroup( This, NULL, lpidGroup, lpGroupName, lpData, dwDataSize, dwFlags,
            TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_CreateGroup
          ( LPDIRECTPLAY2 iface, LPDPID lpidGroup, LPDPNAME lpGroupName,
            LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroup( (IDirectPlay2AImpl*)iface, NULL, lpidGroup,
                            lpGroupName, lpData, dwDataSize, dwFlags, FALSE );
}


static void
DP_SetGroupData( lpGroupData lpGData, DWORD dwFlags,
                 LPVOID lpData, DWORD dwDataSize )
{
  /* Clear out the data with this player */
  if( dwFlags & DPSET_LOCAL )
  {
    if ( lpGData->dwLocalDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpGData->lpLocalData );
      lpGData->lpLocalData = NULL;
      lpGData->dwLocalDataSize = 0;
    }
  }
  else
  {
    if( lpGData->dwRemoteDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpGData->lpRemoteData );
      lpGData->lpRemoteData = NULL;
      lpGData->dwRemoteDataSize = 0;
    }
  }

  /* Reallocate for new data */
  if( lpData != NULL )
  {
    if( dwFlags & DPSET_LOCAL )
    {
      lpGData->lpLocalData     = lpData;
      lpGData->dwLocalDataSize = dwDataSize;
    }
    else
    {
      lpGData->lpRemoteData = HeapAlloc( GetProcessHeap(), 0, dwDataSize );
      CopyMemory( lpGData->lpRemoteData, lpData, dwDataSize );
      lpGData->dwRemoteDataSize = dwDataSize;
    }
  }

}

/* This function will just create the storage for the new player.  */
static
lpPlayerData DP_CreatePlayer( IDirectPlay2Impl* This, LPDPID lpid,
                              LPDPNAME lpName, DWORD dwFlags,
                              HANDLE hEvent, BOOL bAnsi )
{
  lpPlayerData lpPData;

  TRACE( "(%p)->(%p,%p,%u)\n", This, lpid, lpName, bAnsi );

  /* Allocate the storage for the player and associate it with list element */
  lpPData = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpPData ) );
  if( lpPData == NULL )
  {
    return NULL;
  }

  /* Set the desired player ID */
  lpPData->dpid = *lpid;

  DP_CopyDPNAMEStruct( &lpPData->name, lpName, bAnsi );

  lpPData->dwFlags = dwFlags;

  /* If we were given an event handle, duplicate it */
  if( hEvent != 0 )
  {
    if( !DuplicateHandle( GetCurrentProcess(), hEvent,
                          GetCurrentProcess(), &lpPData->hEvent,
                          0, FALSE, DUPLICATE_SAME_ACCESS )
      )
    {
      /* FIXME: Memory leak */
      ERR( "Can't duplicate player msg handle %p\n", hEvent );
    }
  }

  /* Initialize the SP data section */
  lpPData->lpSPPlayerData = DPSP_CreateSPPlayerData();

  TRACE( "Created player id 0x%08x\n", *lpid );

  if( ~dwFlags & DPLAYI_PLAYER_SYSPLAYER )
    This->dp2->lpSessionDesc->dwCurrentPlayers++;

  return lpPData;
}

/* Delete the contents of the DPNAME struct */
static void
DP_DeleteDPNameStruct( LPDPNAME lpDPName )
{
  HeapFree( GetProcessHeap(), HEAP_ZERO_MEMORY, lpDPName->u1.lpszShortNameA );
  HeapFree( GetProcessHeap(), HEAP_ZERO_MEMORY, lpDPName->u2.lpszLongNameA );
}

/* This method assumes that all links to it are already deleted */
static void
DP_DeletePlayer( IDirectPlay2Impl* This, DPID dpid )
{
  lpPlayerList lpPList;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  DPQ_REMOVE_ENTRY( This->dp2->lpSysGroup->players, players, lpPData->dpid, ==, dpid, lpPList );

  if( lpPList == NULL )
  {
    ERR( "DPID 0x%08x not found\n", dpid );
    return;
  }

  /* Verify that this is the last reference to the data */
  if( --(lpPList->lpPData->uRef) )
  {
    FIXME( "Why is this not the last reference to player?\n" );
    DebugBreak();
  }

  /* Delete player */
  DP_DeleteDPNameStruct( &lpPList->lpPData->name );

  CloseHandle( lpPList->lpPData->hEvent );
  HeapFree( GetProcessHeap(), 0, lpPList->lpPData );

  /* Delete Player List object */
  HeapFree( GetProcessHeap(), 0, lpPList );
}

static lpPlayerList DP_FindPlayer( IDirectPlay2AImpl* This, DPID dpid )
{
  lpPlayerList lpPlayers;

  TRACE( "(%p)->(0x%08x)\n", This, dpid );

  if(This->dp2->lpSysGroup == NULL)
    return NULL;

  DPQ_FIND_ENTRY( This->dp2->lpSysGroup->players, players, lpPData->dpid, ==, dpid, lpPlayers );

  return lpPlayers;
}

/* Basic area for Dst must already be allocated */
static BOOL DP_CopyDPNAMEStruct( LPDPNAME lpDst, const DPNAME *lpSrc, BOOL bAnsi )
{
  if( lpSrc == NULL )
  {
    ZeroMemory( lpDst, sizeof( *lpDst ) );
    lpDst->dwSize = sizeof( *lpDst );
    return TRUE;
  }

  if( lpSrc->dwSize != sizeof( *lpSrc) )
  {
    return FALSE;
  }

  /* Delete any existing pointers */
  HeapFree( GetProcessHeap(), 0, lpDst->u1.lpszShortNameA );
  HeapFree( GetProcessHeap(), 0, lpDst->u2.lpszLongNameA );

  /* Copy as required */
  CopyMemory( lpDst, lpSrc, lpSrc->dwSize );

  if( bAnsi )
  {
    if( lpSrc->u1.lpszShortNameA )
    {
        lpDst->u1.lpszShortNameA = HeapAlloc( GetProcessHeap(), 0,
                                             strlen(lpSrc->u1.lpszShortNameA)+1 );
        strcpy( lpDst->u1.lpszShortNameA, lpSrc->u1.lpszShortNameA );
    }
    if( lpSrc->u2.lpszLongNameA )
    {
        lpDst->u2.lpszLongNameA = HeapAlloc( GetProcessHeap(), 0,
                                              strlen(lpSrc->u2.lpszLongNameA)+1 );
        strcpy( lpDst->u2.lpszLongNameA, lpSrc->u2.lpszLongNameA );
    }
  }
  else
  {
    if( lpSrc->u1.lpszShortNameA )
    {
        lpDst->u1.lpszShortName = HeapAlloc( GetProcessHeap(), 0,
                                              (strlenW(lpSrc->u1.lpszShortName)+1)*sizeof(WCHAR) );
        strcpyW( lpDst->u1.lpszShortName, lpSrc->u1.lpszShortName );
    }
    if( lpSrc->u2.lpszLongNameA )
    {
        lpDst->u2.lpszLongName = HeapAlloc( GetProcessHeap(), 0,
                                             (strlenW(lpSrc->u2.lpszLongName)+1)*sizeof(WCHAR) );
        strcpyW( lpDst->u2.lpszLongName, lpSrc->u2.lpszLongName );
    }
  }

  return TRUE;
}

static void
DP_SetPlayerData( lpPlayerData lpPData, DWORD dwFlags,
                  LPVOID lpData, DWORD dwDataSize )
{
  /* Clear out the data with this player */
  if( dwFlags & DPSET_LOCAL )
  {
    if ( lpPData->dwLocalDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpPData->lpLocalData );
      lpPData->lpLocalData = NULL;
      lpPData->dwLocalDataSize = 0;
    }
  }
  else
  {
    if( lpPData->dwRemoteDataSize != 0 )
    {
      HeapFree( GetProcessHeap(), 0, lpPData->lpRemoteData );
      lpPData->lpRemoteData = NULL;
      lpPData->dwRemoteDataSize = 0;
    }
  }

  /* Reallocate for new data */
  if( lpData != NULL )
  {

    if( dwFlags & DPSET_LOCAL )
    {
      lpPData->lpLocalData     = lpData;
      lpPData->dwLocalDataSize = dwDataSize;
    }
    else
    {
      lpPData->lpRemoteData = HeapAlloc( GetProcessHeap(), 0, dwDataSize );
      CopyMemory( lpPData->lpRemoteData, lpData, dwDataSize );
      lpPData->dwRemoteDataSize = dwDataSize;
    }
  }

}

static HRESULT DP_IF_CreatePlayer
( IDirectPlay2Impl* This,
  LPVOID lpMsgHdr, /* NULL for local creation, non NULL for remote creation */
  LPDPID lpidPlayer,
  LPDPNAME lpPlayerName,
  HANDLE hEvent,
  LPVOID lpData,
  DWORD dwDataSize,
  DWORD dwFlags,
  BOOL bAnsi )
{
  HRESULT hr = DP_OK;
  lpPlayerData lpPData;
  lpPlayerList lpPList;
  DWORD dwCreateFlags = 0;

  TRACE( "(%p)->(%p,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, lpidPlayer, lpPlayerName, hEvent, lpData,
         dwDataSize, dwFlags, bAnsi );
  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( dwFlags == 0 )
  {
    dwFlags = DPPLAYER_SPECTATOR;
  }

  if( lpidPlayer == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }


  /* Determine the creation flags for the player. These will be passed
   * to the name server if requesting a player id and to the SP when
   * informing it of the player creation
   */
  {
    if( dwFlags & DPPLAYER_SERVERPLAYER )
    {
      if( *lpidPlayer == DPID_SERVERPLAYER )
      {
        /* Server player for the host interface */
        dwCreateFlags |= DPLAYI_PLAYER_APPSERVER;
      }
      else if( *lpidPlayer == DPID_NAME_SERVER )
      {
        /* Name server - master of everything */
        dwCreateFlags |= (DPLAYI_PLAYER_NAMESRVR|DPLAYI_PLAYER_SYSPLAYER);
      }
      else
      {
        /* Server player for a non host interface */
        dwCreateFlags |= DPLAYI_PLAYER_SYSPLAYER;
      }
    }

    if( lpMsgHdr == NULL )
      dwCreateFlags |= DPLAYI_PLAYER_PLAYERLOCAL;
  }

  /* Verify we know how to handle all the flags */
  if( !( ( dwFlags & DPPLAYER_SERVERPLAYER ) ||
         ( dwFlags & DPPLAYER_SPECTATOR )
       )
    )
  {
    /* Assume non fatal failure */
    ERR( "unknown dwFlags = 0x%08x\n", dwFlags );
  }

  /* If the name is not specified, we must provide one */
  if( *lpidPlayer == DPID_UNKNOWN )
  {
    /* If we are the session master, we dish out the group/player ids */
    if( This->dp2->bHostInterface )
    {
      *lpidPlayer = DP_NextObjectId();
    }
    else
    {
      hr = DP_MSG_SendRequestPlayerId( This, dwCreateFlags, lpidPlayer );

      if( FAILED(hr) )
      {
        ERR( "Request for ID failed: %s\n", DPLAYX_HresultToString( hr ) );
        return hr;
      }
    }
  }
  else
  {
    /* FIXME: Would be nice to perhaps verify that we don't already have
     *        this player.
     */
  }

  /* We pass creation flags, so we can distinguish sysplayers and not count them in the current
     player total */
  lpPData = DP_CreatePlayer( This, lpidPlayer, lpPlayerName, dwCreateFlags,
                             hEvent, bAnsi );

  if( lpPData == NULL )
  {
    return DPERR_CANTADDPLAYER;
  }

  /* Create the list object and link it in */
  lpPList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpPList ) );
  if( lpPList == NULL )
  {
    FIXME( "Memory leak\n" );
    return DPERR_CANTADDPLAYER;
  }

  lpPData->uRef = 1;
  lpPList->lpPData = lpPData;

  /* Add the player to the system group */
  DPQ_INSERT( This->dp2->lpSysGroup->players, lpPList, players );

  /* Update the information and send it to all players in the session */
  DP_SetPlayerData( lpPData, DPSET_REMOTE, lpData, dwDataSize );

  /* Let the SP know that we've created this player */
  if( This->dp2->spData.lpCB->CreatePlayer )
  {
    DPSP_CREATEPLAYERDATA data;

    data.idPlayer          = *lpidPlayer;
    data.dwFlags           = dwCreateFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    TRACE( "Calling SP CreatePlayer 0x%08x: dwFlags: 0x%08x lpMsgHdr: %p\n",
           *lpidPlayer, data.dwFlags, data.lpSPMessageHeader );

    hr = (*This->dp2->spData.lpCB->CreatePlayer)( &data );
  }

  if( FAILED(hr) )
  {
    ERR( "Failed to create player with sp: %s\n", DPLAYX_HresultToString(hr) );
    return hr;
  }

  /* Now let the SP know that this player is a member of the system group */
  if( This->dp2->spData.lpCB->AddPlayerToGroup )
  {
    DPSP_ADDPLAYERTOGROUPDATA data;

    data.idPlayer = *lpidPlayer;
    data.idGroup  = DPID_SYSTEM_GROUP;
    data.lpISP    = This->dp2->spData.lpISP;

    TRACE( "Calling SP AddPlayerToGroup (sys group)\n" );

    hr = (*This->dp2->spData.lpCB->AddPlayerToGroup)( &data );
  }

  if( FAILED(hr) )
  {
    ERR( "Failed to add player to sys group with sp: %s\n",
         DPLAYX_HresultToString(hr) );
    return hr;
  }

#if 1
  if( This->dp2->bHostInterface == FALSE )
  {
    /* Let the name server know about the creation of this player */
    /* FIXME: Is this only to be done for the creation of a server player or
     *        is this used for regular players? If only for server players, move
     *        this call to DP_SecureOpen(...);
     */
#if 0
    TRACE( "Sending message to self to get my addr\n" );
    DP_MSG_ToSelf( This, *lpidPlayer ); /* This is a hack right now */
#endif

    hr = DP_MSG_ForwardPlayerCreation( This, *lpidPlayer);
  }
#else
  /* Inform all other peers of the creation of a new player. If there are
   * no peers keep this quiet.
   * Also, if this was a remote event, no need to rebroadcast it.
   */
  if( ( lpMsgHdr == NULL ) &&
      This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_CREATEPLAYERORGROUP msg;
    msg.dwType = DPSYS_CREATEPLAYERORGROUP;

    msg.dwPlayerType     = DPPLAYERTYPE_PLAYER;
    msg.dpId             = *lpidPlayer;
    msg.dwCurrentPlayers = 0; /* FIXME: Incorrect */
    msg.lpData           = lpData;
    msg.dwDataSize       = dwDataSize;
    msg.dpnName          = *lpPlayerName;
    msg.dpIdParent       = DPID_NOPARENT_GROUP;
    msg.dwFlags          = DPMSG_CREATEPLAYER_DWFLAGS( dwFlags );

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    hr = DP_SendEx( This, DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg,
                    sizeof( msg ), 0, 0, NULL, NULL, bAnsi );
  }
#endif

  return hr;
}

static HRESULT WINAPI IDirectPlay4AImpl_CreatePlayer( IDirectPlay4A *iface, DPID *lpidPlayer,
        DPNAME *lpPlayerName, HANDLE hEvent, void *lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

  if( lpidPlayer == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags & DPPLAYER_SERVERPLAYER )
  {
    *lpidPlayer = DPID_SERVERPLAYER;
  }
  else
  {
    *lpidPlayer = DPID_UNKNOWN;
  }

  return DP_IF_CreatePlayer( This, NULL, lpidPlayer, lpPlayerName, hEvent,
                           lpData, dwDataSize, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_CreatePlayer
          ( LPDIRECTPLAY2 iface, LPDPID lpidPlayer, LPDPNAME lpPlayerName,
            HANDLE hEvent, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;

  if( lpidPlayer == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags & DPPLAYER_SERVERPLAYER )
  {
    *lpidPlayer = DPID_SERVERPLAYER;
  }
  else
  {
    *lpidPlayer = DPID_UNKNOWN;
  }

  return DP_IF_CreatePlayer( This, NULL, lpidPlayer, lpPlayerName, hEvent,
                           lpData, dwDataSize, dwFlags, FALSE );
}

static DPID DP_GetRemoteNextObjectId(void)
{
  FIXME( ":stub\n" );

  /* Hack solution */
  return DP_NextObjectId();
}

static HRESULT WINAPI IDirectPlay4AImpl_DeletePlayerFromGroup( IDirectPlay4A *iface, DPID group,
        DPID player )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_DeletePlayerFromGroup( &This->IDirectPlay4_iface, group, player );
}

static HRESULT WINAPI IDirectPlay4Impl_DeletePlayerFromGroup( IDirectPlay4 *iface, DPID group,
        DPID player )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    HRESULT hr = DP_OK;

    lpGroupData  gdata;
    lpPlayerList plist;

    TRACE( "(%p)->(0x%08x,0x%08x)\n", This, group, player );

    /* Find the group */
    if ( ( gdata = DP_FindAnyGroup( This, group ) ) == NULL )
        return DPERR_INVALIDGROUP;

    /* Find the player */
    if ( ( plist = DP_FindPlayer( This, player ) ) == NULL )
        return DPERR_INVALIDPLAYER;

    /* Remove the player shortcut from the group */
    DPQ_REMOVE_ENTRY( gdata->players, players, lpPData->dpid, ==, player, plist );

    if ( !plist )
        return DPERR_INVALIDPLAYER;

    /* One less reference */
    plist->lpPData->uRef--;

    /* Delete the Player List element */
    HeapFree( GetProcessHeap(), 0, plist );

    /* Inform the SP if they care */
    if ( This->dp2->spData.lpCB->RemovePlayerFromGroup )
    {
        DPSP_REMOVEPLAYERFROMGROUPDATA data;

        TRACE( "Calling SP RemovePlayerFromGroup\n" );
        data.idPlayer = player;
        data.idGroup = group;
        data.lpISP = This->dp2->spData.lpISP;
        hr = (*This->dp2->spData.lpCB->RemovePlayerFromGroup)( &data );
    }

    /* Need to send a DELETEPLAYERFROMGROUP message */
    FIXME( "Need to send a message\n" );

    return hr;
}

typedef struct _DPRGOPContext
{
  IDirectPlay3Impl* This;
  BOOL              bAnsi;
  DPID              idGroup;
} DPRGOPContext, *lpDPRGOPContext;

static BOOL CALLBACK
cbRemoveGroupOrPlayer(
    DPID            dpId,
    DWORD           dwPlayerType,
    LPCDPNAME       lpName,
    DWORD           dwFlags,
    LPVOID          lpContext )
{
  lpDPRGOPContext lpCtxt = (lpDPRGOPContext)lpContext;

  TRACE( "Removing element:0x%08x (type:0x%08x) from element:0x%08x\n",
           dpId, dwPlayerType, lpCtxt->idGroup );

  if( dwPlayerType == DPPLAYERTYPE_GROUP )
  {
    if( FAILED( DP_IF_DeleteGroupFromGroup( lpCtxt->This, lpCtxt->idGroup,
                                            dpId )
              )
      )
    {
      ERR( "Unable to delete group 0x%08x from group 0x%08x\n",
             dpId, lpCtxt->idGroup );
    }
  }
  else if ( FAILED( IDirectPlayX_DeletePlayerFromGroup( &lpCtxt->This->IDirectPlay4_iface,
                                                        lpCtxt->idGroup, dpId ) ) )
      ERR( "Unable to delete player 0x%08x from grp 0x%08x\n", dpId, lpCtxt->idGroup );

  return TRUE; /* Continue enumeration */
}

static HRESULT DP_IF_DestroyGroup
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idGroup, BOOL bAnsi )
{
  lpGroupData lpGData;
  DPRGOPContext context;

  FIXME( "(%p)->(%p,0x%08x,%u): semi stub\n",
         This, lpMsgHdr, idGroup, bAnsi );

  /* Find the group */
  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER; /* yes player */
  }

  context.This    = (IDirectPlay3Impl*)This;
  context.bAnsi   = bAnsi;
  context.idGroup = idGroup;

  /* Remove all players that this group has */
  IDirectPlayX_EnumGroupPlayers( &This->IDirectPlay4_iface, idGroup, NULL, cbRemoveGroupOrPlayer,
          &context, 0 );

  /* Remove all links to groups that this group has since this is dp3 */
  DP_IF_EnumGroupsInGroup( (IDirectPlay3Impl*)This, idGroup, NULL,
                           cbRemoveGroupOrPlayer, (LPVOID)&context, 0, bAnsi );

  /* Remove this group from the parent group - if it has one */
  if( ( idGroup != DPID_SYSTEM_GROUP ) &&
      ( lpGData->parent != DPID_SYSTEM_GROUP )
    )
  {
    DP_IF_DeleteGroupFromGroup( (IDirectPlay3Impl*)This, lpGData->parent,
                                idGroup );
  }

  /* Now delete this group data and list from the system group */
  DP_DeleteGroup( This, idGroup );

  /* Let the SP know that we've destroyed this group */
  if( This->dp2->spData.lpCB->DeleteGroup )
  {
    DPSP_DELETEGROUPDATA data;

    FIXME( "data.dwFlags is incorrect\n" );

    data.idGroup = idGroup;
    data.dwFlags = 0;
    data.lpISP   = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->DeleteGroup)( &data );
  }

  FIXME( "Send out a DESTORYPLAYERORGROUP message\n" );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_DestroyGroup( IDirectPlay4A *iface, DPID idGroup )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_DestroyGroup( This, NULL, idGroup, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_DestroyGroup
          ( LPDIRECTPLAY2 iface, DPID idGroup )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyGroup( This, NULL, idGroup, FALSE );
}

typedef struct _DPFAGContext
{
  IDirectPlay2Impl* This;
  DPID              idPlayer;
  BOOL              bAnsi;
} DPFAGContext, *lpDPFAGContext;

static HRESULT DP_IF_DestroyPlayer
          ( IDirectPlay2Impl* This, LPVOID lpMsgHdr, DPID idPlayer, BOOL bAnsi )
{
  DPFAGContext cbContext;

  FIXME( "(%p)->(%p,0x%08x,%u): semi stub\n",
         This, lpMsgHdr, idPlayer, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( DP_FindPlayer( This, idPlayer ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  /* FIXME: If the player is remote, we must be the host to delete this */

  cbContext.This     = This;
  cbContext.idPlayer = idPlayer;
  cbContext.bAnsi    = bAnsi;

  /* Find each group and call DeletePlayerFromGroup if the player is a
     member of the group */
  IDirectPlayX_EnumGroups( &This->IDirectPlay4_iface, NULL, cbDeletePlayerFromAllGroups, &cbContext,
          DPENUMGROUPS_ALL );

  /* Now delete player and player list from the sys group */
  DP_DeletePlayer( This, idPlayer );

  /* Let the SP know that we've destroyed this group */
  if( This->dp2->spData.lpCB->DeletePlayer )
  {
    DPSP_DELETEPLAYERDATA data;

    FIXME( "data.dwFlags is incorrect\n" );

    data.idPlayer = idPlayer;
    data.dwFlags = 0;
    data.lpISP   = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->DeletePlayer)( &data );
  }

  FIXME( "Send a DELETEPLAYERORGROUP msg\n" );

  return DP_OK;
}

static BOOL CALLBACK
cbDeletePlayerFromAllGroups(
    DPID            dpId,
    DWORD           dwPlayerType,
    LPCDPNAME       lpName,
    DWORD           dwFlags,
    LPVOID          lpContext )
{
  lpDPFAGContext lpCtxt = (lpDPFAGContext)lpContext;

  if( dwPlayerType == DPPLAYERTYPE_GROUP )
  {
    IDirectPlayX_DeletePlayerFromGroup( &lpCtxt->This->IDirectPlay4_iface, dpId, lpCtxt->idPlayer );

    /* Enumerate all groups in this group since this will normally only
     * be called for top level groups
     */
    DP_IF_EnumGroupsInGroup( (IDirectPlay3Impl*)lpCtxt->This,
                             dpId, NULL,
                             cbDeletePlayerFromAllGroups,
                             lpContext, DPENUMGROUPS_ALL,
                             lpCtxt->bAnsi );

  }
  else
  {
    ERR( "Group callback has dwPlayerType = 0x%08x\n", dwPlayerType );
  }

  return TRUE;
}

static HRESULT WINAPI IDirectPlay4AImpl_DestroyPlayer( IDirectPlay4A *iface, DPID idPlayer )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_DestroyPlayer( This, NULL, idPlayer, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_DestroyPlayer
          ( LPDIRECTPLAY2 iface, DPID idPlayer )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_DestroyPlayer( This, NULL, idPlayer, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_EnumGroupPlayers( IDirectPlay4A *iface, DPID group,
        GUID *instance, LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_EnumGroupPlayers( &This->IDirectPlay4_iface, group, instance, enumplayercb,
            context, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_EnumGroupPlayers( IDirectPlay4 *iface, DPID group,
        GUID *instance, LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    lpGroupData  gdata;
    lpPlayerList plist;

    FIXME( "(%p)->(0x%08x,%p,%p,%p,0x%08x): semi stub\n", This, group, instance, enumplayercb,
           context, flags );

    if ( This->dp2->connectionInitialized == NO_PROVIDER )
        return DPERR_UNINITIALIZED;

    /* Find the group */
    if ( ( gdata = DP_FindAnyGroup( This, group ) ) == NULL )
        return DPERR_INVALIDGROUP;

    if ( DPQ_IS_EMPTY( gdata->players ) )
        return DP_OK;

    /* Walk the players in this group */
    for( plist = DPQ_FIRST( gdata->players ); ; plist = DPQ_NEXT( plist->players ) )
    {
        /* We do not enum the name server or app server as they are of no
         * consequence to the end user.
         */
        if ( ( plist->lpPData->dpid != DPID_NAME_SERVER ) &&
                ( plist->lpPData->dpid != DPID_SERVERPLAYER ) )
        {
            /* FIXME: Need to add stuff for flags checking */
            if ( !enumplayercb( plist->lpPData->dpid, DPPLAYERTYPE_PLAYER,
                        &plist->lpPData->name, plist->lpPData->dwFlags, context ) )
              /* User requested break */
              return DP_OK;
        }

        if ( DPQ_IS_ENDOFLIST( plist->players ) )
            break;
    }
    return DP_OK;
}

/* NOTE: This only enumerates top level groups (created with CreateGroup) */
static HRESULT WINAPI IDirectPlay4AImpl_EnumGroups( IDirectPlay4A *iface, GUID *instance,
        LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    return IDirectPlayX_EnumGroupsInGroup( iface, DPID_SYSTEM_GROUP, instance, enumplayercb,
            context, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_EnumGroups ( IDirectPlay4 *iface, GUID *instance,
        LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    return IDirectPlayX_EnumGroupsInGroup( iface, DPID_SYSTEM_GROUP, instance, enumplayercb,
            context, flags );
}

static HRESULT WINAPI IDirectPlay4AImpl_EnumPlayers( IDirectPlay4A *iface, GUID *instance,
        LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    return IDirectPlayX_EnumGroupPlayers( iface, DPID_SYSTEM_GROUP, instance, enumplayercb,
            context, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_EnumPlayers( IDirectPlay4 *iface, GUID *instance,
        LPDPENUMPLAYERSCALLBACK2 enumplayercb, void *context, DWORD flags )
{
    return IDirectPlayX_EnumGroupPlayers( iface, DPID_SYSTEM_GROUP, instance, enumplayercb,
            context, flags );
}

/* This function should call the registered callback function that the user
   passed into EnumSessions for each entry available.
 */
static void DP_InvokeEnumSessionCallbacks
       ( LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
         LPVOID lpNSInfo,
         DWORD dwTimeout,
         LPVOID lpContext )
{
  LPDPSESSIONDESC2 lpSessionDesc;

  FIXME( ": not checking for conditions\n" );

  /* Not sure if this should be pruning but it's convenient */
  NS_PruneSessionCache( lpNSInfo );

  NS_ResetSessionEnumeration( lpNSInfo );

  /* Enumerate all sessions */
  /* FIXME: Need to indicate ANSI */
  while( (lpSessionDesc = NS_WalkSessions( lpNSInfo ) ) != NULL )
  {
    TRACE( "EnumSessionsCallback2 invoked\n" );
    if( !lpEnumSessionsCallback2( lpSessionDesc, &dwTimeout, 0, lpContext ) )
    {
      return;
    }
  }

  /* Invoke one last time to indicate that there is no more to come */
  lpEnumSessionsCallback2( NULL, &dwTimeout, DPESC_TIMEDOUT, lpContext );
}

static DWORD CALLBACK DP_EnumSessionsSendAsyncRequestThread( LPVOID lpContext )
{
  EnumSessionAsyncCallbackData* data = lpContext;
  HANDLE hSuicideRequest = data->hSuicideRequest;
  DWORD dwTimeout = data->dwTimeout;

  TRACE( "Thread started with timeout = 0x%08x\n", dwTimeout );

  for( ;; )
  {
    HRESULT hr;

    /* Sleep up to dwTimeout waiting for request to terminate thread */
    if( WaitForSingleObject( hSuicideRequest, dwTimeout ) == WAIT_OBJECT_0 )
    {
      TRACE( "Thread terminating on terminate request\n" );
      break;
    }

    /* Now resend the enum request */
    hr = NS_SendSessionRequestBroadcast( &data->requestGuid,
                                         data->dwEnumSessionFlags,
                                         data->lpSpData );

    if( FAILED(hr) )
    {
      ERR( "Enum broadcase request failed: %s\n", DPLAYX_HresultToString(hr) );
      /* FIXME: Should we kill this thread? How to inform the main thread? */
    }

  }

  TRACE( "Thread terminating\n" );

  /* Clean up the thread data */
  CloseHandle( hSuicideRequest );
  HeapFree( GetProcessHeap(), 0, lpContext );

  /* FIXME: Need to have some notification to main app thread that this is
   *        dead. It would serve two purposes. 1) allow sync on termination
   *        so that we don't actually send something to ourselves when we
   *        become name server (race condition) and 2) so that if we die
   *        abnormally something else will be able to tell.
   */

  return 1;
}

static void DP_KillEnumSessionThread( IDirectPlay2Impl* This )
{
  /* Does a thread exist? If so we were doing an async enum session */
  if( This->dp2->hEnumSessionThread != INVALID_HANDLE_VALUE )
  {
    TRACE( "Killing EnumSession thread %p\n",
           This->dp2->hEnumSessionThread );

    /* Request that the thread kill itself nicely */
    SetEvent( This->dp2->hKillEnumSessionThreadEvent );
    CloseHandle( This->dp2->hKillEnumSessionThreadEvent );

    /* We no longer need to know about the thread */
    CloseHandle( This->dp2->hEnumSessionThread );

    This->dp2->hEnumSessionThread = INVALID_HANDLE_VALUE;
  }
}

static HRESULT DP_IF_EnumSessions
          ( IDirectPlay2Impl* This, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  TRACE( "(%p)->(%p,0x%08x,%p,%p,0x%08x,%u)\n",
         This, lpsd, dwTimeout, lpEnumSessionsCallback2, lpContext, dwFlags,
         bAnsi );
  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* Can't enumerate if the interface is already open */
  if( This->dp2->bConnectionOpen )
  {
    return DPERR_GENERIC;
  }

#if 1
  /* The loading of a lobby provider _seems_ to require a backdoor loading
   * of the service provider to also associate with this DP object. This is
   * because the app doesn't seem to have to call EnumConnections and
   * InitializeConnection for the SP before calling this method. As such
   * we'll do their dirty work for them with a quick hack so as to always
   * load the TCP/IP service provider.
   *
   * The correct solution would seem to involve creating a dialog box which
   * contains the possible SPs. These dialog boxes most likely follow SDK
   * examples.
   */
   if( This->dp2->bDPLSPInitialized && !This->dp2->bSPInitialized )
   {
     LPVOID lpConnection;
     DWORD  dwSize;

     WARN( "Hack providing TCP/IP SP for lobby provider activated\n" );

     if( !DP_BuildSPCompoundAddr( (LPGUID)&DPSPGUID_TCPIP, &lpConnection, &dwSize ) )
     {
       ERR( "Can't build compound addr\n" );
       return DPERR_GENERIC;
     }

     hr = DP_IF_InitializeConnection( (IDirectPlay3Impl*)This, lpConnection,
                                      0, bAnsi );
     if( FAILED(hr) )
     {
       return hr;
     }

     /* Free up the address buffer */
     HeapFree( GetProcessHeap(), 0, lpConnection );

     /* The SP is now initialized */
     This->dp2->bSPInitialized = TRUE;
   }
#endif


  /* Use the service provider default? */
  if( dwTimeout == 0 )
  {
    DPCAPS spCaps;
    spCaps.dwSize = sizeof( spCaps );

    IDirectPlayX_GetCaps( &This->IDirectPlay4_iface, &spCaps, 0 );
    dwTimeout = spCaps.dwTimeout;

    /* The service provider doesn't provide one either! */
    if( dwTimeout == 0 )
    {
      /* Provide the TCP/IP default */
      dwTimeout = DPMSG_WAIT_5_SECS;
    }
  }

  if( dwFlags & DPENUMSESSIONS_STOPASYNC )
  {
    DP_KillEnumSessionThread( This );
    return hr;
  }

  if( ( dwFlags & DPENUMSESSIONS_ASYNC ) )
  {
    /* Enumerate everything presently in the local session cache */
    DP_InvokeEnumSessionCallbacks( lpEnumSessionsCallback2,
                                   This->dp2->lpNameServerData, dwTimeout,
                                   lpContext );

    if( This->dp2->dwEnumSessionLock != 0 )
      return DPERR_CONNECTING;

    /* See if we've already created a thread to service this interface */
    if( This->dp2->hEnumSessionThread == INVALID_HANDLE_VALUE )
    {
      DWORD dwThreadId;
      This->dp2->dwEnumSessionLock++;

      /* Send the first enum request inline since the user may cancel a dialog
       * if one is presented. Also, may also have a connecting return code.
       */
      hr = NS_SendSessionRequestBroadcast( &lpsd->guidApplication,
                                           dwFlags, &This->dp2->spData );

      if( SUCCEEDED(hr) )
      {
        EnumSessionAsyncCallbackData* lpData
          = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpData ) );
        /* FIXME: need to kill the thread on object deletion */
        lpData->lpSpData  = &This->dp2->spData;

        lpData->requestGuid = lpsd->guidApplication;
        lpData->dwEnumSessionFlags = dwFlags;
        lpData->dwTimeout = dwTimeout;

        This->dp2->hKillEnumSessionThreadEvent =
          CreateEventW( NULL, TRUE, FALSE, NULL );

        if( !DuplicateHandle( GetCurrentProcess(),
                              This->dp2->hKillEnumSessionThreadEvent,
                              GetCurrentProcess(),
                              &lpData->hSuicideRequest,
                              0, FALSE, DUPLICATE_SAME_ACCESS )
          )
        {
          ERR( "Can't duplicate thread killing handle\n" );
        }

        TRACE( ": creating EnumSessionsRequest thread\n" );

        This->dp2->hEnumSessionThread = CreateThread( NULL,
                                                      0,
                                                      DP_EnumSessionsSendAsyncRequestThread,
                                                      lpData,
                                                      0,
                                                      &dwThreadId );
      }
      This->dp2->dwEnumSessionLock--;
    }
  }
  else
  {
    /* Invalidate the session cache for the interface */
    NS_InvalidateSessionCache( This->dp2->lpNameServerData );

    /* Send the broadcast for session enumeration */
    hr = NS_SendSessionRequestBroadcast( &lpsd->guidApplication,
                                         dwFlags,
                                         &This->dp2->spData );


    SleepEx( dwTimeout, FALSE );

    DP_InvokeEnumSessionCallbacks( lpEnumSessionsCallback2,
                                   This->dp2->lpNameServerData, dwTimeout,
                                   lpContext );
  }

  return hr;
}

static HRESULT WINAPI IDirectPlay4AImpl_EnumSessions( IDirectPlay4A *iface, DPSESSIONDESC2 *lpsd,
        DWORD dwTimeout, LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2, void *lpContext,
        DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_EnumSessions( This, lpsd, dwTimeout, lpEnumSessionsCallback2,
                             lpContext, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_EnumSessions
          ( LPDIRECTPLAY2 iface, LPDPSESSIONDESC2 lpsd, DWORD dwTimeout,
            LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
            LPVOID lpContext, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_EnumSessions( This, lpsd, dwTimeout, lpEnumSessionsCallback2,
                             lpContext, dwFlags, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetCaps( IDirectPlay4A *iface, DPCAPS *caps, DWORD flags )
{
    return IDirectPlayX_GetPlayerCaps( iface, DPID_ALLPLAYERS, caps, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_GetCaps( IDirectPlay4 *iface, DPCAPS *caps, DWORD flags )
{
    return IDirectPlayX_GetPlayerCaps( iface, DPID_ALLPLAYERS, caps, flags );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetGroupData( IDirectPlay4A *iface, DPID group,
        void *data, DWORD *size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_GetGroupData( &This->IDirectPlay4_iface, group, data, size, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_GetGroupData( IDirectPlay4 *iface, DPID group,
        void *data, DWORD *size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    lpGroupData gdata;
    DWORD bufsize;
    void *src;

    TRACE( "(%p)->(0x%08x,%p,%p,0x%08x)\n", This, group, data, size, flags );

    if ( ( gdata = DP_FindAnyGroup( This, group ) ) == NULL )
        return DPERR_INVALIDGROUP;

    /* How much buffer is required? */
    if ( flags & DPSET_LOCAL )
    {
        bufsize = gdata->dwLocalDataSize;
        src = gdata->lpLocalData;
    }
    else
    {
        bufsize = gdata->dwRemoteDataSize;
        src = gdata->lpRemoteData;
    }

    /* Is the user requesting to know how big a buffer is required? */
    if ( !data || *size < bufsize )
    {
        *size = bufsize;
        return DPERR_BUFFERTOOSMALL;
    }

    CopyMemory( data, src, bufsize );

    return DP_OK;
}

static HRESULT DP_IF_GetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi )
{
  lpGroupData lpGData;
  LPDPNAME    lpName = lpData;
  DWORD       dwRequiredDataSize;

  FIXME("(%p)->(0x%08x,%p,%p,%u) ANSI ignored\n",
          This, idGroup, lpData, lpdwDataSize, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  dwRequiredDataSize = lpGData->name.dwSize;

  if( lpGData->name.u1.lpszShortNameA )
  {
    dwRequiredDataSize += strlen( lpGData->name.u1.lpszShortNameA ) + 1;
  }

  if( lpGData->name.u2.lpszLongNameA )
  {
    dwRequiredDataSize += strlen( lpGData->name.u2.lpszLongNameA ) + 1;
  }

  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredDataSize )
    )
  {
    *lpdwDataSize = dwRequiredDataSize;
    return DPERR_BUFFERTOOSMALL;
  }

  /* Copy the structure */
  CopyMemory( lpName, &lpGData->name, lpGData->name.dwSize );

  if( lpGData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpGData->name.dwSize,
            lpGData->name.u1.lpszShortNameA );
  }
  else
  {
    lpName->u1.lpszShortNameA = NULL;
  }

  if( lpGData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpGData->name.dwSize,
            lpGData->name.u2.lpszLongNameA );
  }
  else
  {
    lpName->u2.lpszLongNameA = NULL;
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetGroupName( IDirectPlay4A *iface, DPID idGroup,
        void *lpData, DWORD *lpdwDataSize )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_GetGroupName( This, idGroup, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetGroupName
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetGroupName( This, idGroup, lpData, lpdwDataSize, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetMessageCount( IDirectPlay4A *iface, DPID player,
        DWORD *count )
{
    return IDirectPlayX_GetMessageQueue( iface, 0, player, DPMESSAGEQUEUE_RECEIVE, count, NULL );
}

static HRESULT WINAPI IDirectPlay4Impl_GetMessageCount( IDirectPlay4 *iface, DPID player,
        DWORD *count )
{
    return IDirectPlayX_GetMessageQueue( iface, 0, player, DPMESSAGEQUEUE_RECEIVE, count, NULL );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerAddress( IDirectPlay4A *iface, DPID player,
        void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    FIXME("(%p)->(0x%08x,%p,%p): stub\n", This, player, data, size );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetPlayerAddress( IDirectPlay4 *iface, DPID player,
        void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,%p,%p): stub\n", This, player, data, size );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerCaps( IDirectPlay4A *iface, DPID player,
        DPCAPS *caps, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_GetPlayerCaps( &This->IDirectPlay4_iface, player, caps, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_GetPlayerCaps( IDirectPlay4 *iface, DPID player,
        DPCAPS *caps, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    DPSP_GETCAPSDATA data;

    TRACE( "(%p)->(0x%08x,%p,0x%08x)\n", This, player, caps, flags);

    if ( This->dp2->connectionInitialized == NO_PROVIDER )
        return DPERR_UNINITIALIZED;

    /* Query the service provider */
    data.idPlayer = player;
    data.dwFlags = flags;
    data.lpCaps = caps;
    data.lpISP = This->dp2->spData.lpISP;

    return (*This->dp2->spData.lpCB->GetCaps)( &data );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerData( IDirectPlay4A *iface, DPID player,
        void *data, DWORD *size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_GetPlayerData( &This->IDirectPlay4_iface, player, data, size, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_GetPlayerData( IDirectPlay4 *iface, DPID player,
        void *data, DWORD *size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    lpPlayerList plist;
    DWORD bufsize;
    void *src;

    TRACE( "(%p)->(0x%08x,%p,%p,0x%08x)\n", This, player, data, size, flags );

    if ( This->dp2->connectionInitialized == NO_PROVIDER )
        return DPERR_UNINITIALIZED;

    if ( ( plist = DP_FindPlayer( This, player ) ) == NULL )
        return DPERR_INVALIDPLAYER;

    if ( flags & DPSET_LOCAL )
    {
        bufsize = plist->lpPData->dwLocalDataSize;
        src = plist->lpPData->lpLocalData;
    }
    else
    {
        bufsize = plist->lpPData->dwRemoteDataSize;
        src = plist->lpPData->lpRemoteData;
    }

    /* Is the user requesting to know how big a buffer is required? */
    if ( !data || *size < bufsize )
    {
        *size = bufsize;
        return DPERR_BUFFERTOOSMALL;
    }

    CopyMemory( data, src, bufsize );

    return DP_OK;
}

static HRESULT DP_IF_GetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize, BOOL bAnsi )
{
  lpPlayerList lpPList;
  LPDPNAME    lpName = lpData;
  DWORD       dwRequiredDataSize;

  FIXME( "(%p)->(0x%08x,%p,%p,%u): ANSI\n",
         This, idPlayer, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  dwRequiredDataSize = lpPList->lpPData->name.dwSize;

  if( lpPList->lpPData->name.u1.lpszShortNameA )
  {
    dwRequiredDataSize += strlen( lpPList->lpPData->name.u1.lpszShortNameA ) + 1;
  }

  if( lpPList->lpPData->name.u2.lpszLongNameA )
  {
    dwRequiredDataSize += strlen( lpPList->lpPData->name.u2.lpszLongNameA ) + 1;
  }

  if( ( lpData == NULL ) ||
      ( *lpdwDataSize < dwRequiredDataSize )
    )
  {
    *lpdwDataSize = dwRequiredDataSize;
    return DPERR_BUFFERTOOSMALL;
  }

  /* Copy the structure */
  CopyMemory( lpName, &lpPList->lpPData->name, lpPList->lpPData->name.dwSize );

  if( lpPList->lpPData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpPList->lpPData->name.dwSize,
            lpPList->lpPData->name.u1.lpszShortNameA );
  }
  else
  {
    lpName->u1.lpszShortNameA = NULL;
  }

  if( lpPList->lpPData->name.u1.lpszShortNameA )
  {
    strcpy( ((char*)lpName)+lpPList->lpPData->name.dwSize,
            lpPList->lpPData->name.u2.lpszLongNameA );
  }
  else
  {
    lpName->u2.lpszLongNameA = NULL;
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerName( IDirectPlay4A *iface, DPID idPlayer,
        void *lpData, DWORD *lpdwDataSize )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_GetPlayerName( This, idPlayer, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetPlayerName
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPVOID lpData,
            LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_GetPlayerName( This, idPlayer, lpData, lpdwDataSize, FALSE );
}

static HRESULT DP_GetSessionDesc
          ( IDirectPlay2Impl* This, LPVOID lpData, LPDWORD lpdwDataSize,
            BOOL bAnsi )
{
  DWORD dwRequiredSize;

  TRACE( "(%p)->(%p,%p,%u)\n", This, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpData == NULL ) && ( lpdwDataSize == NULL ) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* FIXME: Get from This->dp2->lpSessionDesc */
  dwRequiredSize = DP_CalcSessionDescSize( This->dp2->lpSessionDesc, bAnsi );

  if ( ( lpData == NULL ) ||
       ( *lpdwDataSize < dwRequiredSize )
     )
  {
    *lpdwDataSize = dwRequiredSize;
    return DPERR_BUFFERTOOSMALL;
  }

  DP_CopySessionDesc( lpData, This->dp2->lpSessionDesc, bAnsi );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetSessionDesc( IDirectPlay4A *iface, void *lpData,
        DWORD *lpdwDataSize )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_GetSessionDesc( This, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_GetSessionDesc
          ( LPDIRECTPLAY2 iface, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_GetSessionDesc( This, lpData, lpdwDataSize, TRUE );
}

/* Intended only for COM compatibility. Always returns an error. */
static HRESULT WINAPI IDirectPlay4AImpl_Initialize( IDirectPlay4A *iface, GUID *guid )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    TRACE("(%p)->(%p): no-op\n", This, guid );
    return DPERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI IDirectPlay4Impl_Initialize( IDirectPlay4 *iface, GUID *guid )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    TRACE( "(%p)->(%p): no-op\n", This, guid );
    return DPERR_ALREADYINITIALIZED;
}


static HRESULT DP_SecureOpen
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials,
            BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(%p,0x%08x,%p,%p): partial stub\n",
         This, lpsd, dwFlags, lpSecurity, lpCredentials );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( lpsd->dwSize != sizeof(DPSESSIONDESC2) )
  {
    TRACE( ": rejecting invalid dpsd size (%d).\n", lpsd->dwSize );
    return DPERR_INVALIDPARAMS;
  }

  if( This->dp2->bConnectionOpen )
  {
    TRACE( ": rejecting already open connection.\n" );
    return DPERR_ALREADYINITIALIZED;
  }

  /* If we're enumerating, kill the thread */
  DP_KillEnumSessionThread( This );

  if( dwFlags & DPOPEN_CREATE )
  {
    /* Rightoo - this computer is the host and the local computer needs to be
       the name server so that others can join this session */
    NS_SetLocalComputerAsNameServer( lpsd, This->dp2->lpNameServerData );

    This->dp2->bHostInterface = TRUE;

    hr = DP_SetSessionDesc( This, lpsd, 0, TRUE, bAnsi );
    if( FAILED( hr ) )
    {
      ERR( "Unable to set session desc: %s\n", DPLAYX_HresultToString( hr ) );
      return hr;
    }
  }

  /* Invoke the conditional callback for the service provider */
  if( This->dp2->spData.lpCB->Open )
  {
    DPSP_OPENDATA data;

    FIXME( "Not all data fields are correct. Need new parameter\n" );

    data.bCreate           = (dwFlags & DPOPEN_CREATE ) != 0;
    data.lpSPMessageHeader = (dwFlags & DPOPEN_CREATE ) ? NULL
                                                        : NS_GetNSAddr( This->dp2->lpNameServerData );
    data.lpISP             = This->dp2->spData.lpISP;
    data.bReturnStatus     = (dwFlags & DPOPEN_RETURNSTATUS) != 0;
    data.dwOpenFlags       = dwFlags;
    data.dwSessionFlags    = This->dp2->lpSessionDesc->dwFlags;

    hr = (*This->dp2->spData.lpCB->Open)(&data);
    if( FAILED( hr ) )
    {
      ERR( "Unable to open session: %s\n", DPLAYX_HresultToString( hr ) );
      return hr;
    }
  }

  {
    /* Create the system group of which everything is a part of */
    DPID systemGroup = DPID_SYSTEM_GROUP;

    hr = DP_IF_CreateGroup( This, NULL, &systemGroup, NULL,
                            NULL, 0, 0, TRUE );

  }

  if( dwFlags & DPOPEN_JOIN )
  {
    DPID dpidServerId = DPID_UNKNOWN;

    /* Create the server player for this interface. This way we can receive
     * messages for this session.
     */
    /* FIXME: I suppose that we should be setting an event for a receive
     *        type of thing. That way the messaging thread could know to wake
     *        up. DPlay would then trigger the hEvent for the player the
     *        message is directed to.
     */
    hr = DP_IF_CreatePlayer( This, NULL, &dpidServerId, NULL, 0, NULL,
                             0,
                             DPPLAYER_SERVERPLAYER | DPPLAYER_LOCAL , bAnsi );

  }
  else if( dwFlags & DPOPEN_CREATE )
  {
    DPID dpidNameServerId = DPID_NAME_SERVER;

    hr = DP_IF_CreatePlayer( This, NULL, &dpidNameServerId, NULL, 0, NULL,
                             0, DPPLAYER_SERVERPLAYER, bAnsi );
  }

  if( FAILED(hr) )
  {
    ERR( "Couldn't create name server/system player: %s\n",
         DPLAYX_HresultToString(hr) );
  }

  return hr;
}

static HRESULT WINAPI IDirectPlay4AImpl_Open( IDirectPlay4A *iface, DPSESSIONDESC2 *sdesc,
        DWORD flags )
{
    return IDirectPlayX_SecureOpen( iface, sdesc, flags, NULL, NULL );
}

static HRESULT WINAPI IDirectPlay4Impl_Open( IDirectPlay4 *iface, DPSESSIONDESC2 *sdesc,
        DWORD flags )
{
    return IDirectPlayX_SecureOpen( iface, sdesc, flags, NULL, NULL );
}

static HRESULT DP_IF_Receive
          ( IDirectPlay2Impl* This, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize, BOOL bAnsi )
{
  LPDPMSG lpMsg = NULL;

  FIXME( "(%p)->(%p,%p,0x%08x,%p,%p,%u): stub\n",
         This, lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( dwFlags == 0 )
  {
    dwFlags = DPRECEIVE_ALL;
  }

  /* If the lpData is NULL, we must be peeking the message */
  if(  ( lpData == NULL ) &&
      !( dwFlags & DPRECEIVE_PEEK )
    )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags & DPRECEIVE_ALL )
  {
    lpMsg = This->dp2->receiveMsgs.lpQHFirst;

    if( !( dwFlags & DPRECEIVE_PEEK ) )
    {
      FIXME( "Remove from queue\n" );
    }
  }
  else if( ( dwFlags & DPRECEIVE_TOPLAYER ) ||
           ( dwFlags & DPRECEIVE_FROMPLAYER )
         )
  {
    FIXME( "Find matching message 0x%08x\n", dwFlags );
  }
  else
  {
    ERR( "Hmmm..dwFlags 0x%08x\n", dwFlags );
  }

  if( lpMsg == NULL )
  {
    return DPERR_NOMESSAGES;
  }

  /* Copy into the provided buffer */
  if (lpData) CopyMemory( lpData, lpMsg->msg, *lpdwDataSize );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_Receive( IDirectPlay4A *iface, DPID *lpidFrom,
        DPID *lpidTo, DWORD dwFlags, void *lpData, DWORD *lpdwDataSize )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_Receive( This, lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_Receive
          ( LPDIRECTPLAY2 iface, LPDPID lpidFrom, LPDPID lpidTo,
            DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_Receive( This, lpidFrom, lpidTo, dwFlags,
                        lpData, lpdwDataSize, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_Send( IDirectPlay4A *iface, DPID from, DPID to,
        DWORD flags, void *data, DWORD size )
{
    return IDirectPlayX_SendEx( iface, from, to, flags, data, size, 0, 0, NULL, NULL );
}

static HRESULT WINAPI IDirectPlay4Impl_Send( IDirectPlay4 *iface, DPID from, DPID to,
        DWORD flags, void *data, DWORD size )
{
    return IDirectPlayX_SendEx( iface, from, to, flags, data, size, 0, 0, NULL, NULL );
}

static HRESULT WINAPI IDirectPlay4AImpl_SetGroupData( IDirectPlay4A *iface, DPID group, void *data,
        DWORD size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_SetGroupData( &This->IDirectPlay4_iface, group, data, size, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_SetGroupData( IDirectPlay4 *iface, DPID group, void *data,
        DWORD size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    lpGroupData gdata;

    TRACE( "(%p)->(0x%08x,%p,0x%08x,0x%08x)\n", This, group, data, size, flags );

    /* Parameter check */
    if ( !data && size )
        return DPERR_INVALIDPARAMS;

    /* Find the pointer to the data for this player */
    if ( ( gdata = DP_FindAnyGroup( This, group ) ) == NULL )
        return DPERR_INVALIDOBJECT;

    if ( !(flags & DPSET_LOCAL) )
    {
        FIXME( "Was this group created by this interface?\n" );
        /* FIXME: If this is a remote update need to allow it but not
         *        send a message.
         */
    }

    DP_SetGroupData( gdata, flags, data, size );

    /* FIXME: Only send a message if this group is local to the session otherwise
     * it will have been rejected above
     */
    if ( !(flags & DPSET_LOCAL) )
        FIXME( "Send msg?\n" );

    return DP_OK;
}

static HRESULT DP_IF_SetGroupName
          ( IDirectPlay2Impl* This, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,%u)\n", This, idGroup,
         lpGroupName, dwFlags, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  DP_CopyDPNAMEStruct( &lpGData->name, lpGroupName, bAnsi );

  /* Should send a DPMSG_SETPLAYERORGROUPNAME message */
  FIXME( "Message not sent and dwFlags ignored\n" );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_SetGroupName( IDirectPlay4A *iface, DPID idGroup,
        DPNAME *lpGroupName, DWORD dwFlags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_SetGroupName( This, idGroup, lpGroupName, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetGroupName
          ( LPDIRECTPLAY2 iface, DPID idGroup, LPDPNAME lpGroupName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetGroupName( This, idGroup, lpGroupName, dwFlags, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_SetPlayerData( IDirectPlay4A *iface, DPID player,
        void *data, DWORD size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return IDirectPlayX_SetPlayerData( &This->IDirectPlay4_iface, player, data, size, flags );
}

static HRESULT WINAPI IDirectPlay4Impl_SetPlayerData( IDirectPlay4 *iface, DPID player,
        void *data, DWORD size, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    lpPlayerList plist;

    TRACE( "(%p)->(0x%08x,%p,0x%08x,0x%08x)\n", This, player, data, size, flags );

    if ( This->dp2->connectionInitialized == NO_PROVIDER )
        return DPERR_UNINITIALIZED;

    /* Parameter check */
    if ( !data && size )
        return DPERR_INVALIDPARAMS;

    /* Find the pointer to the data for this player */
    if ( (plist = DP_FindPlayer( This, player )) == NULL )
        return DPERR_INVALIDPLAYER;

    if ( !(flags & DPSET_LOCAL) )
    {
        FIXME( "Was this group created by this interface?\n" );
        /* FIXME: If this is a remote update need to allow it but not
         *        send a message.
         */
    }

    DP_SetPlayerData( plist->lpPData, flags, data, size );

    if ( !(flags & DPSET_LOCAL) )
        FIXME( "Send msg?\n" );

    return DP_OK;
}

static HRESULT DP_IF_SetPlayerName
          ( IDirectPlay2Impl* This, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags, BOOL bAnsi )
{
  lpPlayerList lpPList;

  TRACE( "(%p)->(0x%08x,%p,0x%08x,%u)\n",
         This, idPlayer, lpPlayerName, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpPList = DP_FindPlayer( This, idPlayer ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  DP_CopyDPNAMEStruct( &lpPList->lpPData->name, lpPlayerName, bAnsi );

  /* Should send a DPMSG_SETPLAYERORGROUPNAME message */
  FIXME( "Message not sent and dwFlags ignored\n" );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_SetPlayerName( IDirectPlay4A *iface, DPID idPlayer,
        DPNAME *lpPlayerName, DWORD dwFlags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_IF_SetPlayerName( This, idPlayer, lpPlayerName, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetPlayerName
          ( LPDIRECTPLAY2 iface, DPID idPlayer, LPDPNAME lpPlayerName,
            DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_IF_SetPlayerName( This, idPlayer, lpPlayerName, dwFlags, FALSE );
}

static HRESULT DP_SetSessionDesc
          ( IDirectPlay2Impl* This, LPCDPSESSIONDESC2 lpSessDesc,
            DWORD dwFlags, BOOL bInitial, BOOL bAnsi  )
{
  DWORD            dwRequiredSize;
  LPDPSESSIONDESC2 lpTempSessDesc;

  TRACE( "(%p)->(%p,0x%08x,%u,%u)\n",
         This, lpSessDesc, dwFlags, bInitial, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( dwFlags )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Only the host is allowed to update the session desc */
  if( !This->dp2->bHostInterface )
  {
    return DPERR_ACCESSDENIED;
  }

  /* FIXME: Copy into This->dp2->lpSessionDesc */
  dwRequiredSize = DP_CalcSessionDescSize( lpSessDesc, bAnsi );
  lpTempSessDesc = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwRequiredSize );

  if( lpTempSessDesc == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  /* Free the old */
  HeapFree( GetProcessHeap(), 0, This->dp2->lpSessionDesc );

  This->dp2->lpSessionDesc = lpTempSessDesc;
  /* Set the new */
  DP_CopySessionDesc( This->dp2->lpSessionDesc, lpSessDesc, bAnsi );
  if( bInitial )
  {
    /*Initializing session GUID*/
    CoCreateGuid( &(This->dp2->lpSessionDesc->guidInstance) );
  }
  /* If this is an external invocation of the interface, we should be
   * letting everyone know that things have changed. Otherwise this is
   * just an initialization and it doesn't need to be propagated.
   */
  if( !bInitial )
  {
    FIXME( "Need to send a DPMSG_SETSESSIONDESC msg to everyone\n" );
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_SetSessionDesc( IDirectPlay4A *iface,
        DPSESSIONDESC2 *lpSessDesc, DWORD dwFlags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_SetSessionDesc( This, lpSessDesc, dwFlags, FALSE, TRUE );
}

static HRESULT WINAPI DirectPlay2WImpl_SetSessionDesc
          ( LPDIRECTPLAY2 iface, LPDPSESSIONDESC2 lpSessDesc, DWORD dwFlags )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface;
  return DP_SetSessionDesc( This, lpSessDesc, dwFlags, FALSE, TRUE );
}

/* FIXME: See about merging some of this stuff with dplayx_global.c stuff */
static DWORD DP_CalcSessionDescSize( LPCDPSESSIONDESC2 lpSessDesc, BOOL bAnsi )
{
  DWORD dwSize = 0;

  if( lpSessDesc == NULL )
  {
    /* Hmmm..don't need any size? */
    ERR( "NULL lpSessDesc\n" );
    return dwSize;
  }

  dwSize += sizeof( *lpSessDesc );

  if( bAnsi )
  {
    if( lpSessDesc->u1.lpszSessionNameA )
    {
      dwSize += lstrlenA( lpSessDesc->u1.lpszSessionNameA ) + 1;
    }

    if( lpSessDesc->u2.lpszPasswordA )
    {
      dwSize += lstrlenA( lpSessDesc->u2.lpszPasswordA ) + 1;
    }
  }
  else /* UNICODE */
  {
    if( lpSessDesc->u1.lpszSessionName )
    {
      dwSize += sizeof( WCHAR ) *
        ( lstrlenW( lpSessDesc->u1.lpszSessionName ) + 1 );
    }

    if( lpSessDesc->u2.lpszPassword )
    {
      dwSize += sizeof( WCHAR ) *
        ( lstrlenW( lpSessDesc->u2.lpszPassword ) + 1 );
    }
  }

  return dwSize;
}

/* Assumes that contiguous buffers are already allocated. */
static void DP_CopySessionDesc( LPDPSESSIONDESC2 lpSessionDest,
                                LPCDPSESSIONDESC2 lpSessionSrc, BOOL bAnsi )
{
  BYTE* lpStartOfFreeSpace;

  if( lpSessionDest == NULL )
  {
    ERR( "NULL lpSessionDest\n" );
    return;
  }

  CopyMemory( lpSessionDest, lpSessionSrc, sizeof( *lpSessionSrc ) );

  lpStartOfFreeSpace = ((BYTE*)lpSessionDest) + sizeof( *lpSessionSrc );

  if( bAnsi )
  {
    if( lpSessionSrc->u1.lpszSessionNameA )
    {
      lstrcpyA( (LPSTR)lpStartOfFreeSpace,
                lpSessionDest->u1.lpszSessionNameA );
      lpSessionDest->u1.lpszSessionNameA = (LPSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace +=
        lstrlenA( lpSessionDest->u1.lpszSessionNameA ) + 1;
    }

    if( lpSessionSrc->u2.lpszPasswordA )
    {
      lstrcpyA( (LPSTR)lpStartOfFreeSpace,
                lpSessionDest->u2.lpszPasswordA );
      lpSessionDest->u2.lpszPasswordA = (LPSTR)lpStartOfFreeSpace;
    }
  }
  else /* UNICODE */
  {
    if( lpSessionSrc->u1.lpszSessionName )
    {
      lstrcpyW( (LPWSTR)lpStartOfFreeSpace,
                lpSessionDest->u1.lpszSessionName );
      lpSessionDest->u1.lpszSessionName = (LPWSTR)lpStartOfFreeSpace;
      lpStartOfFreeSpace += sizeof(WCHAR) *
        ( lstrlenW( lpSessionDest->u1.lpszSessionName ) + 1 );
    }

    if( lpSessionSrc->u2.lpszPassword )
    {
      lstrcpyW( (LPWSTR)lpStartOfFreeSpace,
                lpSessionDest->u2.lpszPassword );
      lpSessionDest->u2.lpszPassword = (LPWSTR)lpStartOfFreeSpace;
    }
  }
}


static HRESULT DP_IF_AddGroupToGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup )
{
  lpGroupData lpGData;
  lpGroupList lpNewGList;

  TRACE( "(%p)->(0x%08x,0x%08x)\n", This, idParentGroup, idGroup );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idParentGroup ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Create a player list (ie "shortcut" ) */
  lpNewGList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpNewGList ) );
  if( lpNewGList == NULL )
  {
    return DPERR_CANTADDPLAYER;
  }

  /* Add the shortcut */
  lpGData->uRef++;
  lpNewGList->lpGData = lpGData;

  /* Add the player to the list of players for this group */
  DPQ_INSERT( lpGData->groups, lpNewGList, groups );

  /* Send a ADDGROUPTOGROUP message */
  FIXME( "Not sending message\n" );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_AddGroupToGroup( IDirectPlay4A *iface, DPID idParentGroup,
        DPID idGroup )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_AddGroupToGroup( This, idParentGroup, idGroup );
}

static HRESULT WINAPI DirectPlay3WImpl_AddGroupToGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_AddGroupToGroup( This, idParentGroup, idGroup );
}

static HRESULT DP_IF_CreateGroupInGroup
          ( IDirectPlay3Impl* This, LPVOID lpMsgHdr, DPID idParentGroup,
            LPDPID lpidGroup, LPDPNAME lpGroupName, LPVOID lpData,
            DWORD dwDataSize, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupData lpGParentData;
  lpGroupList lpGList;
  lpGroupData lpGData;

  TRACE( "(%p)->(0x%08x,%p,%p,%p,0x%08x,0x%08x,%u)\n",
         This, idParentGroup, lpidGroup, lpGroupName, lpData,
         dwDataSize, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* Verify that the specified parent is valid */
  if( ( lpGParentData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This,
                                         idParentGroup ) ) == NULL
    )
  {
    return DPERR_INVALIDGROUP;
  }

  lpGData = DP_CreateGroup( (IDirectPlay2AImpl*)This, lpidGroup, lpGroupName,
                            dwFlags, idParentGroup, bAnsi );

  if( lpGData == NULL )
  {
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  /* Something else is referencing this data */
  lpGData->uRef++;

  DP_SetGroupData( lpGData, DPSET_REMOTE, lpData, dwDataSize );

  /* The list has now been inserted into the interface group list. We now
     need to put a "shortcut" to this group in the parent group */
  lpGList = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpGList ) );
  if( lpGList == NULL )
  {
    FIXME( "Memory leak\n" );
    return DPERR_CANTADDPLAYER; /* yes player not group */
  }

  lpGList->lpGData = lpGData;

  DPQ_INSERT( lpGParentData->groups, lpGList, groups );

  /* Let the SP know that we've created this group */
  if( This->dp2->spData.lpCB->CreateGroup )
  {
    DPSP_CREATEGROUPDATA data;

    TRACE( "Calling SP CreateGroup\n" );

    data.idGroup           = *lpidGroup;
    data.dwFlags           = dwFlags;
    data.lpSPMessageHeader = lpMsgHdr;
    data.lpISP             = This->dp2->spData.lpISP;

    (*This->dp2->spData.lpCB->CreateGroup)( &data );
  }

  /* Inform all other peers of the creation of a new group. If there are
   * no peers keep this quiet.
   */
  if( This->dp2->lpSessionDesc &&
      ( This->dp2->lpSessionDesc->dwFlags & DPSESSION_MULTICASTSERVER ) )
  {
    DPMSG_CREATEPLAYERORGROUP msg;

    msg.dwType = DPSYS_CREATEPLAYERORGROUP;
    msg.dwPlayerType = DPPLAYERTYPE_GROUP;
    msg.dpId = *lpidGroup;
    msg.dwCurrentPlayers = idParentGroup; /* FIXME: Incorrect? */
    msg.lpData = lpData;
    msg.dwDataSize = dwDataSize;
    msg.dpnName = *lpGroupName;

    /* FIXME: Correct to just use send effectively? */
    /* FIXME: Should size include data w/ message or just message "header" */
    /* FIXME: Check return code */
    DP_SendEx( (IDirectPlay2Impl*)This,
               DPID_SERVERPLAYER, DPID_ALLPLAYERS, 0, &msg, sizeof( msg ),
               0, 0, NULL, NULL, bAnsi );
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_CreateGroupInGroup( IDirectPlay4A *iface,
        DPID idParentGroup, DPID *lpidGroup, DPNAME *lpGroupName, void *lpData, DWORD dwDataSize,
        DWORD dwFlags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

    *lpidGroup = DPID_UNKNOWN;

    return DP_IF_CreateGroupInGroup( This, NULL, idParentGroup, lpidGroup, lpGroupName, lpData,
            dwDataSize, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_CreateGroupInGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, LPDPID lpidGroup,
            LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  *lpidGroup = DPID_UNKNOWN;

  return DP_IF_CreateGroupInGroup( This, NULL, idParentGroup, lpidGroup,
                                   lpGroupName, lpData, dwDataSize,
                                   dwFlags, FALSE );
}

static HRESULT DP_IF_DeleteGroupFromGroup
          ( IDirectPlay3Impl* This, DPID idParentGroup, DPID idGroup )
{
  lpGroupList lpGList;
  lpGroupData lpGParentData;

  TRACE("(%p)->(0x%08x,0x%08x)\n", This, idParentGroup, idGroup );

  /* Is the parent group valid? */
  if( ( lpGParentData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idParentGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Remove the group from the parent group queue */
  DPQ_REMOVE_ENTRY( lpGParentData->groups, groups, lpGData->dpid, ==, idGroup, lpGList );

  if( lpGList == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  /* Decrement the ref count */
  lpGList->lpGData->uRef--;

  /* Free up the list item */
  HeapFree( GetProcessHeap(), 0, lpGList );

  /* Should send a DELETEGROUPFROMGROUP message */
  FIXME( "message not sent\n" );

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_DeleteGroupFromGroup( IDirectPlay4A *iface,
        DPID idParentGroup, DPID idGroup )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_DeleteGroupFromGroup( This, idParentGroup, idGroup );
}

static HRESULT WINAPI DirectPlay3WImpl_DeleteGroupFromGroup
          ( LPDIRECTPLAY3 iface, DPID idParentGroup, DPID idGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_DeleteGroupFromGroup( This, idParentGroup, idGroup );
}

static BOOL DP_BuildSPCompoundAddr( LPGUID lpcSpGuid, LPVOID* lplpAddrBuf,
                                    LPDWORD lpdwBufSize )
{
  DPCOMPOUNDADDRESSELEMENT dpCompoundAddress;
  HRESULT                  hr;

  dpCompoundAddress.dwDataSize = sizeof( GUID );
  dpCompoundAddress.guidDataType = DPAID_ServiceProvider;
  dpCompoundAddress.lpData = lpcSpGuid;

  *lplpAddrBuf = NULL;
  *lpdwBufSize = 0;

  hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, *lplpAddrBuf,
                                  lpdwBufSize, TRUE );

  if( hr != DPERR_BUFFERTOOSMALL )
  {
    ERR( "can't get buffer size: %s\n", DPLAYX_HresultToString( hr ) );
    return FALSE;
  }

  /* Now allocate the buffer */
  *lplpAddrBuf = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                            *lpdwBufSize );

  hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, *lplpAddrBuf,
                                  lpdwBufSize, TRUE );
  if( FAILED(hr) )
  {
    ERR( "can't create address: %s\n", DPLAYX_HresultToString( hr ) );
    return FALSE;
  }

  return TRUE;
}

static HRESULT WINAPI IDirectPlay4AImpl_EnumConnections( IDirectPlay4A *iface,
        const GUID *lpguidApplication, LPDPENUMCONNECTIONSCALLBACK lpEnumCallback, void *lpContext,
        DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  TRACE("(%p)->(%p,%p,%p,0x%08x)\n", This, lpguidApplication, lpEnumCallback, lpContext, dwFlags );

  /* A default dwFlags (0) is backwards compatible -- DPCONNECTION_DIRECTPLAY */
  if( dwFlags == 0 )
  {
    dwFlags = DPCONNECTION_DIRECTPLAY;
  }

  if( ! ( ( dwFlags & DPCONNECTION_DIRECTPLAY ) ||
          ( dwFlags & DPCONNECTION_DIRECTPLAYLOBBY ) )
    )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( !lpEnumCallback )
  {
     return DPERR_INVALIDPARAMS;
  }

  /* Enumerate DirectPlay service providers */
  if( dwFlags & DPCONNECTION_DIRECTPLAY )
  {
    HKEY hkResult;
    LPCSTR searchSubKey    = "SOFTWARE\\Microsoft\\DirectPlay\\Service Providers";
    LPCSTR guidDataSubKey  = "Guid";
    char subKeyName[51];
    DWORD dwIndex, sizeOfSubKeyName=50;
    FILETIME filetime;

    /* Need to loop over the service providers in the registry */
    if( RegOpenKeyExA( HKEY_LOCAL_MACHINE, searchSubKey,
                         0, KEY_READ, &hkResult ) != ERROR_SUCCESS )
    {
      /* Hmmm. Does this mean that there are no service providers? */
      ERR(": no service providers?\n");
      return DP_OK;
    }


    /* Traverse all the service providers we have available */
    for( dwIndex=0;
         RegEnumKeyExA( hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
                        NULL, NULL, NULL, &filetime ) != ERROR_NO_MORE_ITEMS;
         ++dwIndex, sizeOfSubKeyName=51 )
    {

      HKEY     hkServiceProvider;
      GUID     serviceProviderGUID;
      DWORD    returnTypeGUID, sizeOfReturnBuffer = 50;
      char     returnBuffer[51];
      WCHAR    buff[51];
      DPNAME   dpName;
      BOOL     bBuildPass;

      LPVOID                   lpAddressBuffer = NULL;
      DWORD                    dwAddressBufferSize = 0;

      TRACE(" this time through: %s\n", subKeyName );

      /* Get a handle for this particular service provider */
      if( RegOpenKeyExA( hkResult, subKeyName, 0, KEY_READ,
                         &hkServiceProvider ) != ERROR_SUCCESS )
      {
         ERR(": what the heck is going on?\n" );
         continue;
      }

      if( RegQueryValueExA( hkServiceProvider, guidDataSubKey,
                            NULL, &returnTypeGUID, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
        ERR(": missing GUID registry data members\n" );
        RegCloseKey(hkServiceProvider);
        continue;
      }
      RegCloseKey(hkServiceProvider);

      /* FIXME: Check return types to ensure we're interpreting data right */
      MultiByteToWideChar( CP_ACP, 0, returnBuffer, -1, buff, sizeof(buff)/sizeof(WCHAR) );
      CLSIDFromString( buff, &serviceProviderGUID );
      /* FIXME: Have I got a memory leak on the serviceProviderGUID? */

      /* Fill in the DPNAME struct for the service provider */
      dpName.dwSize             = sizeof( dpName );
      dpName.dwFlags            = 0;
      dpName.u1.lpszShortNameA = subKeyName;
      dpName.u2.lpszLongNameA  = NULL;

      /* Create the compound address for the service provider.
       * NOTE: This is a gruesome architectural scar right now.  DP
       * uses DPL and DPL uses DP.  Nasty stuff. This may be why the
       * native dll just gets around this little bit by allocating an
       * 80 byte buffer which isn't even filled with a valid compound
       * address. Oh well. Creating a proper compound address is the
       * way to go anyways despite this method taking slightly more
       * heap space and realtime :) */

      bBuildPass = DP_BuildSPCompoundAddr( &serviceProviderGUID,
                                           &lpAddressBuffer,
                                           &dwAddressBufferSize );
      if( !bBuildPass )
      {
        ERR( "Can't build compound addr\n" );
        return DPERR_GENERIC;
      }

      /* The enumeration will return FALSE if we are not to continue */
      if( !lpEnumCallback( &serviceProviderGUID, lpAddressBuffer, dwAddressBufferSize,
                           &dpName, dwFlags, lpContext ) )
      {
         return DP_OK;
      }
    }
  }

  /* Enumerate DirectPlayLobby service providers */
  if( dwFlags & DPCONNECTION_DIRECTPLAYLOBBY )
  {
    HKEY hkResult;
    LPCSTR searchSubKey    = "SOFTWARE\\Microsoft\\DirectPlay\\Lobby Providers";
    LPCSTR guidDataSubKey  = "Guid";
    char subKeyName[51];
    DWORD dwIndex, sizeOfSubKeyName=50;
    FILETIME filetime;

    /* Need to loop over the service providers in the registry */
    if( RegOpenKeyExA( HKEY_LOCAL_MACHINE, searchSubKey,
                         0, KEY_READ, &hkResult ) != ERROR_SUCCESS )
    {
      /* Hmmm. Does this mean that there are no service providers? */
      ERR(": no service providers?\n");
      return DP_OK;
    }


    /* Traverse all the lobby providers we have available */
    for( dwIndex=0;
         RegEnumKeyExA( hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
                        NULL, NULL, NULL, &filetime ) != ERROR_NO_MORE_ITEMS;
         ++dwIndex, sizeOfSubKeyName=51 )
    {

      HKEY     hkServiceProvider;
      GUID     serviceProviderGUID;
      DWORD    returnTypeGUID, sizeOfReturnBuffer = 50;
      char     returnBuffer[51];
      WCHAR    buff[51];
      DPNAME   dpName;
      HRESULT  hr;

      DPCOMPOUNDADDRESSELEMENT dpCompoundAddress;
      LPVOID                   lpAddressBuffer = NULL;
      DWORD                    dwAddressBufferSize = 0;

      TRACE(" this time through: %s\n", subKeyName );

      /* Get a handle for this particular service provider */
      if( RegOpenKeyExA( hkResult, subKeyName, 0, KEY_READ,
                         &hkServiceProvider ) != ERROR_SUCCESS )
      {
         ERR(": what the heck is going on?\n" );
         continue;
      }

      if( RegQueryValueExA( hkServiceProvider, guidDataSubKey,
                            NULL, &returnTypeGUID, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
        ERR(": missing GUID registry data members\n" );
        RegCloseKey(hkServiceProvider);
        continue;
      }
      RegCloseKey(hkServiceProvider);

      /* FIXME: Check return types to ensure we're interpreting data right */
      MultiByteToWideChar( CP_ACP, 0, returnBuffer, -1, buff, sizeof(buff)/sizeof(WCHAR) );
      CLSIDFromString( buff, &serviceProviderGUID );
      /* FIXME: Have I got a memory leak on the serviceProviderGUID? */

      /* Fill in the DPNAME struct for the service provider */
      dpName.dwSize             = sizeof( dpName );
      dpName.dwFlags            = 0;
      dpName.u1.lpszShortNameA = subKeyName;
      dpName.u2.lpszLongNameA  = NULL;

      /* Create the compound address for the service provider.
         NOTE: This is a gruesome architectural scar right now. DP uses DPL and DPL uses DP
               nast stuff. This may be why the native dll just gets around this little bit by
               allocating an 80 byte buffer which isn't even a filled with a valid compound
               address. Oh well. Creating a proper compound address is the way to go anyways
               despite this method taking slightly more heap space and realtime :) */

      dpCompoundAddress.guidDataType = DPAID_LobbyProvider;
      dpCompoundAddress.dwDataSize   = sizeof( GUID );
      dpCompoundAddress.lpData       = &serviceProviderGUID;

      if( ( hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, lpAddressBuffer,
                                     &dwAddressBufferSize, TRUE ) ) != DPERR_BUFFERTOOSMALL )
      {
        ERR( "can't get buffer size: %s\n", DPLAYX_HresultToString( hr ) );
        return hr;
      }

      /* Now allocate the buffer */
      lpAddressBuffer = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwAddressBufferSize );

      if( ( hr = DPL_CreateCompoundAddress( &dpCompoundAddress, 1, lpAddressBuffer,
                                     &dwAddressBufferSize, TRUE ) ) != DP_OK )
      {
        ERR( "can't create address: %s\n", DPLAYX_HresultToString( hr ) );
        HeapFree( GetProcessHeap(), 0, lpAddressBuffer );
        return hr;
      }

      /* The enumeration will return FALSE if we are not to continue */
      if( !lpEnumCallback( &serviceProviderGUID, lpAddressBuffer, dwAddressBufferSize,
                           &dpName, dwFlags, lpContext ) )
      {
         HeapFree( GetProcessHeap(), 0, lpAddressBuffer );
         return DP_OK;
      }
      HeapFree( GetProcessHeap(), 0, lpAddressBuffer );
    }
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_EnumConnections( IDirectPlay4 *iface,
        const GUID *application, LPDPENUMCONNECTIONSCALLBACK enumcb, void *context, DWORD flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(%p,%p,%p,0x%08x): stub\n", This, application, enumcb, context, flags );
    return DP_OK;
}

static HRESULT DP_IF_EnumGroupsInGroup
          ( IDirectPlay3AImpl* This, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
            LPVOID lpContext, DWORD dwFlags, BOOL bAnsi )
{
  lpGroupList lpGList;
  lpGroupData lpGData;

  FIXME( "(%p)->(0x%08x,%p,%p,%p,0x%08x,%u): semi stub\n",
         This, idGroup, lpguidInstance, lpEnumPlayersCallback2,
         lpContext, dwFlags, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  if( DPQ_IS_EMPTY( lpGData->groups ) )
  {
    return DP_OK;
  }

  lpGList = DPQ_FIRST( lpGData->groups );

  for( ;; )
  {
    /* FIXME: Should check dwFlags for match here */

    if( !(*lpEnumPlayersCallback2)( lpGList->lpGData->dpid, DPPLAYERTYPE_GROUP,
                                    &lpGList->lpGData->name, dwFlags,
                                    lpContext ) )
    {
      return DP_OK; /* User requested break */
    }

    if( DPQ_IS_ENDOFLIST( lpGList->groups ) )
    {
      break;
    }

    lpGList = DPQ_NEXT( lpGList->groups );

  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_EnumGroupsInGroup( IDirectPlay4A *iface, DPID idGroup,
        GUID *lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, void *lpContext,
        DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_EnumGroupsInGroup( This, idGroup, lpguidInstance,
                                  lpEnumPlayersCallback2, lpContext, dwFlags,
                                  TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_EnumGroupsInGroup
          ( LPDIRECTPLAY3A iface, DPID idGroup, LPGUID lpguidInstance,
            LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, LPVOID lpContext,
            DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_EnumGroupsInGroup( This, idGroup, lpguidInstance,
                                  lpEnumPlayersCallback2, lpContext, dwFlags,
                                  FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetGroupConnectionSettings( IDirectPlay4A *iface,
        DWORD flags, DPID group, void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, flags, group, data, size );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetGroupConnectionSettings( IDirectPlay4 *iface, DWORD flags,
        DPID group, void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, flags, group, data, size );
    return DP_OK;
}

static BOOL CALLBACK DP_GetSpLpGuidFromCompoundAddress(
    REFGUID         guidDataType,
    DWORD           dwDataSize,
    LPCVOID         lpData,
    LPVOID          lpContext )
{
  /* Looking for the GUID of the provider to load */
  if( ( IsEqualGUID( guidDataType, &DPAID_ServiceProvider ) ) ||
      ( IsEqualGUID( guidDataType, &DPAID_LobbyProvider ) )
    )
  {
    TRACE( "Found SP/LP (%s) %s (data size = 0x%08x)\n",
           debugstr_guid( guidDataType ), debugstr_guid( lpData ), dwDataSize );

    if( dwDataSize != sizeof( GUID ) )
    {
      ERR( "Invalid sp/lp guid size 0x%08x\n", dwDataSize );
    }

    memcpy( lpContext, lpData, dwDataSize );

    /* There shouldn't be more than 1 GUID/compound address */
    return FALSE;
  }

  /* Still waiting for what we want */
  return TRUE;
}


/* Find and perform a LoadLibrary on the requested SP or LP GUID */
static HMODULE DP_LoadSP( LPCGUID lpcGuid, LPSPINITDATA lpSpData, LPBOOL lpbIsDpSp )
{
  UINT i;
  LPCSTR spSubKey         = "SOFTWARE\\Microsoft\\DirectPlay\\Service Providers";
  LPCSTR lpSubKey         = "SOFTWARE\\Microsoft\\DirectPlay\\Lobby Providers";
  LPCSTR guidDataSubKey   = "Guid";
  LPCSTR majVerDataSubKey = "dwReserved1";
  LPCSTR minVerDataSubKey = "dwReserved2";
  LPCSTR pathSubKey       = "Path";

  TRACE( " request to load %s\n", debugstr_guid( lpcGuid ) );

  /* FIXME: Cloned code with a quick hack. */
  for( i=0; i<2; i++ )
  {
    HKEY hkResult;
    LPCSTR searchSubKey;
    char subKeyName[51];
    DWORD dwIndex, sizeOfSubKeyName=50;
    FILETIME filetime;

    (i == 0) ? (searchSubKey = spSubKey ) : (searchSubKey = lpSubKey );
    *lpbIsDpSp = (i == 0) ? TRUE : FALSE;


    /* Need to loop over the service providers in the registry */
    if( RegOpenKeyExA( HKEY_LOCAL_MACHINE, searchSubKey,
                         0, KEY_READ, &hkResult ) != ERROR_SUCCESS )
    {
      /* Hmmm. Does this mean that there are no service providers? */
      ERR(": no service providers?\n");
      return 0;
    }

    /* Traverse all the service providers we have available */
    for( dwIndex=0;
         RegEnumKeyExA( hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
                        NULL, NULL, NULL, &filetime ) != ERROR_NO_MORE_ITEMS;
         ++dwIndex, sizeOfSubKeyName=51 )
    {

      HKEY     hkServiceProvider;
      GUID     serviceProviderGUID;
      DWORD    returnType, sizeOfReturnBuffer = 255;
      char     returnBuffer[256];
      WCHAR    buff[51];
      DWORD    dwTemp, len;

      TRACE(" this time through: %s\n", subKeyName );

      /* Get a handle for this particular service provider */
      if( RegOpenKeyExA( hkResult, subKeyName, 0, KEY_READ,
                         &hkServiceProvider ) != ERROR_SUCCESS )
      {
         ERR(": what the heck is going on?\n" );
         continue;
      }

      if( RegQueryValueExA( hkServiceProvider, guidDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
        ERR(": missing GUID registry data members\n" );
        continue;
      }

      /* FIXME: Check return types to ensure we're interpreting data right */
      MultiByteToWideChar( CP_ACP, 0, returnBuffer, -1, buff, sizeof(buff)/sizeof(WCHAR) );
      CLSIDFromString( buff, &serviceProviderGUID );
      /* FIXME: Have I got a memory leak on the serviceProviderGUID? */

      /* Determine if this is the Service Provider that the user asked for */
      if( !IsEqualGUID( &serviceProviderGUID, lpcGuid ) )
      {
        continue;
      }

      if( i == 0 ) /* DP SP */
      {
        len = MultiByteToWideChar( CP_ACP, 0, subKeyName, -1, NULL, 0 );
        lpSpData->lpszName = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, subKeyName, -1, lpSpData->lpszName, len );
      }

      sizeOfReturnBuffer = 255;

      /* Get dwReserved1 */
      if( RegQueryValueExA( hkServiceProvider, majVerDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
         ERR(": missing dwReserved1 registry data members\n") ;
         continue;
      }

      if( i == 0 )
          memcpy( &lpSpData->dwReserved1, returnBuffer, sizeof(lpSpData->dwReserved1) );

      sizeOfReturnBuffer = 255;

      /* Get dwReserved2 */
      if( RegQueryValueExA( hkServiceProvider, minVerDataSubKey,
                            NULL, &returnType, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) != ERROR_SUCCESS )
      {
         ERR(": missing dwReserved1 registry data members\n") ;
         continue;
      }

      if( i == 0 )
          memcpy( &lpSpData->dwReserved2, returnBuffer, sizeof(lpSpData->dwReserved2) );

      sizeOfReturnBuffer = 255;

      /* Get the path for this service provider */
      if( ( dwTemp = RegQueryValueExA( hkServiceProvider, pathSubKey,
                            NULL, NULL, (LPBYTE)returnBuffer,
                            &sizeOfReturnBuffer ) ) != ERROR_SUCCESS )
      {
        ERR(": missing PATH registry data members: 0x%08x\n", dwTemp );
        continue;
      }

      TRACE( "Loading %s\n", returnBuffer );
      return LoadLibraryA( returnBuffer );
    }
  }

  return 0;
}

static
HRESULT DP_InitializeDPSP( IDirectPlay3Impl* This, HMODULE hServiceProvider )
{
  HRESULT hr;
  LPDPSP_SPINIT SPInit;

  /* Initialize the service provider by calling SPInit */
  SPInit = (LPDPSP_SPINIT)GetProcAddress( hServiceProvider, "SPInit" );

  if( SPInit == NULL )
  {
    ERR( "Service provider doesn't provide SPInit interface?\n" );
    FreeLibrary( hServiceProvider );
    return DPERR_UNAVAILABLE;
  }

  TRACE( "Calling SPInit (DP SP entry point)\n" );

  hr = (*SPInit)( &This->dp2->spData );

  if( FAILED(hr) )
  {
    ERR( "DP SP Initialization failed: %s\n", DPLAYX_HresultToString(hr) );
    FreeLibrary( hServiceProvider );
    return hr;
  }

  /* FIXME: Need to verify the sanity of the returned callback table
   *        using IsBadCodePtr */
  This->dp2->bSPInitialized = TRUE;

  /* This interface is now initialized as a DP object */
  This->dp2->connectionInitialized = DP_SERVICE_PROVIDER;

  /* Store the handle of the module so that we can unload it later */
  This->dp2->hServiceProvider = hServiceProvider;

  return hr;
}

static
HRESULT DP_InitializeDPLSP( IDirectPlay3Impl* This, HMODULE hLobbyProvider )
{
  HRESULT hr;
  LPSP_INIT DPLSPInit;

  /* Initialize the service provider by calling SPInit */
  DPLSPInit = (LPSP_INIT)GetProcAddress( hLobbyProvider, "DPLSPInit" );

  if( DPLSPInit == NULL )
  {
    ERR( "Service provider doesn't provide DPLSPInit interface?\n" );
    FreeLibrary( hLobbyProvider );
    return DPERR_UNAVAILABLE;
  }

  TRACE( "Calling DPLSPInit (DPL SP entry point)\n" );

  hr = (*DPLSPInit)( &This->dp2->dplspData );

  if( FAILED(hr) )
  {
    ERR( "DPL SP Initialization failed: %s\n", DPLAYX_HresultToString(hr) );
    FreeLibrary( hLobbyProvider );
    return hr;
  }

  /* FIXME: Need to verify the sanity of the returned callback table
   *        using IsBadCodePtr */

  This->dp2->bDPLSPInitialized = TRUE;

  /* This interface is now initialized as a lobby object */
  This->dp2->connectionInitialized = DP_LOBBY_PROVIDER;

  /* Store the handle of the module so that we can unload it later */
  This->dp2->hDPLobbyProvider = hLobbyProvider;

  return hr;
}

static HRESULT DP_IF_InitializeConnection
          ( IDirectPlay3Impl* This, LPVOID lpConnection, DWORD dwFlags, BOOL bAnsi )
{
  HMODULE hServiceProvider;
  HRESULT hr;
  GUID guidSP;
  const DWORD dwAddrSize = 80; /* FIXME: Need to calculate it correctly */
  BOOL bIsDpSp; /* TRUE if Direct Play SP, FALSE if Direct Play Lobby SP */

  TRACE("(%p)->(%p,0x%08x,%u)\n", This, lpConnection, dwFlags, bAnsi );

  if ( lpConnection == NULL )
  {
    return DPERR_INVALIDPARAMS;
  }

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( This->dp2->connectionInitialized != NO_PROVIDER )
  {
    return DPERR_ALREADYINITIALIZED;
  }

  /* Find out what the requested SP is and how large this buffer is */
  hr = DPL_EnumAddress( DP_GetSpLpGuidFromCompoundAddress, lpConnection,
                        dwAddrSize, &guidSP );

  if( FAILED(hr) )
  {
    ERR( "Invalid compound address?\n" );
    return DPERR_UNAVAILABLE;
  }

  /* Load the service provider */
  hServiceProvider = DP_LoadSP( &guidSP, &This->dp2->spData, &bIsDpSp );

  if( hServiceProvider == 0 )
  {
    ERR( "Unable to load service provider %s\n", debugstr_guid(&guidSP) );
    return DPERR_UNAVAILABLE;
  }

  if( bIsDpSp )
  {
     /* Fill in what we can of the Service Provider required information.
      * The rest was be done in DP_LoadSP
      */
     This->dp2->spData.lpAddress = lpConnection;
     This->dp2->spData.dwAddressSize = dwAddrSize;
     This->dp2->spData.lpGuid = &guidSP;

     hr = DP_InitializeDPSP( This, hServiceProvider );
  }
  else
  {
     This->dp2->dplspData.lpAddress = lpConnection;

     hr = DP_InitializeDPLSP( This, hServiceProvider );
  }

  if( FAILED(hr) )
  {
    return hr;
  }

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_InitializeConnection( IDirectPlay4A *iface,
        void *lpConnection, DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

  /* This may not be externally invoked once either an SP or LP is initialized */
  if( This->dp2->connectionInitialized != NO_PROVIDER )
  {
    return DPERR_ALREADYINITIALIZED;
  }

  return DP_IF_InitializeConnection( This, lpConnection, dwFlags, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_InitializeConnection
          ( LPDIRECTPLAY3 iface, LPVOID lpConnection, DWORD dwFlags )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;

  /* This may not be externally invoked once either an SP or LP is initialized */
  if( This->dp2->connectionInitialized != NO_PROVIDER )
  {
    return DPERR_ALREADYINITIALIZED;
  }

  return DP_IF_InitializeConnection( This, lpConnection, dwFlags, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_SecureOpen( IDirectPlay4A *iface,
        const DPSESSIONDESC2 *lpsd, DWORD dwFlags, const DPSECURITYDESC *lpSecurity,
        const DPCREDENTIALS *lpCredentials )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    return DP_SecureOpen( This, lpsd, dwFlags, lpSecurity, lpCredentials, TRUE );
}

static HRESULT WINAPI DirectPlay3WImpl_SecureOpen
          ( LPDIRECTPLAY3 iface, LPCDPSESSIONDESC2 lpsd, DWORD dwFlags,
            LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* Yes a dp 2 interface */
  return DP_SecureOpen( This, lpsd, dwFlags, lpSecurity, lpCredentials, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_SendChatMessage( IDirectPlay4A *iface, DPID from,
        DPID to, DWORD flags, DPCHAT *chatmsg )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    FIXME("(%p)->(0x%08x,0x%08x,0x%08x,%p): stub\n", This, from, to, flags, chatmsg );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_SendChatMessage( IDirectPlay4 *iface, DPID from, DPID to,
        DWORD flags, DPCHAT *chatmsg )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x,0x%08x,%p): stub\n", This, from, to, flags, chatmsg );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_SetGroupConnectionSettings( IDirectPlay4A *iface,
        DWORD flags, DPID group, DPLCONNECTION *connection )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    FIXME("(%p)->(0x%08x,0x%08x,%p): stub\n", This, flags, group, connection );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_SetGroupConnectionSettings( IDirectPlay4 *iface, DWORD flags,
        DPID group, DPLCONNECTION *connection )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x,%p): stub\n", This, flags, group, connection );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_StartSession( IDirectPlay4A *iface, DWORD dwFlags,
        DPID idGroup )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, dwFlags, idGroup );
  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_StartSession( IDirectPlay4 *iface, DWORD flags, DPID group )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x): stub\n", This, flags, group );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetGroupFlags( IDirectPlay4A *iface, DPID idGroup,
        DWORD *lpdwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpdwFlags );
  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetGroupFlags( IDirectPlay4 *iface, DPID group,
        DWORD *flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,%p): stub\n", This, group, flags );
    return DP_OK;
}

static HRESULT DP_IF_GetGroupParent
          ( IDirectPlay3AImpl* This, DPID idGroup, LPDPID lpidGroup,
            BOOL bAnsi )
{
  lpGroupData lpGData;

  TRACE("(%p)->(0x%08x,%p,%u)\n", This, idGroup, lpidGroup, bAnsi );

  if( ( lpGData = DP_FindAnyGroup( (IDirectPlay2AImpl*)This, idGroup ) ) == NULL )
  {
    return DPERR_INVALIDGROUP;
  }

  *lpidGroup = lpGData->dpid;

  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetGroupParent( IDirectPlay4A *iface, DPID idGroup,
        DPID *lpidGroup )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_GetGroupParent( This, idGroup, lpidGroup, TRUE );
}
static HRESULT WINAPI DirectPlay3WImpl_GetGroupParent
          ( LPDIRECTPLAY3 iface, DPID idGroup, LPDPID lpidGroup )
{
  IDirectPlay3Impl *This = (IDirectPlay3Impl *)iface;
  return DP_IF_GetGroupParent( This, idGroup, lpidGroup, FALSE );
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerAccount( IDirectPlay4A *iface, DPID player,
        DWORD flags, void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
    FIXME("(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, player, flags, data, size );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetPlayerAccount( IDirectPlay4 *iface, DPID player,
        DWORD flags, void *data, DWORD *size )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x,%p,%p): stub\n", This, player, flags, data, size );
    return DP_OK;
}

static HRESULT WINAPI IDirectPlay4AImpl_GetPlayerFlags( IDirectPlay4A *iface, DPID idPlayer,
        DWORD *lpdwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idPlayer, lpdwFlags );
  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetPlayerFlags( IDirectPlay4 *iface, DPID player,
        DWORD *flags )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,%p): stub\n", This, player, flags );
    return DP_OK;
}

static HRESULT WINAPI DirectPlay4AImpl_GetGroupOwner
          ( LPDIRECTPLAY4A iface, DPID idGroup, LPDPID lpidGroupOwner )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  FIXME("(%p)->(0x%08x,%p): stub\n", This, idGroup, lpidGroupOwner );
  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_GetGroupOwner( IDirectPlay4 *iface, DPID group,
        DPID *owner )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,%p): stub\n", This, group, owner );
    return DP_OK;
}

static HRESULT WINAPI DirectPlay4AImpl_SetGroupOwner
          ( LPDIRECTPLAY4A iface, DPID idGroup , DPID idGroupOwner )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  FIXME("(%p)->(0x%08x,0x%08x): stub\n", This, idGroup, idGroupOwner );
  return DP_OK;
}

static HRESULT WINAPI IDirectPlay4Impl_SetGroupOwner( IDirectPlay4 *iface, DPID group ,
        DPID owner )
{
    IDirectPlayImpl *This = impl_from_IDirectPlay4( iface );
    FIXME( "(%p)->(0x%08x,0x%08x): stub\n", This, group, owner );
    return DP_OK;
}

static HRESULT DP_SendEx
          ( IDirectPlay2Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID, BOOL bAnsi )
{
  BOOL         bValidDestination = FALSE;

  FIXME( "(%p)->(0x%08x,0x%08x,0x%08x,%p,0x%08x,0x%08x,0x%08x,%p,%p,%u)"
         ": stub\n",
         This, idFrom, idTo, dwFlags, lpData, dwDataSize, dwPriority,
         dwTimeout, lpContext, lpdwMsgID, bAnsi );

  if( This->dp2->connectionInitialized == NO_PROVIDER )
  {
    return DPERR_UNINITIALIZED;
  }

  /* FIXME: Add parameter checking */
  /* FIXME: First call to this needs to acquire a message id which will be
   *        used for multiple sends
   */

  /* NOTE: Can't send messages to yourself - this will be trapped in receive */

  /* Verify that the message is being sent from a valid local player. The
   * from player may be anonymous DPID_UNKNOWN
   */
  if( idFrom != DPID_UNKNOWN )
  {
    if( DP_FindPlayer( This, idFrom ) == NULL )
    {
      WARN( "INFO: Invalid from player 0x%08x\n", idFrom );
      return DPERR_INVALIDPLAYER;
    }
  }

  /* Verify that the message is being sent to a valid player, group or to
   * everyone. If it's valid, send it to those players.
   */
  if( idTo == DPID_ALLPLAYERS )
  {
    bValidDestination = TRUE;

    /* See if SP has the ability to multicast. If so, use it */
    if( This->dp2->spData.lpCB->SendToGroupEx )
    {
      FIXME( "Use group sendex to group 0\n" );
    }
    else if( This->dp2->spData.lpCB->SendToGroup ) /* obsolete interface */
    {
      FIXME( "Use obsolete group send to group 0\n" );
    }
    else /* No multicast, multiplicate */
    {
      /* Send to all players we know about */
      FIXME( "Send to all players using EnumPlayersInGroup\n" );
    }
  }

  if( ( !bValidDestination ) &&
      ( DP_FindPlayer( This, idTo ) != NULL )
    )
  {
    /* Have the service provider send this message */
    /* FIXME: Could optimize for local interface sends */
    return DP_SP_SendEx( This, dwFlags, lpData, dwDataSize, dwPriority,
                         dwTimeout, lpContext, lpdwMsgID );
  }

  if( ( !bValidDestination ) &&
      ( DP_FindAnyGroup( This, idTo ) != NULL )
    )
  {
    bValidDestination = TRUE;

    /* See if SP has the ability to multicast. If so, use it */
    if( This->dp2->spData.lpCB->SendToGroupEx )
    {
      FIXME( "Use group sendex\n" );
    }
    else if( This->dp2->spData.lpCB->SendToGroup ) /* obsolete interface */
    {
      FIXME( "Use obsolete group send to group\n" );
    }
    else /* No multicast, multiplicate */
    {
      FIXME( "Send to all players using EnumPlayersInGroup\n" );
    }

#if 0
    if( bExpectReply )
    {
      DWORD dwWaitReturn;

      This->dp2->hReplyEvent = CreateEventW( NULL, FALSE, FALSE, NULL );

      dwWaitReturn = WaitForSingleObject( hReplyEvent, dwTimeout );
      if( dwWaitReturn != WAIT_OBJECT_0 )
      {
        ERR( "Wait failed 0x%08lx\n", dwWaitReturn );
      }
    }
#endif
  }

  if( !bValidDestination )
  {
    return DPERR_INVALIDPLAYER;
  }
  else
  {
    /* FIXME: Should return what the send returned */
    return DP_OK;
  }
}


static HRESULT WINAPI DirectPlay4AImpl_SendEx
          ( LPDIRECTPLAY4A iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    dwPriority, dwTimeout, lpContext, lpdwMsgID, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_SendEx
          ( LPDIRECTPLAY4 iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  IDirectPlay2Impl *This = (IDirectPlay2Impl *)iface; /* yes downcast to 2 */
  return DP_SendEx( This, idFrom, idTo, dwFlags, lpData, dwDataSize,
                    dwPriority, dwTimeout, lpContext, lpdwMsgID, FALSE );
}

static HRESULT DP_SP_SendEx
          ( IDirectPlay2Impl* This, DWORD dwFlags,
            LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout,
            LPVOID lpContext, LPDWORD lpdwMsgID )
{
  LPDPMSG lpMElem;

  FIXME( ": stub\n" );

  /* FIXME: This queuing should only be for async messages */

  lpMElem = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *lpMElem ) );
  lpMElem->msg = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwDataSize );

  CopyMemory( lpMElem->msg, lpData, dwDataSize );

  /* FIXME: Need to queue based on priority */
  DPQ_INSERT( This->dp2->sendMsgs, lpMElem, msgs );

  return DP_OK;
}

static HRESULT DP_IF_GetMessageQueue
          ( IDirectPlay4Impl* This, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(0x%08x,0x%08x,0x%08x,%p,%p,%u): semi stub\n",
         This, idFrom, idTo, dwFlags, lpdwNumMsgs, lpdwNumBytes, bAnsi );

  /* FIXME: Do we need to do idFrom and idTo sanity checking here? */
  /* FIXME: What about sends which are not immediate? */

  if( This->dp2->spData.lpCB->GetMessageQueue )
  {
    DPSP_GETMESSAGEQUEUEDATA data;

    FIXME( "Calling SP GetMessageQueue - is it right?\n" );

    /* FIXME: None of this is documented :( */

    data.lpISP        = This->dp2->spData.lpISP;
    data.dwFlags      = dwFlags;
    data.idFrom       = idFrom;
    data.idTo         = idTo;
    data.lpdwNumMsgs  = lpdwNumMsgs;
    data.lpdwNumBytes = lpdwNumBytes;

    hr = (*This->dp2->spData.lpCB->GetMessageQueue)( &data );
  }
  else
  {
    FIXME( "No SP for GetMessageQueue - fake some data\n" );
  }

  return hr;
}

static HRESULT WINAPI DirectPlay4AImpl_GetMessageQueue
          ( LPDIRECTPLAY4A iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );
  return DP_IF_GetMessageQueue( This, idFrom, idTo, dwFlags, lpdwNumMsgs,
                                lpdwNumBytes, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_GetMessageQueue
          ( LPDIRECTPLAY4 iface, DPID idFrom, DPID idTo, DWORD dwFlags,
            LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;
  return DP_IF_GetMessageQueue( This, idFrom, idTo, dwFlags, lpdwNumMsgs,
                                lpdwNumBytes, FALSE );
}

static HRESULT DP_IF_CancelMessage
          ( IDirectPlay4Impl* This, DWORD dwMsgID, DWORD dwFlags,
            DWORD dwMinPriority, DWORD dwMaxPriority, BOOL bAnsi )
{
  HRESULT hr = DP_OK;

  FIXME( "(%p)->(0x%08x,0x%08x,%u): semi stub\n",
         This, dwMsgID, dwFlags, bAnsi );

  if( This->dp2->spData.lpCB->Cancel )
  {
    DPSP_CANCELDATA data;

    TRACE( "Calling SP Cancel\n" );

    /* FIXME: Undocumented callback */

    data.lpISP          = This->dp2->spData.lpISP;
    data.dwFlags        = dwFlags;
    data.lprglpvSPMsgID = NULL;
    data.cSPMsgID       = dwMsgID;
    data.dwMinPriority  = dwMinPriority;
    data.dwMaxPriority  = dwMaxPriority;

    hr = (*This->dp2->spData.lpCB->Cancel)( &data );
  }
  else
  {
    FIXME( "SP doesn't implement Cancel\n" );
  }

  return hr;
}

static HRESULT WINAPI DirectPlay4AImpl_CancelMessage
          ( LPDIRECTPLAY4A iface, DWORD dwMsgID, DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( dwMsgID == 0 )
  {
    dwFlags |= DPCANCELSEND_ALL;
  }

  return DP_IF_CancelMessage( This, dwMsgID, dwFlags, 0, 0, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_CancelMessage
          ( LPDIRECTPLAY4 iface, DWORD dwMsgID, DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  if( dwMsgID == 0 )
  {
    dwFlags |= DPCANCELSEND_ALL;
  }

  return DP_IF_CancelMessage( This, dwMsgID, dwFlags, 0, 0, FALSE );
}

static HRESULT WINAPI DirectPlay4AImpl_CancelPriority
          ( LPDIRECTPLAY4A iface, DWORD dwMinPriority, DWORD dwMaxPriority,
            DWORD dwFlags )
{
  IDirectPlayImpl *This = impl_from_IDirectPlay4A( iface );

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  return DP_IF_CancelMessage( This, 0, DPCANCELSEND_PRIORITY, dwMinPriority,
                              dwMaxPriority, TRUE );
}

static HRESULT WINAPI DirectPlay4WImpl_CancelPriority
          ( LPDIRECTPLAY4 iface, DWORD dwMinPriority, DWORD dwMaxPriority,
            DWORD dwFlags )
{
  IDirectPlay4Impl *This = (IDirectPlay4Impl *)iface;

  if( dwFlags != 0 )
  {
    return DPERR_INVALIDFLAGS;
  }

  return DP_IF_CancelMessage( This, 0, DPCANCELSEND_PRIORITY, dwMinPriority,
                              dwMaxPriority, FALSE );
}

/* Note: Hack so we can reuse the old functions without compiler warnings */
# define XCAST(fun)     (void*)
static const IDirectPlay4Vtbl dp4_vt =
{
    IDirectPlay4Impl_QueryInterface,
    IDirectPlay4Impl_AddRef,
    IDirectPlay4Impl_Release,

  XCAST(AddPlayerToGroup)DirectPlay2WImpl_AddPlayerToGroup,
    IDirectPlay4Impl_Close,
  XCAST(CreateGroup)DirectPlay2WImpl_CreateGroup,
  XCAST(CreatePlayer)DirectPlay2WImpl_CreatePlayer,
    IDirectPlay4Impl_DeletePlayerFromGroup,
  XCAST(DestroyGroup)DirectPlay2WImpl_DestroyGroup,
  XCAST(DestroyPlayer)DirectPlay2WImpl_DestroyPlayer,
    IDirectPlay4Impl_EnumGroupPlayers,
    IDirectPlay4Impl_EnumGroups,
    IDirectPlay4Impl_EnumPlayers,
  XCAST(EnumSessions)DirectPlay2WImpl_EnumSessions,
    IDirectPlay4Impl_GetCaps,
    IDirectPlay4Impl_GetGroupData,
  XCAST(GetGroupName)DirectPlay2WImpl_GetGroupName,
    IDirectPlay4Impl_GetMessageCount,
    IDirectPlay4Impl_GetPlayerAddress,
    IDirectPlay4Impl_GetPlayerCaps,
    IDirectPlay4Impl_GetPlayerData,
  XCAST(GetPlayerName)DirectPlay2WImpl_GetPlayerName,
  XCAST(GetSessionDesc)DirectPlay2WImpl_GetSessionDesc,
    IDirectPlay4Impl_Initialize,
    IDirectPlay4Impl_Open,
  XCAST(Receive)DirectPlay2WImpl_Receive,
    IDirectPlay4Impl_Send,
    IDirectPlay4Impl_SetGroupData,
  XCAST(SetGroupName)DirectPlay2WImpl_SetGroupName,
    IDirectPlay4Impl_SetPlayerData,
  XCAST(SetPlayerName)DirectPlay2WImpl_SetPlayerName,
  XCAST(SetSessionDesc)DirectPlay2WImpl_SetSessionDesc,

  XCAST(AddGroupToGroup)DirectPlay3WImpl_AddGroupToGroup,
  XCAST(CreateGroupInGroup)DirectPlay3WImpl_CreateGroupInGroup,
  XCAST(DeleteGroupFromGroup)DirectPlay3WImpl_DeleteGroupFromGroup,
    IDirectPlay4Impl_EnumConnections,
  XCAST(EnumGroupsInGroup)DirectPlay3WImpl_EnumGroupsInGroup,
    IDirectPlay4Impl_GetGroupConnectionSettings,
  XCAST(InitializeConnection)DirectPlay3WImpl_InitializeConnection,
  XCAST(SecureOpen)DirectPlay3WImpl_SecureOpen,
    IDirectPlay4Impl_SendChatMessage,
    IDirectPlay4Impl_SetGroupConnectionSettings,
    IDirectPlay4Impl_StartSession,
    IDirectPlay4Impl_GetGroupFlags,
  XCAST(GetGroupParent)DirectPlay3WImpl_GetGroupParent,
    IDirectPlay4Impl_GetPlayerAccount,
    IDirectPlay4Impl_GetPlayerFlags,
    IDirectPlay4Impl_GetGroupOwner,
    IDirectPlay4Impl_SetGroupOwner,
  DirectPlay4WImpl_SendEx,
  DirectPlay4WImpl_GetMessageQueue,
  DirectPlay4WImpl_CancelMessage,
  DirectPlay4WImpl_CancelPriority
};
#undef XCAST

static const IDirectPlay4Vtbl dp4A_vt =
{
    IDirectPlay4AImpl_QueryInterface,
    IDirectPlay4AImpl_AddRef,
    IDirectPlay4AImpl_Release,
    IDirectPlay4AImpl_AddPlayerToGroup,
    IDirectPlay4AImpl_Close,
    IDirectPlay4AImpl_CreateGroup,
    IDirectPlay4AImpl_CreatePlayer,
    IDirectPlay4AImpl_DeletePlayerFromGroup,
    IDirectPlay4AImpl_DestroyGroup,
    IDirectPlay4AImpl_DestroyPlayer,
    IDirectPlay4AImpl_EnumGroupPlayers,
    IDirectPlay4AImpl_EnumGroups,
    IDirectPlay4AImpl_EnumPlayers,
    IDirectPlay4AImpl_EnumSessions,
    IDirectPlay4AImpl_GetCaps,
    IDirectPlay4AImpl_GetGroupData,
    IDirectPlay4AImpl_GetGroupName,
    IDirectPlay4AImpl_GetMessageCount,
    IDirectPlay4AImpl_GetPlayerAddress,
    IDirectPlay4AImpl_GetPlayerCaps,
    IDirectPlay4AImpl_GetPlayerData,
    IDirectPlay4AImpl_GetPlayerName,
    IDirectPlay4AImpl_GetSessionDesc,
    IDirectPlay4AImpl_Initialize,
    IDirectPlay4AImpl_Open,
    IDirectPlay4AImpl_Receive,
    IDirectPlay4AImpl_Send,
    IDirectPlay4AImpl_SetGroupData,
    IDirectPlay4AImpl_SetGroupName,
    IDirectPlay4AImpl_SetPlayerData,
    IDirectPlay4AImpl_SetPlayerName,
    IDirectPlay4AImpl_SetSessionDesc,
    IDirectPlay4AImpl_AddGroupToGroup,
    IDirectPlay4AImpl_CreateGroupInGroup,
    IDirectPlay4AImpl_DeleteGroupFromGroup,
    IDirectPlay4AImpl_EnumConnections,
    IDirectPlay4AImpl_EnumGroupsInGroup,
    IDirectPlay4AImpl_GetGroupConnectionSettings,
    IDirectPlay4AImpl_InitializeConnection,
    IDirectPlay4AImpl_SecureOpen,
    IDirectPlay4AImpl_SendChatMessage,
    IDirectPlay4AImpl_SetGroupConnectionSettings,
    IDirectPlay4AImpl_StartSession,
    IDirectPlay4AImpl_GetGroupFlags,
    IDirectPlay4AImpl_GetGroupParent,
    IDirectPlay4AImpl_GetPlayerAccount,
    IDirectPlay4AImpl_GetPlayerFlags,

  DirectPlay4AImpl_GetGroupOwner,
  DirectPlay4AImpl_SetGroupOwner,
  DirectPlay4AImpl_SendEx,
  DirectPlay4AImpl_GetMessageQueue,
  DirectPlay4AImpl_CancelMessage,
  DirectPlay4AImpl_CancelPriority
};

HRESULT dplay_create( REFIID riid, void **ppv )
{
    IDirectPlayImpl *obj;
    HRESULT hr;

    TRACE( "(%s, %p)\n", debugstr_guid( riid ), ppv );

    *ppv = NULL;
    obj = HeapAlloc( GetProcessHeap(), 0, sizeof( *obj ) );
    if ( !obj )
        return DPERR_OUTOFMEMORY;

    obj->IDirectPlay4A_iface.lpVtbl = &dp4A_vt;
    obj->IDirectPlay4_iface.lpVtbl = &dp4_vt;
    obj->numIfaces = 1;
    obj->ref4A = 1;
    obj->ref4 = 0;

    InitializeCriticalSection( &obj->lock );
    obj->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IDirectPlayImpl.lock");

    if ( DP_CreateDirectPlay2( obj ) )
        hr = IDirectPlayX_QueryInterface( &obj->IDirectPlay4A_iface, riid, ppv );
    else
        hr = DPERR_NOMEMORY;
    IDirectPlayX_Release( &obj->IDirectPlay4A_iface );

    return hr;
}


HRESULT DP_GetSPPlayerData( IDirectPlay2Impl* lpDP,
                            DPID idPlayer,
                            LPVOID* lplpData )
{
  lpPlayerList lpPlayer = DP_FindPlayer( lpDP, idPlayer );

  if( lpPlayer == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  *lplpData = lpPlayer->lpPData->lpSPPlayerData;

  return DP_OK;
}

HRESULT DP_SetSPPlayerData( IDirectPlay2Impl* lpDP,
                            DPID idPlayer,
                            LPVOID lpData )
{
  lpPlayerList lpPlayer = DP_FindPlayer( lpDP, idPlayer );

  if( lpPlayer == NULL )
  {
    return DPERR_INVALIDPLAYER;
  }

  lpPlayer->lpPData->lpSPPlayerData = lpData;

  return DP_OK;
}

/***************************************************************************
 *  DirectPlayEnumerateAW
 *
 *  The pointer to the structure lpContext will be filled with the
 *  appropriate data for each service offered by the OS. These services are
 *  not necessarily available on this particular machine but are defined
 *  as simple service providers under the "Service Providers" registry key.
 *  This structure is then passed to lpEnumCallback for each of the different
 *  services.
 *
 *  This API is useful only for applications written using DirectX3 or
 *  worse. It is superseded by IDirectPlay3::EnumConnections which also
 *  gives information on the actual connections.
 *
 * defn of a service provider:
 * A dynamic-link library used by DirectPlay to communicate over a network.
 * The service provider contains all the network-specific code required
 * to send and receive messages. Online services and network operators can
 * supply service providers to use specialized hardware, protocols, communications
 * media, and network resources.
 *
 */
static HRESULT DirectPlayEnumerateAW(LPDPENUMDPCALLBACKA lpEnumCallbackA,
                                     LPDPENUMDPCALLBACKW lpEnumCallbackW,
                                     LPVOID lpContext)
{
    HKEY   hkResult;
    static const WCHAR searchSubKey[] = {
	'S', 'O', 'F', 'T', 'W', 'A', 'R', 'E', '\\',
	'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '\\',
	'D', 'i', 'r', 'e', 'c', 't', 'P', 'l', 'a', 'y', '\\',
	'S', 'e', 'r', 'v', 'i', 'c', 'e', ' ', 'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r', 's', 0 };
    static const WCHAR guidKey[] = { 'G', 'u', 'i', 'd', 0 };
    static const WCHAR descW[] = { 'D', 'e', 's', 'c', 'r', 'i', 'p', 't', 'i', 'o', 'n', 'W', 0 };
    
    DWORD  dwIndex;
    FILETIME filetime;

    char  *descriptionA = NULL;
    DWORD max_sizeOfDescriptionA = 0;
    WCHAR *descriptionW = NULL;
    DWORD max_sizeOfDescriptionW = 0;
    
    if (!lpEnumCallbackA && !lpEnumCallbackW)
    {
	return DPERR_INVALIDPARAMS;
    }
    
    /* Need to loop over the service providers in the registry */
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, searchSubKey,
		      0, KEY_READ, &hkResult) != ERROR_SUCCESS)
    {
	/* Hmmm. Does this mean that there are no service providers? */
	ERR(": no service provider key in the registry - check your Wine installation !!!\n");
	return DPERR_GENERIC;
    }
    
    /* Traverse all the service providers we have available */
    dwIndex = 0;
    while (1)
    {
	WCHAR subKeyName[255]; /* 255 is the maximum key size according to MSDN */
	DWORD sizeOfSubKeyName = sizeof(subKeyName) / sizeof(WCHAR);
	HKEY  hkServiceProvider;
	GUID  serviceProviderGUID;
	WCHAR guidKeyContent[(2 * 16) + 1 + 6 /* This corresponds to '{....-..-..-..-......}' */ ];
	DWORD sizeOfGuidKeyContent = sizeof(guidKeyContent);
	LONG  ret_value;
	
	ret_value = RegEnumKeyExW(hkResult, dwIndex, subKeyName, &sizeOfSubKeyName,
				  NULL, NULL, NULL, &filetime);
	if (ret_value == ERROR_NO_MORE_ITEMS)
	    break;
	else if (ret_value != ERROR_SUCCESS)
	{
	    ERR(": could not enumerate on service provider key.\n");
	    return DPERR_EXCEPTION;
	}
	TRACE(" this time through sub-key %s.\n", debugstr_w(subKeyName));
	
	/* Open the key for this service provider */
	if (RegOpenKeyExW(hkResult, subKeyName, 0, KEY_READ, &hkServiceProvider) != ERROR_SUCCESS)
	{
	    ERR(": could not open registry key for service provider %s.\n", debugstr_w(subKeyName));
	    continue;
	}
	
	/* Get the GUID from the registry */
	if (RegQueryValueExW(hkServiceProvider, guidKey,
			     NULL, NULL, (LPBYTE) guidKeyContent, &sizeOfGuidKeyContent) != ERROR_SUCCESS)
	{
	    ERR(": missing GUID registry data member for service provider %s.\n", debugstr_w(subKeyName));
	    continue;
	}
	if (sizeOfGuidKeyContent != sizeof(guidKeyContent))
	{
	    ERR(": invalid format for the GUID registry data member for service provider %s (%s).\n", debugstr_w(subKeyName), debugstr_w(guidKeyContent));
	    continue;
	}
	CLSIDFromString(guidKeyContent, &serviceProviderGUID );
	
	/* The enumeration will return FALSE if we are not to continue.
	 *
	 * Note: on my windows box, major / minor version is 6 / 0 for all service providers
	 *       and have no relation to any of the two dwReserved1 and dwReserved2 keys.
	 *       I think that it simply means that they are in-line with DirectX 6.0
	 */
	if (lpEnumCallbackA)
	{
	    DWORD sizeOfDescription = 0;
	    
	    /* Note that this is the A case of this function, so use the A variant to get the description string */
	    if (RegQueryValueExA(hkServiceProvider, "DescriptionA",
				 NULL, NULL, NULL, &sizeOfDescription) != ERROR_SUCCESS)
	    {
		ERR(": missing 'DescriptionA' registry data member for service provider %s.\n", debugstr_w(subKeyName));
		continue;
	    }
	    if (sizeOfDescription > max_sizeOfDescriptionA)
	    {
		HeapFree(GetProcessHeap(), 0, descriptionA);
		max_sizeOfDescriptionA = sizeOfDescription;
	    }
	    descriptionA = HeapAlloc(GetProcessHeap(), 0, sizeOfDescription);
	    RegQueryValueExA(hkServiceProvider, "DescriptionA",
			     NULL, NULL, (LPBYTE) descriptionA, &sizeOfDescription);
	    
	    if (!lpEnumCallbackA(&serviceProviderGUID, descriptionA, 6, 0, lpContext))
		goto end;
	}
	else
	{
	    DWORD sizeOfDescription = 0;
	    
	    if (RegQueryValueExW(hkServiceProvider, descW,
				 NULL, NULL, NULL, &sizeOfDescription) != ERROR_SUCCESS)
	    {
		ERR(": missing 'DescriptionW' registry data member for service provider %s.\n", debugstr_w(subKeyName));
		continue;
	    }
	    if (sizeOfDescription > max_sizeOfDescriptionW)
	    {
		HeapFree(GetProcessHeap(), 0, descriptionW);
		max_sizeOfDescriptionW = sizeOfDescription;
	    }
	    descriptionW = HeapAlloc(GetProcessHeap(), 0, sizeOfDescription);
	    RegQueryValueExW(hkServiceProvider, descW,
			     NULL, NULL, (LPBYTE) descriptionW, &sizeOfDescription);

	    if (!lpEnumCallbackW(&serviceProviderGUID, descriptionW, 6, 0, lpContext))
		goto end;
	}
      
      dwIndex++;
  }

 end:
    HeapFree(GetProcessHeap(), 0, descriptionA);
    HeapFree(GetProcessHeap(), 0, descriptionW);
    
    return DP_OK;
}

/***************************************************************************
 *  DirectPlayEnumerate  [DPLAYX.9]
 *  DirectPlayEnumerateA [DPLAYX.2]
 */
HRESULT WINAPI DirectPlayEnumerateA(LPDPENUMDPCALLBACKA lpEnumCallback, LPVOID lpContext )
{
    TRACE("(%p,%p)\n", lpEnumCallback, lpContext);
    
    return DirectPlayEnumerateAW(lpEnumCallback, NULL, lpContext);
}

/***************************************************************************
 *  DirectPlayEnumerateW [DPLAYX.3]
 */
HRESULT WINAPI DirectPlayEnumerateW(LPDPENUMDPCALLBACKW lpEnumCallback, LPVOID lpContext )
{
    TRACE("(%p,%p)\n", lpEnumCallback, lpContext);
    
    return DirectPlayEnumerateAW(NULL, lpEnumCallback, lpContext);
}

typedef struct tagCreateEnum
{
  LPVOID  lpConn;
  LPCGUID lpGuid;
} CreateEnumData, *lpCreateEnumData;

/* Find and copy the matching connection for the SP guid */
static BOOL CALLBACK cbDPCreateEnumConnections(
    LPCGUID     lpguidSP,
    LPVOID      lpConnection,
    DWORD       dwConnectionSize,
    LPCDPNAME   lpName,
    DWORD       dwFlags,
    LPVOID      lpContext)
{
  lpCreateEnumData lpData = (lpCreateEnumData)lpContext;

  if( IsEqualGUID( lpguidSP, lpData->lpGuid ) )
  {
    TRACE( "Found SP entry with guid %s\n", debugstr_guid(lpData->lpGuid) );

    lpData->lpConn = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                dwConnectionSize );
    CopyMemory( lpData->lpConn, lpConnection, dwConnectionSize );

    /* Found the record that we were looking for */
    return FALSE;
  }

  /* Haven't found what were looking for yet */
  return TRUE;
}


/***************************************************************************
 *  DirectPlayCreate [DPLAYX.1]
 *
 */
HRESULT WINAPI DirectPlayCreate
( LPGUID lpGUID, LPDIRECTPLAY *lplpDP, IUnknown *pUnk )
{
  HRESULT hr;
  LPDIRECTPLAY3A lpDP3A;
  CreateEnumData cbData;

  TRACE( "lpGUID=%s lplpDP=%p pUnk=%p\n", debugstr_guid(lpGUID), lplpDP, pUnk );

  if( pUnk != NULL )
  {
    return CLASS_E_NOAGGREGATION;
  }

  if( (lplpDP == NULL) || (lpGUID == NULL) )
  {
    return DPERR_INVALIDPARAMS;
  }

  /* Create an IDirectPlay object. We don't support that so we'll cheat and
     give them an IDirectPlay2A object and hope that doesn't cause problems */
  if ( dplay_create( &IID_IDirectPlay2A, (void**)lplpDP ) != DP_OK )
    return DPERR_UNAVAILABLE;

  if( IsEqualGUID( &GUID_NULL, lpGUID ) )
  {
    /* The GUID_NULL means don't bind a service provider. Just return the
       interface as is */
    return DP_OK;
  }

  /* Bind the desired service provider since lpGUID is non NULL */
  TRACE( "Service Provider binding for %s\n", debugstr_guid(lpGUID) );

  /* We're going to use a DP3 interface */
  hr = IDirectPlayX_QueryInterface( *lplpDP, &IID_IDirectPlay3A,
                                    (LPVOID*)&lpDP3A );
  if( FAILED(hr) )
  {
    ERR( "Failed to get DP3 interface: %s\n", DPLAYX_HresultToString(hr) );
    return hr;
  }

  cbData.lpConn = NULL;
  cbData.lpGuid = lpGUID;

  /* We were given a service provider, find info about it... */
  hr = IDirectPlayX_EnumConnections( lpDP3A, NULL, cbDPCreateEnumConnections,
                                     &cbData, DPCONNECTION_DIRECTPLAY );
  if( ( FAILED(hr) ) ||
      ( cbData.lpConn == NULL )
    )
  {
    ERR( "Failed to get Enum for SP: %s\n", DPLAYX_HresultToString(hr) );
    IDirectPlayX_Release( lpDP3A );
    return DPERR_UNAVAILABLE;
  }

  /* Initialize the service provider */
  hr = IDirectPlayX_InitializeConnection( lpDP3A, cbData.lpConn, 0 );
  if( FAILED(hr) )
  {
    ERR( "Failed to Initialize SP: %s\n", DPLAYX_HresultToString(hr) );
    HeapFree( GetProcessHeap(), 0, cbData.lpConn );
    IDirectPlayX_Release( lpDP3A );
    return hr;
  }

  /* Release our version of the interface now that we're done with it */
  IDirectPlayX_Release( lpDP3A );
  HeapFree( GetProcessHeap(), 0, cbData.lpConn );

  return DP_OK;
}
