#pragma once
#include <synchapi.h>
#include <windows.h>

class CCriticalSection
{
public:
    CCriticalSection(void)
    {
        //InitializeCriticalSection(&m_CS);
        InitializeCriticalSectionAndSpinCount(&m_CS, 4000);  //SpinCount ��� CriticalSection ���� ó��(Win7 ���� ����)
    }

    ~CCriticalSection(void)
    {
        DeleteCriticalSection(&m_CS);
    }

    virtual void Lock(void)
    {
        EnterCriticalSection(&m_CS);
    }

    virtual void Unlock(void)
    {
        LeaveCriticalSection(&m_CS);
        Sleep(0);
    }

    virtual bool Try(void)
    {
        return false;
    }

private:
    CRITICAL_SECTION m_CS;
};

class CAutoLock
{
public:
    CAutoLock(const CCriticalSection& CS, bool bForce = true)
        : m_CS(*const_cast<CCriticalSection*>(&CS)),
        m_bLocked(false)
    {
        if (bForce)
        {
            m_CS.Lock();
            m_bLocked = true;
        }
        else
        {
            m_bLocked = m_CS.Try();
        }
    }

    CAutoLock(const CCriticalSection* pCS, bool bForce = true)
        : m_CS(*const_cast<CCriticalSection*>(pCS)),
        m_bLocked(false)
    {
        if (bForce)
        {
            m_CS.Lock();
            m_bLocked = true;
        }
        else
        {
            m_bLocked = m_CS.Try();
        }
    }

    virtual ~CAutoLock(void)
    {
        printf("���~");
        if (m_bLocked)
            m_CS.Unlock();
    }

    bool IsLocked(void)
    {
        bool bIsLockable;
        bIsLockable = m_CS.Try();

        if (bIsLockable)
            m_CS.Unlock();

        return bIsLockable;
    }

    bool IsLockable(void)
    {
        return m_bLocked;
    }

private:
    void              operator = (CAutoLock&);
    CCriticalSection& m_CS;
    bool              m_bLocked;
};

