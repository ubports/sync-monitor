#include "notify-message.h"

NotifyMessage *NotifyMessage::m_instance = 0;

void NotifyMessage::destroy()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

NotifyMessage *NotifyMessage::instance()
{
    if (m_instance == 0) {
        m_instance = new NotifyMessage();
    }
    return m_instance;
}
