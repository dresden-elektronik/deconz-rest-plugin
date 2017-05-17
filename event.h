#ifndef EVENT_H
#define EVENT_H

#include <QString>

class Event
{
public:
    Event();
    Event(const char *resource, const char *what, const QString &id);
    Event(const char *resource, const char *what, int num);

    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }
    int num() const { return m_num; }


private:
    const char *m_resource;
    const char *m_what;
    QString m_id;
    int m_num;
};

#endif // EVENT_H
