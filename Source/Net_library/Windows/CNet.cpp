#if defined WIN32 || defined WIN64

#include "CNet.h"
#include "Tools.h"
#include "iocp.h"
#include "Header.h"

CNet::CNet(const s8 nThreadCount, const s32 nWaitMs) {
    LogModuleInit();
    m_nThreadCount = nThreadCount;
    m_nWaitMs = nWaitMs;
    m_demo = false;
    memset(m_szCallAddress, 0, sizeof(m_szCallAddress));

    s32 err;
    s32 code = iocp_init(nWaitMs, &err);
    if (code != ERROR_NO_ERROR) {
        LOG_ERROR("init iocp error, last code : %d", err);
        ASSERT(false);
    }

    for (s32 i=0; i<nThreadCount; i++) {
        HANDLE hThread = ::CreateThread(NULL, 0, CNet::NetLoop, (LPVOID)this, 0, NULL);
        CloseHandle(hThread);
    }
}

CNet::~CNet() {
    LogModuleStop();
}

bool CNet::CListen(const char * pStrIP, const s32 nPort, const void * pContext, const s32 nBacklog) {
    s32 err;
    s32 code = async_listen(pStrIP, nPort, &err, (char *)pContext, nBacklog);
    if (code != ERROR_NO_ERROR) {
        LOG_ERROR("async_listen error, last code : %d", err);
        return false;
    }

    return true;
}

bool CNet::CConnectEx(const char * pStrIP, const s32 nPort, const void * pContext) {
    s32 err;
    if (async_connect(pStrIP, nPort, &err, (char *)pContext) != ERROR_NO_ERROR) {
        LOG_ERROR("async_connect error, last code : %d", err);
        return false;
    }

    return true;
}

void CNet::CSend(const s32 nConnectID, const void * pData, const s32 nSize) {
    CConnection * pCConnection = m_ConnectPool[nConnectID];
    if (pCConnection->bShutdown) {
        return;
    }
    s32 size = pCConnection->stream.size();
    pCConnection->stream.in(pData, nSize);
    if (0 == size) {
        struct iocp_event * pEvent = malloc_event();
        ASSERT(pEvent);
        pEvent->event = EVENT_ASYNC_SEND;
        pEvent->wbuf.buf = (char *) pCConnection->stream.buff();
        pEvent->wbuf.len = nSize;
        pEvent->p = pCConnection;
        pEvent->s = pCConnection->s;
        s32 err;
        if (ERROR_NO_ERROR != async_send(pEvent, &err, pCConnection)) {
            //some problem in here must be killed// like 10054
            //ASSERT(false);
            safe_shutdown(pEvent);
        }
    }

}

bool CNet::CClose(const s32 nConnectID) {
    CConnection * pCConnection = m_ConnectPool[nConnectID];
    pCConnection->bShutdown = true;
//     if (0 == pCConnection->stream.size()) {
//         shutdown(pCConnection->s, SD_BOTH);
//     }
    return true;
}

THREAD_FUN CNet::NetLoop(LPVOID p) {
    CNet * pThis = (CNet *)p;
    if (NULL == pThis) {
        ASSERT(false);
        return 0;
    }
    s32 code = 0;
    s32 err = 0;
    struct iocp_event * pEvent;
    while (true) {
        pEvent = NULL;
        code = get_event(&pEvent, &err);
        if (ERROR_NO_ERROR == code && pEvent != NULL) {
            //LOG_TRACE("event %d, code %d, last error %d", pEvent->event, code, err);
            pThis->m_queue.add(pEvent);
        } else {
            if (pEvent != NULL) {
                LOG_ERROR("event %d, code %d, error %d", pEvent->event, code, err);
            }
            CSleep(1);
        }
    }
}


void CNet::DealConnectEvent(struct iocp_event * pEvent) {
    ASSERT(m_szCallAddress[CALL_CONNECTED]);
    if (m_szCallAddress[CALL_CONNECTED] != NULL) {
        if (ERROR_SUCCESS == pEvent->nerron) {
            s32 nConnectID = m_ConnectPool.CreateID();
            m_ConnectPool[nConnectID]->s = pEvent->s;
            SafeSprintf(m_ConnectPool[nConnectID]->szIP, sizeof(m_ConnectPool[nConnectID]->szIP),
                "%s", inet_ntoa(pEvent->remote.sin_addr));
            m_ConnectPool[nConnectID]->nPort = ntohs(pEvent->remote.sin_port);

            m_szCallAddress[CALL_CONNECTED](nConnectID, pEvent->p, 0);
            pEvent->event = EVENT_ASYNC_RECV;
            pEvent->wbuf.buf = pEvent->buff;
            pEvent->wbuf.len = sizeof(pEvent->buff);
            m_ConnectPool[nConnectID]->pContext = pEvent->p;
            pEvent->p = m_ConnectPool[nConnectID];
            s32 err, code;
            if (ERROR_NO_ERROR != (code = async_recv(pEvent, &err, pEvent->p))) {
                m_szCallAddress[CALL_CONNECT_FAILED](-1, pEvent->p, 0);
                SafeShutdwon(pEvent, SD_BOTH);
                LOG_ERROR("async_recv error %d code %d", err, code);
                return;
            }
        } else {
            m_szCallAddress[CALL_CONNECT_FAILED](-1, pEvent->p, 0);
            SafeShutdwon(pEvent, SD_BOTH);
        }

    }

}

