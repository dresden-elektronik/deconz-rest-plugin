#ifndef EVENT_H
#define EVENT_H

#include <QString>

class Resource;
class ResourceItem;

class Event
{
public:
    Event();
    Event(const char *resource, const char *what, const QString &id, ResourceItem *item);
    Event(const char *resource, const char *what, const QString &id, int num = 0);
    Event(const char *resource, const char *what, int num);


    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }
    int num() const { return m_num; }
    int numPrevious() const { return m_numPrev; }


private:
    const char *m_resource;
    const char *m_what;
    QString m_id;
    int m_num;
    int m_numPrev;
};

#endif // EVENT_H
