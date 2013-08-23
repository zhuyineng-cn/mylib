#if defined WIN32 || defined WIN64
#include "CSockDataMgr.h"
#include <algorithm>

CSockDataMgr::CSockDataMgr(const u16 size) {
    m_pSockDataArray = NEW SockData[size];
    m_nMaxCount = size;
    m_nCurrentMark = 1;
    m_IdRecycleBin.clear();
}

CSockDataMgr::~CSockDataMgr() {
    if(m_pSockDataArray != NULL) {
        delete[] m_pSockDataArray;
        m_pSockDataArray = NULL;
    }
}

SockData * CSockDataMgr::CreateSockData() {

    SockData * pSockData = NULL;
    if (m_IdRecycleBin.empty()) {
        CAutoLock lock(&m_lock);
        if(m_nCurrentMark > m_nMaxCount) {
            ECHO_ERROR("SockPlus Pool is full, current count : %d", m_nCurrentMark);
            return NULL;
        }
        pSockData = m_pSockDataArray + m_nCurrentMark++ - 1;
        pSockData->m_id = m_nCurrentMark - 1;
    } else {
        CAutoLock lock(&m_lock);
        LIST_ID::iterator itor = m_IdRecycleBin.begin();
        pSockData = m_pSockDataArray + *itor - 1;
        pSockData->m_id = *itor;
        m_IdRecycleBin.erase(itor);
    }

    return pSockData;
}

SockData * CSockDataMgr::QuerySockData(const u16 id) {

    CAutoLock lock(&m_lock);
    if(!m_IdRecycleBin.empty()) {
        LIST_ID::iterator iend = m_IdRecycleBin.end();
        LIST_ID::iterator ifind = ::find(m_IdRecycleBin.begin(), iend, id);
        if (ifind != iend) {
            ECHO_WARN("Cant find the sockdata which id is %d", id);
            return NULL;
        }
    }

    if (id >= m_nCurrentMark) {
        ASSERT(false);
        return NULL;
    }

    return m_pSockDataArray + id - 1;
}

void CSockDataMgr::RecoveSockData(SockData * pSockData) {

    CAutoLock lock(&m_lock);
    pSockData->ClearBuff();
    pSockData->m_waitclose = false;

    LIST_ID::iterator iend = m_IdRecycleBin.end();
    LIST_ID::iterator ifind = ::find(m_IdRecycleBin.begin(), iend, pSockData->m_id);
    if (ifind != iend) {
        ECHO_WARN("Cant find the sockdata which id is %d", pSockData->m_id);
        return;
    }
    m_IdRecycleBin.push_back(pSockData->m_id);
}

#endif //#if defined WIN32 || defined WIN64