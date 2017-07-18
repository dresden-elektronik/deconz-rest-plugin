#include "resourcelinks.h"

Resourcelinks::Resourcelinks() :
    state(StateNormal),
    m_needSaveDatabase(false)
{
}

bool Resourcelinks::needSaveDatabase() const
{
    return m_needSaveDatabase;
}

void Resourcelinks::setNeedSaveDatabase(bool needSave)
{
    m_needSaveDatabase = needSave;
}