void CNet::DealAcceptEvent(struct iocp_event * pEvent) {
    ASSERT(m_szCallAddress[CALL_REMOTE_CONNECTED]);
    if (m_szCallAddress[CALL_REMOTE_CONNECTED] != NULL) {
        if (ERROR_SUCCESS == pEvent->nerron) {
            s32 nConnectID = m_ConnectPool.CreateID();

            m_ConnectPool[nConnectID]->s = pEvent->s;
            SafeSprintf(m_ConnectPool[nConnectID]->szIP, sizeof(m_ConnectPool[nConnectID]->szIP),
                "%s", inet_ntoa(pEvent->remote.sin_addr));
            m_ConnectPool[nConnectID]->nPort = ntohs(pEvent->remote.sin_port);

            m_szCallAddress[CALL_REMOTE_CONNECTED](nConnectID, pEvent->p, 0);

            pEvent->event = EVENT_ASYNC_RECV;
            pEvent->wbuf.buf = pEvent->buff;
            pEvent->wbuf.len = sizeof(pEvent->buff);
            m_ConnectPool[nConnectID]->pContext = pEvent->p;
            pEvent->p = m_ConnectPool[nConnectID];
            s32 err;
            if (ERROR_NO_ERROR != async_recv(pEvent, &err, pEvent->p)) {
                ASSERT(false);
                SafeShutdwon(pEvent, SD_BOTH);
                return;
            }
        } else {
            ASSERT(false);
            SafeShutdwon(pEvent, SD_BOTH);
        }
    }
}

void CNet::DealRecvEvent(struct iocp_event * pEvent) {
    CConnection * pCConnection = (CConnection *) (pEvent->p);
    if (ERROR_SUCCESS == pEvent->nerron && pEvent->ioBytes != 0) {
        m_szCallAddress[CALL_RECV_DATA](m_ConnectPool.QueryID(pCConnection), pEvent->wbuf.buf, pEvent->ioBytes);
        if (pCConnection->bShutdown) {
            m_szCallAddress[CALL_CONNECTION_BREAK](m_ConnectPool.QueryID(pCConnection), pCConnection->pContext, 0);
            SafeShutdwon(pEvent, SD_RECEIVE);
            return;
        }

        pEvent->event = EVENT_ASYNC_RECV;
        pEvent->wbuf.buf = pEvent->buff;
        pEvent->wbuf.len = sizeof(pEvent->buff);
        s32 err;
        if (ERROR_NO_ERROR != async_recv(pEvent, &err, pEvent->p)) {
            ASSERT(false);
            m_szCallAddress[CALL_CONNECTION_BREAK](m_ConnectPool.QueryID(pCConnection), pCConnection->pContext, 0);
            SafeShutdwon(pEvent, SD_RECEIVE);
            return;
        }
    } else {
        m_szCallAddress[CALL_CONNECTION_BREAK](m_ConnectPool.QueryID(pCConnection), pCConnection->pContext, 0);
        SafeShutdwon(pEvent, SD_RECEIVE);
    }
}

void CNet::DealSendEvent(struct iocp_event * pEvent) {
    if (ERROR_SUCCESS == pEvent->nerron) {
        CConnection * pCConnection = (CConnection *) (pEvent->p);
        pCConnection->stream.out(pEvent->ioBytes);
        s32 nSize = pCConnection->stream.size();
        if (0 == nSize) {
            if (pCConnection->bShutdown) {
                SafeShutdwon(pEvent, SD_SEND);
            }
        } else {
            s32 err;
            pEvent->wbuf.buf = (char *)pCConnection->stream.buff();
            pEvent->wbuf.len = nSize;
            if (ERROR_NO_ERROR != async_send(pEvent, &err, pEvent->p)) {
                ASSERT(false);
                SafeShutdwon(pEvent, SD_SEND);
            }
        }
    } else {
        SafeShutdwon(pEvent, SD_SEND);
    }
}

THREAD_FUN CNet::DemonLoop(LPVOID p) {
    CNet * pThis = (CNet *)p;
    s64 lTick = ::GetCurrentTimeTick();
    while (true) {
        struct iocp_event * pEvent = NULL;
        if (pThis->m_queue.read(pEvent)) {
            if (pEvent != NULL) {
                switch (pEvent->event) {
                case EVENT_ASYNC_ACCEPT:
                    pThis->DealAcceptEvent(pEvent);
                    break;
                case EVENT_ASYNC_CONNECT:
                    pThis->DealConnectEvent(pEvent);
                    break;
                case EVENT_ASYNC_RECV:
                    pThis->DealRecvEvent(pEvent);
                    break;
                case EVENT_ASYNC_SEND:
                    pThis->DealSendEvent(pEvent);
                    break;
                default:
                    break;
                }
            }
        }

        if(!pThis->m_demo) {
            if (::GetCurrentTimeTick() - lTick > (u32)(pThis->m_nFrameMs) && pThis->m_nFrameMs != 0) {
                break;
            }
        }

    }
    return 0;
}

void CNet::CLoop(bool demon, s32 nFramems) {
    if (demon) {
        m_demo = true;
        HANDLE hThread = ::CreateThread(NULL, 0, CNet::DemonLoop, (LPVOID)this, 0, NULL);
        CloseHandle(hThread);
    } else {
        if (m_demo) {
            LOG_ERROR("has already looped as demon");
            return;
        }
        m_nFrameMs = nFramems;
        DemonLoop(this);
    }
}

void CNet::CRemoteInfo(const s32 nConnectID, const char * & ip, s32 & nPort) {
    ip = m_ConnectPool[nConnectID]->szIP;
    nPort = m_ConnectPool[nConnectID]->nPort;
}

void CNet::CSetCallBackAddress(const CALLBACK_TYPE eType, const CALL_FUN address) {
    if (eType >= CALL_TYPE_COUNT) {
        LOG_ERROR("unknown call type");
        ASSERT(false);
    } else {
        m_szCallAddress[eType] = address;
    }
}

#endif //#if defined WIN32 || defined WIN64
