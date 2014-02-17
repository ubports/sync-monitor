#include <QtCore/QString>

class NotifyMessage
{
public:
    void destroy();
    void show(const QString &title, const QString &msg);

    static NotifyMessage *instance();
private:
    static NotifyMessage *m_instance;

    NotifyMessage();
    ~NotifyMessage();
};
