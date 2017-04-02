#ifndef EVENT_H
#define EVENT_H

#include <QString>

class Event
{
public:
    Event();
    Event(const char *resource, const char *what, const QString &id);

    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }

private:
    const char *m_resource;
    const char *m_what;
    QString m_id;
};

#endif // EVENT_H
