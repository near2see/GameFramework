#ifndef UI_WINDOWMSGLOOP_H
#define UI_WINDOWMSGLOOP_H

#include <event2/event.h>
#include <event2/event_struct.h>
#include <ui/gdi/Gdi.h>

class WindowMsgLoop
{
public:
    WindowMsgLoop();

    void Run();
    BOOL Event();
    BOOL PumpMessage();
    BOOL IsIdleMessage(MSG* pMsg);
    BOOL OnIdle(LONG lCount);

    using MSGMAP = std::unordered_map<UINT, CString>;
    LPCTSTR DebugGetMessageName(UINT message);

    void SetEventBase(struct event_base*);

    static friend void msg_timer(evutil_socket_t fd, short event, void* arg);

private:
    int m_nDisablePumpCount{ 0 };
    UINT m_nMsgLast{ 0 };
    MSG m_msg{ 0 };
    CPoint m_ptMousePos;
    MSGMAP m_msgMap;

    struct event_base* evbase{ nullptr };
    struct event msgtimer {0};
};

#endif